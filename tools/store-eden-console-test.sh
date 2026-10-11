#!/usr/bin/env bash
# Prospero BlackBox - Build the isolated console migration test payload.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
sdk="${PS5_PAYLOAD_SDK:-$root/.deps/native/ps5-payload-sdk}"
out="$root/build/console-migration-test"
mkdir -p "$out"
cxx="$sdk/bin/prospero-clang++"
cc="$sdk/bin/prospero-clang"
flags=(-std=c++20 -O1 -g -ffunction-sections -fdata-sections -Isrc -Wall -Wextra -Werror)
objects=()
for name in monocypher/monocypher monocypher/monocypher-ed25519 yyjson/yyjson qrcodegen/qrcodegen miniz/miniz miniz/miniz_tinfl miniz/miniz_tdef miniz/miniz_zip; do
    object="$out/${name//\//_}.o"
    "$cc" -std=c11 -O1 -ffunction-sections -fdata-sections -w -c "src/third_party/$name.c" -o "$object"
    objects+=("$object")
done
for source in tests/store_eden_console_test.cpp src/install/archive.cpp src/install/files.cpp src/install/transaction.cpp src/system/inventory.cpp src/system/locations.cpp src/system/storage_probe.cpp src/system/running.cpp src/catalog/catalog.cpp src/install/worker.cpp src/core/save_file.cpp; do
    object="$out/${source//\//_}.o"
    "$cxx" "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
"$cxx" -Wl,--gc-sections -Wl,--build-id=none -o "$out/eden-migration-test.elf" "${objects[@]}"
python3 tools/validate-loader-elf.py "$out/eden-migration-test.elf"
