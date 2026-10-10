# Task 24: a softer far edge (fog curve and fade-in)

The operator finds it odd how distant things pop into view as the
camera turns or the player walks. Raising `draw_distance` is not the
answer: it thins the fog, so the pops get more visible. Build two
port-only options that hide them:

1. **A fog curve that ends where the map ends.** The game's depth cue
   (psyz's `RTP_DEPTH`: dp = DQB + DQA · h / SZ) is fixed in shape: it
   rises steeply just past fogNear and flattens out, reaching ONE only
   at five times fogNear. A different curve, clear up close and blending
   to the fog colour just inside the distance the map always covers, so
   that anything switched on at the edge is already fully fogged.
2. **Newly shown cells fade in.** A map cell that wasn't drawn last
   frame starts in the fog colour and clears over about half a second,
   so pops left after 1 become fades.

Both default to the console's behaviour, and both change drawing only.

Read first: `docs/design.md` ("Draw distance", "Widescreen edges"),
`docs/tasks/14-draw-distance-handover.md`,
`docs/tasks/17-widescreen-edges-handover.md`, `src/draw_distance.c`,
`src/widescreen.c`, `decomp/src/graphics/tmd_renderer.c`
(`TransformAndCullPoly`, `ADD_CLUT_ROWS`, `DP_CLUT_SHIFT_*`),
`decomp/src/app/viewport.c` (`SetFogNear` and `SetFarColor` in the
update), `decomp/src/world/stage_map.c` (the footprint and how cells are
shown and hidden), `psyz/psyz/src/psyz/libgte.c` (`RTP_DEPTH`, both
variants, `SetFogNear`, the colour commands that interpolate toward the
far colour by IR0), the docstrings of `tools/lockstep.py` and
`tools/ds_spot.py`. Task 01's Rules apply.

## Where things stand (2026-10-10)

- lsd-port `main` `9ca58b5`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `e56d5e1`. Task 23 (second ring) is not started; this task doesn't
  need it, but should say how the two would combine.
- What is thought to cause the pops (not yet confirmed):
  - The StageMap draws a 20 x 20-cell window (2048 units a cell),
    axis-aligned by quadrant and shifted toward the look direction.
    Turning or crossing a cell switches whole rows on and off at its
    edges, about 40960 units ahead.
  - At fog levels 0 to 2 (most days) the fog there is thin: with
    dp = (1 - fogNear / z) · 5/4, the far edge is about 44 % fogged at
    26624 (level 0, or any day at `draw_distance = 4`), 63 % at 20480 and
    81 % at 14336. Levels 3 and 4 already cull before the edge.
  - At 16:9 the window's side can come as near as 8.4 cells (about
    17000 units) near a chunk corner, inside fogNear on clear days, so
    cells appear there with no fog at all (task 17).
- How the game uses dp: `TransformAndCullPoly` stores IR0 after RTPT
  (`gte_stdp`) once per face, and drops the face at dp >= ONE, or when
  FLAG has any bit but the SZ3/OTZ saturation. Textured faces move their
  CLUT down `dp >> 9` rows (8 fog steps, the stage's fog palettes) when
  lit with fog, `>> 16` (none) otherwise. Untextured faces are cued by
  the GTE's colour commands toward the far colour by IR0.

## Questions, in order

1. **Confirm the cause.** Headless, at a few spots (task 14's eight, and
   Happy Town for 16:9): turn on the spot and walk, with a screenshot
   every frame, at 4:3 and 16:9 and `draw_distance` 1 and 4. Are the pops
   footprint cells switched on at the window's edges, or also something
   else (models, sprites or effects with their own culling, chunk loads)?
   Count them by kind if it is more than one. If it's mostly something
   else, stop and report.
2. **The curve (option 1).** Where to put the depth-cue change: a psyz
   switch (an LSD-only delta behind a define or a runtime setting, as the
   fork's other ones are, close to upstream) that maps SZ to IR0 by a
   curve the port sets, instead of DQA/DQB. Then decide:
   - the shape (smoothstep between a start and an end, or anything else
     that looks right), and how it follows the game's fogNear, so that
     foggy stages stay foggy and clear ones stay clear up close;
   - where it ends: radial, at the distance the window always covers in
     the view (task 17's model: 4:3 11.6 cells, 16:9 8.4 cells nearest,
     which may be too close to look good), or by the distance to the
     window's own edge in the look direction. Try the options, and show
     screenshots of each;
   - what it does to `draw_distance` (does that setting still mean
     anything with it on?).
   Keep IR0's FLAG bits what the game would see for the same dp, so that
   `TransformAndCullPoly` culls the same way apart from dp itself.
3. **The fade-in (option 2).** A cell shown this frame that wasn't shown
   before gets extra fog that drops to none over about half a second of
   frame time (not dream ticks: with `smooth` on, frames fall between
   ticks). It probably needs a psyz hook to raise IR0 while one cell
   draws, set from a wrapper around the cell's draw. Check that
   widescreen.c's extra cells, which it shows and hides every tick, don't
   restart their fade every tick. Textured faces have only 8 fog steps:
   say whether the steps show in motion, and what would smooth them if
   they do.
4. **Settings.** Probably one setting, `fog = console | soft` (command
   line, environment, `settings.ini`, the settings menu), or two if the
   fade earns its own. Default `console`, which must leave psyz's GTE
   exactly as it is.
5. **Check**:
   - lockstep STATE and SAVEBLK lines identical with it off and on, at
     task 14's eight spots and days 22 and 340, at 4:3 and 16:9;
   - with it off, freeze screenshots byte-identical to `main`;
   - the same turns and walks as in 1, with it on: pops counted before
     and after, and screenshots at the worst spots and headings (in the
     scratchpad);
   - frame times (task 14's method);
   - builds: x86_64 and i686, Windows (MinGW); psyz's host tests.
6. README ("Picture"), `docs/design.md` section, and handover
   `docs/tasks/24-soft-fog-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- lsddecomp: no changes. Wrap method tables from the port, as
  widescreen.c and draw_distance.c do.
- psyz fork: push and merge freely, one topic branch from the fork's
  `main`; keep the delta small and switched off by default; never push
  to or open anything on the original psyz.
- Headless only (offscreen, the debug server; DuckStation on Xvfb as its
  memory notes say); kill what you start, by pid. At most three lockstep
  runs at once.
- No game data, screenshots or recordings in any repository.

## Report back with

What the pops were (by kind, from 1); the curve chosen, where it ends,
and why, with screenshots against `console`; how the fade-in works and
whether its steps show; the setting(s); pops counted before and after;
lockstep, screenshot and frame-time results; what task 23 would add on
top.
