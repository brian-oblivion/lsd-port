# Task 09: widescreen

Handover. Read `docs/PLAN.md` (its "Later" list, `widescreen` and
`resolution`), `docs/tasks/01-game-c-builds-for-host.md` (its Rules
section applies unchanged) and `docs/tasks/08-statics-handover.md` first.

This is an experiment: it stays on the branch until the operator says it
is good, and it may be dropped.

## Where things stand (2026-10-04)

- lsd-port branch **`widescreen`**, from `main` `a2b1fc0` (the `decomp/`
  pin moved to lsddecomp `main` `f3b9ccbc4`, task 08's fixes; that commit
  is on local `main`, not yet pushed). Work on `widescreen`, never on
  `main`.
- psyz fork `main` is `26fad08`; lsddecomp `main` is `f3b9ccbc4`.

## The goal

A 16:9 picture in the dream that shows more of the world at the sides,
not a stretched 4:3 one, behind an option that defaults to the console's
4:3. The 2D screens (title menu, graph, diary, comment entry, movies)
stay 4:3, centred with black side bars.

## What is known

- **The 3D projection all goes through psyz's software GTE.** The game's
  `gte_rtps()`/`gte_rtpt()` are `Psyz_GteRtps`/`Psyz_GteRtpt` on the host
  (`psyz/psyz/include/libgte.h`), implemented in
  `psyz/psyz/src/psyz/libgte.c` (`RTPS_BODY`, `RTPT_BODY`). libgs's
  transforms and `DIVPOLYGON` (`libgte_div.c`) project through the same
  code. The game sets one projection, `GsSetProjection(self->projH)` in
  `Viewport__Update` (`decomp/src/app/viewport.c`, default h 256), and
  the screen is 320x240 (`DrawSystem`, `GsInitGraph`).
- **The display side**: psyz's SDL3 backends size the picture with
  `GetCurrentGameAspectRatio` and `FitGameToWindow`
  (`psyz/psyz/src/platform/sdl3_common.h`). The modes are
  `PSYZ_ASPECT_DISPLAY` (4:3 from the display ranges) and
  `PSYZ_ASPECT_SQUARE`; there is no wide mode yet.
- **VRAM**: the framebuffers sit beside the textures in the PS1's 1 MB
  VRAM, so a wider framebuffer (say 427x240) would overlap texture pages.
  Don't do that.
- **Culling the game does itself**: `TransformAndCullPoly`
  (`decomp/src/graphics/tmd_renderer.c`) culls per polygon from the GTE's
  FLAG register, winding and depth cue; `DIVPOLYGON` clips to
  `sDivClipWidth`/`sDivClipHeight` (320x240). Whether the game also
  culls whole objects or map blocks by view angle or distance before
  that has not been looked at. `viewport_draw.c` and the stage/map code
  are where to start.

## The approach (recommended; argue if you find better)

**Anamorphic**: keep the 320x240 framebuffer, squeeze the 3D horizontally
by 3/4 around the screen centre (OFX) in the GTE's perspective step, and
show the framebuffer at 16:9. DuckStation's GTE widescreen hack works the
same way. The squeezed picture holds a 4/3 wider field of view. Screen-space
clipping still matches the framebuffer, and VRAM is untouched.

Then, in order:

1. **psyz: the squeeze.** A setting (for example
   `Psyz_GteSetWidescreenScale` or a 16.16 x scale) applied to SX in every
   perspective path (RTPS, RTPT and anything else that writes SXY), off
   by default, with psyz host tests for both settings.
2. **psyz: a wide display.** An aspect mode or ratio override so the
   SDL3 GPU and GL backends show 320x240 at 16:9, and a way to switch
   back to 4:3 per frame for the 2D screens. Default window size follows
   the aspect.
3. **Which screens are 3D.** The port must know when the dream is on
   screen. Prefer an explicit hook (the port sets wide mode when the
   dream starts and clears it when it ends; a `#ifdef PLATFORM_PC` call
   in the game's C on an lsddecomp topic branch, or something the port
   can see from outside, such as a function it can wrap) over a
   heuristic in psyz ("RTPS ran this frame").
4. **2D inside the dream.** Anything drawn as 2D primitives during a
   dream (fade boxes, flashes, the decoration bands, backgrounds or sky
   sprites, text) is stretched by 4/3 on a 16:9 display. Full-screen
   fills are fine stretched. List the rest and say which look wrong.
   Counter-squeezing them is a later step unless one is glaring.
5. **Edges.** Walk several stages at 16:9 and look for things popping in
   and out at the new edges. If the game culls objects or map blocks by
   view angle, find where and widen it behind the option (a
   `PLATFORM_PC` hook on an lsddecomp topic branch, PS1 bytes unchanged,
   `./build-and-verify.sh` and `tools/lint.sh` green).
6. **The option.** Default 4:3. A command-line option and an environment
   variable, like `--saves`/`LSD_SAVES` (for example `--aspect 16:9`,
   `LSD_ASPECT`), and say whether it should also live in a settings file
   beside `controls.ini`. README section.
7. **Internal resolution (if time allows).** Stretching 320 columns to
   16:9 looks coarse; psyz's `Psyz_VideoSetInternalResolution` (x2, x4)
   helps. Check it works with the squeeze, with the 2D screens and the
   movies, and expose it the same way. Report rather than fix if it is
   broken.

Not in scope: other aspect ratios beyond 16:9 (but don't hard-code 3/4
where a ratio parameter costs nothing), high fps, mouse look.

## How to check

- Screenshot pairs, 4:3 against 16:9, from the same spot in several
  stages (start of a day; a few days in, with days set from gdb as in
  task 08). The 16:9 one should show the same centre plus extra at the
  sides, with nothing squashed. psyz's screenshots are of the
  framebuffer (squeezed); make sure the report's images show what the
  player sees (the window), or stretch them for the report.
- The 2D screens and a movie at 16:9: 4:3 with bars.
- 4:3 (the default) unchanged: a screenshot pair against `main`'s build
  from the same spot should be identical.
- The sanitizer build (`-DLSD_SANITIZE=address,bounds`) clean on a dream
  at 16:9, if game C changed.
- Windows (MinGW, under Wine) still builds and starts.

## Rules (in addition to task 01's)

- **lsd-port: work on `widescreen`.** Don't merge to `main`; the operator
  decides when it is good.
- **psyz: a topic branch** on the fork (for example `widescreen`), with
  `psyz/` pointed at it on lsd-port's `widescreen` branch. Not to the
  fork's `main` and not upstream without the operator.
- **lsddecomp: never commit on `main`.** Topic branch, with
  `./build-and-verify.sh` and `tools/lint.sh` green; PS1 bytes unchanged.
- **Never push to any repo's `main` without the operator's yes.** Topic
  branches you may push.
- No game data, screenshots or saves in any repository.
- No windows on the operator's display: headless, or `xvfb-run -a` for
  the GL build (`~/.claude/CLAUDE.md`). Window-size behaviour can be
  checked under Xvfb.

## Report back with

The approach as built and anything you changed from the one above; the
psyz, lsddecomp and lsd-port commits and branches; the screenshot pairs
(paths in the scratchpad, not the repo); the 2D-in-dream list with what
looks wrong; the edge findings (object culling found or not, what was
widened); the option's name and default; internal resolution's state;
what is left before it could go to `main`.
