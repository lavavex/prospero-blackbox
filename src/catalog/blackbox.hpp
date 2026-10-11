// Prospero BlackBox - The built-in catalog: the Black Box Need for Speed games.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "catalog/catalog.hpp"
#include <span>
#include <string>

namespace store::catalog
{
struct Game
{
    const char *id;          // its own Home-screen title, PPSA9940N
    const char *folder;      // base-games/<folder> and the USB folder name
    const char *name;
    const char *released;    // ISO date, for sorting
    const char *art;         // assets/games/<art>.png in the app folder
    const char *description;
    bool available;          // false: listed, greyed out as in progress
};

std::span<const Game> blackbox_games();

// The catalog the store shows: every game, its icon a path in the app folder.
Snapshot blackbox_snapshot(const std::string &app_folder);

// An icon that names a file in the app folder rather than an address.
inline bool local_icon(const std::string &icon)
{
    return !icon.empty() && icon.front() == '/';
}
} // namespace store::catalog
