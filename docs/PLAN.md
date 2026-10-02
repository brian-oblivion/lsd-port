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

## What the port has to replace (measured in lsddecomp)

- **Every game function is C** (`python3 tools/progress.py`: 100% of game
  code) and compiles with the PS1 toolchain. None of it has been compiled
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
  (docs/design.md has the measurements).

Still open:

- `design`: `docs/design.md` answers, with a recommendation each:
  - the platform layer: **psyz** (Xeeynamo's Psy-Q reimplementation for PC,
    used by sotn-decomp's PC build) against one written for this game;
    measure what psyz covers of the ~290 calls, and check its licence;
  - the renderer: an OpenGL (or SDL_gpu/Vulkan) GPU of ordering tables and
    primitives with a VRAM model, or a software rasteriser of the PS1 GPU;
  - audio: an SPU emulation fed by the game's own libsnd calls, or libsnd
    reimplemented on SDL audio; CD-DA/XA and the MDEC movies;
  - disc access: reading the user's `.bin/.cue` (ISO9660 plus XA sectors);
  - the build (CMake or Make) and how it pulls lsddecomp's sources;
  - where changes to the shared C go: upstream to lsddecomp (they must stay
    byte-exact there: `#ifdef PLATFORM_PC` only where C cannot be shared)
    or as patches here;
  - the licence, compatible with psyz's and lsddecomp's.
- `approach` (*decision*): the operator approves the design.

### 2. The game's C builds for Linux

- `submodule`: lsddecomp pinned as `decomp/`; the build compiles every game
  `.c` from it with the host compiler (`-DPLATFORM_PC`), with zero errors,
  and lists the unresolved Sony symbols: the platform layer's surface.
- `gte-c`: host versions of the `gte_*` macros, equal to the PS1's results
  for the calls the game makes.
- `portability`: no pointer held in an integer type, and layout assumptions
  guarded by static asserts, so the shared C is correct at the chosen
  pointer width. Changes that belong upstream land in lsddecomp with
  `./build-and-verify.sh` green there.

### 3. The platform layer

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

### 4. Playable on Linux

- `boots`: from the user's disc image to the title menu.
- `plays`: a day start to finish: the dream, links, flashbacks, the graph,
  a save and its reload.
- `sound`: music, sound effects and movies play correctly.
- `release`: README instructions to build and run, and a CI build of the
  port that needs no disc.

### Later (not needed for "playable")

- 64-bit clean, if track 2 chose 32-bit first.
- Enhancements a port can have and the decomp cannot: resolution scaling,
  widescreen, frame pacing, controller remapping, bug fixes behind options.
