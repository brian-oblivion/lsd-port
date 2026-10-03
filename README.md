# lsd-port

A native port of *LSD: Dream Emulator* (PlayStation, 1998, Asmik Ace /
OutSide Directors Company), built from the matching decompilation
[lsddecomp](https://github.com/brian-oblivion/lsddecomp). Linux comes first;
the code is kept portable so Windows (and others) can follow.

> **Work in progress: not playable yet.** There is no release to download.
> A build plays the intro movies, the title menu and the day's dream, with
> its music, and saves and loads; it has not been played through, and
> only the 32-bit build gets that far. Follow along, but don't expect to
> play it.

**Status:** the game's C compiles and links against the platform layer,
[psyz](https://github.com/Xeeynamo/psyz) (through the fork
[lsd-psyz](https://github.com/brian-oblivion/lsd-psyz)), with no
stand-ins left. With the real disc, the i686 build plays the intro, the
title menu, a day's dream with its music and links to other stages, the
dream graph, SAVE, LOAD, FLASHBACK and the special-day movies
(`docs/research/host-link-surface.md` has the stops on the way). Memory
card files go to `bu00/` and `bu10/` in the working directory for now.
See [docs/PLAN.md](docs/PLAN.md) for the tracks to a playable build.

## The game

You need your own copy of the game: the Japanese release, *LSD* (SLPS-01556),
as a `.bin`/`.cue` disc image (one data track, `MODE2/2352`). No game data
is, or will be, in this repository.

Put the image in `disc/` at the top of the checkout:

```
lsd-port/
└── disc/
    ├── LSD - Dream Emulator (Japan).cue
    └── LSD - Dream Emulator (Japan).bin
```

`disc/` is in `.gitignore`, as are `*.bin` and `*.cue` anywhere, so the
image cannot be committed by accident. `lsd` uses the one `.cue` in `disc/`
under the directory it is started from. To keep the image elsewhere, pass
`--disc path/to/game.cue` or set `LSD_DISC`.

The `FILE` line in the `.cue` must name the `.bin` beside it exactly. Some
dumps name a file that isn't there (`... [SLPS-01556].bin` for a `.bin`
without the tag); if so, edit the `.cue` or rename the `.bin` to match.

## Building

The default build is 32-bit (i686): the game's C assumes 32-bit pointers,
and the port keeps them while it is brought up (`docs/design.md`). On an
x86_64 Linux machine that needs the 32-bit (multilib) libraries.

You need `git`, `cmake` (3.21+), `ninja` and a C/C++ compiler, plus the
development headers SDL3 builds against. SDL3 itself is built from psyz's
submodule and linked statically.

- Arch: enable `[multilib]` in `/etc/pacman.conf`, then
  `pacman -S base-devel cmake ninja lib32-glibc lib32-gcc-libs lib32-libx11
  lib32-libxext lib32-libxtst lib32-alsa-lib lib32-vulkan-icd-loader`, plus
  your GPU's 32-bit Vulkan driver (`lib32-vulkan-radeon`,
  `lib32-vulkan-intel` or `lib32-nvidia-utils`). Wayland and PulseAudio
  support need `lib32-wayland lib32-libxkbcommon lib32-libpulse` too;
  without them the build uses X11 and ALSA.
- Debian/Ubuntu: `sudo dpkg --add-architecture i386`, then
  `sudo apt install build-essential cmake ninja-build gcc-multilib
  g++-multilib libsdl2-dev:i386 libxtst-dev:i386 mesa-vulkan-drivers:i386`
  (`libsdl2-dev` is not linked; it pulls in the headers SDL3 needs).

```sh
git clone https://github.com/brian-oblivion/lsd-port.git
cd lsd-port
git submodule update --init decomp psyz
git -C psyz submodule update --init external/SDL external/cimgui
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/lsd
```

Without a disc image `lsd` exits with status 2. Other options: `--frames N`
exits after N frames; `LSD_DEBUG_PORT=<port>` starts psyz's debug server on
127.0.0.1; `LSD_VSYNC=auto|on|off|limitless` sets the frame pacing.

The 64-bit build compiles, but does not run the game yet; it is kept
building so that it does not fall behind. It needs a build directory of its
own (a directory configured as one width will not switch to the other):

```sh
cmake -S . -B build-x86_64 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSD_ARCH=x86_64
cmake --build build-x86_64
```

Windows, cross-compiled from Linux with MinGW (`mingw-w64`; 32-bit is
`cmake/windows-i686.cmake`, 64-bit is psyz's toolchain file):

```sh
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=cmake/windows-i686.cmake
cmake --build build-win
```

## Layout

| path | what |
| --- | --- |
| `decomp/` | lsddecomp, the game's C (submodule) |
| `psyz/` | lsd-psyz, the Psy-Q replacement for PC (submodule) |
| `src/` | the port's own code |
| `cmake/` | toolchain files: Linux i686 (the default) and Windows i686 |
| `disc/` | your disc image (not committed) |
| `tools/` | measurement scripts |
| `docs/` | the plan and design notes |

## Licence

MIT (see `LICENSE`). lsddecomp is CC0; psyz's parts carry their own licences
(MIT, MPL 2.0, and unlicensed SDK headers/decompiled code).
