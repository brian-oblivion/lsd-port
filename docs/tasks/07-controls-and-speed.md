# Task 07: controls and speed

Handover. Read `docs/PLAN.md` (its "Later" list has the new entries
`speed`, `controls` and `mouse-look`), `docs/tasks/01-game-c-builds-for-host.md`
(its Rules section applies unchanged) and `docs/tasks/06-release-handover.md`
first.

## Where things stand (2026-10-04)

- lsd-port `main` is `89c2712` plus this handover. Work on a new branch
  from it, `task-07-controls`.
- psyz fork `main` is `0762235` (pushed: task 06's card branches and
  `pads-keyboard-seen`); lsddecomp `main` is `0355ebad0`.
- Releases: `.github/workflows/release.yml` is on `main` and has never run;
  no tag. Saves live in `~/.local/share/lsd-dream-emulator/`
  (`%APPDATA%\lsd-dream-emulator\`).
- The operator played the Linux build and reported:
  1. turning and moving **feel faster than on the PlayStation**;
  2. the arrow-key controls feel dated: they want **WASD and modern
     bindings**;
  3. later, as an option: **mouse look** (PLAN "Later", not this task).
  The fourth report, no keys under Wine, was fixed in psyz
  `pads-keyboard-seen`.

## What is known

- The dream's controls (`DreamSys__OnPadEvent`, lsddecomp
  `src/world/dream_sys.c`), all "held" unless noted: d-pad up/down
  forward/back, left/right turn (6 degrees per tick, `sTurnRotations`);
  triangle look up, square look down; cross with forward runs; L1/R1 look
  left/right (up to 45 degrees, springs back); L2/R2 strafe; circle
  (pressed) sets `linkCommandFlag`. START (pressed) pauses
  (`ObjM__DispatchPadEvent`); SELECT held then triangle ends the dream.
  Menus: up/down, circle confirms, cross backs out (check).
- The keyboard map is psyz's (`keyb_p1`, `psyz/src/platform/sdl3_common.h`):
  arrows d-pad, Enter START, Backspace SELECT, D circle, S triangle,
  X cross, Z square, Q L1, W L2, E R2, R R1, 1/2 L3/R3. **Escape quits at
  once**, with no question (`PollEvents`); F4 fullscreen, F6 VRAM view.
  Gamepads go through SDL's gamepad mapping.
- The port runs the dream logic at exactly 20 ticks per second (the game
  calls `VSync(3)`; psyz paces 3 vblanks at 59.94 Hz, metrics
  `psyz_frame_time_microseconds` 50050). Per tick the game does the same
  as on the console, so if the console feels slower it is because it
  **runs fewer ticks per second**: a PlayStation that misses the 50 ms
  deadline waits for the next vblank, and the dream drops to 15 or 12
  ticks per second where it is heavy. The port never misses one. This is
  the hypothesis; measure it.

## The task

One stop at a time, as before.

1. **Measure the speed** on DuckStation (`tools/ds_drive.py`, the
   reference) against the port, in the same places: the first room of
   day 1, an outdoor stage with many objects, a busy one (several days in;
   use edited saves as task 04 did, never committed). Ticks per second in
   emulated time: for example DuckStation's lossless video capture (count
   frames that change; the dream draws once per tick) or reading a game
   counter and libetc's `Vcount` (lsddecomp `build/lsdde.elf`) through the
   GDB stub at two moments, without breakpoints in hot code. Also check
   DuckStation's own settings that change speed (CPU overclock, "fast
   boot" and the like must be off; its CD read speed setting does not
   matter here). Report ticks per second per place for both. If the
   console really is slower, **stop and report**: whether the port copies
   the slowdown (it would need a model of the console's frame cost, which
   psyz does not have) or stays at a steady 20 is the operator's
   decision. Don't build a slowdown model without that.
2. **Keyboard remapping**: the keys must be configurable, in a small text
   file in the saves folder (for example `controls.ini`, written with the
   defaults on first start; one line per PlayStation button, SDL key
   names), and gamepads keep SDL's mapping. Where it lives is open: psyz
   (a `Psyz_SetKeyboardMap`-style call, which upstream may want) or the
   port's `src/main.c` reading the file. Prefer psyz if the change is
   small and general; then it is a psyz branch with a section in
   `docs/upstream/psyz-prs.md`.
3. **Modern default bindings.** Propose a default layout and **ask the
   operator before making it the default** (it is their call, like the
   console's behaviour defaults elsewhere). A starting point:
   W/S forward/back, A/D turn (LSD steers like a tank; turning is the main
   action), Q/E strafe (L2/R2), Shift run (cross), arrow up/down look
   up/down (triangle/square), arrow left/right glance (L1/R1), Space or
   E the link button (circle), Enter/Space confirm and Backspace back in
   menus, Escape pause (START) instead of quitting at once, with quitting
   behind a second press or the window's close button. Menus must still
   work: in LSD circle confirms, so whatever key is "confirm" maps to
   circle. Keep the console's layout available as a preset (`--controls
   classic` or a line in the file).
4. **README**: the controls (keyboard and gamepad), how to change them,
   and the result of stop 1.

Mouse look is **not** part of this task (PLAN "Later": it needs a hook in
the game's camera, `#ifdef PLATFORM_PC`, and is an option like
widescreen). Note what you learn about it on the way: `DreamSys`'s turn
and look commands are digital, a fixed step per tick.

## How to work (from task 06; reuse it)

- `tools/lsd_drive.py` drives a headless port run (debug server input,
  screenshots, `XDG_DATA_HOME` in its `out` directory so saves stay in
  scratch). The debug server's `/input` presses pad buttons, not keys: to
  test the keyboard map, send real keys with `xdotool` to a window inside
  `xvfb-run -a` (the GL build, `-DPSYZ_RENDERER=sdl3_gl`, for Xvfb on
  Linux; under Wine the D3D12 build works on Xvfb).
- Wine (`06-release-handover.md`): scratch `WINEPREFIX`,
  `WINEDLLOVERRIDES="mscoree,mshtml="`, `SDL_*` variables only through
  the prefix's `HKCU\Environment`; never kill wineserver by name (the
  operator's Steam/Proton runs one).
- DuckStation (`tools/ds_drive.py`, memory `duckstation-reference`): own
  config in scratch, OpenGL presenter on Xvfb, capture in emulated time,
  GDB stub reports late. lsddecomp's absolute (`A`) symbols take
  `break *0x...`, not names.
- Getting into a dream fast and to a given day: task 06's handover ("How
  task 06 worked").
- psyz host tests in a Debug build at both widths (fork `main`: x86_64
  316 passed / 2 skipped; i686 317 passed / 1 failed,
  `gte::read_rot_matrix...`, pre-existing).

## Rules (in addition to task 01's)

- **lsddecomp: never commit on `main`.** Topic branch, with
  `./build-and-verify.sh` and `tools/lint.sh` green; point `decomp/` at it
  meanwhile. This task should not need lsddecomp changes.
- **Never push to any repo's `main` without the operator's yes** for that
  push. Topic branches you may push. Pins must be on their remotes before
  lsd-port's `main` moves.
- psyz branches start from `upstream/main` (or from the merge base of
  `upstream/main` and the fork's `main`, or stacked, saying so), merge
  into the fork's `main`, and get a section in `docs/upstream/psyz-prs.md`.
- No game data, BIOS, recordings, screenshots or save files in any
  repository or release artifact.
- No windows on the operator's display: headless or `xvfb-run -a`
  (`~/.claude/CLAUDE.md`).

## Report back with

The speed measurements (ticks per second per place, both machines, how
measured) and, if they differ, the question for the operator; the
remapping design and where it lives; the proposed default layout and the
operator's answer; what was checked (keys under Xvfb on Linux and Wine,
gamepad untouched, menus, the dream, pause, SAVE); README changes; the
branches and commits waiting for review or push.
