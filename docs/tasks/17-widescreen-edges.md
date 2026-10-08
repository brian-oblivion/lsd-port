# Task 17: things missing at the sides in widescreen

The operator, playing at `--aspect 16:9 --resolution 6 --draw-distance 4`
(Happy Town, 2026-10-07), suspects the usual widescreen-port bug: objects
or ground that are there in the 4:3 middle but missing ("deloaded") in the
extra width at the sides. Find out whether it happens, what causes it, and
fix it if it can be fixed without changing the game.

Read first: `docs/research/host-link-surface.md` ("Widescreen (task 09)"),
`src/widescreen.c`, `docs/tasks/14-draw-distance-handover.md` (the
footprint section: what the StageMap draws), `docs/design.md` ("Draw
distance"), and the docstrings of `tools/lockstep.py` and
`tools/lockstep_gdb.py`. Task 01's Rules apply.

## Where things stand (2026-10-07)

- lsd-port `main` `f5fc766`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `main` `ad64361`.
- Widescreen is anamorphic: psyz's GTE squeezes projected X by 3/4
  (`Psyz_GteSetScreenXScale`) while a DayTask runs, and the display
  stretches it back. Per polygon the game culls only on GTE flags,
  winding and the fog's depth cue; `DIVPOLYGON` clips to 320x240 in
  screen space, which the squeeze keeps. Task 09 walked six stages at
  16:9 and saw nothing missing, but with the console's fog, which hid the
  far sides.
- A quick search found no culling by screen position in the game's world
  code. World sprites (`Viewport__DrawNode`) are projected by the game
  itself, `x * projH / z`, clamped to ±512 (`SPRITE_POS_LIMIT`), and only
  then squeezed through `GsSortSprite`'s GTE path. Task 09 noted that a
  plain sprite (scale 1, unrotated) is not squeezed at all.

## The operator's screenshot (Happy Town, 16:9, draw distance 4)

Taken on the red-tile ground with the tall brown hexagonal pencil-shaped
building on the left, the "LOVE" sign (seen from behind) and a small green
house beyond it, a slanted red-and-yellow face panel in the middle. Not in
the repository; this is what it shows:

- **Ground missing on the left, close to the camera.** The red-tile ground
  ends in a straight line from the left screen edge, roughly 37 PSX lines
  below the horizon, up to the foot of the brown building; left of the
  building there is only sky down to that line. On the right the ground
  (the purple brick strip with its yellow line) runs on to the horizon
  and the screen edge. A straight world-space edge on the side the view
  turns away from: that looks like suspect 1, the footprint shifted
  toward the look direction, and it is near enough that no fog level
  except 4 would hide it, so it should show at draw distance 1 as well.
- In the 4:3 part of the picture the same edge would fall behind the
  brown building, so from that spot the console wouldn't show it; only
  the extra width exposes it.
- A thin straight line from the top edge down to above the brown
  building's tip (about a fifth of the way in from the left), and a few
  single dark dots in the sky. Could be a long sliver of a polygon with a
  vertex far off-screen (GTE saturation, or something the squeeze lets
  through that 4:3 keeps off-screen); find which primitive it is.
- The mouse pointer shows over the game window (not this task's; note it
  for the operator if it's cheap in psyz).

## Suspects, most likely first

1. **The map's footprint.** The StageMap draws only 20 x 20 cells (2048
   units each), from the player's cell forward, axis-aligned by quadrant
   and shifted toward the look direction by at most 9 cells
   (`StageMap__ComputeFootprintFromRotation`). Everything placed on the
   map (ground, buildings, props) belongs to a cell, so whole cells
   appear and vanish together. At 4:3 (half-angle tan 0.6) the footprint's
   sides come into view from about 27000 units ahead when looking along an
   axis; at 16:9 (tan 0.8) from about 20000, and they move as the player
   turns past a quadrant boundary. With `draw_distance` above 1 the fog no
   longer hides them. The sides change with the view direction, which
   would look exactly like things "deloading" at the edges.
2. **Sprites.** A world sprite at scale 1 (a SPRT) isn't squeezed, so it
   would sit 4/3 too far from the centre and could leave the screen early;
   the ±512 clamp is in unsqueezed units.
3. **Something the game decides by angle or distance** (an Entity
   spawning, a trigger), unrelated to the picture: check before blaming
   the renderer.

## The work

1. **Reproduce.** At a fixed spot (lockstep `spawn`, freezes, a slow
   turn), compare 4:3 and 16:9 shots at `draw_distance` 1 and 4: is
   anything in the 4:3 picture's area missing at 16:9, or anything at the
   16:9 sides cut off in a way 4:3 would never show? Kyoto, Happy Town
   and Natural World on level-0 to 3 fog days are good places to look
   (task 14's spots). Screenshots stay out of the repositories.
2. **Explain each case**: footprint, sprite or something else, by name
   (a node, a cell, a function).
3. **Fix what's cheap**, behind the widescreen setting only, so 4:3 stays
   the console's picture byte for byte, and with the game's state
   untouched (lockstep: identical STATE lines with and without the fix).
   For the footprint, the one-chunk ring limits how far it can go (task
   14: 20 cells is the most that always stays inside the loaded chunks);
   widening it sideways only, or not shifting it toward the look
   direction, might be within reach; say what it costs if not.
4. Report and handover, `docs/tasks/17-widescreen-edges-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz.
- lsddecomp changes only if they keep the PS1 bytes identical, on a topic
  branch for review.
- Headless only (offscreen, the debug server); kill what you start, by
  pid.
- No game data, screenshots or recordings in any repository.

## Report back with

Whether things go missing at 16:9, and which ones; the cause of each; what
was fixed and how, with the lockstep result; what wasn't, and why.
