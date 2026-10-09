# Task 20 handover: precise geometry

Branch `task-20-precise-geometry` (lsd-port), not merged or pushed. psyz:
`precise-geometry` on the fork's `main`. The design and numbers are in
`docs/design.md`, "Precise geometry".

## What was done

- `geometry = console|precise|perspective` (`--geometry`, `LSD_GEOMETRY`;
  default `console`), psyz's new `Psyz_VideoSetGeometry`. `precise` draws
  the vertices the GTE projected at their sub-pixel positions; `perspective`
  also maps their textures with perspective.
- The build option the operator asked for: `-DLSD_PRECISE_GEOMETRY=OFF`
  (default ON) sets psyz's `PSYZ_PRECISE_GEOMETRY` and leaves the whole
  thing out: no shadow vertices in the GTE or the GPU queue, and
  `geometry` stays `console` with a message on stderr if asked for more.
- psyz: the GTE keeps a precise vertex beside its SXY FIFO; every SXY
  store files it by destination address (with the stored word and the
  frame); `GPU_Enqueue` looks the packet's words up while they are still
  where the game built them; `RCpoly*` carries corners and midpoints
  through subdivision. The GPU vertex is float x, y plus a w; a
  perspective-correct primitive sets `TPAGE_PRECISE` (0x0800) and keeps its
  colour `noperspective`. Both renderers; shader headers regenerated.
- README ("Picture", the options table, "Building"), `settings.ini`
  defaults.

## Questions

1. **Design.** Chosen: a table keyed by the store address, validated by
   the stored word and its frame. Rejected: a cache keyed by the 32-bit SXY
   value (two vertices on one pixel share an entry and can swap depths, so
   wrong perspective), and re-projecting at draw time (packets don't carry
   the model vertex). What survives: SortTmdObject's faces (stored straight
   into the packets) and RCpoly's subdivisions (corners taken from the
   packet the game copied them from, checked equal). The game's own
   decisions (nclip, avsz3/OT, the 256-pixel subdivision box, RCpoly's
   rejection) read the untouched 16-bit values.
2. **Precise positions**: per vertex, not per polygon. Deciding per polygon
   left cracks where one neighbour fell back. A vertex with a precise value
   uses its true projection even where the GTE saturated (far SZ, clamped
   IR/SX/SY), so all polygons sharing it agree.
3. **Perspective textures**: hardware perspective interpolation of the UV
   (w = view depth); the texture window, CLUT and `resolveTexel` sample
   point apply to the interpolated UV unchanged; colour stays affine.
4. **What it must not change**: see Results.
5. **In motion**: see Results.

## Results

- Coverage (Natural World day 5, scratch counter in `Draw_PushPrim`):
  4.80 M quads and 273 k triangles drawn with every vertex precise, 129
  triangles partly, none partly among the quads; the 407 k quads with none
  are the intro's and menu's 2D (task 19 counted 371 k before the first
  menu).
- Not precise, by source: world sprites (projected by the game on the CPU,
  `Viewport__DrawNode`), ScreenSprites, psyz's `GsSortSprite` GTE corners
  (copied through locals), the sky strips, tiles, fades and other 2D, and
  any vertex at or behind the eye.
- Lockstep (x86_64 Debug, resolution 6, seed 1234, Natural World day 5
  spawn 3/30, walk and turn, a STATE line every tick): `main`, the branch
  at `console` and at `perspective`, STATE/SAVEBLK identical over 3 199
  ticks (the day timeout); the turn/walk runs, console, precise and
  perspective, identical over their 396 ticks.
  Kyoto day 5 spawn 2/0, `console` against `perspective`: identical over
  3 200 ticks. OpenGL (x86_64 Debug on Xvfb), `main`, `console` and
  `perspective` at Natural World: identical over 2 064 ticks.
- Screenshot hashes with it off (the default against `main`, 1920x1440
  scratch capture): the menu and the freezes at ticks 60 and 130
  byte-identical; at 200 and 300 they differ only in water-blue pixels,
  the water scrolling on frame time while the dream clock is frozen (task
  19's caveat). OpenGL at 320x240: the menu and one freeze identical, the
  other three differ only on the water and its reflections (75 to 95 %
  water-blue), as task 19 found.
- A turn and a walk (scratchpad `t20/out/mv-*`, freezes at ticks 80 to 86
  turning and 220 to 226 walking; `t20/cmp3_224.png`, `t20/walk_edge.png`):
  with `console`, the ground's polygon edges step a console pixel (six
  screen pixels) at a time, the grass folds into chevrons along each
  quad's diagonal, and thin seams of sky open and close between ground
  polygons from tick to tick. `precise` puts the edges where they belong
  and the seams go, but the textures still bend at the diagonals and shift
  where a polygon switches between subdivided and whole. `perspective`
  removes the bend and the switching; the water's texels become even
  squares, and Kyoto's raked gravel (`t20/kycmp.png`) runs in straight
  lines instead of zig-zags.
- What still wobbles with `perspective`: sprites (sparkles, the world's
  sprite effects), whose position the game rounds itself; the slits where
  the game's own models don't meet (under the cliffs at Natural World: in
  all three modes); vertices at or behind the eye. In-between frames
  (`smooth`) project again, so they are precise too; widescreen's X
  squeeze is applied to the precise X as to the GTE's.
- Frame time (RelWithDebInfo, resolution 6, psyz's draw time, 30 s in the
  first dream's room): medians `main` 247 µs, `console` 256, `perspective`
  329, `precise` 346.
- Builds: x86_64 Debug and RelWithDebInfo (Vulkan), x86_64 Debug OpenGL,
  x86_64 with `LSD_PRECISE_GEOMETRY=OFF`, i686 RelWithDebInfo: only the two
  old "function called through a non-compatible type" warnings. psyz's
  tests (Debug, sdl3_gpu): 319 passed, 0 failed.

Scratch (this session's scratchpad `t20/`): the configs, `out/` (runs and
shots), `ft/` (frame times), `hires.py`/`hires_off.py` (task 19's full-size
capture), `stats.py` (the coverage counter), `b.sh` (builds with both
patched in and takes them out again); `build-main-*` are `main` builds
(their worktree removed).

## Open

- Merging and pushing this branch: the operator's. The psyz fork's `main`
  has `precise-geometry` merged and pushed (as task 19's rules allow).
- GLSL ES (the web build) has no `noperspective`, so there colour is
  interpolated with perspective too; not tried.
- Sprites could be made sub-pixel by giving `Viewport__DrawNode`'s
  projection a fractional result; that is the game's code (lsddecomp), not
  attempted.
- Not tried on Windows or macOS (the shader headers for D3D12 and Metal
  are regenerated, but not run).
