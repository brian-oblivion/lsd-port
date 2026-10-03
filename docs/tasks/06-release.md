# Task 06: release

Handover. Read `docs/PLAN.md` (its "Decided" list has two new entries:
save files and the console reference) and `docs/tasks/01-game-c-builds-for-host.md`
(its Rules section applies unchanged) first, then the stop of task 05 in
`docs/research/host-link-surface.md` ("Against the console").

## Where things stand (2026-10-03)

- lsd-port `main` is `7398af0` plus this handover. Work on a new branch
  from it, `task-06-release`.
- psyz fork `main` is `4f0a7fa` (pushed); lsddecomp `main` is
  `0355ebad0` (unchanged since task 03).
- The i686 Linux build plays a whole day, and since task 05 its sound and
  picture match DuckStation, apart from differences the operator accepted
  (PLAN, "Decided"). Memory cards are still `bu00/` and `bu10/` in the
  working directory.
- CI (`.github/workflows/build.yml`) already builds Linux i686 and
  x86_64 and Windows i686 and x86_64 (MinGW) with no disc, and checks that
  `lsd` exits 2 without one. **The Windows build has never been run.**
- Still partial in psyz: `_card_info`/`_card_load` always answer
  "formatted card present", with none of the console's "new card" event;
  `EnableEvent` and `DisableEvent`.
- x86_64 still crashes at boot (`BMemPMgrAlloc`, `ETC\DREAME5.TMD`):
  under PLAN's "Later". Out of scope here.

## The task

Make the i686 build something a person can download and play, on Linux
and Windows. One stop at a time, as before.

1. **Save files in the per-user folder** (decided, PLAN): `bu00/` and
   `bu10/` under `SDL_GetPrefPath(...)`, with `--saves DIR` and
   `LSD_SAVES` to put them elsewhere. psyz already routes `buXX:` paths
   through `Psyz_AdjustPath` (`psyz/psyz/src/platform/psyz.c`), and
   `Psyz_AdjustPathCB` lets the port supply its own mapping from
   `src/main.c`. `_bu_init` (`psyz/src/psyz/libcard.c`) still `mkdir`s
   `bu00`/`bu10` in the working directory: make it go through the same
   mapping (a psyz branch). Don't move old saves from a working directory
   silently: if `bu00/` exists where `lsd` starts and the per-user folder
   has none, say so on stderr, and say in the README how to move them.
   Check SAVE, LOAD after a restart, and both `--saves` and `LSD_SAVES`.
2. **The card model:** `_card_info` reports a new card once (the
   console's "new card" event, `EvSpNEW`), then a known card; and
   `EnableEvent`/`DisableEvent`. Check what LSD does with each (its card
   code is in lsddecomp `src/`, and `src/psyq/libcard_card.c`); the game
   must still save and load as before.
3. **Windows.** Run `lsd.exe` (i686 MinGW build) under Wine, **only under
   `xvfb-run -a`** (`~/.claude/CLAUDE.md`: Wine never on the operator's
   display), with a scratch `WINEPREFIX`. Get it to the title menu, a
   dream with sound (SDL's `disk` audio driver works under Wine too, or
   use the debug server's metrics), SAVE and LOAD. Fix what blocks it,
   one stop at a time: psyz's `plat_win.c` and the port's `src/` are the
   likely places. If Wine cannot run it headless and repeatably, stop and
   report.
4. **Packaging.** A workflow that, on a tag, builds the Linux i686 and
   Windows i686 binaries and attaches them to a GitHub release (archives
   with the binary, `LICENSE`, the README's "The game" section as a text
   file, and the licences of what is linked in). **No game data, no
   BIOS.** Prepare the workflow; **do not tag or publish a release**:
   that is the operator's.
5. **README:** the status ("playable on Linux i686 and Windows", with
   what is known not to match the console, from task 05), where saves
   live and the options, how to run the release binaries, and the
   Linux runtime libraries a downloaded binary needs.

Stop and report if a stop needs a design decision. One is likely, and it
is the operator's, not yours: **what the released binaries may contain**.
psyz's parts carry their own licences, including "unlicensed SDK
headers/decompiled code" (README, Licence). List exactly what ends up in
the binaries and under which licence, and ask before the release workflow
is merged.

## How task 05 worked (reuse it)

- **`tools/lsd_drive.py`** drives a headless port run (optionally under
  gdb): buttons, screenshots, pacing, raw audio. **`tools/ds_drive.py`**
  does the same for DuckStation as the console reference, with its own
  config in your scratch space. **`tools/snd_compare.py`** measures two
  recordings (loudness, pitch, tempo, balance, spectrograms).
- `--saves` work: psyz writes cards relative to the working directory
  today, so `lsd_drive.Run` starts the game in its `out` directory; keep
  every save a test writes in scratch space, and make sure a run with the
  new default does not write into your real per-user folder (point
  `XDG_DATA_HOME` or `--saves` at scratch).
- Pitfalls from task 05:
  - DuckStation's GDB stub reports breakpoint hits late and slows
    emulation per hit; a breakpoint on a hot function (VSync) stalls it.
    Run gdb under `stdbuf -o0` when you read its log as it comes.
  - A gdb breakpoint on an inlined function has several locations and
    prints once per location: count "seen since a mark", not totals.
  - Screenshots lag ~0.3 s; a screen shown for under a second at
    fast-forward is easily missed.
  - `pgrep -f "<pattern>"` in a loop matches the shell running the loop:
    wait on pids or files instead.
  - From task 04, still true: START in a dream is the pause (mutes the
    SPU); `/input` frames are game frames; psyz host tests only in a
    Debug build; clang-format only the lines you change.
- psyz host tests on fork `main` (Debug): x86_64 314 passed / 2 skipped;
  i686 315 passed / 1 failed (`gte::read_rot_matrix...`, pre-existing).
- Two `lsd` processes from before task 03 (pids 785164, 790598) belong to
  no session; leave them unless the operator says otherwise.

## Rules (in addition to task 01's)

- **lsddecomp: never commit on `main`.** Topic branch, with
  `./build-and-verify.sh` and `tools/lint.sh` green; point `decomp/` at it
  meanwhile and re-pin after the merge.
- **Never push to any repo's `main` without the operator's yes** for that
  push. Topic branches you may push. lsd-port's `psyz/` and `decomp/` pins
  must be on their remotes before lsd-port's `main` moves.
- psyz branches start from `upstream/main` (or stack on the branch they
  need, saying so; if `upstream/main` has moved past what the fork has
  merged, start from their merge base so the branch merges cleanly into
  both), merge into the fork's `main`, and get a section in
  `docs/upstream/psyz-prs.md`.
- No game data, BIOS, recordings, screenshots or save files in any
  repository or release artifact.
- No tags, releases or other publishing without the operator's yes.

## Report back with

Where saves now live and how that was checked, the card model's
behaviour, how far the Windows build got under Wine and the stops on the
way, the release workflow (not run on a tag) with the list of what the
binaries contain and their licences, the README changes, and the
branches/commits waiting for review or push.
