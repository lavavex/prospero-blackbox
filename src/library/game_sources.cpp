// Prospero BlackBox - What a game's plan needs, put in mods-cache.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/game_sources.hpp"
#include "core/save_file.hpp"
#include "library/game_install.hpp"
#include "library/sha256_hex.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <unistd.h>

namespace store::library
{
std::vector<const Source *> needed_sources(const InstallPlan &plan)
{
    std::vector<const Source *> needed;
    std::set<std::string> seen;
    const auto add = [&](const std::string &id)
    {
        if (const auto *source = plan.source(id); source && seen.insert(id).second)
            needed.push_back(source);
    };
    for (const auto *list : {&plan.game_files, &plan.system_files})
        for (const auto &placement : *list)
            add(placement.source);
    for (const auto &upgrade : plan.upgrades)
        add(upgrade.source);
    return needed;
}

namespace
{
// A file being written beside its final name, hashed as it goes; it takes its name only
// whole and right.
struct Part
{
    std::string path, final_path;
    int fd = -1;
    std::uint64_t written = 0;
    picosha2::hash256_one_by_one hasher;

    bool open_for(const std::string &cache, const Source &source, std::string &error)
    {
        final_path = cache + "/" + source.sha256;
        path = final_path + ".part";
        unlink(path.c_str());
        fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
        if (fd < 0)
            error = "Can't write to mods-cache: " + std::string(std::strerror(errno));
        return fd >= 0;
    }
    bool write(std::string_view piece)
    {
        hasher.process(piece.begin(), piece.end());
        written += piece.size();
        while (!piece.empty())
        {
            const ssize_t step = ::write(fd, piece.data(), piece.size());
            if (step <= 0)
                return false;
            piece.remove_prefix(static_cast<std::size_t>(step));
        }
        return true;
    }
    // Closes it; true and named when it is the source, removed otherwise.
    bool finish(const Source &source, std::string &error)
    {
        bool ok = fd >= 0 && fsync(fd) == 0;
        if (fd >= 0 && close(fd) != 0)
            ok = false;
        fd = -1;
        hasher.finish();
        if (ok && (written != source.size || sha256_hex(hasher) != source.sha256))
        {
            error = source.name + " didn't arrive as the plan pins it (size or SHA-256)";
            ok = false;
        }
        else if (!ok)
            error = "Can't write " + source.name + " to mods-cache: " + std::strerror(errno);
        if (ok && rename(path.c_str(), final_path.c_str()) != 0)
        {
            error = "Can't put " + source.name + " in mods-cache: " + std::strerror(errno);
            ok = false;
        }
        if (!ok)
            unlink(path.c_str());
        return ok;
    }
    ~Part()
    {
        if (fd >= 0)
        {
            close(fd);
            unlink(path.c_str());
        }
    }
};
} // namespace

bool fetch_sources(const InstallPlan &plan, const std::string &cache, const std::string &bundled,
                   const Download &download,
                   const std::function<void(const FetchStatus &)> &progress,
                   const std::atomic<bool> &cancel, std::string &error)
{
    if (!hui::save::ensure_directory(cache))
    {
        error = "Can't make " + cache;
        return false;
    }
    std::vector<const Source *> missing;
    for (const auto *source : needed_sources(plan))
        if (find_cached(cache, *source, cancel).empty())
            missing.push_back(source);
    FetchStatus status;
    status.count = missing.size();
    for (const auto *source : missing)
    {
        if (cancel.load())
        {
            error = "Cancelled";
            return false;
        }
        status.name = source->name;
        status.done = 0;
        status.total = source->size;
        ++status.index;
        if (progress)
            progress(status);
        Part part;
        if (!part.open_for(cache, *source, error))
            return false;
        const auto report = [&](std::string_view piece)
        {
            if (cancel.load() || !part.write(piece))
                return false;
            status.done = part.written;
            if (progress)
                progress(status);
            return true;
        };
        const auto shipped = bundled.empty() ? std::string() : find_cached(bundled, *source, cancel);
        if (!shipped.empty())
        {
            const int in = ::open(shipped.c_str(), O_RDONLY | O_NOFOLLOW);
            std::vector<char> buffer(1u << 20);
            ssize_t count = in < 0 ? -1 : 0;
            while (in >= 0 && (count = read(in, buffer.data(), buffer.size())) > 0 &&
                   report(std::string_view(buffer.data(), static_cast<std::size_t>(count))))
            {
            }
            if (in >= 0)
                close(in);
            if (count != 0)
            {
                error = cancel.load() ? "Cancelled" : "Can't copy " + source->name + " from the manager";
                return false;
            }
        }
        else if (!source->url.empty() && download)
        {
            std::string why;
            if (!download(source->url, source->size, report, why))
            {
                error = cancel.load() ? "Cancelled"
                                      : "Can't download " + source->name + (why.empty() ? "" : ": " + why);
                return false;
            }
        }
        else
        {
            error = source->name + " can't be downloaded: put it in mods-cache by hand";
            return false;
        }
        if (!part.finish(*source, error))
            return false;
    }
    return true;
}
} // namespace store::library
