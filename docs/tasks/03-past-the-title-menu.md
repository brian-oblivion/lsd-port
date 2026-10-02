# Task 03: past the title menu, into the dream

Handover. Read `docs/PLAN.md` and `docs/tasks/01-game-c-builds-for-host.md`
(its Rules section applies unchanged) first.

## Where things stand (2026-10-02)

- The default build is i686 (`LSD_ARCH`, `cmake/linux-i686.cmake`); CI
  builds i686 and x86_64 on Linux and MinGW. The disc image lives in
  `disc/` (gitignored); `lsd` finds the one `.cue` there.
- With the real disc, i686 plays the intro (logos, movies) and waits at
  the title menu. `docs/research/host-link-surface.md` has what each
  stop took and the 30 SDK functions still stubbed in `src/stubs.c`.
- x86_64 still stops in `BMemPMgrAlloc` loading `ETC\DREAME5.TMD`
  (64-bit layouts; not this task).
- psyz topic branches waiting for upstream PRs, with their descriptions:
  `docs/upstream/psyz-prs.md`. `libgs-2d` is stacked on
  `libgs-sdk-header` + `libgs-drawbuff`.

## The task

Press START at the title menu and follow the boot path into a dream,
one stop at a time, as task 02 did. Expected first: the libgs 3D calls
(`GsInitCoordinate2`, `GsGetLs`, `GsGetLws`, `GsSetLsMatrix`,
`GsSetLightMatrix`, `GsSetRefView2`, `GsSetProjection`, `GsSetNearClip`,
`GsSetAmbient`, `GsSetFlatLight`, `GsSetLightMode`, `GsMapModelingData`,
`GsLinkObject4`) and libgte (`ApplyMatrixLV/SV`, `MulMatrix2`, `Square0`,
`RCpoly*`). The game draws TMDs itself (`tmd_renderer.c`), not with
`GsSortObject4`.

## How task 02 worked (reuse it)

- **Sony's real code is the reference.** `~/git/lsddecomp/lib/libgs/*.o`
  (and libgte etc.) are Psy-Q's objects; disassemble with
  `~/git/lsddecomp/tools/binutils/bin/mipsel-linux-gnu-objdump -drz`.
  Objects not in `lib/` are in `~/git/lsddecomp/asm/psyq_*.s` (e.g.
  `GsSetDrawBuffOffset` in `psyq_15020.s`). Write C that behaves the
  same; never commit Sony code or data.
- **Run headless** with the debug server and take screenshots:
  `env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEO_DRIVER=offscreen
  LSD_VSYNC=off LSD_DEBUG_PORT=<port> ./build-i686/lsd`, then
  `curl 127.0.0.1:<port>/screenshot` (`/vram`, `/input` for pad presses:
  read `psyz/psyz/src/dbgserver/dbgserver.c` for its format). The title
  is reached about 80 s in (the intro cannot be skipped without input).
- **Crashes**: run under `gdb -batch` with `bt`; for hangs, start under
  gdb and send SIGINT after a while. `break CdSearchFile` with a
  `printf` of `name` shows the file reads.
- **Before blaming the game, run psyz's own host tests at the width you
  build** (`psyz/psyz/tests`, configure with the i686 toolchain file):
  the black-screen bug in task 02 was psyz's, visible in 37 failing GPU
  tests. Fork `main` today: 271 passed, 5 failed at both widths (the
  `bu`/`truncation` file tests and two gte tests, pre-existing).
- psyz branches start from `upstream/main` (or stack on the fork branch
  they need, saying so); merge into the fork's `main`; remove the
  matching stand-ins from `src/stubs.c` (brace-counted, one-line stubs
  are easy to over-delete). lsddecomp fixes go under `HOST_BUILD` with
  `./build-and-verify.sh` green, then bump `decomp/`.

## Known loose ends

- Sound unheard: install `lib32-alsa-lib` (and `lib32-libpulse` for
  Pulse/PipeWire) for SDL audio at i686.
- psyz's `Draw_LoadImage` does not flush pending primitives first
  (ordering, not yet seen to matter).
- The MDEC, streaming and 2D sorts have no host tests; a synthetic BS
  frame and a small GsSortSprite/GsSortBg test would let them go
  upstream with tests.

## Report back with

The stops hit and what fixed each, the remaining stub count, and the
branches/commits waiting to be pushed.
