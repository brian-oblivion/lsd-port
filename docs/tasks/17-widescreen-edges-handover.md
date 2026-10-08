# Task 17: things missing at the sides in widescreen — where it ended

Done 2026-10-07/08 on lsd-port branch `task-17-widescreen-edges` (from
`main` `98a42d1`), not merged or pushed. No change in lsddecomp or psyz.

## What went missing at 16:9, and why

1. **Ground and buildings at one side: the map's footprint.** Reproduced
   at Happy Town (spawns 3, 8 and 15, day 9, turning on the spot): the
   ground and the far wall end in a diagonal cut toward one screen edge,
   and buildings at the other edge are absent, where the fixed build
   shows them. `StageMap__ComputeFootprintFromRotation` shows a 20 x 20
   window of cells, shifted toward the look direction by up to 9 cells
   for the console's view (half-width 0.6 of the depth); at 16:9 (0.8)
   the side it was shifted away from comes into view as near as 4.9
   cells (10 000 units), against 11.6 at 4:3. The worst headings are 20
   to 30 degrees off an axis. Only level 4's fog hides that, so it shows
   at every draw distance. That is the operator's screenshot.
2. **Sparkles 4/3 too far out: plain world sprites.** Suspect 2 was
   real: StyleEffect's VariantSprites at scale 1 and rotation 0 (all five
   in the plain kind, the first in the jitter kind) are drawn by psyz as
   a SPRT, not squeezed. Seen at Kyoto, Monument Park and Natural World.
   Misplaced, not missing.
3. Nothing found of suspect 3 (the game deciding by angle or distance):
   StageMap's window is drawing only (below), and every STATE line
   matched with the window widened.

The thin line in the operator's screenshot is most likely the jitter
kind's other sprites: thin streaks (scale 256 x 12288 and the like) at a
random rotation, the game's own effect. Not confirmed at that spot.

## What was fixed (src/widescreen.c, only when wider than 4:3)

- `gStageMapMethods.refreshFootprint` wrapped: after the game's window,
  every loaded cell within the window's ahead extent (the horizon stays)
  and in the wider view cone (this tick's yaw and the last one's, for
  `smooth`'s in-between frames; about two cells' margin) is shown, and
  hidden again before the next refresh. Model over positions and
  headings: a side gap seen in 80 % of cases instead of all, nearest at
  8.4 cells instead of 4.9, and in 95 % of cases no nearer than 13.9
  (the console's 4:3: 13.3).
- `gVariantSpriteMethods.reset` and `.updateScale` wrapped: a sprite
  left at exactly scale 1 gets scalex ONE + 1, so psyz squeezes it like
  its siblings.
- docs/design.md "Widescreen edges"; host-link-surface.md's task-09 note
  points there.

## What wasn't, and why

- The rest of the gap near a chunk's corner looking diagonally (8.4
  cells at worst): the ring holds only the player's chunk and six
  neighbours, and the rows before and after cover x -10 to 30 of the
  centre's 0 to 20. A second ring is task 14's estimate: days, and
  StageMap's tracking redone.
- At 16:9 the centre now shows a little more than the console's 4:3
  where the window's side crossed the middle of the view far out
  (Happy Town spawn 15, a far object above the horizon). Restricting the
  extra cells to the side strips would make a seam; left as is.
- The mouse pointer over the window (seen in the screenshot): not
  looked at.

## Checks

- Lockstep (x86_64 Debug, pace 14, smooth on, seed 4321, draw distance
  4, 16:9): task 14's eight spots (Natural World, Kyoto, Happy Town,
  Monument Park, two days each) walking for 700 ticks, through links into
  stages 0 to 6, 12 and 13, with a STATE line every tick: the same 646 or
  647 STATE lines and SAVEBLK with and without the widening and the
  sprite wrap. Turning on the spot at Happy Town (three spawns, 35
  freezes each): the same STATE; the shots differ only in the side strips
  (and once, far out, inside the 4:3 part, above).
- At 4:3 (Happy Town and Kyoto walks): the same STATE and byte-identical
  freeze shots; nothing is installed.
- Frame times (RelWithDebInfo x86_64, 16:9, draw distance 4, psyz's draw
  time, 12 s standing and 12 s turning at Kyoto, Natural World and Happy
  Town): medians 240 to 440 µs per frame either way; the widened build's
  differ by -8 to +55 µs, within run-to-run noise (task 14: -80 to +60).
- Builds: x86_64 Debug and RelWithDebInfo, i686 RelWithDebInfo, Windows
  x86_64 (MinGW), with only the two old "function called through a
  non-compatible type" warnings.
- Shots taken in a freeze can differ between runs when the runs' timing
  differs (the sprite counter slows one): some objects (Natural World's
  big rock) animate on frame time while the dream clock is frozen. Two
  unfixed runs with the same config gave byte-identical shots; against
  the fixed build only the extra cliff at the left edge and a sparkle
  differed.

## Reproducing

Scratch tools (not committed) in this session's scratchpad `t17/`:
`fp_log.py` (gdb: the window at every refresh), `sprite_log.py` /
`sprite_log2.py` (GsSortSprite calls that are SPRTs, and their node
class), `model.py` / `dist.py` (the window and the ring against the view
cone), `tri.py` / `rank.py` (shots side by side, and where the fix
changes them). A turn on the spot, freezes every 2 ticks (freeze_len
must be long enough for the screenshot, ~30, or the shots lag behind):

```json
{"seed": 1234, "days": [8], "spawn": [4, 15],
 "freeze": [66, 68, ..., 134], "freeze_len": 30, "input": [[65, 136, "left"]],
 "env": {"LSD_PACE": "14", "LSD_SMOOTH": "on", "LSD_DRAW_DISTANCE": "4",
         "LSD_ASPECT": "16:9"},
 "steps": [["menu"], ["day"]], "day_timeout": 200}
```

More than about six lockstep runs at once stalled in the intro movies
(nine runs, unfixed build too), and once a run stalled at a freeze; three
at a time ran clean.
