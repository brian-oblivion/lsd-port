# Task 13: speckles on the ground — where it ended

Handover from task 13 (`13-terrain-speckles.md`), 2026-10-06.

## What draws them

psyz's renderers, not the game and not the console. Both (`sdl3_gpu.c`
with `shaders/psx.*.glsl`, and `sdl3_gl.c`) took a pixel's texel from a
different point than the one they tested it at:

- Vertices sat on pixel corners, so the GPU tested coverage and
  interpolated the UV at each pixel's centre. The PS1 does both at the
  pixel's integer position.
- `resolveTexel` moved the UV back by `0.5 * (abs(dFdx(u)), abs(dFdy(v)))`
  to make up for it. That holds for axis-aligned, unflipped mappings
  only: when u falls across the screen it moves the wrong way, and it
  leaves out `dFdy(u)` and `dFdx(v)`. On ground seen at a grazing angle
  those are several texels per pixel, so the UV lands texels outside the
  face's own.
- `FixupFlipUV` (sdl3_common.h) then added one to every UV of a face whose
  UVs run against its screen axes, a patch for the flipped-UV tests, which
  on a 3D face reads one texel past its edge.

Inside a face this only shifts which texel of its own texture is drawn.
At a face's edge it reads whatever lies next to its texture in VRAM:
other tiles, other colours. Hence dark and bright texels in rows along
the ground's faces, densest where the faces are smallest and most
oblique: from the fog line down to mid-distance, and on hills. A
throwaway shader that painted magenta wherever the old rule picked a
texel 2 or more steps from the PS1's sample point covered whole far
ground surfaces in every shot (day 3 and 22 runs).

Not the leads in the task doc: the CLUT row is the same on both paths
(`ADD_CLUT_ROWS` is applied before `SubmitPoly*`, and `FillDivPolygonHeader`
copies `prim->clut`), dp stays below ONE (`TransformAndCullPoly` culls at
ONE, so at most 7 rows), and `RCpoly*` midpoints of in-range UVs stay in
range. The fix is in the renderer, so it covers faces from both paths
alike; I did not trace single faces to GridCells, as no face-level fault
was left to find.

## The console

DuckStation (software renderer, 1x) at the same day and spawn draws the
field clean. Natural World, day 5 (days 1–10 load only TEXA, so both sides
draw the same textures), spawn `sStageSpawnPoints[3][30]` (also 90),
standing still:

- port before: rows of dark, blue and bright green texels from the fog
  line down, along the faces' edges
- port after: clean, and the edges of the pink paths now fall where the
  console draws them (before, a pixel to one side)
- console: clean

Texel choice still differs from the console in many single pixels on
these noisy textures (about 60–66 % of ground pixels differ by more than
one 5-bit step after the fix, 64–73 % before): the PS1 interpolates UVs
in fixed point and rounds differently from a float interpolator, and
DuckStation dithers. That gap predates this task and doesn't show as
speckle; not pursued.

## The fix (psyz fork, pushed)

`gpu: take a pixel's texel where the PS1 does`:

- Vertex shaders move everything but lines half a render pixel
  (`samplePoint`: `0.5 / (internal_res * grid_scale_x)`, `0.5 /
  internal_res` VRAM units; the GPU backend passes it in the UBO's two
  spare floats, GL as a uniform). The pixel centre the GPU tests is then
  the PS1's sample point at 1x; at Nx the samples fall inside the PS1
  pixel from its sample point on. Coverage and UV come from the same
  point, so a covered pixel's UV is always inside its primitive.
- `resolveTexel` is `floor(rawUV + 1/512)`, without the derivatives.
- `FixupFlipUV` is gone: the hardware-captured flipped-UV tests
  (`flipped_uv`, `flipped_xy_uv`, ...) pass without it.
- Lines (quads already laid out around pixel centres) carry a new
  `TPAGE_LINE` vertex flag and are not moved; moving them failed
  `draw_lines`.

Branches: `gpu-texel-sample-point` from `upstream/main` (for an upstream
PR, the operator's call), and the same commit as
`gpu-texel-sample-point-lsd` from the fork's `main`, merged into `main`
(`58e73b8`). lsd-port's `psyz/` pin moves to it. psyz host tests: 319
(Vulkan) and 38 (GL gpu) pass on the fork's `main`, the same as before;
342 on the upstream-based branch.

**Not done: MSL and DXIL.** Only the SPIR-V headers are regenerated
(glslangValidator here reproduces the committed SPIR-V byte for byte from
the old sources). `build_shaders.sh` also needs `spirv-cross` (Metal) and
`dxc` (D3D12), which aren't installed; both are in Arch's `extra`. Until
then a Metal or D3D12 build runs the old shaders without `FixupFlipUV`:
no fix, and flipped UVs one texel off. `sdl3_gpu.c` picks the format at
compile time (`_WIN32`: DXIL, Apple: MSL, else SPIR-V), so **every Windows
build of the fork's `main` is affected**, whichever SDL backend runs.
lsd-port's `main` still pins the old psyz, so nothing released changes
until `task-13-speckles` is merged; the headers must be regenerated first
(or the Windows package switched to the GL renderer, which needs no
precompiled shaders).

## Checks

| | resolution 1 | resolution 2 |
|---|---|---|
| Vulkan (sdl3_gpu) before | speckles | speckles |
| Vulkan after | clean | clean |
| GL (sdl3_gl) before | speckles | speckles |
| GL after | clean | clean |

GL and Vulkan shots at 1x are pixel-identical, before and after. The
debug server's screenshots at 2x come back at 320x240.

Lockstep, the fixed build against main's (x86_64 Debug, pace 14, smooth
on), every STATE line the same:

- days 3, 22 (task 11's r1 input, freezes 100–600)
- days 88, 130, 150, 177 (r2 "wander", freezes every 150 ticks to 2350):
  42 STATE and 4 SAVEBLK lines
- day 5 at spawns 3/30 and 3/90, both renderers, both resolutions

Builds with the new pin: x86_64 Debug, Linux i686, Windows x86_64 and
i686 (MinGW), all RelWithDebInfo but the first, without new warnings (the
two old "function called through a non-compatible type" in the game's
C). Not run: Windows, macOS.

Build directories (all from this branch's pins, ignored by git):
`build-t13-dbg` (fixed, Vulkan), `build-t13-before` (main's psyz, a copy
of the binary), `build-t13-gl` / `build-t13-gl-before` (GL),
`build-t13-diag` (the magenta diagnostic shader), `build-t13-i686`,
`build-t13-win64`, `build-t13-win32`.

## Reproducing

Lockstep (`tools/lockstep.py`; `spawn` is new):

```json
{"seed": 1234, "days": [5], "spawn": [3, 30], "freeze": [60, 100],
 "freeze_len": 10, "env": {"LSD_PACE": "14", "LSD_SMOOTH": "on"},
 "steps": [["menu"], ["day"]], "day_timeout": 30}
```

The speckles are in `f00100_1.png` along the horizon, x 0–320, y 125–155.
Add `"LSD_RESOLUTION": "2"` to `env` for 2x; a `-DPSYZ_RENDERER=sdl3_gl`
build runs offscreen too.

The console at the same spot (`tools/ds_spot.py`, new):

```json
{"day": 5, "spawn": [3, 30], "wait": 7, "shots": 3, "gap": 1.0}
```

## Tools added

- `tools/lockstep_gdb.py`: config `spawn` [stage, index], the day's first
  spawn (after `DreamSys__InitSpawnLoc`).
- `tools/ds_spot.py`: DuckStation at a day and spawn. Its gdb hooks poke
  DreamSys by offset (`lsdde.elf` has no types), and DuckStation's stub
  reports breakpoints as a plain SIGTRAP, so each breakpoint is used once
  and the hooks run in a `continue` loop. Its output path must be
  absolute (DuckStation runs from `/opt/duckstation-qt`); the tool does
  that itself.

## What's left

- MSL and DXIL headers (above).
- Upstream PR from `gpu-texel-sample-point`: the operator's call, and it
  changes hardware-matched behaviour, so the maintainer may want a PS1
  hardware run of the gpu tests (`make test-ps1-hw`) first.
- Texel choice against the console's fixed-point interpolation (above).
