# lsd-port

A native port of *LSD: Dream Emulator* (PlayStation, 1998, Asmik Ace /
OutSide Directors Company), built from the matching decompilation
[lsddecomp](https://github.com/brian-oblivion/lsddecomp), for Linux and
Windows.

**Status: playable on Linux and Windows (32-bit builds).** With your own
disc image, the i686 build plays the intro, the title menu, whole days of
dreams with their music and links, the dream graph, SAVE, LOAD, FLASHBACK
and the special-day movies. The Windows build has been checked under Wine
(title menu, a dream with its music, SAVE and LOAD), not yet on Windows
itself. Against the console (DuckStation as the reference) sound and
picture match, apart from what "Known differences" lists. The 64-bit build
compiles but does not run the game yet.

The game's C compiles and links against the platform layer,
[psyz](https://github.com/Xeeynamo/psyz) (through the fork
[lsd-psyz](https://github.com/brian-oblivion/lsd-psyz));
`docs/research/host-link-surface.md` has the stops on the way and
[docs/PLAN.md](docs/PLAN.md) what comes next.

## The game

You need your own copy of the game: the Japanese release, *LSD* (SLPS-01556),
as a `.bin`/`.cue` disc image (one data track, `MODE2/2352`). No game data
is, or will be, in this repository or in a release.

Put the image in a folder `disc/`, either in the folder you start `lsd`
from or beside `lsd` itself (for a release, in the folder you unpacked):

```
lsd/
├── lsd            (lsd.exe on Windows)
└── disc/
    ├── LSD - Dream Emulator (Japan).cue
    └── LSD - Dream Emulator (Japan).bin
```

`lsd` uses the one `.cue` it finds there. To keep the image elsewhere,
pass `--disc path/to/game.cue` or set `LSD_DISC`.

The `FILE` line in the `.cue` must name the `.bin` beside it exactly. Some
dumps name a file that isn't there (`... [SLPS-01556].bin` for a `.bin`
without the tag); if so, edit the `.cue` or rename the `.bin` to match.

In a checkout, `disc/` is in `.gitignore`, as are `*.bin` and `*.cue`
anywhere, so the image cannot be committed by accident.

## Saves

The memory cards are two folders, `bu00/` (slot 1) and `bu10/` (slot 2),
with one file per save, in your per-user folder:

- Linux: `~/.local/share/lsd-dream-emulator/`
  (`$XDG_DATA_HOME/lsd-dream-emulator/` when that is set)
- Windows: `%APPDATA%\lsd-dream-emulator\`

`--saves DIR` or `LSD_SAVES=DIR` puts them in `DIR` instead (made if it
does not exist); `--saves` wins over `LSD_SAVES`.

Builds from before October 2026 kept `bu00/` and `bu10/` in the folder
`lsd` was started from. They are not moved for you; `lsd` says so when it
finds them there. To keep them, move both folders into the per-user folder
above, or keep using them where they are with `--saves .`.

## Controls

The keyboard, by default:

| key | button | in a dream | in menus |
| --- | --- | --- | --- |
| W / S, or Up / Down | d-pad up / down | walk forward / back | move the cursor |
| A / D, or Left / Right | d-pad left / right | turn | |
| Q / E | L2 / R2 | step sideways | |
| Shift (held with forward) | cross | run | |
| R / F | triangle / square | look up / down | |
| Z / C | L1 / R1 | glance left / right | |
| Space or Enter | circle | the link button | confirm |
| Backspace | cross | | back |
| Escape | START | pause (again to go on) | start the day |
| Tab | SELECT | | |

To leave a dream early, pause (Escape), hold Tab and press R (SELECT and
triangle on the console). Escape does not quit: close the window to quit.

A gamepad works as a PlayStation pad through SDL's mapping (south button
cross, east circle, west square, north triangle; shoulders L1/R1,
triggers L2/R2; Start; the d-pad). The sticks do nothing; the game reads
a digital pad.

### Changing the keys

The keys live in `controls.ini` in the saves folder (above), written with
the defaults the first time `lsd` starts. Its first setting picks a
layout:

- `layout = modern`: the table above;
- `layout = classic`: the console's buttons spread over the keyboard, as
  in builds before October 2026: arrows d-pad, X cross, D circle, Z
  square, S triangle, Q L1, R R1, W L2, E R2, Enter START, Backspace
  SELECT, 1/2 L3/R3. In this layout Escape quits at once.

Below it, one commented line per button shows the layout's keys. To change
a button, remove the `#` and list its keys, separated by commas:

```
layout = modern
circle = Space, K
start = P
```

Key names are SDL's ([SDL_Scancode](https://wiki.libsdl.org/SDL3/SDL_Scancode),
without `SDL_SCANCODE_`): `W`, `Space`, `Left Shift`, `Return`, `Escape`,
`Up`, `F1`... They name a key's place on a US keyboard, so on AZERTY
`W` is the key labelled Z. `lsd` prints a line for any setting it does not
understand and keeps the rest. Delete the file to get the defaults back.
Escape quits only when no button uses it.

## Running a release

A release has one archive per platform: the program, these instructions
as `README.txt`, the licences (`licences/`, with `SOURCES.txt` saying what
is linked in and where its source is), and nothing of the game.

- **Windows** (32-bit, runs on 64-bit Windows 10 and 11): unzip, put
  `disc/` beside `lsd.exe` and start `lsd.exe`. A console window with the
  log opens beside the game; when the game cannot start, a message box
  says why.
- **Linux** (32-bit, for x86 and x86_64): unpack, put `disc/` beside `lsd`
  and run `./lsd`. On a 64-bit system it needs the 32-bit libraries:
  - always: glibc and libgcc (Debian/Ubuntu `libc6:i386 libgcc-s1:i386`,
    Arch `lib32-glibc lib32-gcc-libs`), the Vulkan loader and your GPU's
    32-bit Vulkan driver (`libvulkan1:i386 mesa-vulkan-drivers:i386`;
    Arch `lib32-vulkan-icd-loader` and `lib32-vulkan-radeon`,
    `lib32-vulkan-intel` or `lib32-nvidia-utils`);
  - a display: X11 (`libx11-6:i386 libxext6:i386`; `lib32-libx11
    lib32-libxext`), or Wayland (`libwayland-client0:i386
    libxkbcommon0:i386`; `lib32-wayland lib32-libxkbcommon`);
  - sound: ALSA (`libasound2:i386`; `lib32-alsa-lib`) or PulseAudio
    (`libpulse0:i386`; `lib32-libpulse`).

Options, all optional:

| option | environment | what |
| --- | --- | --- |
| `--disc FILE.cue` | `LSD_DISC` | the disc image (default: `disc/`, above) |
| `--saves DIR` | `LSD_SAVES` | where the memory cards live (above) |
| `--frames N` | | exit after N frames (for tests) |
| | `LSD_VSYNC` | frame pacing: `auto`, `on` (the display's VSync), `off` (psyz's own limiter), `limitless` |
| | `LSD_DEBUG_PORT` | psyz's debug server on 127.0.0.1 at that port (screenshots, input) |

`lsd` exits with status 2 when it has no usable disc image or saves folder.

## Known differences from the console

Measured against DuckStation (`docs/research/host-link-surface.md`,
"Against the console") and accepted:

- The game runs at psyz's NTSC rate, 59.94 Hz; a PlayStation's is
  59.826 Hz, so the music plays about 0.2% fast (inaudible).
- Loading is instant: the title menu and a dream's music come up seconds
  sooner than on the console, and the console's short silences in the
  first seconds of a dream are not there.
- Your GPU draws the picture: in places, shading differs from the
  console's by one 5-bit step and texture edges by about a texel.
- Speed: the game asks for 20 steps a second in a dream, and the port
  runs exactly that. A PlayStation cannot keep up with its own request in
  most places and runs fewer: on DuckStation 13.8 steps a second on
  average over seven stages (12.8 to 17.8 per stage while walking, about
  10.5 while turning in the first room). Each step turns, walks or runs
  the same distance on both, so on the port everything moves about 1.45
  times as fast (turning nearly twice as fast) as on the console.

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

The options are under "Running a release".

The 64-bit build compiles, but does not run the game yet; it is kept
building so that it does not fall behind. It needs a build directory of its
own (a directory configured as one width will not switch to the other):

```sh
cmake -S . -B build-x86_64 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSD_ARCH=x86_64
cmake --build build-x86_64
```

Windows, cross-compiled from Linux with MinGW (`mingw-w64`; 32-bit is
`cmake/windows-i686.cmake`, 64-bit is psyz's toolchain file); `lsd.exe`
links MinGW's runtime statically:

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
| `tools/` | measurement and test-driving scripts; `package.sh` makes a release archive |
| `docs/` | the plan and design notes |

## Licence

MIT (see `LICENSE`). lsddecomp is CC0; psyz's parts carry their own licences
(MIT, MPL 2.0, and unlicensed SDK headers/decompiled code), and SDL 3 is
zlib. A release archive lists what its binary contains, and under which
licence, in `licences/SOURCES.txt` (`tools/package.sh`).
