#!/usr/bin/env bash
# Home-screen art for Prospero BlackBox: icon0.png, pic0.dds, pic1.dds.
#
# The art uses EA's Need for Speed emblem, so it is never committed (see
# .gitignore). It is taken from the workspace's local art folder at build time:
#   PB_ICON        512x512 PNG   (default: ../art/prospero-blackbox-icon0.png)
#   PB_BACKGROUND  3840x2160 PNG (default: ../art/prospero-blackbox-bg.png)
#   PB_ART         the games' icons (default: ../art/<game>.png)
#   PS5_ASSETS_TOOL the BC7 converter (default: ../PS5_Vulkan/tools/prepare-assets.sh)
# Without them the app is built with a plain stand-in icon and no background (the
# console shows a dark tile) and a warning is printed.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
icon=${PB_ICON:-$root/../art/prospero-blackbox-icon0.png}
background=${PB_BACKGROUND:-$root/../art/prospero-blackbox-bg.png}
tool=${PS5_ASSETS_TOOL:-$root/../PS5_Vulkan/tools/prepare-assets.sh}
out=$root/sce_sys
art=${PB_ART:-$root/../art}

# The games' icons (cover art, so local only too): assets/games/<art>.png, the
# names the built-in catalog (src/catalog/blackbox.cpp) uses.
mkdir -p "$root/assets/games"
for game in nfs-underground nfs-underground-2 nfs-most-wanted-2005 nfs-carbon nfs-prostreet nfs-undercover; do
    if [[ -f $art/$game.png ]]; then
        cp "$art/$game.png" "$root/assets/games/$game.png"
    else
        echo "==> [art] no $art/$game.png: that game shows without a picture" >&2
    fi
done

# The emblem without its background, for the opening, the top bar and About
# (art/prospero-blackbox-logo.png, cut from the icon's source; local only).
if [[ -f $art/prospero-blackbox-logo.png ]]; then
    cp "$art/prospero-blackbox-logo.png" "$root/assets/logo.png"
else
    echo "==> [art] no $art/prospero-blackbox-logo.png: the icon stands in for the logo" >&2
fi

if [[ ! -f $icon ]]; then
    # A clean clone has no art: a plain dark 512x512 icon (nothing of EA's) lets the
    # build go through; the console then shows that on the Home screen.
    if [[ ! -f $out/icon0.png ]]; then
        python3 - "$out/icon0.png" <<'PY2'
import struct, sys, zlib
size, colour = 512, bytes((24, 24, 28))
rows = b"".join(b"\0" + colour * size for _ in range(size))
def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
open(sys.argv[1], "wb").write(png)
PY2
    fi
    echo "==> [art] no icon at $icon: building with a plain stand-in icon, no background" >&2
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
