# lsd-port

A native port of *LSD: Dream Emulator* (PlayStation, 1998, Asmik Ace /
OutSide Directors Company), built from the matching decompilation
[lsddecomp](https://github.com/brian-oblivion/lsddecomp), for Linux and
Windows.

**Status: playable on Linux and Windows (64-bit builds).** With your own
disc image, the port plays the intro, the title menu, whole days of
dreams with their music and links, the dream graph, SAVE, LOAD, FLASHBACK
and the special-day movies. The Windows build has been checked under Wine
(title menu, a dream with its music, SAVE and LOAD), not yet on Windows
itself. Against the console (DuckStation as the reference) sound and
picture match, apart from what "Known differences" lists. The 32-bit
builds (i686) play the same, frame for frame, and can still be built.
macOS (Apple Silicon) builds, but has not been played on a Mac yet.

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

## Picture

The picture settings live in `settings.ini` in the saves folder, beside
`controls.ini`, written with the defaults and explained on first start.
The command line and the environment win over it (the table under
"Running a release").

- `aspect = 4:3` (`--aspect`, `LSD_ASPECT`): the dream's width:height.
  4:3 is the console's; `16:9` (or any wider `W:H`) shows more of the
  dream at the sides instead of stretching the picture, and the window
  opens at that shape. The title menu, the graph, the diary and the
  movies stay 4:3 with black bars at the sides. The pause text and other
  2D drawn over the dream are stretched to the wider screen; fades fill
  it.
- `resolution = 1` (`--resolution`, `LSD_RESOLUTION`): the 3D drawn at N
  times 320x240, 1 to 8, for sharper edges. 2D images and movies keep
  their pixels.
- `scale = sharp` (`--scale`, `LSD_SCALE`): how the picture is scaled to
  the window. `sharp` keeps every pixel the same size and crisp at any
  window size (blending only at the pixel edges); `nearest` is crisp but,
  at window sizes that are not a whole multiple of 320x240, makes some
  pixels a column or row wider than others (most visible in the menu
  text); `smooth` blurs; `integer` uses whole multiples only, with a
  black border.
- `draw_distance = 1` (`--draw-distance`, `LSD_DRAW_DISTANCE`): how far
  the dream shows before it fades into the fog, 1 to 4. On some days a
  stage's fog closes in a few steps ahead; N moves it N times further
  away, but never past the clearest fog any stage has, so the clearest
  ones stay as they are. Far out the ground then ends at a straight edge
  against the sky, as it does on those stages on the console: the game
  draws only so many squares of ground ahead. 1 is the console's.
- `dither = on` (`--dither`, `LSD_DITHER`): the console's 4x4 dither
  pattern, over the lit and shaded 3D and over the menus and movies too.
  Scaled up with `resolution`, it shows as a fine grain; `off` drops it,
  and the console's 15-bit colour then shows in steps instead.
- `colour = console` (`--colour`, `LSD_COLOUR`): `full` keeps lighting,
  shading and fog at 24 bits, so darker and fogged surfaces keep the
  detail the console's 15 bits round away, and turns dithering off
  whatever `dither` says. `console` rounds as the console does.
- `geometry = console` (`--geometry`, `LSD_GEOMETRY`): the console puts
  every corner of the 3D on a whole pixel of its 320x240 screen and maps
  textures flat across each polygon, so at a higher `resolution` the
  ground and walls wobble as the view moves and textures bend on large
  polygons close by. `precise` draws the corners where they really fall
  between pixels; `perspective` does that and maps the textures in
  perspective too. Sprites, the 2D and anything the game places itself
  stay on whole pixels. Needs a build with `LSD_PRECISE_GEOMETRY` (the
  default; see "Building").

## Pace

Two more settings in the same file change how the dream moves.
`pace = 20` and `smooth = off` give the game exactly as its code runs it.

- `pace = 14` (`--pace N`, `LSD_PACE`): the dream's steps a second, 10 to
  30. 14 is about what a PlayStation managed (see "Known differences"),
  so a dream feels and lasts as it did there; the game's code asks for
  20. A dream's timers count steps, so a faster pace ends a day sooner.
  The music keeps its tempo, and the title menu, the graph and the movies
  keep the game's own pace.
- `smooth = on` (`--smooth on|off`, `LSD_SMOOTH`): frames drawn between
  the dream's steps, at 59.94 a second, with the camera and whatever moves
  blended between one step and the next, so turning and walking look
  smooth at any pace. Creatures' own animation is blended too, and the
  ground rising or sinking; a jump (a link, a respawn) is shown as a jump.
  `off`
  shows each step as it is. It draws about four times as many frames;
  measured on a desktop, each took about a third of a millisecond.
- `frame_rate = 60` (`--frame-rate`, `LSD_FRAME_RATE`): with smooth on,
  the dream's frames a second. `60` is the console's rate; `display`
  uses your display's refresh rate (120, 144, ...), presented with its
  VSync; a number from 30 to 360 asks for that many. The dream's pace
  stays what `pace` says either way; menus and movies stay at 60.

## Running a release

A release has one archive per platform: the program, these instructions
as `README.txt`, the licences (`licences/`, with `SOURCES.txt` saying what
is linked in and where its source is), and nothing of the game.

- **Windows** (64-bit, Windows 10 and 11): unzip, put
  `disc/` beside `lsd.exe` and start `lsd.exe`. A console window with the
  log opens beside the game; when the game cannot start, a message box
  says why.
- **Linux** (64-bit, x86_64): unpack, put `disc/` beside `lsd` and run
  `./lsd`. It needs what a desktop system usually has:
  - always: glibc, the Vulkan loader and your GPU's Vulkan driver
    (Debian/Ubuntu `libvulkan1 mesa-vulkan-drivers`; Arch
    `vulkan-icd-loader` and `vulkan-radeon`, `vulkan-intel` or
    `nvidia-utils`);
  - a display: X11 (`libx11-6 libxext6`; `libx11 libxext`), or Wayland
    (`libwayland-client0 libxkbcommon0`; `wayland libxkbcommon`);
  - sound: ALSA (`libasound2`; `alsa-lib`) or PulseAudio (`libpulse0`;
    `libpulse`).

Options, all optional:

| option | environment | what |
| --- | --- | --- |
| `--disc FILE.cue` | `LSD_DISC` | the disc image (default: `disc/`, above) |
| `--saves DIR` | `LSD_SAVES` | where the memory cards live (above) |
| `--aspect W:H` | `LSD_ASPECT` | the dream's aspect ratio, `4:3` (default) or wider, such as `16:9` ("Picture") |
| `--resolution N` | `LSD_RESOLUTION` | the 3D drawn at N times 320x240, 1 (default) to 8 ("Picture") |
| `--scale MODE` | `LSD_SCALE` | `sharp` (default), `nearest`, `smooth` or `integer` ("Picture") |
| `--pace N` | `LSD_PACE` | the dream's steps a second, 10 to 30; 14 (default) is about the console's, 20 the game's ("Pace") |
| `--smooth on\|off` | `LSD_SMOOTH` | frames drawn between the dream's steps, `on` (default) or `off` ("Pace") |
| `--frame-rate N\|display` | `LSD_FRAME_RATE` | with smooth on, the dream's frames a second: `60` (default), `display` or 30 to 360 ("Pace") |
| `--draw-distance N` | `LSD_DRAW_DISTANCE` | the dream's fog N times further away, 1 (default) to 4 ("Picture") |
| `--dither on\|off` | `LSD_DITHER` | the console's dither pattern, `on` (default) or `off` ("Picture") |
| `--colour console\|full` | `LSD_COLOUR` | 15-bit colour (`console`, default) or 24-bit (`full`) ("Picture") |
| `--geometry MODE` | `LSD_GEOMETRY` | `console` (default), `precise` or `perspective` ("Picture") |
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
- Speed: the game asks for 20 steps a second in a dream. A PlayStation
  cannot keep up with its own request in most places and runs fewer: on
  DuckStation 13.8 steps a second on average over seven stages (12.8 to
  17.8 per stage while walking, about 10.5 while turning in the first
  room). The port runs a steady 14 by default ("Pace"), the console's
  average; it does not copy the console's slowdown from place to place.
  With `pace = 20` everything moves about 1.45 times as fast as on the
  console (turning nearly twice as fast).

## Building

The default build is 64-bit (x86_64), as the releases are.

You need `git`, `cmake` (3.21+), `ninja` and a C/C++ compiler, plus the
development headers SDL3 builds against. SDL3 itself is built from psyz's
submodule and linked statically.

- Arch: `pacman -S base-devel cmake ninja libx11 libxext libxtst alsa-lib
  vulkan-icd-loader`, plus your GPU's Vulkan driver (`vulkan-radeon`,
  `vulkan-intel` or `nvidia-utils`). Wayland and PulseAudio support need
  `wayland libxkbcommon libpulse` too; without them the build uses X11 and
  ALSA.
- Debian/Ubuntu: `sudo apt install build-essential cmake ninja-build
  libsdl2-dev libxtst-dev mesa-vulkan-drivers` (`libsdl2-dev` is not
  linked; it pulls in the headers SDL3 needs).

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

`-DLSD_PRECISE_GEOMETRY=OFF` builds without the `geometry` setting's
`precise` and `perspective` (psyz then keeps no precise vertices beside the
console's, and `geometry` is always `console`); it is on by default.

The 32-bit build (i686) plays as the 64-bit one does (checked tick for
tick and frame for frame, `docs/design.md`); it keeps the PS1's 4-byte
pointers, which makes it the one for comparing memory layouts with the
console. It needs the 32-bit (multilib) libraries: on Arch `[multilib]`
and the `lib32-` versions of the packages above, on Debian/Ubuntu
`sudo dpkg --add-architecture i386` and `gcc-multilib g++-multilib
libsdl2-dev:i386 libxtst-dev:i386 mesa-vulkan-drivers:i386`. It needs a
build directory of its own (a directory configured as one width will not
switch to the other):

```sh
cmake -S . -B build-i686 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSD_ARCH=i686
cmake --build build-i686
```

macOS (Apple Silicon), with the Xcode command line tools
(`xcode-select --install`) and Homebrew's `cmake` and `ninja`: the same
commands as on Linux, and SDL3 draws through Metal. CI builds it on every
push, but it has not yet been played on a Mac; please report how it goes.

```sh
brew install cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/lsd --disc /path/to/disc.cue
```

Windows, cross-compiled from Linux with MinGW (`mingw-w64`; 64-bit is
psyz's toolchain file, 32-bit is `cmake/windows-i686.cmake`); `lsd.exe`
links MinGW's runtime statically:

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
| `cmake/` | toolchain files: Linux i686 and Windows i686 (the 32-bit builds) |
| `disc/` | your disc image (not committed) |
| `tools/` | measurement and test-driving scripts; `package.sh` makes a release archive |
| `docs/` | the plan and design notes |

## Licence

MIT (see `LICENSE`). lsddecomp is CC0; psyz's parts carry their own licences
(MIT, MPL 2.0, and unlicensed SDK headers/decompiled code), and SDL 3 is
zlib. A release archive lists what its binary contains, and under which
licence, in `licences/SOURCES.txt` (`tools/package.sh`).
