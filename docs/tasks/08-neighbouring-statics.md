# Task 08: neighbouring statics

Handover. Read `docs/PLAN.md`, `docs/tasks/01-game-c-builds-for-host.md`
(its Rules section applies unchanged) and `docs/tasks/07-controls-handover.md`
first; this task is the first item of the latter's "Open" list.

## Where things stand (2026-10-04)

- lsd-port `main` is `0809ca2` plus this handover, tagged release `v0.1`
  at `3c4443b`. Work on a new branch from it, `task-08-statics`.
- psyz fork `main` is `26fad08`; lsddecomp `main` is `865b85328`, and
  lsd-port pins `863ea0777` (the move fix).

## The problem

The decomp is byte-exact on the PS1, so every static sits where retail's
linker put it. Code that reaches one static through another's address
works there and breaks silently on the PC, where the host compiler and
linker place statics as they like. Task 07 found one only by comparing
with DuckStation: `Actor__AddLocalTranslation` read x, y and z through
`&sActorLocalMove[0]`, but z was a separate static that retail's small
data put after y (lsddecomp `863ea0777`: one `s16[3]` instead, same PS1
bytes). On the PC, forward and back moved by whatever lay next in memory.

Likely shapes of the same bug:

- an array read or written past its end into the next symbol (short
  arrays, `[2]`, `[3]`, and scalars declared next to each other);
- a pointer to one static used as the base of a struct that spans
  several (`(Foo *)&sA`, then `->b`);
- a loop or `memset`/`memcpy` over a range of statics, from the first's
  address to the last's (clearing "the block" of a module's state);
- code that takes the difference of two statics' addresses, or compares
  them.

It is not limited to small data (`config/gp-symbols.txt`, 234 symbols):
`.data` and `.bss` neighbours break the same way.

## The task

1. **Find candidates, two ways, and report both lists.**
   - *At run time*: an i686 build with `-fsanitize=address` (its global
     redzones report a read or write past a global's end) and
     `-fsanitize=bounds`, on the game's C at least; psyz may stay
     uninstrumented if that is simpler. Play it headless through the
     usual paths (`tools/lsd_drive.py`: intro, title, a whole day, a few
     days in with edited saves, a flashback, the graph, SAVE and load).
     Every report is a candidate. If ASan does not work at i686 with the
     game's allocator or psyz, say why, and try a host linker option that
     spreads globals instead (padding between them changes the PC's
     behaviour where a bug exists; compare against a normal build).
   - *In the source*: retail's adjacent symbols (the symbol file and
     `gp-symbols.txt`, in address order) against the C that uses them:
     indexes past a declared size, casts of a static's address to a
     larger type, address arithmetic across statics. GCC's
     `-Warray-bounds=2` and `-Wstringop-overflow` on the host build help.
2. **Confirm each candidate** before fixing it: say what the PS1 reads or
   writes there (the neighbour in retail's order) and what the PC does
   now. Not every report is a bug: an index past the end may be dead or
   guarded.
3. **Fix each in lsddecomp**, keeping the PS1 bytes, as `863ea0777` did:
   one array or struct instead of separate statics, and the uses through
   it. One commit per bug, with the PS1 versus PC explanation in the
   message. `./build-and-verify.sh` and `tools/lint.sh` green for each.
   Where a fix that keeps the bytes is impossible, stop and report it;
   `#ifdef HOST_BUILD` is the last resort and needs the operator's yes.
4. **Check the fixes in the port**: the sanitizer build clean on the same
   paths, and the paths that changed compared against DuckStation where
   the change is visible (`tools/ds_drive.py`, as in task 07).

Don't fix other bugs found on the way; list them.

## How to work

- Reuse task 07's handover ("How task 07 worked"): reading game state
  from `/proc/<pid>/mem`, DuckStation's GDB stub, getting into a dream
  and to a given day.
- Builds: `build-i686` (release) and `build-i686-dbg` exist; make a new
  one for the sanitizers (for example `build-i686-asan`) with a CMake
  option, not by editing flags by hand, so it can be rebuilt.
- lsddecomp: a topic branch from its `main` (for example
  `host-neighbour-statics`), with `decomp/` pointed at it meanwhile.

## Rules (in addition to task 01's)

- **lsddecomp: never commit on `main`.** Topic branch, with
  `./build-and-verify.sh` and `tools/lint.sh` green.
- **Never push to any repo's `main` without the operator's yes** for that
  push. Topic branches you may push. Pins must be on their remotes before
  lsd-port's `main` moves.
- No game data, BIOS, recordings, screenshots or save files in any
  repository.
- No windows on the operator's display: headless or `xvfb-run -a`
  (`~/.claude/CLAUDE.md`).

## Report back with

Both candidate lists (sanitizer reports with the path that triggered
them; source findings), each with its verdict (bug, dead, guarded); the
fixes (lsddecomp commits, what each changes on the PC, verified how);
what could not be fixed with the bytes kept; the sanitizer build option
and whether it is worth keeping in CI; other bugs noticed; the branches
waiting for review or push.
