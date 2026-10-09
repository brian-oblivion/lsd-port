# Task 23: a second ring of map chunks

StageMap keeps the player's chunk and its six neighbours loaded (the
ring) and draws a 20 x 20-cell window of cells from them (the
footprint). Two things end there:
- At 16:9, looking diagonally near a chunk's corner, a gap can still
  show at the side, as near as 8.4 cells (task 17).
- With `draw_distance` past the footprint, on clear days the view ends
  at the footprint's straight edge, about 40960 units ahead, not at the
  fog (task 14).
Both need a wider footprint, so a second ring: 19 chunks instead of 7.
Task 14 estimated this, but nobody has built it. The operator finds
the picture good already, so this is a "make it as good as it gets"
task. Stop and report if it turns out to change the game.

Read first: `docs/tasks/14-draw-distance-handover.md` ("The footprint
(estimate, not built)"), `docs/tasks/17-widescreen-edges-handover.md`,
`docs/design.md` ("Draw distance", "Widescreen edges"),
`decomp/src/world/stage_map.c` (`BuildFootprintRects`,
`SplitFootprintRect`, `LoadChunksAround`, `UpdateFootprintTracking`,
`StageMap__SetFootprintVisible`, `DispatchToRectCells`, the neighbour and
reslot tables), `src/widescreen.c` (the `refreshFootprint` wrapper),
`src/draw_distance.c`, the docstrings of `tools/lockstep.py` and
`tools/ds_spot.py`. Task 01's Rules apply.

## Where things stand (2026-10-09)

- lsd-port `main` `6d1d2b6`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `main` `ad64361`.
- A 30-cell `gridSpan` crashes at once in
  `StageMap__SetFootprintVisible`. `BuildFootprintRects` and
  `SplitFootprintRect` assume the window crosses at most one chunk
  boundary each way (`CellRect e[4]`).
- Memory is not the problem: chunk files are 17 to 35 KB on average,
  63 KB at most, and the pool had 2.2 MB of 8 in use at Natural World.
- The footprint is not only drawing: `DispatchToRectCells` sends
  commands to the cells in it, so a wider one could change the game.
- The fog can't go past about 27238 (the GTE's 16-bit DQA), so drawing
  further than that also needs psyz's depth cue changed.
- `widescreen.c` already shows extra loaded cells inside the wider view
  cone, and hides them before the next refresh. That is the model for
  keeping the game's own footprint untouched.

## Questions, in order

1. **Keep the game's footprint, add a port-only one.** Can the second
   ring and the wider window be the port's own, shown for drawing only
   (as widescreen.c does), so that StageMap's footprint, its commands and
   its tracking stay exactly the game's? Then the game's state can't
   change. Check that against `DispatchToRectCells` and anything else
   that walks the cells.
2. **Loading.** Chunks two away loaded ahead of being seen, from the
   same files the game loads (`Mnnn.LBD`), into cells the game doesn't
   own. When do loads happen (the CD service, a link, a respawn), and
   what does a load cost per frame?
3. **Drawing.** The extra cells are drawn and culled by the fog like the
   others. Beyond 26624 the fog needs a psyz change to the depth cue
   (DQA/DQB). Decide whether the far view is worth it, or whether this
   task only closes the 16:9 corner gap.
4. **Check**: lockstep STATE and SAVEBLK lines identical with it on, at
   task 14's eight spots and days 22 and 340. Measure the 16:9 gap again
   with task 17's model (nearest gap, share of headings), and take
   screenshots at the worst headings. Look at memory use and frame
   times.
5. Report and handover, `docs/tasks/23-second-ring-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- lsddecomp changes only if they keep the PS1 bytes identical, on a topic
  branch for review. The aim is none: wrap method tables from the port,
  as widescreen.c and draw_distance.c do.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz.
- Headless only (offscreen, the debug server; DuckStation on Xvfb as its
  memory notes say); kill what you start, by pid. At most three lockstep
  runs at once.
- No game data, screenshots or recordings in any repository.

## Report back with

Whether the game's footprint stays untouched; how the second ring loads
and what it costs; the gap measured before and after; whether the far
view (past 26624) was done, and if not why; lockstep results; memory and
frame times.
