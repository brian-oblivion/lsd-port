# Task 18: the fish at Natural World that tears — where it ended

Done 2026-10-08/09 on lsd-port branch `task-18-natural-world-fish` (from
`main` `e7c33db`), not merged or pushed. No change in lsddecomp or psyz.

## The object

- **An Entity, not the StyleEffect.** Class 0x1F234 (Entity, a
  TodActor), under the StageMap, at scale 6.0 (24576), with Actor parts
  (0x34) three to six coordinates deep: a 27-polygon body, fins of two,
  a tail of four. Hiding every StyleEffect and its model children
  (GsDOFF from gdb) leaves the fish as it was. The StyleEffects there
  are DREAMER.TMD model 0, a single polygon at scale 6 x 1 x 7: the thin
  green streak in the sky, not the fish.
- **Who updates it, how often.** `TodActor__Tick`, on the dream
  FrameClock's RUNNING event: once a dream tick, 14 a second at pace 14,
  20 on the console. Each tick it steps the TOD animation and sets the
  Entity's `coord2->flg = 0`. It is drawn every frame: 60 a second in
  the port with `smooth`, 20 on the console, one draw a tick. (The
  StyleEffects are ticked by ObjM's update, once a pass; it keeps
  running while the lockstep freeze stops the dream clock, which is why
  task 15 saw an `0xef34` actor move in a freeze.)

## The console

DuckStation at day 9, spawn [3, 30], standing (`tools/ds_spot.py`, 30
shots 0.7 s apart): a fish's head at the left edge, in the fog, whole
(eye, mouth, scales) in every shot. The console doesn't fold it. Not the
same fish or the same moment as the port's (the console's `rand()` is
not seeded, the player doesn't walk there), but the same kind of model.

## The cause: `SortTmdObject` and libgs's matrix cache

- `SortTmdObject` (the game's GsSortObject4, `tmd_renderer.c`), once
  the GTE has the object's matrices, multiplies the object's **cached
  local-to-world matrix** (`coord2->workm`) by its parent's in place,
  column by column (`gte_rtir` under the parent's `workm`). The draw
  itself isn't affected, but the cache now holds the parent's rotation
  and scale twice.
- `GsGetLw` (PSY-Q's, and psyz's after it, checked against the retail
  code at 0x80014768) reuses a coordinate's `workm` when no coordinate
  up its chain is marked changed. On the console every draw follows a
  tick, and the tick's `TodActor__Tick` marks the Entity changed, so its
  parts are always computed afresh and the multiplied cache is never
  read.
- `smooth`'s in-between frames draw without a tick. The Entity isn't
  marked changed (it hadn't moved, so `BlendTree` didn't blend it);
  the body's `workm` came from the cache with the 6.0 applied twice
  (elements saturated at 32767), so it was drawn folded and stretched,
  in every frame but the tick's: the one whole picture in four. Parts
  that the TOD animation moved that tick were blended, marked changed,
  and drawn right, hence a fin floating loose beside the torn body.
  gdb at `SortTmdObject`, ticks 290 to 300: body `workm` at most 24576
  on the tick's frames, 32768 on all 36 in-between frames.
- Not `smooth`'s blending and not the GTE: the tick's frames were right,
  the matrices are the game's own, and the cache is restored after each
  in-between frame (so the game's state never saw it).
- The lockstep freeze shows the same thing for another reason: it stops
  the dream clock and the world is still drawn, without `TodActor__Tick`,
  so the cached body compounds every frame. The game never draws with
  its clock stopped: its pause (`ObjM__AdvancePauseSetup`) turns the
  viewport's drawing off first. Noted in `tools/lockstep.py`'s
  docstring.

## The fix (`src/pacing.c`)

After each tick's pass, `PacedRunLoop` takes libgs's frame stamp (the
`flg` that `GsGetLw` writes into a coordinate it computes; PSDCNT is
libgs's own, so `FrameStamp` computes a root coordinate of ours and
reads its `flg`). `BlendTree` marks changed (`flg = 0`) every coordinate
whose `flg` is that stamp, i.e. every coordinate the tick's draw computed,
so the in-between frame computes them again instead of taking the cache.
A coordinate the tick's draw took from the cache is left to the cache, as
the console's next draw would. `Restore` puts every `flg` and `workm`
back as before. `docs/design.md` ("Smooth"), the comments in `pacing.c`.

## Checks

- Turned toward the fish (ticks 280 to 292, then standing; 16 shots
  0.05 s apart, x86_64 Debug, 16:9): `main` shows a dark folded sheet
  in all 16 (in one, two fins standing loose before it), the fix a whole
  fish in all 16 (eye, mouth, scales, fins). The debug server's shots
  caught no tick frame on either build.
- gdb, same run: the body's `workm` at most 24576 on every frame, tick
  or not.
- Lockstep, `main` against the branch (x86_64 Debug, pace 14, smooth on,
  seed 4321): the task's config (day 9, Natural World, 16:9, freeze at
  300; 641 STATE lines to tick 700), and days 22 and 340 at 4:3
  (walking, freezes at 300 and 600; 1192 STATE lines each, to tick
  1200): every STATE and SAVEBLK line the same.
- `smooth = off` and 4:3: untouched (the code runs only for in-between
  frames, which `smooth = off` doesn't draw; nothing depends on the
  aspect).
- Builds: x86_64 Debug and RelWithDebInfo, i686 RelWithDebInfo; only the
  two old "function called through a non-compatible type" warnings.
- Frame times (RelWithDebInfo x86_64, 59.94 Hz, psyz `/metrics` draw
  time, Natural World day 9 at the spawn, 16:9, about 230 samples a
  run, two runs each): medians 358 and 294 µs on `main`, 276 and 302 on
  the branch; 90th percentiles 448/400 against 341/407. Run-to-run
  noise. (The in-between frames now compute what the tick's frame
  computed, the cost a tick's frame already had.)

## Not done, and why

- The tail fin's matrix grows to saturation over a few seconds on tick
  frames as well (its parent's `workm` goes 19914 → 32767 between ticks
  290 and 300), the same on `main` and the branch. That is the TOD
  animation's own scale, drawn alike on every frame now; whether the
  console's tail does the same at that moment was not seen (the console
  run had no fish close by). Worth a look if the tail still looks
  stretched while playing.
- Freeze shots still tear TOD models: the freeze is the tool's, not the
  game's (above). Making the lockstep freeze faithful would mean turning
  drawing off, i.e. no freeze shots; left documented instead.
- A small grey triangle moving across the sky near the fish is in both
  builds' frames alike, steadily from frame to frame: another of the
  day's objects, likely the operator's "small triangle at a different
  spot each time". Not identified.

## Reproducing

The scratch tools are not committed (gdb hooks in the scratchpad). The
task's lockstep config; the view runs replace its input with

```json
"input": [[61, 200, "up"], [200, 215, "left"], [215, 280, "up"],
          [280, 292, "right"]],
"steps": [["menu"], ["press", "start", 1.0], ["wait_tick", 300],
          ["tick_shots", "v", 16, 0.05]]
```

and drop `freeze`. The fish is Entity `0x7ffff66103e8` under gdb in that
run (addresses repeat run to run there).
