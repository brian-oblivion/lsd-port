# Task 19: dithering off and full colour depth

The operator plays at `--resolution 6`. At that size the PS1's 4x4
dither pattern, scaled up with everything else, shows as grain over
gradients, fog and Gouraud-shaded walls. With dithering simply turned
off, the 15-bit colour shows as bands instead. DuckStation offers
"disable dithering" with "true colour" (24-bit) rendering for this
reason. Add both as options that default to the console's look.

Read first: `psyz/psyz/src/platform/shaders/psx.frag.glsl` (`applyDither`,
the texture modulation just above it), `psyz/psyz/src/platform/sdl3_common.h`
(`dither_mode`, `GetCurrentDither`, `CanPolyDither`,
`Psyz_VideoSetDitheringMode`), `psyz/psyz/include/psyz/video.h`,
`src/settings.c` and the README's "Picture" section (how an option is
added: setting, flag, environment variable, `settings.ini` default),
`docs/design.md` ("Widescreen edges" is a recent example of a
port-only option). Task 01's Rules apply.

## Where things stand (2026-10-09)

- lsd-port `main` `6d1d2b6`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `main` `ad64361`.
- psyz already has a dither switch: `Psyz_VideoSetDitheringMode(
  PSYZ_DITHER_OFF)` turns dithering off for every primitive. The port
  never calls it.
- With dithering off, the fragment shader rounds colour to 5 bits per
  channel (`floor(prod8 / 8.0) / 31.0` for textured primitives,
  `applyDither` otherwise), as the PS1 writes 15-bit VRAM. The render
  target is RGBA8 (`SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM`), so it could
  keep 8 bits per channel. But the same VRAM texture is read back as
  15-bit data (texture pages, CLUTs, `rgb5551ToU16`, `LoadImage`/
  `StoreImage`, the movies), so "full colour" has to stay out of that.
- Not measured: which of the game's primitives set the dither bit, and
  where the banding would show (fog, the sky, Gouraud shading on
  ground and walls, the fades).

## Questions, in order

1. **What dithers, and where.** Count the primitives with the dither bit
   by kind (Gouraud, textured, shaded-textured, lines, sprites) over a
   dream and the menus. Take screenshots at resolution 6 with dithering
   on and off at a few spots, using `ds_spot` spawns and lockstep's
   `spawn`, e.g. Natural World day 5, spawn 3/30, and Kyoto. Where does
   the grain show, and where would banding replace it?
2. **Dithering off** (`dither = on|off`; default on): wire psyz's
   existing switch to a setting. Check whether it should apply in the
   dream only or everywhere (the menus and the diary are 2D).
3. **Full colour** (`colour = console|full`, or a better name; default
   console): with dithering off, keep the shaded colour at 8 bits per
   channel in what is drawn to the screen. Find where 8 bits can survive
   and where they can't:
   - semi-transparency blending;
   - the readbacks and copies the game makes of what it drew (MoveImage,
     StoreImage, the fades, any effect that samples VRAM it rendered).
     These must still see 15-bit values, or the game's own effects
     change.
   A psyz change, on a topic branch from `upstream/main`, behind a psyz
   setting that defaults to the console's behaviour.
4. **Check** that the defaults draw exactly what `main` draws: hash the
   screenshots of a lockstep run at the same ticks on both builds. Check
   that the state doesn't move (lockstep STATE and SAVEBLK lines
   identical with the options on), and look at the Vulkan (`sdl3_gpu`) and
   OpenGL (`sdl3_gl`) renderers both.
5. Report and handover, `docs/tasks/19-colour-depth-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz. Keep psyz changes small and console-default, so they
  could go upstream.
- Headless only (offscreen, the debug server; DuckStation on Xvfb as its
  memory notes say); kill what you start, by pid.
- At most three lockstep runs at once.
- No game data, screenshots or recordings in any repository.

## Report back with

The dither count by primitive kind; before and after screenshots
described (where they are in the scratchpad); what each option changes
and where it doesn't apply, and why (readbacks); the lockstep and
screenshot-hash results with the options off; frame-time cost.
