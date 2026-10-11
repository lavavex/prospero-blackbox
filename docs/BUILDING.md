# Building Prospero BlackBox

Built on Linux (developed on Arch/CachyOS with clang 23). The build fetches the pinned
public PS5 Payload SDK, the ps5-opengl SDK and the PacBrew libraries into `.deps/` by itself.

## Host tools

`make doctor` checks them: bash, make, ninja, python3, clang/clang++, clang-format,
clang-tidy, llvm-ar/llvm-ranlib, wget, unzip, tar, sha256sum, git, curl, pkg-config, ffmpeg.
`ccache` is optional (`USE_CCACHE=0` turns it off).

## Build

```sh
make DEVELOPMENT=1 USE_CCACHE=0 PS5_CLANG=clang app
```

- `PS5_CLANG=clang` uses the system clang; the build otherwise looks for `clang-18`.
- The app folder is `dist/PPSA99400/` (and `dist/PPSA99400.zip`).

## Home-screen art

The art uses EA's Need for Speed emblem, so it is **not** in this repository.
`tools/blackbox-assets.sh` (run by `make app`) copies it in from the workspace:

| Variable | Default | What |
| --- | --- | --- |
| `PB_ICON` | `../art/prospero-blackbox-icon0.png` | 512x512 PNG |
| `PB_BACKGROUND` | `../art/prospero-blackbox-bg.png` | 3840x2160 RGB PNG |
| `PS5_ASSETS_TOOL` | `../PS5_Vulkan/tools/prepare-assets.sh` | BC7 converter for `pic0.dds`/`pic1.dds` |

Without them the app builds with no Home-screen art.

## Deploy

Copy `dist/PPSA99400` into a folder ShadowMountPlus scans (for example `/data/homebrew`),
with every file `777`.
