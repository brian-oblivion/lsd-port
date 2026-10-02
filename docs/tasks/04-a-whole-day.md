# Task 04: a whole day, with sound, saves and flashbacks

Handover. Read `docs/PLAN.md` and `docs/tasks/01-game-c-builds-for-host.md`
(its Rules section applies unchanged) first, then task 03's stops in
`docs/research/host-link-surface.md` ("Into the dream").

## Where things stand (2026-10-03)

- i686, real disc: intro → title menu → START → a dream (textured, lit,
  fogged; walking, terrain, links into other areas) → dream graph → title
  menu at Day 002. A ~3 minute walk-around ran without a crash.
- `src/stubs.c` has 5 stand-ins left: `SsSeqPause`, `SsSeqReplay`,
  `SsSetMute`, `SsUtAutoVol` (libsnd) and `SetMem` (libapi).
- psyz fork `main` is `f86c38e`; lsddecomp `main` is `0355ebad0` (task 03's
  host fixes merged). Their PR texts: `docs/upstream/psyz-prs.md`.
- Seen in passing, not yet followed up:
  - psyz logs `SsSeqOpen` and `_SsVmSetSeqVol`/`_SsVmSeqKeyOff` as not
    implemented when a dream starts: dream music is not played.
  - `DisableEvent`, `OpenEvent` (spec 0x20) are partial in psyz.
  - x86_64 still stopped in `BMemPMgrAlloc` (`ETC\DREAME5.TMD`) before
    task 03; the host pool is 4 MB now, so the stop may have moved.

## The task

Make a whole day work as on the console, one stop at a time, as tasks
02 and 03 did:

1. **Sound.** On this machine the 32-bit ALSA library is missing, so
   nothing has ever been heard at i686. Ask the operator to install
   `lib32-alsa-lib` and `lib32-libpulse` (never install packages
   yourself). Without them, check sound headless: SDL's `disk` audio
   driver (`SDL_AUDIO_DRIVER=disk`, output file in
   `SDL_AUDIO_DISK_OUTPUT_FILE`, raw samples)
   writes what the SPU mixes, so a WAV-sized file that is not silence
   shows it works. Then the dream's music: psyz's `SsSeqOpen` and the
   libsnd sequencer calls it lacks, plus the four libsnd stand-ins.
   lsddecomp carries Sony's sequencer as C (`decomp/src/psyq/`, libsnd):
   check what the port compiles of it before writing anything new.
2. **Saves.** SAVE and LOAD on the title menu go through libcard and the
   kernel's `bu00:` files (psyz's `libcard.c`, `_card_*` partly done).
   Save on day 2, restart, load, and see day 2 come back.
3. **Flashbacks and the graph.** FLASHBACK unlocks after enough days;
   a loaded save that has it shows whether flashback and the graph room
   (GRAPH) work. Special-day movies (`FILM\SPDAY*.STR`) play from the
   graph when it scores.
4. **A dream to its natural end** (up to ten minutes, or a fall), and the
   next days' dreams: different stages load different files.

Stop and report if a stop needs a design decision (for example, where
save files live on the host).

## How task 03 worked (reuse it)

- **Run headless** with the debug server: `env -u DISPLAY -u
  WAYLAND_DISPLAY SDL_VIDEO_DRIVER=offscreen LSD_VSYNC=off
  LSD_DEBUG_PORT=<port> ./build-i686/lsd`. `curl 127.0.0.1:<port>/screenshot`
  and `/vram` take PNGs; **`/input` must be POST**:
  `curl -X POST "127.0.0.1:<port>/input?buttons=start&frames=4"`
  (button names in `psyz/psyz/src/dbgserver/dbgserver.c`). Press START
  every ~4 s to skip the movies to the menu (~60 s), then once more for
  the day.
- **Under gdb**, `dprintf` is the tool: `dprintf
  GameApplication__RunTitleMenu,"RunTitleMenu\n"` marks the menu in the
  log, so a script can wait for it before pressing START. Keep a `-O0`
  build (`cmake -S . -B build-i686-dbg -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DLSD_ARCH=i686`) for tracing: at `-O2` calls are devirtualised and
  dprintfs miss. Breakpoint hit counts (`ignore N 1000000`, then `info
  breakpoints`) count events without slowing the run to a crawl. A small
  gdb Python command that walks the OT from `GsDrawOt`'s `ot->tag`
  showed what reached the GPU.
- **Kill your runs by pid** (`pgrep -x gdb`); `pkill -f <pattern>` kills
  the shell running it. Two `lsd` processes from before task 03 (pids
  785164, 790598) belong to no session; leave them unless the operator
  says otherwise.
- **The console survives reads through NULL into low memory; the host
  does not.** Twice in task 03 the fix was to skip, under `HOST_BUILD`,
  what retail does harmlessly. Look for this first on a crash.
- **Sony's code is the reference** (`~/git/lsddecomp/lib/*/*.o`,
  `~/git/lsddecomp/asm/psyq_*.s`, SDK headers in
  `~/git/lsddecomp/sdk/work/*/psx/INCLUDE/`). Disassemble with
  `~/git/lsddecomp/tools/binutils/bin/mipsel-linux-gnu-objdump -drz`;
  filtering out `nop` lines hides delay slots, so check branches in the
  raw output. Never commit Sony code or data.
- **Names matter:** the decomp's `include/gte.h` is replaced on the host
  by psyz's `libgte.h`, so a decomp macro under a different name than
  Sony's silently changes meaning (task 03's `gte_stflg_4`).
- **psyz host tests** at both widths before blaming the game. Fork
  `main` today: x86_64 308 passed / 2 skipped; i686 306 passed / 1 failed
  (`gte::read_rot_matrix_reads_rotation_and_translation`, which memcmps
  uninitialised padding: pre-existing). Run the binary from
  `psyz/psyz/tests`.
- **Parallel psyz work:** `git worktree add -b <branch> <dir>
  upstream/main` in `psyz/`, and copy `external/SDL` into it with
  `rsync -a --exclude .git` (a symlink breaks git status). Remove the
  worktrees when done.

## Rules (in addition to task 01's)

- **lsddecomp: never commit on `main`.** Work on a topic branch
  (`task-04-host`), with `./build-and-verify.sh` and `tools/lint.sh`
  green; the operator has it reviewed and merged. Point `decomp/` at the
  branch meanwhile and re-pin to `main` after the merge.
- **Never push to any repo's `main` without the operator's yes** for
  that push. Topic branches you may push. lsd-port's `psyz/` and
  `decomp/` pins must be on their remotes before lsd-port's `main` moves,
  or its CI fails at the Submodules step.
- psyz branches start from `upstream/main` (or stack on the branch they
  need, saying so), merge into the fork's `main`, and get a section in
  `docs/upstream/psyz-prs.md`.

## Report back with

The stops hit and what fixed each, the stub count, what was heard and
how it was checked, and the branches/commits waiting for review or push.
