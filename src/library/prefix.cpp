// Prospero BlackBox - The shared Wine prefix, made from the bare prefix the manager carries.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/prefix.hpp"
#include "core/save_file.hpp"
#include "library/install_plan.hpp"
#include "library/tree.hpp"
#include "third_party/miniz/miniz.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace store::library
{
namespace
{
bool is_file(const std::string &path)
{
    struct stat info{};
    return lstat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool is_dir(const std::string &path)
{
    struct stat info{};
    return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

// path's folders under root, made as needed (path is already a safe relative path).
bool make_parents(const std::string &root, const std::string &path)
{
    for (auto slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
    {
        const auto folder = root + "/" + path.substr(0, slash);
        if (mkdir(folder.c_str(), 0777) != 0 && errno != EEXIST)
            return false;
    }
    return true;
}

struct Sink
{
    int fd = -1;
    std::uint64_t *written = nullptr;
    const std::atomic<bool> *cancel = nullptr;
    const std::function<void(std::uint64_t, std::uint64_t)> *progress = nullptr;
    std::uint64_t total = 0;
};

size_t write_piece(void *opaque, mz_uint64, const void *data, size_t size)
{
    auto &sink = *static_cast<Sink *>(opaque);
    if (sink.cancel->load())
        return 0;
    const char *bytes = static_cast<const char *>(data);
    std::size_t left = size;
    while (left > 0)
    {
        const ssize_t step = write(sink.fd, bytes, left);
        if (step <= 0)
            return 0;
        bytes += step;
        left -= static_cast<std::size_t>(step);
    }
    *sink.written += size;
    if (*sink.progress)
        (*sink.progress)(*sink.written, sink.total);
    return size;
}

struct Zip
{
    mz_zip_archive archive{};
    bool open = false;
    ~Zip()
    {
        if (open)
            mz_zip_reader_end(&archive);
    }
};
} // namespace

bool prefix_ready(const std::string &win_root, const std::string &name)
{
    const auto prefix = win_root + "/prefixes/" + name;
    return is_file(prefix + "/system.reg") && is_file(prefix + "/user.reg") &&
           is_dir(prefix + "/drive_c/windows");
}

bool ensure_prefix(const std::string &win_root, const std::string &name, const std::string &zip,
                   const std::function<void(std::uint64_t done, std::uint64_t total)> &progress,
                   const std::atomic<bool> &cancel, std::string &error)
{
    if (prefix_ready(win_root, name))
        return true;
    const auto prefixes = win_root + "/prefixes";
    const auto final_path = prefixes + "/" + name;
    if (!safe_relative(name) || name.find('/') != std::string::npos)
    {
        error = "The prefix name " + name + " isn't a plain name";
        return false;
    }
    if (access(final_path.c_str(), F_OK) == 0)
    {
        error = final_path + " is there but isn't a Wine prefix; it was left as it is";
        return false;
    }
    Zip archive;
    archive.open = mz_zip_reader_init_file(&archive.archive, zip.c_str(), 0);
    if (!archive.open)
    {
        error = "The bare prefix the manager carries can't be read (" + zip + ")";
        return false;
    }
    const mz_uint count = mz_zip_reader_get_num_files(&archive.archive);
    std::uint64_t total = 0;
    for (mz_uint i = 0; i < count; ++i)
    {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive.archive, i, &stat))
        {
            error = "The bare prefix is damaged";
            return false;
        }
        std::string path = stat.m_filename;
        if (!path.empty() && path.back() == '/')
            path.pop_back();
        const unsigned mode = stat.m_external_attr >> 16;
        if (!safe_relative(path) || S_ISLNK(mode) || stat.m_is_encrypted || !stat.m_is_supported)
        {
            error = "The bare prefix holds an entry it may not: " + std::string(stat.m_filename);
            return false;
        }
        total += stat.m_uncomp_size;
    }
    const auto building = final_path + ".unpacking";
    if (!hui::save::ensure_directory(win_root) || !hui::save::ensure_directory(prefixes) ||
        (access(building.c_str(), F_OK) == 0 && !remove_all(building)) ||
        mkdir(building.c_str(), 0777) != 0)
    {
        error = "Can't make " + building + ": " + std::strerror(errno);
        return false;
    }
    const auto fail = [&](const std::string &why)
    {
        error = cancel.load() ? "Cancelled" : why;
        remove_all(building);
        return false;
    };
    std::uint64_t written = 0;
    for (mz_uint i = 0; i < count; ++i)
    {
        mz_zip_archive_file_stat stat{};
        mz_zip_reader_file_stat(&archive.archive, i, &stat);
        std::string path = stat.m_filename;
        const bool directory = mz_zip_reader_is_file_a_directory(&archive.archive, i);
        if (directory && !path.empty() && path.back() == '/')
            path.pop_back();
        if (!make_parents(building, path))
            return fail("Can't make the folders for " + path + ": " + std::strerror(errno));
        const auto target = building + "/" + path;
        if (directory)
        {
            if (mkdir(target.c_str(), 0777) != 0 && errno != EEXIST)
                return fail("Can't make " + path + ": " + std::strerror(errno));
            continue;
        }
        Sink sink{::open(target.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666), &written,
                  &cancel, &progress, total};
        if (sink.fd < 0)
            return fail("Can't write " + path + ": " + std::strerror(errno));
        const std::uint64_t before = written;
        const bool ok = mz_zip_reader_extract_to_callback(&archive.archive, i, write_piece, &sink, 0) &&
                        written - before == stat.m_uncomp_size && fsync(sink.fd) == 0;
        if (close(sink.fd) != 0 || !ok)
            return fail("Can't unpack " + path);
    }
    if (!prefix_ready(win_root, name + ".unpacking"))
        return fail("The bare prefix isn't a Wine prefix (no system.reg, user.reg or drive_c/windows)");
    if (rename(building.c_str(), final_path.c_str()) != 0)
        return fail("Can't put the prefix in place: " + std::string(std::strerror(errno)));
    return true;
}
} // namespace store::library
