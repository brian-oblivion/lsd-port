# Task 12: smooth frames and the console's pace — where it ended

Handover from task 12 (`12-smooth-and-pace.md`), 2026-10-06. All five
steps done: pace, smooth camera, jumps, objects, docs. Both settings
default off; the defaults are the operator's call after playing
`pace = 14, smooth = on`.

## Where things stand

- Build directories: `build-t12-dbg2` (x86_64 Debug, the lockstep runs)
  and `build-t12-rel` (x86_64 Release, the frame times).
- **lsd-port** branch `task-12-smooth-pace` (from `main` `435a2f4`):
  `src/pacing.c`/`.h` (new), `src/settings.c`/`.h` and `src/main.c` (the
  two settings), `CMakeLists.txt`, the lockstep tools, README, PLAN,
  design.md, this handover. Not committed yet (see the end).
- **psyz fork**: branch `vsync-run-callbacks` from `56aac8c`, one change,
  `Psyz_VSyncRunCallbacks(n)` (`include/psyz/system.h`,
  `src/psyz/libapi.c`). psyz's host tests: 319 passed. Not committed or
  pushed yet; `decomp/` is unchanged (`95f67e0fb`), no lsddecomp changes.

## What each setting does, and where it hooks in

- `pace = N` (`--pace N`, `LSD_PACE`; 10 to 30, default 20): the dream's
  ticks a second. `src/pacing.c` replaces `gDrawSystemMethods.runLoop`
  and wraps DayTask's `onInit`/`onDeinit` (as `widescreen.c` does). In a
  dream the loop makes one pass per 59.94 Hz blank (`Psyz_VideoVSync(0)`)
  and every 60 / N blanks runs a tick: `Psyz_VSyncRunCallbacks(3)` (what
  `VSync(3)` does after its wait: pads, the debug server's hook once, the
  VSyncCallbacks — the CD service — three times), then RunLoop's callback
  and notification. Each tick sees what it sees at 20. Outside a dream
  (title menu, graph, movies, diary) RunLoop's own loop runs unchanged.
  With the defaults nothing is installed.
- `smooth = on` (`--smooth`, `LSD_SMOOTH=on`): the passes between ticks
  draw a frame each (the Viewport's `flip`, then `update`, as a tick's pass
  does) with every moving node, the camera (DreamSys) included, i / n of
  the way between where the last tick drew it and where the logic put it.
  Every coordinate is copied before and put back after, so the logic finds
  what it left. TOD parts and GridCells are not blended (design.md, "Pace
  and smooth", says why).

The order inside a pass (gdb trace): Flip, then FrameClock tick →
`Viewport__Update` (draws the world as the previous tick left it) →
`DreamSys__TimerTick` and the rest of the logic → `Pad__DispatchEvents`.
So the game's own picture already lags its logic by a tick, and blending
between "drawn" and "now" adds no latency.

## Lockstep

x86_64 Debug (`build-t12-dbg2`), task 11's configs (seed 1234), each
variant against pace 20 with smooth off. "Same" is every STATE and SAVEBLK
line.

| run | days | lines | smooth, pace 20 | pace 14 | smooth, pace 14 |
|---|---|---|---|---|---|
| rt (STATE every tick) | 22, 150, 240, 320 | 12955 | same | same | same |
| r1 | 3, 22, 30, 57 | 9 | same | same | same |
| r2 | 88, 130, 150, 177 | 15 | same | same | same |
| r3 | 210, 240, 250, 275 | 16 | same | same | same |
| r4 | 290, 320, 340, 362 | 10 | same | same | same |

The baselines end each day on the ticks task 11's table gives (683, 243,
182, 182 for r1 and so on). An earlier sweep with the camera-only build
gave the same for r1 to r4. Freeze screenshots were not compared (task 11:
they catch fades and sprites at wall-clock moments).

## The music

psyz runs libsnd's sequencer from the audio thread
(`audio_callback → Psyz_SpuPullSamples → Psyz_RcntAdd → _SsTrapIntrVSync
→ SsSeqCalledTbyT`, gdb backtrace), not from the game's `VSync()`, so the
pace can't change its tempo. Measured anyway: disk audio of day 3,
standing, the last 60 s, at pace 20 and 14. The onset envelope's beat
period is 2.79 s in both; the time stretch that best maps one onto the
other is 1.00 (correlation 0.83, against 0.03–0.04 at 0.70, 0.85, 0.95 and
1.05).

## Jumps

A distance and angle threshold per tick: a node that moved more than 4096
along an axis, or more than 45 degrees, is drawn where the last tick drew
it; when that node is DreamSys the in-between frames are skipped and the
tick's own picture stays up. Chosen over DreamSys's link state because it
covers links, respawns, the day's start and objects that teleport alike.
From four days of every-tick STATE lines: walking moves DreamSys up to
~130 a tick, some stages carry it 256 or 512 a tick for a while (blended),
a turn is 68 (6 degrees), and every link or respawn moved it 9824 to
258 196. Checked: the link on day 22 tick 118 → 119 (33 344) draws no
in-between frame (`draw_trace`).

Grid cells needed excluding by class: the StageMap moves them by 2048
(with quarter turns) to reuse them on the other side as the player walks,
under the threshold.

## In-between frames

- `draw_trace` (new in `tools/lockstep_gdb.py`) logs the camera at every
  Viewport update. Day 22 at pace 14, during a turn: yaw 2048 (tick),
  2028, 2012, 1996, 1980 (tick), 1965, 1949, 1933, 1917, 1912 (tick), ...,
  the position likewise. Steps are even across each tick's 4 or 5 passes.
- Screenshots: the debug server captures on a tick pass, which at pace 14
  shows an in-between frame (at 20 it shows a tick's). `tick_shots`
  (new step) took 30 during the turn and 24 of an entity flying 512 a tick
  (day 22); the pictures are clean, no tearing or stray geometry.
- Objects: about 2900 nodes are recorded a tick (mostly grid cells);
  blended between ticks were DreamSys, an Actor subclass `0xef34` that
  follows it, Entities (`0x1f234`) moving 50 or 512 a tick (days 340 and
  22), and plain Actors under `0xef34` (day 340).

## Frame times

Release x86_64, headless, Ryzen 9 7950X / Radeon RX 9070, `LSD_VSYNC=off`
(59.94 Hz), a dream while turning (psyz `/metrics`, 200 samples):

| | frames/s | ticks/s | frame work, mean / max |
|---|---|---|---|
| game's own (pace 20, smooth off) | 20.0 | 19.98 | 338 / 731 µs |
| smooth, pace 20 | 59.9 | 19.98 | 339 / 1596 µs |
| pace 14 | 59.9 | 13.99 | 46 / 74 µs (mostly unchanged frames) |
| smooth, pace 14 | 59.9 | 13.99 | 349 / 644 µs |
| smooth, pace 14, i686 | 59.9 | 13.99 | 488 / 722 µs |

Uncapped (`limitless`), smooth at pace 14: 3700 frames and 870 ticks a
second in a dream. (Other uncapped runs mostly missed the dream: a day
passes in seconds.)

## Builds

x86_64 Debug and Release, i686 (Linux), Windows x86_64 and i686 (MinGW)
all build without new warnings. Not run: Windows (not even under Wine) and
CI, which needs the branches pushed.

## Tools added

- `tools/lockstep.py`: `gdb_extra` (more gdb Python files, for one-off
  tracers), steps `wait_tick` and `tick_shots` (screenshots named by the
  dream tick), `DRAW` lines kept.
- `tools/lockstep_gdb.py`: `draw_trace` [from, to].

## What's left

- The defaults (the operator plays `pace = 14, smooth = on`).
- Display rates above 60 Hz: frames go out at psyz's 59.94 (its limiter on
  a 120/144 Hz display). Presenting at the display's own rate needs psyz
  to present independently of its blank count (a fork API).
- `Psyz_VSyncRunCallbacks` could go upstream; the user's call.
- TOD animation stays at the tick rate by design; a creature moved by its
  TOD root part rather than its own coordinate moves at the tick rate.
- Windows run, CI.
