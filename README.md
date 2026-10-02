# lsd-port

A native port of *LSD: Dream Emulator* (PlayStation, 1998, Asmik Ace /
OutSide Directors Company), built from the matching decompilation
[lsddecomp](https://github.com/brian-oblivion/lsddecomp). Linux comes first;
the code is kept portable so Windows (and others) can follow.

**Status:** skeleton. The build brings up the platform layer,
[psyz](https://github.com/Xeeynamo/psyz) (through the fork
[lsd-psyz](https://github.com/brian-oblivion/lsd-psyz)), and draws a
placeholder screen; none of the game's C is compiled yet. See
[docs/PLAN.md](docs/PLAN.md) for the tracks to a playable build.

You will need your own copy of the game: the port reads the game's data from
a disc image you supply. No game data is, or will be, in this repository.

## Building

You need `git`, `cmake` (3.21+), `ninja` and a C/C++ compiler, plus the
development headers SDL3 builds against (on Debian/Ubuntu, installing
`libsdl2-dev` pulls them in). SDL3 itself is built from psyz's submodule and
linked statically.

```sh
git clone https://github.com/brian-oblivion/lsd-port.git
cd lsd-port
git submodule update --init decomp psyz
git -C psyz submodule update --init external/SDL external/cimgui
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/lsd
```

Windows, cross-compiled from Linux with MinGW (`mingw-w64`):

```sh
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=psyz/psyz/cmake/windows-x86_64.cmake
cmake --build build-win
```

## Layout

| path | what |
| --- | --- |
| `decomp/` | lsddecomp, the game's C (submodule) |
| `psyz/` | lsd-psyz, the Psy-Q replacement for PC (submodule) |
| `src/` | the port's own code |
| `docs/` | the plan and design notes |

## Licence

MIT (see `LICENSE`). lsddecomp is CC0; psyz's parts carry their own licences
(MIT, MPL 2.0, and unlicensed SDK headers/decompiled code).
