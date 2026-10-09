# Task 19 handover: dithering off and full colour depth

Branch `task-19-colour-depth` (lsd-port), not merged or pushed. psyz:
`gpu-color-depth` (from `upstream/main` `9157560`, for an upstream PR) and
`gpu-color-depth-lsd` (the same change on the fork's `main`), merged into
the fork's `main` as `cc592fa`; nothing pushed. The details and numbers
are in `docs/design.md`, "Dithering and colour depth".

## What was done

- `dither = on|off` (`--dither`, `LSD_DITHER`) sets psyz's existing
  `PSYZ_DITHER_OFF`, for the whole game: the menus and movies are drawn as
  lit textured polygons too and dither on the console.
- `colour = console|full` (`--colour`, `LSD_COLOUR`) sets the new
  `Psyz_VideoSetColorDepth(PSYZ_COLOR_DEPTH_24)`: texture times colour and
  Gouraud colour kept at 8 bits per channel, no dithering. A spare TPAGE
  bit (0x1000) on each primitive, folded by the vertex shader into the
  `dither` varying (2 = keep 8 bits); the fragment shader widens the texel
  so brightness 128 still draws it exactly. Both renderers.
- README "Picture" and the options table, the `settings.ini` defaults.

## Results

- Dither count (Natural World day 5, spawn 3/30): every primitive carries
  the game's dither bit. Dithered: 5.53 M lit textured polygons, 91 k
  Gouraud polygons; not: 8 k flat polygons, 150 k tiles, 3.7 k sprites;
  no Gouraud-textured, raw-textured or line primitives. The intro and menu
  (371 k before the first menu) are all lit textured polygons.
- Where it shows: the grain is a crosshatch over every texture; with it
  off, darker lit walls and the fogged middle distance lose steps, and the
  first dream's Gouraud room walls band. Full colour keeps those (16 % of
  a Natural World frame changes by 1 to 8 levels against dither off). The
  sky's gradient is the game's own flat strips, the same in all three.
- Readbacks: none of the game's reach drawn pixels (it textures from TIMs
  and the tile atlas at x 640 to 960, StoreImage/MoveImage touch the fade
  CLUTs and scrolling texture strips), so full colour changes only the
  screen.
- Defaults against `main` (lockstep, x86_64 Debug, resolution 6):
  STATE/SAVEBLK identical; screenshots byte-identical at 320x240, and at
  1920x1440 identical but for the water, which scrolls on frame time while
  the dream clock is frozen (task 11's caveat). OpenGL on Xvfb alike.
  With dither off and full colour (Natural World and Kyoto, Vulkan; full
  colour on OpenGL): STATE identical over 2 850 to 3 199 ticks.
- Frame time (RelWithDebInfo, resolution 6, 30 s): medians 314 / 313 /
  307 µs, defaults / dither off / full, i.e. noise.
- psyz's own tests on `gpu-color-depth`: 372 passed, 0 failed, 1 skipped;
  both renderers compile on the upstream base.

Scratch (this session's scratchpad `t19/`): `out/hnw-*`, `out/hky-*`
(1920x1440 freeze shots: default, ditheroff, full; `hnw-main` is `main`),
`out/z_grass3.png`, `out/z_fog3.png`, `out/zk_stone.png`, `out/zk_wall.png`
(crops: on / off / full), `out/menu3.png`, `ft/` (frame-time runs), the
configs, `count_gdb.py` and `hires.py` (the full-size capture patch, applied
to scratch builds only).

## Open

- **D3D12 headers not regenerated.** The system `dxc` (1.10 from Arch)
  crashes on these shaders; the last regeneration used Microsoft's Linux
  DXC release v1.9.2609 (psyz `d6a2dd4`). Until then D3D12 keeps the old
  shaders, where full colour does nothing (the bit is ignored) and dither
  off still works. Needs the release downloaded (a yes from the operator).
- The 320x240 debug-server capture point-samples the scaled image, so at
  resolution > 1 it aliases the dither pattern into stripes; a full-size
  `/screenshot` option in psyz would help such checks (scratch patch only).
- i686 and Windows builds not tried (the C change is small).
- Pushing the psyz fork's `main`, merging this branch, and the upstream PR
  are the operator's.
