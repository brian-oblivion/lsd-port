# Task 14: draw distance

On several stages the fog meets the sky colour a short way ahead, and
little more than the floor right in front of the player shows. The
operator would like a setting that draws further, off by default (the
console's look is the fog), if it can be had without much complexity.

This task is research first: find every limit on how far the dream
draws, measure what moving each one does, and report what is cheap and
what is not. Build the cheap part only if it turns out cheap; otherwise
stop at the report.

Read first: `docs/PLAN.md` ("Later", the enhancement options and how
they are wired), `src/widescreen.c` and `src/pacing.c` (hooking the
game's methods from lsd-port without touching lsddecomp),
`docs/research/host-link-surface.md` ("Widescreen (task 09)", the
paragraph on edges and the 20x20-cell footprint), `src/settings.c`, and
the docstrings of `tools/lockstep.py`, `tools/lockstep_gdb.py` and
`tools/ds_spot.py`. Task 01's Rules apply.

## Where things stand (2026-10-06)

- lsd-port `main` `b05d1b9` (task 13 merged): `decomp/` pins lsddecomp
  `95f67e0fb`, `psyz/` the fork's `main` `2af13ca`.
- Fog in the dream, as far as known:
  - Each stage's style picks a fog distance: `RegisterStyleConfig`
    (src/world/style_layer.c) sets `style->fogNear` from
    `sStyleFogNears[cfg->fogLevel]` (six values, 26624 down to 2048,
    style_layer_tables.inc), and ObjM (objm.c) hands it to the Viewport's
    `setFogNear`. `Viewport__Update` (src/app/viewport.c) applies it with
    `SetFogNear` and `GsSetLightMode`, and sets the far colour.
  - `SetFogNear` sets the GTE's depth cue (`DQA`/`DQB`) in psyz's libgte.c:
    the depth-cue factor dp is 0 at the fog distance and ONE (4096) at
    five times it.
  - The TMD renderer (tmd_renderer.c, task 13's handover has a summary)
    culls any face whose dp reaches ONE (`TransformAndCullPoly`), and
    darkens textured faces by moving their CLUT `dp >> 9` palette rows
    down (`ADD_CLUT_ROWS`): up to 7 rows the stage's TIX supplies.
    Untextured and Gouraud faces are depth-cued by the GTE itself
    (`dpcs`, `dpct`, `ncds`).
  - The clear colour and the far colour together make the sky the fog
    fades into.
- The map draws a footprint of 20x20 grid cells (2048 units each)
  around and ahead of the player (`StageMap__ComputeFootprintFromRotation`),
  reusing the cells as the player walks. Task 09 found that the fog ends
  the view before the footprint's sides on the stages it walked.
- Other possible limits to check: the projection's far clip and
  `GsSetNearClip`, the OT's size against far screen Z (`otShift`), the
  GTE's Z saturation (faces with saturated SZ go through `RCpoly*`),
  the subdivision clip window, and how far ahead map chunks (`Mnnn.LBD`)
  are loaded from the disc.

## The work

1. **Map the limits.** For each limit above (and any others found): where
   it is set, what value each stage gets, and what moving it would change.
   Which one ends the view first on which stages: the fog cull (dp at
   ONE), the footprint, or something else?
2. **Try the fog.** In a scratch build (a gdb poke or a temporary hook,
   nothing committed yet): push `fogNear` out (twice, four times, no
   cull) on stages where the fog is close, and take freeze screenshots
   with the lockstep tools (`spawn` puts a day at a chosen spot). What
   shows up past the old fog: more ground, the footprint's edge, holes,
   pop-in as cells are reused, missing chunks? What happens to the
   palette-row darkening, and to faces that must not be culled now?
   Frame times (Release, psyz `/metrics`).
3. **The footprint**, only if step 2 shows its edge: what widening it
   would take (cell pool sizes, memory, the reuse logic, streaming), as
   an estimate, not an implementation.
4. **Report and recommend**: what a `draw_distance` setting could do
   cheaply (for example fog distance times N with the cull moved
   accordingly, a few steps), what it costs, and what it would look
   like, with before and after screenshots per stage (kept out of the
   repositories).
5. **Build it, if step 4 shows a cheap version**: a setting in
   settings.ini, `--draw-distance`, `LSD_DRAW_DISTANCE`, defaulting to the
   console's behaviour, hooked from lsd-port (a wrapped method, as
   widescreen.c and pacing.c do) or a psyz fork API. lsddecomp changes
   only if they keep the PS1 bytes identical, on a topic branch for
   review. With the default, lockstep against `main` shows identical
   STATE lines and identical freeze screenshots; with it on, identical
   STATE lines (drawing further must not change the game).

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- psyz fork: push and merge freely; never push to or open anything on the
  original psyz.
- Headless only (offscreen, the debug server; DuckStation on Xvfb). Kill
  what you start, by pid.
- No game data, screenshots or recordings in any repository.

## Report back with

Which limits there are and which ends the view on which stages; what
pushing the fog out shows; the footprint estimate if it matters; the
recommendation (a cheap version, or "not worth it" and why); and, if
built, the setting, its steps and the lockstep result. A handover,
`docs/tasks/14-draw-distance-handover.md`.
