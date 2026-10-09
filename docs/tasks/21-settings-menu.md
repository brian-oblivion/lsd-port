# Task 21: an in-game settings menu

Every port option is set on the command line, through an environment
variable, or in `settings.ini` in the saves folder: aspect, resolution,
scale, pace, smooth, frame rate, draw distance, and the controls in
`controls.ini`. Changing one means quitting, editing and restarting.
Modern ports (Ship of Harkinian, Zelda64Recomp, the Mario 64 PC port)
open an overlay on a key instead. Build one: a key (F1, say, and a pad
combination) opens a menu over the game. It changes the settings, applies
what can be applied live, and writes them back to `settings.ini`.

Read first: `src/settings.c` / `settings.h`, `src/controls.c`,
`src/ini.c`, `src/main.c` (how settings reach psyz and the port's
modules), `src/pacing.c` (`Pacing_Init`), `src/widescreen.c`,
`src/draw_distance.c`, the README's "Picture", "Pace" and "Controls"
sections, `psyz/psyz/include/psyz/video.h` (what psyz can change at run
time). psyz has `external/cimgui` as a submodule, but nothing in the port's
build links it today: check that first. Task 01's Rules apply.

## Where things stand (2026-10-09)

- lsd-port `main` `6d1d2b6`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `main` `ad64361`.
- Settings are read once at start (`SetUpSettings`), in the order
  command line, environment, `settings.ini`. Several are applied by
  patching method tables when a dream starts (`pacing.c`,
  `widescreen.c`, `draw_distance.c`), others in psyz at video init
  (resolution, scale).
- The window and renderer are psyz's (`sdl3_gpu` Vulkan by default,
  `sdl3_gl`); the game sees only its PS1 libraries.

## Questions, in order

1. **Which settings can change live**, and from where: in a dream, in the
   menus, mid-movie. List each with what changing it at run time needs:
   - aspect: GTE X scale, display stretch, StageMap footprint wrapper;
   - resolution: psyz render targets;
   - scale, pace, smooth, frame rate;
   - draw distance: the fog, already set up for the day;
   - controls.
   Settings that can only change at the next dream or the next start
   are fine, if the menu says so.
2. **The overlay.** Dear ImGui through cimgui is the obvious choice, if
   it can draw over psyz's present on both renderers (Vulkan through SDL3
   GPU, and OpenGL). The alternative is drawing the menu with psyz's own
   2D primitives. Decide, and keep the overlay out of the game's VRAM
   and out of screenshots taken from the debug server (or document that
   it shows).
3. **Input.** While the menu is open the game must not see the keys or
   pad that drive it. Pausing the dream is not required, but say what
   happens to the dream clock and the music. A pad must be able to open
   and drive it (Steam Deck, task 22).
4. **Saving.** The menu writes `settings.ini` and `controls.ini` keeping
   the operator's comments and unknown lines (`ini.c`), and the command
   line still wins for that run (show it in the menu).
5. **Check**: with the menu never opened, lockstep STATE and SAVEBLK
   lines and screenshots identical to `main`. Opening and closing it in a
   dream changes nothing in the state either. Build on Windows (MinGW) as
   well; the CI covers macOS.
6. README section, and handover `docs/tasks/21-settings-menu-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes. New dependencies (cimgui) need their licence in
  `tools/package.sh`'s `licences/`.
- psyz fork: push and merge freely (an overlay hook in psyz's present,
  if needed); never push to or open anything on the original psyz.
- Headless for tests (offscreen; the debug server); a visible window only
  on i3 workspace 10 with `SDL_APP_ID=claude-lsd`, after the operator's
  yes, for them to try. Kill what you start, by pid.
- No game data, screenshots or recordings in any repository.

## Report back with

Which settings change live and which at the next dream or start, and
why; the overlay's technique; how input is kept from the game; the
lockstep and screenshot checks; what it looks like (screenshots in the
scratchpad).
