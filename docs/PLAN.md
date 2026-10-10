# lsd-port plan

A native Linux build of *LSD: Dream Emulator*, from the matching
decompilation in [lsddecomp](https://github.com/brian-oblivion/lsddecomp). This
repository holds the port: the platform layer that replaces the PlayStation
and its Psy-Q SDK, the renderer, audio, disc access and packaging. The game's
C stays in lsddecomp, pinned here as a submodule, so the two cannot drift.

This plan moved here from lsddecomp's `docs/FINISHING-PLAN.md` (its phase 4,
removed at its plan revision 46, 2026-09-29). The decomp is finished; the port
is not bound by its rules (byte-exact output, no behaviour changes), which is
why it is its own project, as the ports of sm64 (sm64-port, sm64ex) and of
oot/mm (Ship of Harkinian) are. sotn-decomp is the in-repo counter-example:
it builds its PC version from `src/pc/` and links psyz as a submodule.

## What the port had to replace (measured in lsddecomp, 2026-09-29)

Kept as the starting point; tracks 2 to 4 below have closed every item.

- **Every game function is C** (`python3 tools/progress.py`: 100% of game
  code) and compiles with the PS1 toolchain. None of it had been compiled
  for a host yet.
- **The Psy-Q SDK is linked from Sony's own objects**: 178 MIPS `.o` files
  (`config/psyq-objects.txt`). None of that can run on a PC, so the port
  replaces the whole SDK surface the game calls: 146 functions across
  libgpu/libgs, libgte, libcd, libspu/libsnd, libpad/libetc, libcard/libapi
  and the kernel, of which psyz implements about half
  (`docs/research/psyz-coverage-2026-10-02.md`).
- **Some Sony code is carried as C** (`src/psyq/`: libsnd's sequencer,
  libcd's bios, libcard), because no SDK disc has the exact build. It
  compiles for the host as-is, or the platform layer replaces it.
- **GTE access** goes through the `gte_*` macros in `include/gte.h` (inline
  COP2 assembly). The host needs C versions of them.
- **Known portability debt** (lsddecomp's release review,
  `docs/research/release-review-2026-09-28.md`): pointers passed or stored
  as `s32` (`New_TimBlockSrc`, `ModelData__ForwardScan*`,
  `RegisterStyleConfig`, `PickStageBgm`), K&R prototypes kept for byte
  reasons (`BMemPMgrAlloc`), struct layouts that assume 32-bit pointers
  (every class object's method-table pointer at offset 0, sized by hand).
- **Data comes from the user's disc**: the game reads its files through
  libcd, so the port serves them from a disc image the user supplies.
  Nothing from the disc is ever committed.

## Tracks

Each track is a checklist; an item is done when its sentence is true and
measured. Items marked *decision* are the operator's.

Tracks 1 to 4 are done: the game builds from lsddecomp's C for Linux,
Windows and macOS, and plays on Linux and Windows from the user's disc
image, with sound, movies and saves, checked against DuckStation as the
console reference (tasks 01 to 06; releases `v0.1` 2026-10-04 and `v0.2`
2026-10-06). What follows them is under "Later".

### 1. Design

Decided (2026-10-02), so not reopened without the operator:

- platform layer: **psyz**, through the fork `brian-oblivion/lsd-psyz` as the
  `psyz/` submodule; SDK gaps are fixed there and sent upstream as PRs;
- renderer and audio: psyz's (SDL3 GPU or GL; its SPU emulation), extended
  rather than replaced; enhancements (widescreen, resolution) come after
  "playable" and mostly live in the layer;
- build: CMake, pulling lsddecomp's sources from the `decomp/` submodule;
- platforms: Linux first, portable from the start (Windows is requested):
  no OS calls outside SDL3 or one `src/platform/` file, and CI cross-builds
  Windows with MinGW;
- licence: MIT;
- pointer width (2026-10-02): **32-bit first** (i686 on Linux and
  Windows), with the x86_64 build kept compiling in CI and its width
  warnings only going down; 64-bit clean stays under "Later"
  (docs/design.md has the measurements). Done in task 10 (2026-10-05):
  x86_64 plays as i686, width warnings 0. **64-bit by default**
  (2026-10-05, after task 11's soak): x86_64 is the default build and the
  release (Linux and Windows); i686 stays in CI, keeping the PS1-sized
  layouts checked.
- TMD and sub-block tables (2026-10-05, task 10): on every host the file's
  32-bit words keep offsets, resolved at each use (`TMD_LIST_ADDR`,
  `SUBBLOCK_OBJ`, psyz's `GsTMDAddr`), addresses only on the PS1; the one
  `#ifdef HOST_BUILD` pair each in lsddecomp (docs/design.md).
- save files (2026-10-03): memory cards live in SDL's per-user folder
  (`SDL_GetPrefPath`: `~/.local/share/lsd-dream-emulator/` on Linux,
  `%APPDATA%\lsd-dream-emulator\` on Windows; renamed from `lsd-port/lsd/`
  on 2026-10-04), as `bu00/` and `bu10/` there, with an option (and an
  environment variable) to put them somewhere else;
- console reference (2026-10-03): DuckStation, run headless by
  `tools/ds_drive.py`; differences task 05 left (psyz's 59.94 Hz NTSC
  pacing, the port's instant loading, host-GPU rasterisation) are
  accepted.

The design questions the track opened with (platform layer, renderer,
audio, disc access, build, where shared-C changes go, licence) are all
answered in that list; the operator approved it on 2026-10-02.

### 2. The game's C builds for Linux (done, task 01)

- `submodule`: lsddecomp pinned as `decomp/`; the build compiles every game
  `.c` from it with the host compiler (`-DHOST_BUILD`), with zero errors,
  and lists the unresolved Sony symbols: the platform layer's surface.
- `gte-c`: host versions of the `gte_*` macros, equal to the PS1's results
  for the calls the game makes.
- `portability`: no pointer held in an integer type, and layout assumptions
  guarded by static asserts, so the shared C is correct at the chosen
  pointer width. Changes that belong upstream land in lsddecomp with
  `./build-and-verify.sh` green there.

### 3. The platform layer (done, tasks 01 to 05)

- `gpu`: the libgpu/libgs calls the game makes (ordering tables, primitives,
  VRAM transfers, TIM loads, display and draw environments) drawn on the
  host.
- `gte`: libgte's library calls (beyond the macros) in C.
- `cd`: libcd and the game's file reads served from the disc image.
- `input-time`: libpad, libetc and the kernel: input, VSync, root counters,
  events and callbacks, with the game's main loop run at the PS1's rate.
- `audio`: libspu/libsnd: sequences, VAB voices, CD audio and XA.
- `movies`: the MDEC stream decode for the intro, ending and special days.
- `card`: libcard/libapi memory card calls backed by save files.

### 4. Playable on Linux (done, tasks 03 to 06)

- `boots`: from the user's disc image to the title menu.
- `plays`: a day start to finish: the dream, links, flashbacks, the graph,
  a save and its reload.
- `sound`: music, sound effects and movies play correctly.
- `release`: README instructions to build and run, and a CI build of the
  port that needs no disc.

### Later (not needed for "playable")

- 64-bit clean: done (task 10), and the default since task 11.
- macOS: builds in CI on Apple Silicon (arm64, Clang, SDL3 on Metal) and
  passes the no-disc smoke test; not yet played on a Mac, and no release
  archive (signing, an app bundle) yet.
- Texel choice against the console (found in task 13, 2026-10-06): since
  psyz samples each pixel at the PS1's sample point, the speckles are
  gone, but on noisy ground textures many single pixels still show the
  neighbouring texel of the console's (about 60 % of ground pixels differ
  by more than one 5-bit step from DuckStation at Natural World, day 5,
  spawn 3/30; dithering is part of it). The PS1 interpolates UVs per
  pixel in fixed point and rounds differently from the GPU's float
  interpolation. Matching it means doing the PS1's UV setup in the
  shader (per-primitive gradients, its rounding) and comparing against
  DuckStation's software renderer with dithering off;
  `tools/ds_spot.py` and lockstep's `spawn` put both at the same spot.
  Not visible as an artefact, so low priority.
- Enhancements a port can have and the decomp cannot, each behind an
  option that defaults to the console's behaviour, with the decomp kept
  byte-exact (`#ifdef PLATFORM_PC` hooks or psyz settings):
  - `resolution` (task 09): `--resolution N`,
    psyz's internal resolution; the menu, movies and pause text checked
    at 4.
  - `scaling` (2026-10-04): `scale = sharp`
    (default), `nearest`, `smooth` or `integer`; psyz's present step.
    Sharp (integer nearest prescale, then bilinear) fixes the uneven
    pixels nearest gives at window sizes that are not whole multiples,
    worst in the menu text. Upscaling filters (xBR and the like) not
    done; AI upscaling is ruled out by the operator.
  - `settings`: `settings.ini` in the saves folder holds aspect,
    resolution, scale, pace, smooth and draw_distance, under the command
    line and environment, so a double-clicked `lsd.exe` can use them.
  - `widescreen` (task 09): `--aspect 16:9`, anamorphic. psyz's GTE
    squeezes projected X by 3/4 in the dream and the display stretches it
    back; the 2D screens stay 4:3 with bars. No edge culling to widen was
    found (the map draws a 20x20-cell footprint, wider than the view up to
    the fog); 2D drawn over the dream (pause text) is stretched. Details
    in `docs/research/host-link-surface.md` ("Widescreen (task 09)").
  - `high-fps` (task 12): `smooth = on` (`--smooth on|off`,
    `LSD_SMOOTH`; on by default since 2026-10-06) draws a frame at every
    59.94 Hz blank between the dream's ticks, with the camera (DreamSys's
    coordinate) and every moving node blended between the last two ticks,
    a TodActor's parts (its TOD animation) included since task 15; the
    StageMap's grid cells only in scale, and a move of more than 4096 or
    45 degrees in a tick (a link, a respawn) is drawn as a jump. The logic stays at its pace; lockstep
    shows identical state with it on and off. `src/pacing.c`,
    `docs/design.md` ("Pace and smooth"). `frame_rate = display` (or
    30 to 360; task 16) presents the smooth dream at the display's
    refresh rate through psyz's `Psyz_VideoPresent`, the ticks scheduled
    by time; not yet seen on a real high-refresh display.
  - `speed` (reported 2026-10-04; task 12): the port feels faster than
    the console. Measured (task 07): the port runs the dream at 20.0 ticks
    per second everywhere, DuckStation at 13.8 on average (12.8 to 17.8
    per stage, ~10.5 turning in the first room). `pace = N` (`--pace`,
    `LSD_PACE`; 10 to 30) runs the dream at N ticks a second; menus, the
    graph and movies keep the game's own 20. Default 14 with smooth on,
    the operator's choice after playing it (2026-10-06).
    Copying the console's per-scene slowdown needs a frame-cost model psyz
    lacks. Part of the "too fast" was a port bug, fixed in task 07: forward
    and back moved by a wrong, fixed step (lsddecomp `host-actor-local-move`).
  - `controls` (done, task 07): `controls.ini` in the saves folder,
    `layout = modern` (WASD or arrows, the default) or `classic`, and
    per-button keys; psyz `Psyz_PadsSetKeyboardMap`.
  - `draw-distance` (task 14): `draw_distance = N` (`--draw-distance`,
    `LSD_DRAW_DISTANCE`; 1 to 4, default 1) moves the dream's fog N times
    further away, never past 26624, the clearest fog the game uses (the
    GTE's 16-bit DQA allows about 27238). Only fog levels 3 and 4 (8192,
    4096; about a third of the days on the six stages without a fixed
    style) cull before the map's footprint, 20 cells ahead; past that the
    footprint's straight edge ends the view, as on the console's clear
    stages. Drawing beyond it needs a wider footprint and a second ring of
    map chunks (StageMap's tracking rewritten), not done. Lockstep shows
    identical state at 1 and 4. `src/draw_distance.c`, `docs/design.md`
    ("Draw distance").
  - `settings-menu` (task 21): F1, or a pad's Guide button or both
    sticks, or SETTINGS in the title menu, opens a Dear ImGui menu over
    the game (psyz's overlay hooks, on both renderers), in the title
    menu's font (read from the disc) and colours, with every setting and
    the keyboard's keys; it
    applies them at once (aspect from the next dream), writes them back
    into `settings.ini` and `controls.ini` keeping the rest of the files,
    and holds the keys and pads from the game while open
    (`Psyz_PadsHold`). `src/menu.cpp`, `docs/design.md` ("Settings
    menu").
  - `mouse-look`: dropped by the operator (2026-10-04).
  - bug fixes behind options.
