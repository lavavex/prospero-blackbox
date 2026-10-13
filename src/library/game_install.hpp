// Prospero BlackBox - A game installed into the shared prefix from its base copy and a plan.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "library/base_game.hpp"
#include "library/install_plan.hpp"
#include <atomic>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace store::library
{
struct InstallPaths
{
    std::string data_root;                   // the manager's: base-games, mods-cache, installs
    std::string win_root = "/data/prospero-win"; // prospero-win's: prefixes, profiles
};

struct InstallOptions
{
    std::time_t now = 0;      // stamps the registry keys it adds and the record
    std::string app_version;  // the manager's, for the record
    bool without_pending = false; // install although the plan lists pieces it can't install yet
    // Another prefix and profile than the plan's: a test install beside the live one.
    std::string prefix;     // empty: the plan's
    std::string profile_id; // empty: the plan's
    std::string profile_name;
    std::vector<std::string> chosen; // the plan's options to add (ids); unknown ones are ignored
};

// A game install's stages, as the manager's page lists them (its job's progress).
inline constexpr const char *kGameStages[] = {"Downloads",  "Wine prefix",   "Checks",
                                              "Game files", "Wine settings", "Home app"};

// One of a plan's options as the page offers it: on when the last install chose it, or,
// before any install, when the plan turns it on by default.
struct OptionView
{
    std::string id, title, description;
    bool on = false;
};

struct InstallStatus
{
    std::string step;                  // what is happening, for the page
    std::uint64_t done = 0, total = 0; // bytes, in this step
};

// mods-cache/<sha256> or mods-cache/<sha256>.<anything> holding exactly the source's
// bytes; "" when there is none. Each candidate is hashed whole.
std::string find_cached(const std::string &cache, const Source &source,
                        const std::atomic<bool> &cancel);

// The sources the plan's placements use that mods-cache doesn't hold.
std::vector<const Source *> missing_sources(const InstallPlan &plan, const std::string &cache,
                                            const std::atomic<bool> &cancel);

// Installs the game: everything is checked first (the base copy is the plan's release,
// the prefix is there, every source is in mods-cache and hashes right, both registry
// files read), then the game's folder is built beside the old one and swapped in,
// and only then are the prefix's system files, registry and profile changed. Running
// it again over an install repairs it. Saves (in the prefix's user folders) are never
// touched. False with error set. A failure before the new folder is swapped in leaves
// the old one as it was; one after it (the prefix's files, registry, profile) is mended
// by installing again.
bool install_game(const InstallPlan &plan, const Manifest &manifest, const InstallPaths &paths,
                  const InstallOptions &options,
                  const std::function<void(const InstallStatus &)> &progress,
                  const std::atomic<bool> &cancel, std::string &error);

// The game's folder, its profile and its record removed. The prefix's shared files and
// registry, and the saves, stay: other games use them.
bool uninstall_game(const InstallPlan &plan, const InstallPaths &paths,
                    const InstallOptions &options, std::string &error);

// The plan with the options' prefix and profile in place of its own, and the chosen plan
// options' files, edits, expected hashes and DLL overrides added to its own.
InstallPlan effective_plan(const InstallPlan &plan, const InstallOptions &options);

// The plan's options, each on or off as the last install (its record) chose, or as the plan
// says when there is no install yet.
std::vector<OptionView> option_views(const InstallPlan &plan, const InstallPaths &paths,
                                     const InstallOptions &options);

// The record of the last install: installs/<prefix>/<game>.json under the data root (the
// effective plan's prefix: a test install keeps its own).
std::string record_path(const InstallPlan &plan, const InstallPaths &paths);
} // namespace store::library
