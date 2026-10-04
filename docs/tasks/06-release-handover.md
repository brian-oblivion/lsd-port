# Task 06: release — where it ended

Handover from task 06 (`06-release.md`) to whoever picks up next. The
details of each stop are in `docs/research/host-link-surface.md`
("Release (task 06)") and `docs/upstream/psyz-prs.md`.

## Where things stand (2026-10-04)

- lsd-port `main` has task 06 (`task-06-release`, fast-forwarded); psyz
  fork `main` is `d80533c` (pushed), with `libcard-bu-init-path` and
  `libcard-new-card` merged; lsddecomp `main` is unchanged (`0355ebad0`).
- The operator approved pushing everything, including the release
  workflow and what the release archives contain (the licence list
  below). **No tag has been made and the workflow has never run.**

## What task 06 did

1. **Saves in the per-user folder.** `bu00/` and `bu10/` live in
   `SDL_GetPrefPath(NULL, "lsd-dream-emulator")`
   (`~/.local/share/lsd-dream-emulator/`, `%APPDATA%\lsd-dream-emulator\`;
   `lsd-port/lsd/` until 2026-10-04), or in `--saves DIR` / `LSD_SAVES`
   (`--saves` wins), through `Psyz_AdjustPathCB` in `src/main.c`. psyz's
   `_bu_init` makes the card directories where they are mapped. Old
   `bu00/` in the working directory: a note on stderr, never moved.
   Checked: SAVE, LOAD after a restart, default folder, `LSD_SAVES`,
   `--saves` over `LSD_SAVES`.
2. **The card model.** SwCARD events are delivered by `_card_info` /
   `_card_load` through the kernel's event states (Enable/Disable/Test/
   DeliverEvent). A card is new until written, but `_bu_init` marks both
   known, because DuckStation shows no "memory card was swapped" message
   on LSD's first SAVE/LOAD after boot (the handover had asked for
   EvSpNEW once; the reference said otherwise). SAVE, LOAD and LOAD on an
   empty card look as on DuckStation.
3. **Windows under Wine.** `lsd.exe` (i686 MinGW) reaches the menu, plays
   a dream with its music (-30.3 dB RMS, console -30.24), SAVEs and LOADs
   after a restart. No code fix was needed. Not yet run on real Windows.
4. **Release workflow** (`.github/workflows/release.yml`,
   `tools/package.sh`): on a `v*` tag, Linux and Windows i686 archives in
   a *draft* release; by hand (workflow_dispatch) only the archives.
   Startup changes for downloaded builds: `disc/` is also found beside
   the executable, Windows shows startup errors in a message box,
   `lsd.exe` links MinGW's runtime statically.
5. **README**: status, saves and options, running a release, Linux 32-bit
   runtime libraries, known differences from the console.

What a release binary contains (each archive's `licences/SOURCES.txt`):
the port (MIT), lsddecomp's game C (CC0; not the game itself), psyz's
platform layer (MPL 2.0; its text is downloaded from mozilla.org at build
time, as psyz ships none), psyz's reconstructed Psy-Q functions (MIT),
psyz's SDK headers (unlicensed), stb_image_write (MIT/public domain),
SDL 3 (zlib), and on Windows the MinGW-w64 runtime.

## Open, in rough order

- **Run the release workflow by hand** (Actions, "release", Run
  workflow) before the first tag: it is untested on GitHub's runners.
  Things that may need a fix there: the MinGW runtime licence path on
  Ubuntu (`/usr/share/doc/mingw-w64-common/copyright` is a guess;
  `package.sh` fails loudly if missing), the glibc version the Linux
  binary needs (printed in the job summary; ubuntu-24.04 builds need
  2.39+), and the DLL whitelist (Ubuntu's MinGW links `msvcrt.dll`, not
  UCRT, which the list allows). Unpack both archives and run them once.
- **Real Windows**: one run on Windows 10/11 (the Wine check covered
  D3D12 through vkd3d only, without a swapchain).
- `lsd.exe` is a console program: a log window opens beside the game.
  A GUI-subsystem build (`-mwindows`, errors already go to a message
  box) is the operator's call.
- Still partial in psyz: `_card_write`/`_card_read` (no raw sectors; LSD
  uses files), HwCARD events fixed, `_card_status`.
- Unchanged from before: x86_64 crashes at boot (PLAN "Later"); psyz's
  59.94 Hz pacing (accepted); upstream PRs for the psyz branches are the
  operator's to open.
- Next by PLAN: the "Later" enhancements (resolution, widescreen,
  high-fps), or 64-bit clean.

## How task 06 worked (reuse it)

- Linux: `tools/lsd_drive.py` as before; it now sets `XDG_DATA_HOME`
  inside its `out` directory (and takes `saves=`), so runs never touch the
  real per-user folder.
- Getting a day and a save headless: a gdb `dprintf` on
  `GameApplication__RunTitleMenu` (it prints twice per menu: two
  locations), START, `vsync limitless` until two more prints, then
  classify screenshots against locally made references (menu, graph,
  comment entry, dream diary) and press circle on the graph. Menu order:
  START, SAVE (down 1), LOAD (down 2). SAVE: circle on the comment entry
  saves. LOAD: file list, circle, "load? YES" circle.
- DuckStation card trace: break by address on lsddecomp's absolute
  symbols (`break *0x80050b18` for `_card_info`; `_card_load` 0x80050b08,
  `_card_write` 0x80050b58, `_bu_init` 0x80050b88): gdb does not take
  `A` symbols by name.
- Wine: always `xvfb-run -a`, scratch `WINEPREFIX`,
  `WINEDLLOVERRIDES="mscoree,mshtml="` (else a fresh prefix hangs on the
  Mono/Gecko prompt), `WINEDEBUG=-all`. Wine drops `SDL_*` from the
  environment: set them with `wine reg add "HKCU\Environment" /v NAME /d
  VALUE /f` in the prefix. `LSD_*` pass through. Paths as `Z:/...`. Kill
  with `wineserver -k` for that prefix (the operator's Steam/Proton
  wineserver also runs: never kill by name). Intro movies run at 15 fps
  on every platform; measure speed in the menu or a dream.
- psyz host tests (Debug): fork `main` x86_64 316 passed / 2 skipped,
  i686 317 passed / 1 failed (`gte::read_rot_matrix...`, pre-existing).
  The `card` test must run before anything calls `_bu_init` (it does, by
  file order).
- Two `lsd` processes from before task 03 (pids 785164, 790598) and an
  `Xvfb :99` belong to no session; still left alone.
