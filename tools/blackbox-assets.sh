#!/usr/bin/env bash
# Home-screen art for Prospero BlackBox: icon0.png, pic0.dds, pic1.dds.
#
# The art uses EA's Need for Speed emblem, so it is never committed (see
# .gitignore). It is taken from the workspace's local art folder at build time:
#   PB_ICON        512x512 PNG   (default: ../art/prospero-blackbox-icon0.png)
#   PB_BACKGROUND  3840x2160 PNG (default: ../art/prospero-blackbox-bg.png)
#   PS5_ASSETS_TOOL the BC7 converter (default: ../PS5_Vulkan/tools/prepare-assets.sh)
# Without them the app is built without Home-screen art (the console shows a
# blank tile) and a warning is printed.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
icon=${PB_ICON:-$root/../art/prospero-blackbox-icon0.png}
background=${PB_BACKGROUND:-$root/../art/prospero-blackbox-bg.png}
tool=${PS5_ASSETS_TOOL:-$root/../PS5_Vulkan/tools/prepare-assets.sh}
out=$root/sce_sys

if [[ ! -f $icon ]]; then
    echo "==> [art] no icon at $icon: building without Home-screen art" >&2
    exit 0
fi
cp "$icon" "$out/icon0.png"
if [[ ! -f $background || ! -f $tool ]]; then
    echo "==> [art] icon only (no background at $background or no converter at $tool)" >&2
    exit 0
fi
# Convert only when the background changed since the last conversion.
if [[ ! -f $out/pic0.dds || $background -nt $out/pic0.dds ]]; then
    work=$(mktemp -d)
    trap 'rm -rf -- "$work"' EXIT
    cp "$icon" "$work/icon0.png"   # the converter validates its folder as a whole sce_sys
    bash "$tool" --background "$background" --output-directory "$work" >/dev/null
    cp "$work/pic0.dds" "$work/pic1.dds" "$out/"
fi
echo "==> [art] icon0.png, pic0.dds, pic1.dds from the local art folder"
