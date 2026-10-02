# Task 01: the game's C builds for the host

Handover for the agent picking this up. It covers PLAN.md's track 2
(`submodule`, `gte-c`, `portability`), up to the point where the game's C
compiles and links against psyz on Linux. Read `docs/PLAN.md` first; this
file is the concrete plan for that track, not a replacement for it.

## Where things stand (2026-10-02)

- `decomp/`: lsddecomp, pinned at `0b1f6e689`. All 1446 game functions are
  C. The repo is byte-matching: it builds the retail executable exactly,
  with the PS1 toolchain.
- `psyz/`: `brian-oblivion/lsd-psyz`, the project's fork of
  `Xeeynamo/psyz`, pinned to its `main` (upstream plus the libgs fix). Its own submodules
  `external/SDL` and `external/cimgui` are initialised; nugget and the decomp
  tools are not needed.
- `CMakeLists.txt` builds psyz (with a static SDL3) and an `lsd`
  executable from `src/main.c`, a placeholder that draws text through psyz.
  `./build/lsd --frames 120` runs and exits 0.
- `src/main.c` options for tools and agents: `LSD_DEBUG_PORT=<port>`
  starts psyz's debug server on 127.0.0.1 (`/screenshot`, `/vram`,
  `/metrics`, `/input`). `LSD_VSYNC=off` paces at 59.94 fps (headless runs
  are otherwise uncapped). Run headless with
  `env -u DISPLAY SDL_VIDEO_DRIVER=offscreen`; never open windows on the
  desktop.
- psyz's libgs couldn't draw: the drawing area was never set, so
  everything came out black. This is fixed on the fork's `main` (branch
  `libgs-drawbuff`, tests in `psyz/tests/test_libgs.c`). Expect more such
  gaps in libgs; the coverage file lists them.
- CI (`.github/workflows/build.yml`) builds Linux and runs 60 frames
  offscreen, and cross-builds Windows with MinGW. CI has no game data and
  never will.
- `docs/research/psyz-coverage-2026-10-02.md`: the 146 SDK functions the
  game calls, and which of them psyz implements (about half). It was a regex
  measurement; the linker replaces it in step 3 below.

## Decided: don't reopen these

psyz is the platform layer. CMake is the build. Linux comes first but
nothing may be Linux-only (no OS calls outside SDL3/psyz or one file under
`src/platform/`; Windows is requested). The licence is MIT. See PLAN.md's
"Decided" list.

## Rules

- **No game or Sony data in any repo**: no disc image, no executable, no
  SDK headers or objects from Sony discs. psyz's own headers
  (`psyz/psyz/include`) are what the host build uses.
- **The shared C lives in lsddecomp and must stay byte-exact there.** A
  change to `decomp/` is made in the full lsddecomp checkout
  (`~/git/lsddecomp`, which has the disc and SDK the oracle needs). There,
  `./build-and-verify.sh` must pass before committing. Push, then bump the
  `decomp/` pin here. Use `#ifdef PLATFORM_PC` only where the C genuinely
  cannot be shared; prefer a change that is correct on both (for example a
  pointer type instead of `s32`, if the PS1 bytes don't change).
- **psyz changes go in `psyz/`** on a branch started from `upstream/main`
  (`git -C psyz fetch upstream`), one topic per branch, so each can become
  one upstream PR. Merge the branch into the fork's `main` for the port to
  use, and bump the pin. Pushing and opening PRs are the operator's call:
  prepare them, then ask.
- Don't change git config, remotes or push URLs; they are set up.
- Commit messages: a short subject line, then a body that says why.

## Steps

### 1. Compile the game's C for the host (`submodule`)

Add a CMake target (an object library, for example `lsd_game`) for every
`decomp/src/**/*.c` **except `decomp/src/psyq/`**. Those files are Sony
code carried as C, some still as `INCLUDE_ASM`, and psyz replaces them. The
game's own files contain no `INCLUDE_ASM`.

- Includes: `decomp/include` and psyz's include directory (through linking
  `psyz`). Define `PLATFORM_PC`, and include `<psyz.h>` first as psyz's
  porting guide requires (a forced include `-include psyz.h` avoids
  touching 84 files).
- lsddecomp's Makefile flags (`-undef -nostdinc -Dmips ... -D_PSYQ`) are
  for the PS1 compiler; don't copy them.
- **Expected first conflicts:**
  - `decomp/include/gte.h` defines the `gte_*` macros as inline COP2
    assembly. psyz's `libgte.h` already defines about 111 of the same macros
    in C. Under `PLATFORM_PC`, `gte.h` should defer to psyz's macros. Make
    that change upstream in lsddecomp. Any macro the game uses that psyz
    lacks belongs in psyz. This is the `gte-c` item: compare the results
    with the PS1's for the calls the game makes.
  - Places where a decomp header and a psyz header declare the same SDK
    type or function differently. Prefer psyz's definition, and fix the
    decomp side upstream.
  - `SDATA` and similar section attributes, `INCLUDE_RODATA`, and any
    GCC 2.6-era habits. Look in `decomp/include/common.h` and
    `include_asm.h` first.
- Done when: the whole target compiles with zero errors on GCC (Linux).
  Record the warning count and treat it as debt. Then make the MinGW build
  compile too.

### 2. Pointer width: measure, then ask (*decision*)

PLAN.md leaves this open: 32-bit (`-m32`) first, or 64-bit from the start.
Measure both before anyone rewrites code:

- 64-bit: compile with `-Wpointer-to-int-cast -Wint-to-pointer-cast` and
  count the sites. PLAN.md lists the known ones (`New_TimBlockSrc`,
  `ModelData__ForwardScan*`, `RegisterStyleConfig`, `PickStageBgm`, the
  hand-sized method-table layouts). Also check the class structs the game
  sizes by hand: grep for size/offset asserts and `sizeof` arithmetic.
- 32-bit: check what `-m32` costs. SDL3 needs 32-bit development libraries,
  which are awkward on Arch and in CI. psyz supports i686.
- Note that 64-bit Windows is LLP64 (`long` stays 32 bits); psyz's `u_long`
  accounts for this.
- Write the counts and a recommendation into `docs/design.md` (create it),
  and stop for the operator's decision before the big portability change
  (`portability` item).

### 3. Link, and replace the coverage estimate with the real surface

- Replace `src/main.c` with the game's own `main` (in `decomp/src`). Keep
  `--frames N` working somehow for the CI smoke test, even if only as a
  `PLATFORM_PC` hook.
- Link against psyz. The undefined symbols are the platform layer's real
  to-do list. Write them, grouped by library, to
  `docs/research/host-link-surface.md`, and compare them with the coverage
  snapshot. Expected categories:
  - the libgs 3D calls (about 20);
  - the libgte math (`ApplyMatrixLV/SV`, `MulMatrix2`, `Square0`,
    `RCpoly*`);
  - `SsSeqPause`, `SsSeqReplay`, `SsSetMute`, `SsUtGetVabHdr`;
  - the `St*` ring buffer calls;
  - `Enter/ExitCriticalSection` and `SetMem`;
  - `GsIDMATRIX` and `GsLIGHT_MODE`.
- Kernel file I/O: the game calls the PS1 kernel's `open`, `close`,
  `read`, `write`, `lseek`, `delete` and `format` (the memory card,
  `bu00:`). These collide with POSIX and Windows CRT names. Route them
  through psyz-side names under `PLATFORM_PC`. Check how psyz's `libapi.h`
  already handles this before inventing anything.
- To get to a running binary, temporary stubs for missing functions are
  fine. Put them in one clearly named file (`src/stubs.c`) that logs each
  call once, like psyz's `NOT_IMPLEMENTED`. Each stub is a to-do item for
  psyz, not a permanent fix.
- Done when: `lsd` links on Linux, starts, and fails only for lack of the
  disc. Disc access is track 3's `cd` item and not part of this task. CI
  stays green; adjust the smoke test if `--frames` changes meaning.

## Out of scope

The platform layer's implementations (track 3: MDEC movies, libgs 3D, disc
image reading, audio), anything needing the disc, and enhancements. Note
what you find for them, but don't start them.

## Report back with

- what compiles and links, the error and warning counts, and the
  `host-link-surface.md` list;
- the pointer-width measurement and recommendation;
- the changes waiting to be pushed: lsddecomp commits (with
  `build-and-verify.sh` green) and psyz branches (one topic each), so the
  operator can push them and open PRs.
