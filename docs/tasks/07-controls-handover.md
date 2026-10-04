# Task 07: controls and speed — where it ended

Handover from task 07 (`07-controls-and-speed.md`) to whoever picks up
next. The details are in `docs/research/host-link-surface.md` ("Controls
and speed (task 07)"), `docs/upstream/psyz-prs.md` (`pads-keyboard-map`)
and the README ("Controls", "Known differences from the console").

## Where things stand (2026-10-04)

- lsd-port `main` is `3c4443b`, tagged **`v0.1`: the first release is
  published** (Linux and Windows i686 archives, built by
  `.github/workflows/release.yml` from the tag).
- psyz fork `main` is `26fad08` (pushed), with `pads-keyboard-map`
  merged; lsd-port pins it.
- lsddecomp `main` is `865b85328` (the move fix `863ea0777`, then the
  operator's config cleanup). lsd-port pins `863ea0777`; the newer commit
  only drops a symbol from `config/`, which the port does not use, so the
  pin need not move.

## What task 07 did

1. **Speed measured.** Ticks per second in a dream: port 20.0
   everywhere; DuckStation 13.8 on average (12.8 to 17.8 per stage,
   about 10.5 while turning in the first room), because the console
   misses most of `VSync(3)`'s deadlines. The operator's decision: keep
   20 for now; the feel may need tuning later (PLAN "Later", `speed`).
2. **A move bug fixed** (lsddecomp `863ea0777`): forward and back moved by
   a wrong, fixed step on the PC (about 280 a tick instead of 64, and back
   went forward), because `Actor`'s local move vector had its z as a
   separate static that only retail's small data put after x and y. Part
   of the "too fast" report was this bug.
3. **Configurable keys.** psyz `Psyz_PadsSetKeyboardMap` (several keys per
   button; Escape quits only while unbound). The port's `src/controls.c`
   reads `controls.ini` from the saves folder, writing it with the
   defaults on first start: `layout = modern | classic`, then optional
   per-button key lists (SDL key names).
4. **Modern default layout** (the operator's choice): WASD or arrows for
   the d-pad (menus too), Q/E strafe, Shift run, R/F look, Z/C glance,
   Space/Enter circle, Backspace cross, Tab SELECT, Escape START (pause).
   Closing the window quits. `classic` is the old map.
5. **README** (and the release's README.txt, via `tools/package.sh`):
   controls, gamepad, `controls.ini`, the speed difference.
6. **v0.1**: the release workflow ran by hand, both archives were tested
   as a player would (Linux headless; Windows under Wine): disc found
   beside the program, menu, a dream with music, a whole day, SAVE,
   `controls.ini` written. Then tag, draft, published by the operator.

## Open, in rough order

- **Other "neighbouring statics"** like the move bug. The decomp is
  byte-exact on the PS1, where small-data statics sit in retail's order;
  any code that reaches one static through another's address (an array
  read past its end into the next symbol, a struct overlaid on two
  statics) works there and breaks silently on the PC. The move bug was
  found only by comparing with DuckStation. Worth a systematic look:
  `config/gp-symbols.txt` and the symbol file list retail's adjacent
  small-data symbols; candidates are short arrays (`[2]`, `[3]`) and
  scalars declared next to each other, and functions that take `&s[0]`
  of one and index it. A fix keeps the PS1 bytes (one array or struct
  instead of two statics, as in `863ea0777`); check each with
  `./build-and-verify.sh` and `tools/lint.sh` on an lsddecomp topic
  branch. This is the recommended next task: it is correctness, and it
  can explain other "feels different" reports.
- **Speed feel**: if the port still feels fast after the move fix, an
  option for a fixed slower pacing (about 14 ticks a second) is cheap
  (psyz paces `VSync(3)`; the option would pace it longer). Copying the
  console's per-scene slowdown needs a frame-cost model psyz lacks. The
  operator's call.
- **Real Windows**: never run there (the operator has no Windows
  machine); the release notes ask for reports.
- **glibc 2.38**: the Linux binary needs it (ubuntu-24.04 runner), which
  leaves out Debian 12 and Ubuntu 22.04. Building on an older runner, or
  in an older container, would widen that.
- `lsd.exe` is a console program (a log window opens beside the game); a
  GUI-subsystem build is the operator's call.
- **Gamepads** were not tested in task 07 (none attached); their path is
  unchanged since task 06.
- **psyz upstream PRs**: `pads-keyboard-seen` and `pads-keyboard-map`
  (stacked) join the earlier branches in `docs/upstream/psyz-prs.md`;
  opening them is the operator's call.
- **Mouse look** (PLAN "Later"): notes in the research log. `DreamSys`'s
  turn is 6 degrees per tick (`sTurnRotations`), the glance 45 a tick up
  to 180, look up/down 600 a tick up to 9000, all from held pad buttons
  in `DreamSys__OnPadEvent`; a mouse needs a `#ifdef PLATFORM_PC` hook
  that adds an arbitrary yaw through `updateRotation`.
- Unchanged: x86_64 crashes at boot (PLAN "Later", 64-bit clean);
  `resolution`, `widescreen`, `high-fps` (PLAN "Later").

## How task 07 worked (reuse it)

- **Tick counting.** The dream's FrameClock, `sDreamAuxFrameClock`
  (`frameCount` at +0x0C, `paused` at +0x10), counts one per tick;
  `sDreamAuxStage` is the stage, `sDreamAuxWorld` the `DreamSys`
  (`coord2` at +20, its `coord.t` at +24; `moveMode` +172, `lookYaw`
  +148; `ptype /o DreamSys` in gdb for the rest).
  - Port: read them from `/proc/<pid>/mem` of a child process (the parent
    may read it with ptrace_scope 1), with `open(..., buffering=0)`: a
    buffered file returns stale bytes. Addresses from `nm` of that very
    binary plus the PIE base from `/proc/<pid>/maps`.
  - DuckStation: a small GDB-remote client over the stub (port 2345):
    send 0x03 to stop, `m<addr>,<len>` to read, `c` to continue; no
    breakpoints. PS1 addresses from lsddecomp's `build/lsdde.elf`
    (`Vcount` 0x8006d358, `sDreamAuxFrameClock` 0x8008ac08,
    `sDreamAuxWorld` 0x8008ac00, `sDreamAuxStage` 0x8008abf8). Emulated
    time is `Vcount` / 59.94, so the stub's pauses do not count.
  - Cross-check without the stub: DuckStation's media capture with
    `VideoCodec=ffv1` in mkv records every vblank; count frames that
    change while turning.
- **Real keys.** The GL build (`build-i686-gl`, `-DPSYZ_RENDERER=
  sdl3_gl`) in a private Xvfb (`Xvfb -displayfd`), `SDL_APP_ID=
  claude-lsd`, keys with `xdotool keydown/keyup` after `windowfocus`;
  game state from `/proc/<pid>/mem` as above, screens classified with
  `lsd_drive.classify` against locally made references (menu, graph,
  comment, diary). The close button without a window manager: send
  `WM_PROTOCOLS`/`WM_DELETE_WINDOW` with a ten-line Xlib program
  (`xdotool windowquit` needs a WM).
- Under Wine, the same `xdotool` keys inside `xvfb-run` reach `lsd.exe`
  (task 06's prefix setup).
- In the title menu START starts the day; the intro movies do not skip
  (about 80 s to the menu); SELECT + triangle ends a dream only from the
  pause screen (`ObjM__UpdateCloseReadyFlag` needs `pauseSetupStep`).
- psyz host tests: run from `psyz/psyz/tests` (else about 50 fail on
  missing `expected/` files) and headless (`env -u DISPLAY -u
  WAYLAND_DISPLAY SDL_VIDEO_DRIVER=offscreen`; they open a window
  otherwise). Fork `main`: x86_64 316 passed / 2 skipped; i686 317
  passed / 1 failed (`gte::read_rot_matrix...`, pre-existing).
- Rules unchanged (task 01's and task 07's): lsddecomp only on topic
  branches; no push to any `main` without the operator's yes; no game
  data, recordings or screenshots in any repository; no windows on the
  operator's display.
