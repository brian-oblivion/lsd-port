# Task 08: neighbouring statics — where it ended

Handover from task 08 (`08-neighbouring-statics.md`). The details are in
`docs/research/host-link-surface.md` ("Neighbouring statics (task 08)").

## Where things stand (2026-10-04)

- lsddecomp branch `host-neighbour-statics` (local, not pushed), two
  commits on `main` `865b85328`, each with `make extract`,
  `./build-and-verify.sh` and `tools/lint.sh` green:
  - `bf626d791` the decoration set's place and size as structs;
  - `f3b9ccbc4` the spawn heights and Violence District's configs as one
    block.
- lsd-port `main` has the task doc, `LSD_SANITIZE` (CMake), the
  `lsd_drive.stop()` fix, the research section and this handover.
  `decomp/` still pins `863ea0777`: the two fixes reach the port when the
  lsddecomp branch is reviewed, merged and pinned.

## What task 08 found

1. **The decoration bands** (`StyleBuildDecorSet`, `StyleUpdateDecorSet`):
   position and size copied whole through the first of two separate
   statics. v0.1 reads them as (-100, 773874725) and (320, -100) instead
   of the console's (-100, -60) and (320, 144), so the bands under the
   fade box were never where they belong on the PC.
2. **A spawn height** (`StyleFillEffectKind0`): a pick of 4 reads past
   `sStyleSpawnYChoices[4]`, into the first Violence District config on
   the PS1 (0x0A0A0200) and into another symbol on the PC.
3. Harmless: `Viewport__DrawNode`'s end pointer `&coord.m[3][0]`.

Nothing else reported, over: 15 walked days, 25 days with the day set
from gdb (15 special days and their movies), a flashback session (28
minutes at full speed, cut off by the script's timeout), pause, SAVE
(comment entry), LOAD and GRAPH. The source review (casts of statics'
addresses, comments on neighbours, statics the PS1 places by address,
GCC's static bounds warnings) found nothing more.

## Open

- **lsddecomp review**: `host-neighbour-statics` goes to a reviewer
  first; then push, merge, and move lsd-port's `decomp/` pin.
- **Not compared with DuckStation**: both fixes keep the PS1 bytes and
  make the PC read what the PS1 reads (shown above), but no screenshot
  pair of the bands was made. A stage whose style config picks
  decoration variant 1 or 2 (variant 0 records 0..5,
  `PickStyleFallbackConfig`) would show them.
- **Coverage gaps**: the sanitizer only sees paths that run. Not driven:
  the ending, a year's wrap (day 364 to 0 was set but not played
  through), every stage's every link, the gamepad. The sanitizer build is
  cheap to run again after any lsddecomp change.
- **CI**: an ASan run needs the disc, so it cannot be a CI job; a
  build-only job with `LSD_SANITIZE` would only keep the option compiling.

## How task 08 worked (reuse it)

- Build: `cmake -S . -B build-i686-asan -G Ninja
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSD_SANITIZE=address,bounds`
  (add `-DLSD_DECOMP_DIR=$HOME/git/lsddecomp` to test a branch). Run with
  `ASAN_OPTIONS=halt_on_error=0:detect_leaks=0`; reports go to the run's
  log.
- gdb markers: Python breakpoints that `gdb.write` and `gdb.flush`
  (`dprintf` is buffered; its `call` style deadlocks). A Python
  breakpoint on `DreamSys__StartDay` sets `self->currentDay` (special
  days are `sSpecialDays` minus one); one on `UpdateFlashbackLock` sets
  `((DreamSaveBlock *)self->saveBlock)->totalFlashbackUnlockScore` above
  9999999, and FLASHBACK shows once a day has stored one.
- Menus: the title menu falls into the attract movie after about 10 s.
  Drive it from the screen (`lsd_drive.classify` against local
  references of the menu and the graph), all in one script. Menu order
  without FLASHBACK: START, SAVE, LOAD, GRAPH; SAVE is the comment entry
  (circle saves), LOAD the file list then YES (circle). SAVE on a new
  game before any day returns to the menu.
- The pause text blinks; read the pause state from memory, not from one
  screenshot.
- Rules unchanged.
