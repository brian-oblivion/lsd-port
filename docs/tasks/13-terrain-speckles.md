# Task 13: speckles on the ground

The operator, playing pace 14 with smooth on (2026-10-06): a pattern of
stray dark and bright texels, checkerboard-like, on the ground, worst on
far terrain and on hills, "pretty visible all over" while terrain loads in.
In the screenshot it shows on a grass field up a hill: a band of speckles
from the fog line down to mid-distance, thinning toward the camera, in
rows that follow the ground's faces. Find what draws them and fix it in
the port (psyz fork or lsd-port), unless the console does the same.

Read first: `src/graphics/tmd_renderer.c` and `viewport_draw.c` in
lsddecomp (the dream's TMD renderer), psyz's `libgte_div.c` (the `RCpoly*`
subdividers) and its renderer (`platform/sdl3_gpu.c`, `sdl3_gl.c`),
`docs/tasks/05-*.md` and the memory notes on DuckStation as the console
reference (`tools/ds_drive.py`), and the docstrings of `tools/lockstep.py`
and `tools/lockstep_gdb.py`. Task 01's Rules apply.

## Where things stand (2026-10-06)

- lsd-port `main` has task 12 (pace and smooth); branch
  `defaults-pace-smooth` makes pace 14 and smooth on the defaults. `psyz/`
  pins the fork's `main` `daa6e3c`, `decomp/` lsddecomp `95f67e0fb`.
- How the ground is drawn: `Viewport__DrawNode` hands each GridCell's
  GsDOBJ2 to `SortTmdObject` (tmd_renderer.c). Per face it projects through
  the GTE, culls, and either `addPrim`s the primitive or, when it is too
  large on screen or its screen Z saturates, copies it into a DIVPOLYGON
  and has psyz's `RCpoly*` subdivide it.
- Fog is faked with palettes: a textured face's CLUT is moved
  `dp >> dpShift` rows down (`ADD_CLUT_ROWS`, `DP_CLUT_SHIFT_CUED` 9), so
  far faces use darker palette rows the game uploaded for that.

## Leads, untested

- **The palette row.** A face given a row that isn't a fog row (a CLUT
  moved past the end of its block, or onto another texture's palette)
  would draw wrong colours texel by texel. Check that the subdivided path
  (`RCpoly*`) carries the row shift too and lands where the direct path
  does, and what `dp` the GTE gives far faces (psyz's depth cueing against
  the console's: `DQA`/`DQB`, the `dp` clamp).
- **Subdivision.** The speckles come in rows along the ground's faces.
  Rounding in psyz's subdivider (texture coordinates per sub-polygon,
  edges between them) could leave texels from outside a face's texture
  window.
- **Texture sampling in psyz's renderer**: texture windows, 4-bit or
  8-bit CLUT lookup at a block edge, `--resolution` above 1. Try
  `--resolution 1` and `2`, and the GL renderer against the GPU one.
- **The console.** It may draw the same. DuckStation at the same day,
  stage and position settles that before any fix.

## The work

1. Reproduce headless: a lockstep config (seed, day, input) that stands on
   or looks across a speckled field, with freeze screenshots. Keep the
   config in the handover; no images in the repository.
2. DuckStation at the same place (`tools/ds_drive.py`). If the console
   speckles too, stop and report: it isn't a port bug.
3. Find the faces: which GridCell and TMD primitives the bad texels come
   from (psyz's debug server `/vram` dump, a gdb probe in `SortTmdObject`
   or the submit wrappers), which path drew them (direct or `RCpoly*`),
   and the CLUT and texture coordinates they were given against the
   console's.
4. Fix it where it lives: the psyz fork (free to push), or lsd-port. An
   lsddecomp change only if it keeps the PS1 bytes identical, on a topic
   branch for review.
5. Check: the same screenshots clean, at `--resolution` 1 and 2, both
   renderers; lockstep against main shows identical STATE lines (a
   renderer fix must not touch the game); psyz's host tests pass.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz.
- Headless only (offscreen, the debug server; DuckStation on Xvfb). Kill
  what you start, by pid. Run output off `/tmp`'s quota.
- No game data, screenshots or recordings in any repository.

## Report back with

What draws the speckles and why; whether the console does it; the fix and
where it lives; before and after screenshots (kept out of the
repositories); the lockstep result. A handover,
`docs/tasks/13-terrain-speckles-handover.md`.
