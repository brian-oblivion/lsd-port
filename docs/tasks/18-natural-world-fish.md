# Task 18: the fish at Natural World that tears

The operator, playing at Natural World (2026-10-08, green sky, the
settings of task 17: `--aspect 16:9 --resolution 6 --draw-distance 4`),
took four screenshots in a row of a large flying fish beside a row of
tree-stump cliffs. In one it is whole: head, eye, scaled body, fins,
tail. In the other three the same model is folded flat into a sheet,
stretched sideways, the tail fin pulled far out to the left, a fin
floating loose below it, and a small green triangle at a different spot
each time. Find out whether the console shows this, and if not, what the
port does wrong, and fix it.

Read first: `include/style_effect.h` and `src/world/style_effect.c`,
`src/world/style_layer.c` (StyleUpdateEffectSlots and its caller),
`docs/tasks/15-smooth-animation-handover.md` (the last part of "Checks"),
`docs/design.md` ("Pace and smooth", "Smooth"), the docstrings of
`tools/lockstep.py`, `tools/ds_drive.py` and `tools/ds_spot.py`. Task 01's
Rules apply.

## Where things stand (2026-10-08)

- lsd-port `main` `98a42d1` (task 17's branch `task-17-widescreen-edges`
  is not merged; it doesn't touch models); `decomp/` `95f67e0fb`;
  `psyz/` the fork's `main` `ad64361`.
- Seen headless as well (task 17, x86_64 Debug, lockstep config below):
  at Natural World, day 9 (style day 10), spawn [3, 30], walking, a large
  pink-grey textured object at the right of the picture near tick 300
  that keeps changing shape **while the dream clock is frozen**: eight
  shots 0.13 s apart in one freeze are all different.
- **Not `smooth`'s blending.** The same freeze with `LSD_SMOOTH=off`
  cycles through the same shapes. Task 15 saw the same object ("an
  `0xef34` actor, which moves on frame time"; eight shots across a freeze
  cycle through four pictures) and put it down to timing.
- Class 0xEF34 is **StyleEffect**: the style layer's decoration, models
  from DREAMER.TMD (ETC.TIM texture), kept at an offset from the player.
  StyleUpdateEffectSlots updates it, and its `tick` counts updates, not
  dream ticks. Kind 0 (`STYLE_EFFECT_MODEL_ROW`) is a model plus two
  copies that spin and drift along z after 500 updates. Whether the fish
  is a StyleEffect is the first thing to confirm; it may be an Entity or
  a TodActor instead.

## Questions, in order

1. **Which object is it?** Its class, its model (which TMD, which
   object index), who updates it, and how often: per dream tick (15 a
   second at pace 14) or per drawn frame (60 or more in the port; how many
   on the console, where the dream draws at 20?).
2. **What does the console show?** DuckStation at the same day and spawn
   (`tools/ds_spot.py`), screenshots across a few seconds. Whole fish
   only, or the same folds? If the console folds it too, it is the game's
   look, and the task ends with a note in design.md.
3. If the port differs: **where?** Candidates, none checked yet:
   - It is updated per drawn frame, and the port draws more frames than
     the console (the 20 fps dream, `pace`, `frame_rate`), so it animates
     faster or with steps the console never shows. Compare its update
     count per second on both.
   - Its matrices overflow: large scale ratios (Ratio16 → 4.12 in an s16)
     or MulMatrix products that wrap differently in psyz's software GTE
     than in the console's GTE (16-bit matrix elements, 44-bit MAC
     saturation, the IR flags). Dump `coord2->coord` and `workm` of each
     part in a torn frame and a whole one.
   - A model whose parts are drawn under the wrong parent coordinate
     (GsGetLws / GsGetLs over a dirty `flg`), or the vertex data read
     from the wrong place (a 64-bit layout difference: try the i686 build).
   - The TMD renderer (`TransformAndCullPoly`, `DIVPOLYGON`) with a
     polygon crossing the near plane or SZ saturation: the "triangle in a
     different spot each time" may be that.
4. **Fix what's cheap**, without changing the game's state (lockstep:
   identical STATE lines with and without the fix), and with 4:3 and
   `smooth = off` still matching the console where they did.
5. Report and handover, `docs/tasks/18-natural-world-fish-handover.md`.

## Reproducing (task 17's config)

```json
{"seed": 4321, "days": [9], "spawn": [3, 30],
 "freeze": [300], "freeze_len": 60, "freeze_shots": [8, 0.13],
 "trace": [60, 700],
 "input": [[61, 200, "up"], [200, 215, "left"], [215, 400, "up"],
           [400, 410, "right"], [410, 700, "up"]],
 "env": {"LSD_PACE": "14", "LSD_SMOOTH": "on", "LSD_DRAW_DISTANCE": "4",
         "LSD_ASPECT": "16:9"},
 "steps": [["menu"], ["day"]], "day_timeout": 400}
```

The object is at the right edge of the picture at tick 300, close to
the player; walking or turning toward it gives a better view. Freeze
shots of things that move on frame time differ between runs whose
timing differs (a gdb counter slows a run); compare runs of the same
config.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz.
- lsddecomp changes only if they keep the PS1 bytes identical, on a topic
  branch for review.
- Headless only (offscreen, the debug server; DuckStation on Xvfb as
  its memory notes say); kill what you start, by pid.
- At most three lockstep runs at once: nine together stalled in the
  intro movies (task 17).
- No game data, screenshots or recordings in any repository.

## Report back with

What the object is and how often it updates on each side; whether the
console folds it too; if not, the cause, by name (a function, a field, a
psyz routine); what was fixed and how, with the lockstep result; what
wasn't, and why.
