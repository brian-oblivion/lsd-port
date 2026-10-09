# Task 20: precise geometry (no wobble, no texture warping)

The PS1 draws every vertex at a whole pixel of its 320x240 screen and
maps textures without perspective correction. The port reproduces both:
at `--resolution 6` the picture is sharp, but the ground and walls
still wobble as the camera moves, and textures bend on large polygons
near the camera. DuckStation's PGXP removes both. It keeps a precise
position and depth beside each screen coordinate the GTE produces, and
the GPU uses them. Find out how much of that the port can do, and do the
part that is clean.

Read first: `psyz/psyz/src/psyz/libgte.c` (`RTP_VERTEX`, `RTPS_BODY`,
`RTPT_BODY`, `SX_SCALE`: the projection and its 16-bit screen
coordinates), `psyz/psyz/src/platform/sdl3_gpu.c` (the vertex formats
around line 170: positions are `SHORT2`), the shaders
`psx.vert.glsl` / `psx.frag.glsl`, `decomp/src/graphics/tmd_renderer.c`
(`TransformAndCullPoly`, `ProjectTriFace`, `ProjectQuadFace`, the
`gte_stsxy*` stores, the `RCpoly*`/DIVPOLYGON subdivision),
`docs/design.md` ("Widescreen" and "Pace and smooth": how the port changes
drawing without changing the game), and DuckStation's PGXP description
(its source, `src/core/cpu_pgxp.cpp`, or its documentation) for the idea.
Task 01's Rules apply.

## Where things stand (2026-10-09)

- lsd-port `main` `6d1d2b6`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `main` `ad64361`.
- The game draws its 3D through its own TMD renderer (`SortTmdObject`),
  not libgs's GsSortObject: `gte_rtpt`/`gte_rtps`, then
  `gte_stsxy3` etc. into the POLY_xx packets. Other code (sprites,
  `ApplyMatrixToLVArray`, StageMap) projects through the GTE too.
- psyz's GTE is software: every projection runs in `RTP_VERTEX`, where
  the precise x, y (before rounding and the ±0x400 saturation) and z are
  at hand. The packet stores only the 16-bit SX/SY.
- psyz's GPU queues primitives and draws them with SDL3 GPU (Vulkan) or
  OpenGL. Vertex positions go to the GPU as `SHORT2`. There is no depth
  per vertex, so textures are interpolated affinely, as on the PS1.

## Questions, in order

1. **The design.** How to carry a precise vertex from the GTE to the
   GPU: PGXP keys a shadow value on the memory word the SXY was stored
   to; psyz could key it on the packet address and offset when
   `gte_stsxy*` stores into a primitive, or keep a small cache keyed by
   the 32-bit SXY value. What survives the game's handling between the
   two? It copies SXYs between packets, subdivides polygons
   (DIVPOLYGON3/4), compares and clips them (`FlagLargePolyForDivide`),
   averages Z. Write the options down with their failure modes before
   building.
2. **Precise positions** (`geometry = console|precise`; default
   console): sub-pixel vertex positions for primitives whose SXYs came
   from the GTE unchanged; others stay as they are. The GPU vertex
   format needs fractional positions (in the internal resolution).
3. **Perspective-correct textures**: with a w per vertex, interpolate UVs
   perspective-correctly. Check the PS1's own texture-window, CLUT and
   sample-point rules still hold (task 13's texel work,
   `docs/PLAN.md` "Texel choice").
4. **What it must not change**: the game reads SXY and SZ back for its
   own decisions (culling by `nclip`, OT slots, subdivision). Those must
   stay the 16-bit values, so lockstep STATE and SAVEBLK lines are
   identical with the option on. With the option off, screenshots must
   hash the same as `main`'s.
5. Look at it in motion: widescreen's X squeeze (`SX_SCALE`), `smooth`'s
   in-between frames, the near plane (polygons crossing it), the
   sprites. Describe what still wobbles.
6. Report and handover, `docs/tasks/20-precise-geometry-handover.md`.

This is the largest of the picture tasks. If question 1 shows it can't
be done cleanly in psyz, stop there and report the design and why.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz. The GTE and GPU changes go there, behind a setting that
  defaults to the console's behaviour.
- lsddecomp changes only if they keep the PS1 bytes identical, on a topic
  branch for review (none expected).
- Headless only; kill what you start, by pid. At most three lockstep runs
  at once. No game data, screenshots or recordings in any repository.

## Report back with

The design chosen and the ones rejected; what is precise and what isn't
(by primitive source); lockstep results with the option on; screenshot
hashes with it off; a description of a turn and a walk with it on and off
(screenshot series in the scratchpad); frame-time cost.
