# Task 12: smooth frames and the console's pace

Two settings that change how the dream feels, each off (the game's own
behaviour) unless the player turns it on:

- `smooth = on`: frames drawn between the game's ticks, with the camera and
  objects blended between the last two ticks, at the display's rate.
- `pace = N`: the dream's logic at N ticks a second. 20 is what the game's
  code asks for and what the port runs today; about 14 is what a
  PlayStation managed (task 07).

Together they let the port pace the dream like the console without
looking choppier than it. Read first: `docs/PLAN.md` ("Later",
`high-fps` and `speed`), `docs/tasks/07-controls-handover.md` (the
13.8-ticks-a-second measurement), `src/widescreen.c` (how the port hooks
the game's methods without touching lsddecomp), and the docstrings of
`tools/lockstep.py` and `tools/lockstep_gdb.py`. Task 01's Rules apply.

## Where things stand (2026-10-06)

- lsd-port `main` `727ee58` (v0.2): x86_64 by default, i686 in CI, macOS
  builds. `decomp/` pins lsddecomp `main` `95f67e0fb`, `psyz/` the fork's
  `main` `56aac8c`.
- The game paces itself: `DrawSystem__Init` sets `vsyncCount` 3 and
  `DrawSystem__RunLoop` (draw_system.c) calls `VSync(3)` and then its
  callback, one tick per three vertical blanks: 20 a second at psyz's
  59.94 Hz.
- psyz's `VSync(n)` (`Psyz_VideoVSync`, platform/sdl3_common.h) presents
  one frame and then waits n frames, so the port shows each tick once.
- A dream tick draws through the Viewport (app/viewport.c):
  `Viewport__Update` (on the FrameClock's event) sets the projection and
  `GsSetRefView2(&refView)`, clears this half's OT, and walks the scene
  graph (`drawNode`: `GsGetLs` on each node's `coord2`, then the game's
  own `GsSortObject4` in tmd_renderer.c); `Viewport__Flip` (on the
  DrawSystem's VSync event) sorts the clear and `GsDrawOt`s it, then
  flips `otIndex`.
- The camera is a view child: DayTask and ObjM call
  `attachViewChild(vp, dreamSys, &viewPoint, &viewRef, ...)`, so
  `refView` is relative to DreamSys's `GsCOORDINATE2` (`refView.super`).
  Blending DreamSys's coordinate between ticks moves the camera with it;
  objects are the same mechanism on their own nodes.
- Each stage's time limit (`sStageTimeLimits`, 15 ticks a "second")
  counts ticks, so at a slower pace a dream lasts longer in real time, as
  it did on the console.

## The work, in order

Stop where the time runs out; 1 and 2 are a useful result on their own.

### 1. Pace

`pace = N` (settings.ini, `--pace N`, `LSD_PACE`; 10 to 30, default 20):
the dream's logic runs N ticks a second.

- Find where to set it without changing lsddecomp: psyz's VSync wait
  (a fork API such as a target tick rate for `VSync(n)`, kept upstreamable),
  or a wrapped DrawSystem method, as widescreen.c wraps DayTask's.
  Menus, the graph and movies may keep their own pacing; say what each
  does.
- **First check the music.** On the PS1 the sequencer runs from the
  VSync interrupt, independent of the game's loop. Find out whether psyz
  runs those callbacks (`VSyncCallback`, libsnd's `SsSeqCalledTbyT`
  timing) from the game's `VSync()` calls; if it does, a slower pace
  slows the music, and the callbacks must keep 59.94 Hz. Measure: disk
  audio (`SDL_AUDIO_DRIVER=disk`) of the same dream at pace 20 and 14,
  and compare the music's tempo (onsets or the RMS envelope per second),
  not its length.
- Lockstep: pace 14 and pace 20 must give identical STATE lines (the pad
  script is indexed by ticks, so the same ticks get the same input).

### 2. Smooth camera

`smooth = on` (settings.ini, `--smooth`, `LSD_SMOOTH`; default off):
between two ticks, present a frame at each display refresh with the
camera blended between the previous tick and the current one.

- Keep each tick's DreamSys coordinate (position and rotation) and the
  one before. For an in-between frame at fraction t: write the blended
  coordinate into DreamSys's `coord2`, mark it for recompute (`flg = 0`),
  run the Viewport's draw pass and flip, then put the real coordinate
  back. Blend the rotation as an angle (DreamSys keeps its yaw), not by
  mixing matrices.
- The double buffer: an in-between frame must not leave `otIndex`, the
  packet area or psyz's queue different from what the next real tick
  expects.
- **The draw pass must not change the game.** If drawing steps an
  animation, a timer or `rand()`, extra draws change the game. Check with
  lockstep: smooth on and off must give identical STATE and SAVEBLK
  lines over several days (task 11's configs). A difference names the
  tick; find what the draw pass touched there.
- 2D drawn in the dream (the pause text, fade boxes) is redrawn as it is;
  only coordinates are blended.
- Present at the display's refresh rate (psyz's VSync modes; `LSD_VSYNC`
  as today). Headless, measure frame times from psyz's metrics instead.

### 3. Jumps

A link, a teleport, a respawn, the start of a day or a flashback moves
the player in one tick; blending across it would sweep the camera through
the world. Detect these (DreamSys's link state and `ExecuteLink`, or a
distance and angle threshold per tick) and draw the new position without
blending. Write down which you used and why.

### 4. Objects

The same blending for nodes that move between ticks (creatures, the
flying objects, events). Only nodes whose coordinate changed between the
two ticks need it. TOD shape animation stays at the tick rate, as on the
console.

### 5. Docs and defaults

README ("Running a release": the options table, settings.ini), PLAN
(`high-fps` and `speed` entries), design.md (how it works, what was
measured). Both settings default off; the operator chooses the defaults
after playing `pace = 14, smooth = on`.

## Checks

- Lockstep, x86_64 Debug: smooth on vs off, and pace 14 vs 20, identical
  STATE and SAVEBLK lines over task 11's r1 to r4 day lists.
- Screenshots during a turn with smooth on: in-between frames show
  in-between angles, and none is drawn across a link.
- Music tempo the same at pace 14 and 20.
- Speed: a Release build holds the display rate with smooth on (psyz's
  metrics; headless runs uncapped first, then `LSD_VSYNC=off` at 59.94).
- i686 and Windows build; CI green.

## Rules (in addition to task 01's)

- No lsddecomp changes unless one is byte-exact on the PS1, and then on a
  topic branch for review. Hooks live in lsd-port (as widescreen.c) or in
  the psyz fork.
- psyz fork: push and merge freely; never push to or open anything on
  the original psyz.
- lsd-port: work on a branch; `main` is pushed by the operator.
- Headless only (offscreen, the debug server). Kill what you start, by
  pid. Keep run output off `/tmp`'s quota: delete run folders as you go.
- No game data, screenshots, recordings or save files in any repository.

## Report back with

What each setting does and where it hooks in; the lockstep results (smooth
on/off, pace 14/20); the music measurement; how jumps are detected; frame
times; what's left (objects, if not done). A handover,
`docs/tasks/12-smooth-and-pace-handover.md`.
