// Prospero BlackBox - The built-in catalog: the Black Box Need for Speed games.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalog/blackbox.hpp"

namespace store::catalog
{
namespace
{
constexpr Game kGames[] = {
    {"PPSA99401", "NFSU", "Need for Speed: Underground", "2003-11-17", "nfs-underground",
     "Street racing in Olympic City: tuners, drag and drift, and the cars of 2003.", false},
    {"PPSA99402", "NFSU2", "Need for Speed: Underground 2", "2004-11-09", "nfs-underground-2",
     "Free roam through Bayview with deeper tuning and the Underground crew.", false},
    {"PPSA99403", "NFSMW", "Need for Speed: Most Wanted", "2005-11-15", "nfs-most-wanted-2005",
     "Climb the Blacklist in Rockport with the cops on your tail. Installed with the Xbox 360 "
     "visual pack and its recommended mods, at 4K.",
     true},
    {"PPSA99404", "NFSC", "Need for Speed: Carbon", "2006-10-31", "nfs-carbon",
     "Canyon duels and crews in Palmont City.", false},
    {"PPSA99405", "NFSPS", "Need for Speed: ProStreet", "2007-11-14", "nfs-prostreet",
     "Organised race days on closed circuits, with damage and tuning.", false},
    {"PPSA99406", "NFSUC", "Need for Speed: Undercover", "2008-11-18", "nfs-undercover",
     "Undercover in the Tri-City Bay Area, back to the open world.", false},
};
} // namespace

std::span<const Game> blackbox_games()
{
    return kGames;
}

Snapshot blackbox_snapshot(const std::string &app_folder)
{
    Snapshot snapshot;
    for (const auto &game : kGames)
    {
        Entry entry;
        entry.id = game.id;
        entry.name = game.name;
        entry.author = "EA Black Box";
        entry.kind = "game";
        entry.status = game.available ? "available" : "coming_soon";
        entry.released = game.released;
        entry.description = game.description;
        entry.icon = app_folder + "/assets/games/" + game.art + ".png";
        snapshot.entries.push_back(std::move(entry));
    }
    // Built into the app: nothing to verify or download.
    snapshot.verified = true;
    snapshot.accepted = true;
    snapshot.online = false;
    return snapshot;
}
} // namespace store::catalog
