# Task 21 handover: the settings menu

Branch `task-21-settings-menu` (lsd-port), not merged or pushed. psyz:
`settings-menu-input` (`Psyz_PadsHold`), merged into the fork's `main` and
pushed, as the rules allow. The design is in `docs/design.md`, "Settings
menu"; the player's side in the README, "The settings menu".

## What was done

- F1, or a gamepad's Guide button or both sticks pressed in together,
  opens a menu over the game (`src/menu.cpp`): every setting of
  `settings.ini` (aspect, resolution, scale, dither, colour, geometry,
  pace, smooth, frame rate, draw distance) and the keyboard's keys
  (layout; per button, add a key or clear them). Closing writes what
  changed to `settings.ini` and `controls.ini`.
- `src/settings.c`: a table of the settings, with `Settings_Value`,
  `Settings_Set`, `Settings_Apply`, `Settings_Save`, `Settings_From`
  (the option or variable that gave it for this run). `main.c` fills the
  command line and environment through it.
- `src/controls.c`: the keys kept, with `Controls_Keys`,
  `Controls_SetKeys`, `Controls_SetLayout`, `Controls_Save`.
- `src/ini.c`: `Ini_Update`, which rewrites only the lines that set what
  changed (keeping a comment after the value), uncomments a `# name = ...`
  line for one set nowhere else, appends otherwise, and writes through a
  `.new` file renamed over the old one.
- `src/pacing.c`, `src/widescreen.c`, `src/draw_distance.c`: their
  wrappers are now always installed and take a change while the game runs
  (`Pacing_Set`, `Widescreen_Set`, `DrawDistance_Set`); at the console's
  values they leave the game as it was (the lockstep below).
- psyz: `Psyz_PadsHold` (input.h): while held, the keyboard's and the
  gamepads' pads read as nothing pressed, sticks centred, and Escape
  doesn't quit; once released, a button still held stays released until
  let go. The debug server's injected input is not held.
- Build: `project(lsd-port C CXX)`; Dear ImGui is built from psyz's
  `external/cimgui/imgui` (a submodule of that submodule, now initialised
  in CI and the README's build steps) with the backend of the configured
  renderer. Licence: `tools/package.sh` adds `licences/dear-imgui-MIT.txt`
  (with its fonts' notices) and lists libstdc++ for the Windows build; it
  now also puts the README's "Pace" section (missing before) and "The
  settings menu" into `README.txt`.

## Questions

1. **What changes live.** Everything but aspect, at once, in a dream, in
   the menus and in a movie: resolution, scale, dither, colour and
   geometry are psyz's setters; pace, smooth and frame rate are read by the
   paced loop every pass (it is now always installed, and runs the game's
   own `VSync`/callback/notify loop at pace 20 without smooth); draw
   distance gives the dream's Viewport the game's distance again at the new
   scale (its update hands it to `SetFogNear` the next frame); keys go to
   `Psyz_PadsSetKeyboardMap`. Aspect waits for the next dream: the GTE
   squeeze, the display stretch and the footprint widening are taken up at
   DayTask's onInit, and the VariantSprites a dream makes are made un-plain
   when they are created; the window keeps the shape it opened with (the
   picture letterboxes until it is resized). The menu says "from the next
   dream" beside it. The menu's changes reach the modules from a VSync
   callback, between frames, not inside psyz's present where the menu runs
   (a new internal resolution recreates the render targets).
2. **The overlay.** Dear ImGui through psyz's existing overlay hooks, on
   both renderers: after psyz blits the display into the window, the frame
   callback builds the menu; with SDL GPU its vertices go up in a command
   buffer of its own, submitted ahead of psyz's, and it is drawn in psyz's
   render pass on the swapchain texture; with OpenGL it is drawn into the
   default framebuffer before the swap. Never in the VRAM, so the debug
   server's `/screenshot` and `/vram` don't show it (checked: the shots
   taken with the menu open are the game's alone). cimgui's C bindings are
   not used: they have no SDL GPU backend, so `src/menu.cpp` is C++ against
   `imgui.h`. While shut, no frame is built and no events are passed on.
3. **Input.** `Psyz_PadsHold` while open. The game keeps running: a
   dream's clock runs on and its music plays (START pauses it; pausing for
   the menu would change the game's state). Keyboard: arrows, Space/Enter,
   Escape (backs out of a list, then closes). Gamepad: d-pad, circle
   (east) changes, cross (south) backs out and closes, as in the game
   (Dear ImGui's `ConfigNavSwapGamepadButtons`). Mouse works. F1, F4 and F6
   can't be bound.
4. **Saving.** As above (`Ini_Update`); a setting the command line or the
   environment gave is shown greyed with its option ("--pace, for this
   run"), can't be changed, and isn't written.
5. **Checks.** See Results.

## Results

- Lockstep against `main` with the menu never opened (x86_64 Debug,
  Vulkan headless, resolution 6, seed 1234, Natural World day 5 spawn
  3/30, walk and turn, freezes at ticks 60, 130, 200 and 300): STATE and
  SAVEBLK identical over the whole day (3 622 lines) at the defaults, at
  pace 20 with smooth off (the paced loop now always installed, running the
  game's own) and at 16:9 with draw distance 3 (the wrappers now always
  installed). Screenshots: the defaults byte-identical (the menu and all
  four freezes); at pace 20 and 16:9 the freezes differ from that `main`
  run in 4 to 7 % of pixels, but a second `main` run differs from the
  first by exactly the same counts and the branch is byte-identical to it
  in 9 of 10 shots; the tenth differs between all three runs in the same
  rectangle (the grass, water and cliff textures animate on frame time
  while the dream clock is frozen, tasks 19 and 20).
- OpenGL (x86_64 Debug on Xvfb, the same config): `main`, the branch
  untouched, and the branch with the menu opened twice by F1 (xdotool) in
  the dream, moved about in and W held in it: STATE and SAVEBLK identical
  as far as all three got (2 304 and 2 293 lines, to tick 2 300 or so of
  3 628; llvmpipe at resolution 6, three at once, ran into lockstep's 300 s
  day timeout). The menu shot and the tick-200 freeze byte-identical in
  both, the others differing on the water and its edges (51 to 94 % of the
  differing pixels water-blue), as tasks 19 and 20 found on OpenGL.
- Opening and closing it in a dream: the same run on a build with a
  scratch virtual gamepad (an SDL virtual joystick driven from a file;
  not committed), the menu opened with Guide twice (between the 130 and
  200 freezes, and after 300), the d-pad moved about in it and held for
  2 s, closed with cross: STATE and SAVEBLK identical to the run that
  never opened it, over the whole day; four of five shots byte-identical,
  the fifth differing as `main` differs from itself. Changing draw
  distance 1 to 4 in the menu mid-dream: state identical too, and the
  freezes after it show the far cliffs out of the fog (scratchpad
  `t21/dd300.png`).
- Input held: with the menu open, W held 3 s on the keyboard (OpenGL on
  Xvfb, xdotool) or the d-pad held 3 s on the virtual pad: 0 pixels of the
  dream change; with it shut, the same moves the player (63 877 and
  57 605 of 76 800 pixels). A key held while the menu closes doesn't walk
  until it is pressed again. Escape closes the menu without quitting under
  the classic layout (where it otherwise quits).
- The pad drives it: Guide opens and closes it, L3+R3 opens it, the d-pad
  moves, circle toggles a checkbox and opens a list, cross closes the list
  and then the menu, which saved `dither = off`. The keyboard: a slider
  moved with Space, Right, Right, Space (resolution 3), a list picked with
  Space, Down, Down, Space (aspect 16:9).
- Live changes seen: resolution 1 to 4 on Vulkan (headless) and OpenGL;
  aspect 16:9 and draw distance 3 set during the intro movie, and the
  first dream then drawn 16:9 (letterboxed in the 4:3 window).
- The saved files: only the changed lines are rewritten; every comment,
  blank line and the commented-out key lines of `controls.ini` stay.
- Screenshots (scratchpad `t21/`): `look/menu_win.png` and
  `dbg_vs_win.png` (the window with the menu, beside the debug server's
  shot of the same moment, without it), `vk/vkgrab2.png` (the menu as the
  SDL GPU backend drew it on Vulkan, a list open), `padgrid2.png` (driven
  by the pad), `look/wide.png` (the dream after picking 16:9).
- Builds: x86_64 Debug (Vulkan and OpenGL), i686
  RelWithDebInfo, x86_64 with `LSD_PRECISE_GEOMETRY=OFF`, Windows x86_64
  (MinGW, RelWithDebInfo; imports no C++ runtime DLL): only the two old
  "function called through a non-compatible type" warnings; `src/menu.cpp`
  clean under `-Wall -Wextra` with GCC and Clang. psyz's tests (Debug,
  sdl3_gpu): 319 passed, 0 failed. `tools/package.sh` run on the Windows
  build: the new licence file and SOURCES lines are in.

## Open

- Merging and pushing this branch: the operator's.
- Not run on Windows (the MinGW x86_64 build links, with no C++ runtime
  DLL) or macOS. On D3D12, Dear ImGui's SDL GPU backend uses DXBC shaders
  while psyz creates the device for DXIL; SDL checks the format only in
  debug mode, and D3D12 takes both, but it hasn't been seen.
- The Vulkan path was checked headless (SDL's offscreen driver): the menu
  is built and drawn (a scratch copy of its frame rendered into a texture
  of its own and read back looks right), but the composite over the game
  in the swapchain was only seen with OpenGL on Xvfb. A look on a real
  display (workspace 10) would settle it.
- The menu opens on the gamepad for task 22 (Steam Deck): Guide is often
  Steam's own there; both sticks pressed in is the fallback.
- Not done: binding gamepad buttons (they keep SDL's mapping), and the
  window reshaping itself after an aspect change.
