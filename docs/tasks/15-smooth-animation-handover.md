# Task 15: smooth animation — where it ended

Done in the session after task 14 (2026-10-07), at the operator's request
("4" of the list after task 14: smooth's leftovers). No task doc came
before it; this handover is the record.

## What changed

`smooth = on` (task 12) drew frames between the dream's ticks with the
camera and moving objects blended, but left two things at the tick rate:

- **A TodActor's parts**, whose moves are its TOD animation: a creature's
  limbs, wings, a flying thing's body. Now blended like everything else.
  `TodActor__ApplyTodPacket` writes `param->rotate`, `param->scale` and
  coord.t from `param->trans`, the pose pacing.c already blends. A parent
  packet (a part moved under another part) changes the part's parent
  coordinate, which is now part of the recorded pose and counts as a
  jump, so it isn't blended across two parents.
- **GridCells**: still not blended in position (the StageMap moves them by
  whole cells to reuse them as the player walks), but now in scale, while
  the StageMap's scale ramp runs (`StartScaleRamp`, from Entity events:
  the ground rising or sinking by 1/64 or 1/4 a tick; `scaleRampTicks`
  non-zero when the tick is recorded). Outside a ramp the ~2800 cells are
  not recorded at all: recording them always cost 50 to 100 µs a frame.

Scale is blended for every node now, and a change of more than 1.0 in one
tick is a jump. `Restore` puts scales back with the rotations.

All in `src/pacing.c` (`Pose` gains `scale` and `super`; `BlendKind`
replaces `IsBlended` and the TodActor walk; `PutBlended`). README ("Pace"),
PLAN ("high-fps") and design.md ("Smooth") say so. No change in lsddecomp
or psyz.

This departs from the console in one more way: on the PS1 a creature's
animation steps at the tick rate with everything else. With smooth on the
whole dream is blended now; `smooth = off` keeps the console's look.

## Checks

- Day 22 (seed 1234, pace 14), a gdb counter on `PutBlended`: TOD parts
  (Actors under a TodActor) blended about 4100 times in the run, beside
  plain actors and the camera. One part's z rotation across a tick:
  163 → 127 drawn as 154, 145, 136, then the tick (passes 1 to 3 of 4;
  4 of 5 at the ticks that come a blank later, at pace 14).
- Scale ramp forced at tick 100 (gdb: `scaleStep = sScaleStepUpSlow`, 64
  ticks): about 2800 grid cells scale-blended per in-between frame; a
  freeze shot during the ramp shows the room's walls scaled up, drawn
  cleanly.
- Lockstep (x86_64 Debug, pace 14, smooth on, seed 4321), `main` against
  this branch: task 14's eight spots walking for 700 ticks, and days 22
  and 340 (entities flying and walking): every STATE and SAVEBLK line the
  same (61 STATE lines), on the first version and again on the final one
  (with the ramp gate).
- Freeze screenshots: 42 of 58 identical. The rest are timing, not this
  change: at Monument Park `main` differs from itself between batches
  (a link fade), and run in the same batch the two builds give identical
  shots; at Natural World a large object near the player (an `0xef34`
  actor, which moves on frame time) keeps moving while the dream clock is
  frozen, and eight shots across one freeze cycle through the same four
  pictures on both builds, so which one a shot catches depends on when it
  is taken. While frozen only that actor is blended, on both builds.

## Open

- A looping TOD animation snaps from its last frame to its first on the
  console. Blended, that step is drawn like any other when the two poses
  are within 45 degrees (and 4096, and a scale of 1.0); a larger one is a
  jump. Not looked at in pictures: worth a look while playing.
- Frame times (RelWithDebInfo x86_64, 59.94 Hz, psyz draw time, Kyoto and
  Natural World, standing and turning): medians 253 to 303 µs against 253
  to 371 for task 14's build, within run-to-run noise. A first version
  that recorded every grid cell all the time was 50 to 100 µs slower.
