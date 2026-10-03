# Task 05: against the console

Handover. Read `docs/PLAN.md` and `docs/tasks/01-game-c-builds-for-host.md`
(its Rules section applies unchanged) first, then the stops of tasks 03
and 04 in `docs/research/host-link-surface.md` ("Into the dream", "A whole
day").

## Where things stand (2026-10-03)

- lsd-port `main` has tasks 01–04 (merged from `task-04-a-whole-day`).
  Work on a new branch from it, `task-05-against-the-console`.
- psyz fork `main` is `74fce1f` (pushed); lsddecomp `main` is `0355ebad0`
  (unchanged since task 03). `src/stubs.c` is gone: no stand-ins left.
- The i686 build plays a whole day: intro, title menu, the dream with its
  SEQ music, links between stages, the dream graph, SAVE and LOAD,
  FLASHBACK, a special-day movie. Three walked days ran without a crash.
- **Nothing has been compared with the console yet**, and nobody has
  listened to the sound. psyz's sequencer (`libsnd-seq`) was written from
  libsnd 3.3's code, LSD's own build, but it has never been checked
  against what a PS1 plays.
- Still partial in psyz: `_SsSndTempo` (unused by the game), `EnableEvent`
  and `DisableEvent`, and `_card_info`/`_card_load`, which always answer
  "formatted card present", with none of the console's "new card" event.
- x86_64 crashes at boot (`BMemPMgrAlloc`, `ETC\DREAME5.TMD`): the 64-bit
  layout debt, still under PLAN's "Later". Out of scope here.

## The task

Make "plays as on the console" a measured statement, for sound first, then
picture. Fix what differs, one stop at a time, as before.

1. **A reference.** DuckStation is installed (`/usr/sbin/duckstation-qt`),
   with a BIOS in `~/.local/share/duckstation/bios`. Run it only under
   `xvfb-run -a` (never on the operator's display; see
   `~/.claude/CLAUDE.md`), with its **own** settings and data directory in
   your scratch space: never read or write the operator's DuckStation
   config, saves or memory cards. Reading the BIOS file from its place is
   fine; never copy it into a repository. Find out what it offers for
   recording audio (an audio dump or a WAV writer) and for screenshots and
   input from a script. If DuckStation cannot be driven headless and
   repeatably, **stop and report** with what you found, rather than
   building something elaborate.
2. **Sound.** Record the same moments on both: the title menu's
   button tones (`SsUtKeyOn` through `VabStreamObj__PlayTone`), the first
   dream's music (day 1 starts in the same room on both), a movie's XA
   audio. Compare loudness over time, pitch (dominant frequencies),
   stereo balance and tempo (the SEQ's loop length). Differences in the
   music are most likely in `libsnd-seq` (psyz `decomp/src/libsnd/`:
   `vm_*.c`, `midi*.c`, `cc_*.c`) or in psyz's SPU mixer
   (`psyz/src/psyz/psyz_spu.c`, `spu_voice.h`). The 3.3 code it follows
   is in lsddecomp (`src/psyq/libsnd_*.c`, `asm/nonmatchings/psyq/`,
   `lib/libsnd/*.o`). Give the operator a WAV of each pair to listen to
   (as files, sent to them, never into a repository).
3. **Picture.** Screenshots at matched moments (the title menu, the first
   dream room, the dream graph, a movie frame): side by side, and the
   differences listed (colour, dithering, fog, geometry, timing).
   Fix what is clearly psyz's; report the rest.
4. **If time is left:** the card model (`_card_info` reporting a new card
   once, as the console does), and `EnableEvent`/`DisableEvent`.

Stop and report if a stop needs a design decision. One is already open
from task 04: **where save files live on the host** (now `bu00/`/`bu10/`
in the working directory). Don't decide it yourself; if the operator has
decided by the time you start, do that first.

## How task 04 worked (reuse it)

- **`tools/lsd_drive.py`** starts a headless run (optionally under gdb)
  in a scratch directory, presses buttons, takes screenshots, switches the
  pacing and measures the raw audio. Everything a run writes stays in its
  `out` directory; psyz writes memory cards to `bu00/` there.
- **Sound headless:** SDL's `disk` driver (`SDL_AUDIO_DRIVER=disk`,
  `SDL_AUDIO_DISK_OUTPUT_FILE`) writes the mix as raw S16LE stereo at
  44.1 kHz. `lsd_drive.loudness()` gives peak and RMS per second; wrap the
  raw in a WAV header for listening.
- **Fast-forward:** `vsync('limitless')` runs a whole day in about 2 s
  (the intro movies stay ~80 s). Under gdb, a breakpoint on
  `GameApplication__RunTitleMenu` whose `commands` `call
  (int)Psyz_VideoSetVsyncMode(2)` and `continue` drops back to normal speed
  when the menu or the post-day graph appears. Breakpoint hit counts
  (`ignore N 100000000`, then SIGINT the game and `info breakpoints`) count
  calls cheaply.
- **Menus:** the title menu times out after ~10 s, faster than a tool
  round trip, so run each menu sequence inside one script. Down/up move,
  circle confirms, cross cancels. Tell screens apart with
  `lsd_drive.classify()` against reference shots you take locally (game
  imagery: never commit them).
- **Pitfalls that cost time:**
  - START in a dream is the pause, and the pause mutes the SPU. A START
    still held when the dream begins pauses it at once: silence that
    looks like a sound bug.
  - `/input` frames are game frames (a dream runs at 20 fps). Wait for
    each press to drain, or the queue fills and the server answers 400.
  - psyz's host tests must be built **Debug**: the `bu` teardown deletes
    its files inside `assert()`, so a Release run leaves them and the next
    run fails `bu`/`truncation`. Fork `main` today: x86_64 313 passed / 2
    skipped; i686 314 passed / 1 failed (`gte::read_rot_matrix...`,
    pre-existing).
  - clang-format only the lines you change: some upstream psyz headers
    (`libetc.h`) are not clang-format clean, and reformatting them buries
    the change.
  - zsh: `$list` does not word-split (use `xargs`); `pkill -f` can match
    and kill the calling shell, so kill by pid.
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
  need, saying so), merge into the fork's `main`, and get a section in
  `docs/upstream/psyz-prs.md`.
- No game data, BIOS, recordings or screenshots in any repository.

## Report back with

How the reference was set up (or why it could not be), each comparison and
its result, the stops hit and what fixed each, the WAV pairs sent to the
operator, what is still different and why, and the branches/commits
waiting for review or push.
