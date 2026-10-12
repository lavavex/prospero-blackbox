// Prospero BlackBox - The user's clean copy of a game: found, checked, imported.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/base_game.hpp"
#include "third_party/miniz/miniz.h"
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

namespace store::library
{
namespace
{
// ---- the games ----------------------------------------------------------------

// Most Wanted (2005) for PC exists as 1.2 (retail) and 1.3 (EA's last patch). The patch
// changes speed.exe and five data files; GLOBAL/GAMEPLAY.BIN tells the two apart, and
// no mod in the set we install touches it.
constexpr const char *kMostWantedFolders[] = {"CARS",      "FRONTEND", "GLOBAL", "LANGUAGES",
                                              "MOVIES",    "NIS",      "SOUND",  "TRACKS"};
constexpr const char *kMostWantedFiles[] = {"speed.exe", "GLOBAL/GAMEPLAY.BIN",
                                            "GLOBAL/INGAMEB.BUN", "FRONTEND/FRONTB.LZC"};
constexpr Release kMostWantedReleases[] = {
    {"1.3", 2105216, 0xc5153d5a, false},
    {"1.2", 2103680, 0x91a3ddf1, true},
};
// What mod installs leave in the game's folder: the ASI loader and its plugins.
constexpr const char *kAsiLoaderFiles[] = {"scripts", "dinput8.dll", "d3d9.dll", "dsound.dll"};
constexpr Manifest kMostWanted{"NFSMW",        kMostWantedFolders,  kMostWantedFiles,
                               "GLOBAL/GAMEPLAY.BIN", kMostWantedReleases, kAsiLoaderFiles};

constexpr const Manifest *kManifests[] = {&kMostWanted};

// ---- files ----------------------------------------------------------------------

bool is_directory(const std::string &path)
{
    struct stat info{};
    return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool is_file(const std::string &path, std::uint64_t *size = nullptr)
{
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
        return false;
    if (size)
        *size = static_cast<std::uint64_t>(info.st_size);
    return true;
}

// One name in a folder, matched without regard to case ("" when none).
std::string find_name(const std::string &folder, const std::string &name)
{
    DIR *directory = opendir(folder.c_str());
    if (!directory)
        return {};
    std::string found;
    while (const dirent *entry = readdir(directory))
        if (strcasecmp(entry->d_name, name.c_str()) == 0)
        {
            found = entry->d_name;
            if (found == name)
                break; // an exact match wins over one that differs only in case
        }
    closedir(directory);
    return found;
}

bool crc_of(const std::string &path, std::uint32_t &crc)
{
    const int file = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (file < 0)
        return false;
    std::vector<unsigned char> buffer(1u << 20);
    mz_ulong value = mz_crc32(0, nullptr, 0);
    ssize_t count = 0;
    while ((count = read(file, buffer.data(), buffer.size())) > 0)
        value = mz_crc32(value, buffer.data(), static_cast<size_t>(count));
    close(file);
    crc = static_cast<std::uint32_t>(value);
    return count == 0;
}

std::string join(const std::vector<std::string> &items)
{
    std::string text;
    for (std::size_t i = 0; i < items.size(); ++i)
        text += (i == 0 ? "" : i + 1 == items.size() ? " and " : ", ") + items[i];
    return text;
}

// What other computers leave beside the files they copy: macOS's resource forks and
// folder settings, Windows's thumbnails. Never part of a game; never copied.
bool litter(const std::string &name)
{
    return name.starts_with("._") || name == ".DS_Store" || name == ".Spotlight-V100" ||
           name == ".fseventsd" || name == ".Trashes" || name == "Thumbs.db" ||
           name == "desktop.ini";
}

// Every regular file under a folder, with sizes; false at a link or special file.
bool walk(const std::string &root, const std::string &relative,
          std::vector<std::pair<std::string, std::uint64_t>> &files, std::vector<std::string> &dirs,
          std::string &error)
{
    const std::string folder = relative.empty() ? root : root + "/" + relative;
    DIR *directory = opendir(folder.c_str());
    if (!directory)
    {
        error = "Can't read " + folder + ": " + std::strerror(errno);
        return false;
    }
    std::vector<std::string> names;
    while (const dirent *entry = readdir(directory))
        if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0 &&
            !litter(entry->d_name))
            names.emplace_back(entry->d_name);
    closedir(directory);
    for (const auto &name : names)
    {
        const std::string inner = relative.empty() ? name : relative + "/" + name;
        struct stat info{};
        if (lstat((root + "/" + inner).c_str(), &info) != 0)
        {
            error = "Can't read " + inner;
            return false;
        }
        if (S_ISDIR(info.st_mode))
        {
            dirs.push_back(inner);
            if (!walk(root, inner, files, dirs, error))
                return false;
        }
        else if (S_ISREG(info.st_mode))
            files.emplace_back(inner, static_cast<std::uint64_t>(info.st_size));
        else
        {
            error = inner + " is a link or a special file; copy the game's real files";
            return false;
        }
    }
    return true;
}

bool remove_all(const std::string &path)
{
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0)
        return errno == ENOENT;
    if (!S_ISDIR(info.st_mode))
        return unlink(path.c_str()) == 0;
    DIR *directory = opendir(path.c_str());
    if (!directory)
        return false;
    std::vector<std::string> names;
    while (const dirent *entry = readdir(directory))
        if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0)
            names.emplace_back(entry->d_name);
    closedir(directory);
    bool ok = true;
    for (const auto &name : names)
        ok = remove_all(path + "/" + name) && ok;
    return rmdir(path.c_str()) == 0 && ok;
}
} // namespace

const Manifest *manifest_for(const std::string &folder)
{
    for (const auto *manifest : kManifests)
        if (folder == manifest->folder)
            return manifest;
    return nullptr;
}

std::string find_entry(const std::string &folder, const std::string &relative)
{
    std::string path = folder;
    std::size_t at = 0;
    while (at <= relative.size())
    {
        const auto slash = std::min(relative.find('/', at), relative.size());
        const auto name = find_name(path, relative.substr(at, slash - at));
        if (name.empty())
            return {};
        path += "/" + name;
        at = slash + 1;
    }
    return path;
}

BaseGame inspect(const std::string &folder, const Manifest &manifest)
{
    BaseGame result;
    result.path = folder;
    if (!is_directory(folder))
        return result;
    std::vector<std::string> missing;
    for (const auto *name : manifest.folders)
        if (const auto path = find_entry(folder, name); path.empty() || !is_directory(path))
            missing.emplace_back(name);
    for (const auto *name : manifest.files)
        if (const auto path = find_entry(folder, name); path.empty() || !is_file(path))
            missing.emplace_back(name);
    if (!missing.empty())
    {
        result.state = State::incomplete;
        result.detail = "Missing " + join(missing) + ". Copy the whole game folder.";
        return result;
    }
    std::vector<std::string> mods;
    for (const auto *name : manifest.mod_files)
        if (!find_entry(folder, name).empty())
            mods.emplace_back(name);
    if (!mods.empty())
    {
        result.state = State::modified;
        result.detail = "Mods are installed in it (" + join(mods) +
                        "). Copy a clean install: the mods are added for you.";
        return result;
    }
    const auto version_path = find_entry(folder, manifest.version_file);
    std::uint64_t size = 0;
    std::uint32_t crc = 0;
    if (!is_file(version_path, &size) || !crc_of(version_path, crc))
    {
        result.state = State::incomplete;
        result.detail = std::string("Can't read ") + manifest.version_file + ".";
        return result;
    }
    for (const auto &release : manifest.releases)
        if (release.size == size && release.crc == crc)
        {
            result.state = release.needs_patch ? State::needs_patch : State::ready;
            result.version = release.version;
            result.detail = release.needs_patch
                                ? "Version " + result.version + ": patched to " +
                                      manifest.releases.front().version + " when it is installed."
                                : "Version " + result.version + ".";
            return result;
        }
    result.state = State::unknown;
    result.detail = std::string(manifest.version_file) +
                    " matches no release we know: the copy is modified, or it is a release we "
                    "haven't seen. Copy a clean install.";
    return result;
}

BaseGame locate(const std::string &data_root, std::span<const std::string> roots,
                const Manifest &manifest)
{
    const auto base = data_root + "/base-games/" + manifest.folder;
    if (is_directory(base))
        return inspect(base, manifest);
    for (const auto &root : roots)
        if (const auto path = find_entry(root, manifest.folder); !path.empty() && is_directory(path))
        {
            auto found = inspect(path, manifest);
            found.on_usb = true;
            return found;
        }
    BaseGame absent;
    absent.path = base;
    absent.detail = "Put a clean install in " + base + " over FTP, or on a USB drive in a folder "
                    "named " + manifest.folder + ".";
    return absent;
}

std::vector<std::string> usb_roots()
{
    std::vector<std::string> roots;
    for (int i = 0; i < 8; ++i)
        if (const auto path = "/mnt/usb" + std::to_string(i); is_directory(path))
            roots.push_back(path);
    for (int i = 0; i < 2; ++i)
        if (const auto path = "/mnt/ext" + std::to_string(i); is_directory(path))
            roots.push_back(path);
    return roots;
}

bool import_base(const std::string &from, const std::string &data_root, const Manifest &manifest,
                 const std::function<void(const ImportProgress &)> &progress,
                 const std::atomic<bool> &cancel, std::string &error)
{
    const auto games = data_root + "/base-games";
    const auto target = games + "/" + manifest.folder, work = target + ".importing";
    if (is_directory(target))
    {
        error = target + " already exists";
        return false;
    }
    std::vector<std::pair<std::string, std::uint64_t>> files;
    std::vector<std::string> dirs;
    if (!walk(from, "", files, dirs, error))
        return false;
    ImportProgress state;
    for (const auto &file : files)
        state.total += file.second;
    // A copy left by an earlier attempt is started again.
    if (!remove_all(work) || (mkdir(games.c_str(), 0777) != 0 && errno != EEXIST) ||
        mkdir(work.c_str(), 0777) != 0)
    {
        error = "Can't make " + work + ": " + std::strerror(errno);
        return false;
    }
    const auto fail = [&](std::string why)
    {
        error = std::move(why);
        remove_all(work);
        return false;
    };
    for (const auto &dir : dirs)
        if (mkdir((work + "/" + dir).c_str(), 0777) != 0)
            return fail("Can't make " + dir + ": " + std::strerror(errno));
    std::vector<char> buffer(4u << 20);
    for (const auto &[name, size] : files)
    {
        state.file = name;
        progress(state);
        const int in = open((from + "/" + name).c_str(), O_RDONLY | O_NOFOLLOW);
        const int out = in < 0 ? -1
                               : open((work + "/" + name).c_str(),
                                      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
        if (in < 0 || out < 0)
        {
            if (in >= 0)
                close(in);
            return fail("Can't copy " + name + ": " + std::strerror(errno));
        }
        std::uint64_t copied = 0;
        bool ok = true;
        while (ok)
        {
            if (cancel.load())
            {
                close(in);
                close(out);
                return fail("Cancelled");
            }
            const ssize_t count = read(in, buffer.data(), buffer.size());
            if (count <= 0)
            {
                ok = count == 0;
                break;
            }
            for (ssize_t written = 0; ok && written < count;)
            {
                const ssize_t step = write(out, buffer.data() + written,
                                           static_cast<size_t>(count - written));
                ok = step > 0;
                written += ok ? step : 0;
            }
            copied += static_cast<std::uint64_t>(count);
            state.done += static_cast<std::uint64_t>(count);
            progress(state);
        }
        ok = ok && fsync(out) == 0;
        close(in);
        if (close(out) != 0 || !ok || copied != size)
            return fail("Copying " + name + " failed: " + std::strerror(errno ? errno : EIO));
    }
    // Checked as a whole before it takes the place of the base copy.
    const auto copy = inspect(work, manifest);
    if (copy.state != State::ready && copy.state != State::needs_patch)
        return fail("The copy didn't check out: " + copy.detail);
    if (rename(work.c_str(), target.c_str()) != 0)
        return fail("Can't move the copy into place: " + std::string(std::strerror(errno)));
    state.file.clear();
    progress(state);
    return true;
}
} // namespace store::library
