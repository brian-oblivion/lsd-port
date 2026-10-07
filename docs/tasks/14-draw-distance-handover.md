# Task 14: draw distance — where it ended

Handover from task 14 (`14-draw-distance.md`), 2026-10-07. All five steps
done. The cheap version turned out cheap and is built: `draw_distance = N`,
1 to 4, default 1 (the console's).

## Where things stand

- **lsd-port** branch `task-14-draw-distance` (from `main` `f2a0f0e`), one
  commit: `src/draw_distance.c`/`.h` (new), `src/settings.c`/`.h`,
  `src/main.c`, `CMakeLists.txt`, README ("Picture" and the options table),
  `docs/design.md` ("Draw distance"), `docs/PLAN.md` ("Later"), this
  handover. Not merged or pushed.
- No change in lsddecomp or psyz: `decomp/` `95f67e0fb`, `psyz/` `2af13ca`.
- Build directories (ignored by git): `build-t14-dbg` (x86_64 Debug, the
  lockstep runs), `build-t14-main-dbg` (`main`, built from a scratch
  worktree), `build-t14-rwdi` (RelWithDebInfo, the frame times),
  `build-t14-rel`, `build-t14-i686`, `build-t14-win64`.

## The limits, and which ends the view

| limit | where it is set | value | ends the view? |
|---|---|---|---|
| fog cull, dp = ONE | `RegisterStyleConfig` → `sStyleFogNears[fogLevel]` → ObjM `setFogNear` → `Viewport__Update` `SetFogNear`; `TransformAndCullPoly` | 5 × fogNear | levels 3 and 4 only |
| footprint | `StageMap__ComputeFootprintFromRotation`, `gridSpan` 40960 (ObjM's default) | 20 x 20 cells of 2048, from the player's cell forward | levels 0 to 2 |
| GTE Z | 16-bit SZ | 65535 | no: a saturated face is subdivided (`RCpoly*`), not dropped, unless dp reaches ONE |
| DQA | `SetFogNear`: -320 · fogNear / h into a 16-bit register | fogNear ≤ 27238 at h = 266 | caps how far the fog can go |
| OT | `otLength` 13, `otShift` 16 - 13 = 3 | 8192 tags over 0..65535 | no |
| projection, near clip | `ObjM__SetupSceneStyle` (160 · 5 / 3 = 266), `nearZ` 10 | | no |
| subdivision clip | `DIVPOLYGON` window 320 x 240 | | no (screen space) |
| chunk loading | StageMap's ring: the player's chunk and its six neighbours, 20 x 20 cells each (`Mnnn.LBD`) | | no: the footprint never leaves the ring |

How the fog works out per level (dp = 5120 · (1 - fogNear / SZ), from
psyz's `SetFogNear` and `RTP_DEPTH`; textured faces take `dp >> 9` palette
rows):

| level | fogNear | palette row 7 from | culled at | at the footprint's edge (40960) |
|---|---|---|---|---|
| 0 | 26624 | — (dp < 3584 up to 65535) | never | dp 1792, row 3 |
| 1 | 20480 | — | never | row 5 |
| 2 | 14336 | 47787 | never (dp 4000 at 65535) | row 6 |
| 3 | 8192 | 27307 | 40960 | culled, exactly at the edge |
| 4 | 4096 | 13653 | 20480 (10 cells) | culled |

Level 5 (2048) is in the table but no config picks it.

Which stages get which: those with a fixed config use its first entry,
level 2 (Violence District, Flesh Tunnels, Long Hallway) or 0 (Moonlight
Tower, Temple Dojo, Clockwork Machines, Sun Faces Heave, Black Space).
The six without one (Bright Moon Cottage, Pit & Temple, Kyoto, Natural
World, Happy Town, Monument Park) pick by day + stage
(`PickStyleFallbackConfig`); over a year about 30 % of their days are
level 3 and 3 % level 4, the rest 0 to 2. The day is `currentDay + 1`, so
lockstep's `days` is one less than the style's day.

So on the console the footprint ends the view on every stage at level 0
to 2 (a straight horizon against the sky), and on level 3 the fog was set
to vanish exactly at the footprint's edge. Level 4 halves that. Those two
are what the operator saw.

## Pushing the fog out

Scratch: a gdb hook on `Viewport__SetFogNear` (×2, ×4, capped at 26624,
and 26624 outright), freeze screenshots with lockstep's `spawn`, four
headings each. Spots: Natural World (3/30) days 46 and 10 (style day; fog
4096 and 8192), Kyoto (2/5) 47 and 11, Happy Town (4/3) 13 and 9, Monument
Park (13/1) 4 and 6.

- More ground, in its own colours, then distant buildings, torii, trees
  and towers, up to a straight horizon. Kyoto gains most. Monument Park
  is walled in and gains little.
- The horizon at the far settings is the footprint's edge, not the map:
  with `gridSpan` poked to 30720 (15 cells) at fog 26624 the horizon
  comes closer and the far towers go.
- No holes, no missing chunks, no garbage from the palette rows. The
  darkening moves out with the fog, as it should; the CLUT rows are the
  stage's own.
- Level 4 days keep their decoration box (a semi-transparent grey over
  the picture, `ApplyStyleDecorationIfSet`), so the further view is seen
  through the haze. It looks like what it is, a hazy day.
- Walking (16 freezes, two ticks apart, at fog 26624): the horizon moves
  by 1 to 4 pixels when the footprint steps a cell, and small far objects
  appear at the edge. Turning through 360° shows no gaps where the
  footprint switches axis. Both are what the console shows on its clear
  stages.
- Faces that the cull hid: past 13107 a face whose Z saturates (beyond
  65535) is no longer culled by dp, as on level 0 to 2 stages already.
  Nothing out of place showed.
- Frame times: medians of 200 to 400 µs a frame at 1 and at 4 alike
  (RelWithDebInfo x86_64, psyz `/metrics` draw time, 59.94 Hz); the
  differences, -80 to +60 µs, are within run-to-run noise. The cells are
  transformed either way; the fog only decides whether a face is
  submitted.

## The footprint (estimate, not built)

Drawing past 40960 needs a wider footprint:

- `gridSpan` is a parameter, but `BuildFootprintRects` and
  `SplitFootprintRect` assume the window crosses at most one chunk
  boundary each way (`CellRect e[4]`), and the ring holds only the
  immediate neighbours. A 30-cell span crashes at once in
  `StageMap__SetFootprintVisible` (scratch run).
- It would take a second ring (19 chunks instead of 7: the neighbour and
  reslot tables, `LoadChunksAround`, `UpdateFootprintTracking`), rect
  splitting into up to nine pieces, and the ring's loads timed so that a
  chunk two away is in before it is seen. Memory is not the problem
  (chunk files are 17 to 35 KB on average, 63 KB at most; the pool had
  2.2 MB of 8 in use at Natural World).
- The footprint is not only drawing: `DispatchToRectCells` sends commands
  to the cells in it, so a wider one could change the game, and a port
  version of StageMap's tracking would need its own lockstep proof.
- And the fog would have to go past 27238 (DQA's 16 bits), which means a
  psyz change to the depth cue too.

Days of work and risk, for a view that the clearest stages don't have on
the console either. Not recommended.

## What was built

`draw_distance = N` in `settings.ini` (written with the default and a
comment for new files; an existing file without the line gets 1),
`--draw-distance N`, `LSD_DRAW_DISTANCE`; 1 to 4, default 1.
`src/draw_distance.c` wraps `gNodeGuardedViewportMethods.setFogNear`
(only ObjM calls it, once per dream) with min(fogNear · N, 26624), never
lowering a fog that is already further. At 1 nothing is installed.

| style fog | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| 4096 (level 4) | 4096 | 8192 | 12288 | 16384 |
| 8192 (level 3) | 8192 | 16384 | 24576 | 26624 |
| 14336 (level 2) | 14336 | 26624 | 26624 | 26624 |
| 20480 (level 1) | 20480 | 26624 | 26624 | 26624 |
| 26624 (level 0) | 26624 | 26624 | 26624 | 26624 |

2 already reaches the footprint on level 3 days; 4 shows the most on
level 4 days, still with a little haze.

## Checks

- Lockstep, x86_64 Debug, pace 14, smooth on, the eight spots above with a
  walking input for 700 ticks (links took it into Pit & Temple, Violence
  District, Bright Moon Cottage and Moonlight Tower too): `main`, the
  branch at 1 and the branch at 4 print the same 50 STATE and 8 SAVEBLK
  lines; the 48 freeze screenshots at 1 are byte-identical to `main`'s,
  and at 4 the ones looking at open ground differ.
- STATE every tick for two of them (688 lines each, six stages): the
  same at 4 as on `main`.
- Errors: `--draw-distance 5` and `LSD_DRAW_DISTANCE=x` stop with
  "--draw-distance wants 1 to 4"; `draw_distance = 7` in the file is
  reported and the default used.
- Builds: x86_64 Debug, Release and RelWithDebInfo, i686 RelWithDebInfo
  (smoke run with `--draw-distance 4`), Windows x86_64 (MinGW), with only
  the two old "function called through a non-compatible type" warnings.
  Not run on Windows; macOS not built (CI covers it).

Screenshots (outside the repositories, in this session's scratchpad,
`.../scratchpad/t14/shots/`): per spot, rows ×1, ×2, ×4, four headings
(`nw46`, `nw10`, `ky47`, `ky11`, `ht13`, `ht9`, `mp4`, `mp6`); the walk
and turn strips (×1 left, 26624 right); `footprint15_vs_20.png`.

## Reproducing

The scratch tools are not committed (gdb hooks and drivers, in the
scratchpad). The lockstep config for a spot, with the setting:

```json
{"seed": 4321, "days": [45], "spawn": [3, 30],
 "freeze": [100, 200, 300, 400, 500, 600], "freeze_len": 10,
 "input": [[61, 200, "up"], [200, 215, "left"], [215, 400, "up"],
           [400, 410, "right"], [410, 700, "up"]],
 "env": {"LSD_PACE": "14", "LSD_SMOOTH": "on", "LSD_DRAW_DISTANCE": "4"},
 "steps": [["menu"], ["day"]], "day_timeout": 75}
```

`days` 45 is style day 46 at Natural World: fog 4096. Avoid special days
(`sSpecialDays`) and the day before one: day 14 ended at once with no
world.
