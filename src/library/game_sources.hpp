// Prospero BlackBox - What a game's plan needs, put in mods-cache.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "library/install_plan.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace store::library
{
// Every source the plan can use: its placements' and its upgrades'.
std::vector<const Source *> needed_sources(const InstallPlan &plan);

// Downloads url, handing each piece to sink (false from it stops); at most limit bytes.
// True when the whole file arrived; error otherwise. The caller's transport (the store's
// HTTPS stack on the console, a local server in tests).
using Download = std::function<bool(const std::string &url, std::uint64_t limit,
                                    const std::function<bool(std::string_view)> &sink,
                                    std::string &error)>;

struct FetchStatus
{
    std::string name;                  // the source being fetched, for the page
    std::size_t index = 0, count = 0;  // which of how many still missing
    std::uint64_t done = 0, total = 0; // bytes of this one
};

// Puts every needed source the cache lacks into it, as <sha256>: from bundled (a folder of
// <sha256>[.ext] files shipped with the manager; empty for none), else from its url. Each
// file is written beside its final name and hashed before it takes it, so the cache only
// ever holds whole, right files. A source with neither is named in error (it has to be put
// in mods-cache by hand). False with error set at the first one that can't be had.
bool fetch_sources(const InstallPlan &plan, const std::string &cache, const std::string &bundled,
                   const Download &download,
                   const std::function<void(const FetchStatus &)> &progress,
                   const std::atomic<bool> &cancel, std::string &error);
} // namespace store::library
