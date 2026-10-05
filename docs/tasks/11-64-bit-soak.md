# Task 11: 64-bit soak

A long unattended run (about two hours) that widens what task 10 checked.
Read `docs/tasks/10-64-bit-handover.md` and `docs/design.md` ("64-bit clean
(task 10)") first, and the docstrings of `tools/lockstep.py`,
`tools/lockstep_gdb.py` and `tools/coverage_run.py`. Task 01's Rules apply.

The operator is away for the whole run. Nothing here needs a decision; if
something does, write it down and move on.

## Where things stand (2026-10-05)

- lsd-port `task-10-64-bit` (task 10, with `main`'s LSD_CFI commit merged
  in): `decomp/` pins lsddecomp `host-64-bit` `090be31e1`, `psyz/` pins
  the fork's `lsd-64-bit` `56aac8c`. Nothing is pushed.
- Task 10's lockstep runs found x86_64 and i686 identical over five
  ordinary days, a flashback, SAVE/LOAD and widescreen. Not yet driven at 64
  bits: most days and stages, the special days in a dream sequence, the
  ending, a year's wrap, the GRAPH menu item.
- `docs/research/mistyped-calls.md` (the CFI run, at i686) left two
  uninitialized arguments open in lsddecomp: `p` in
  `TmdModel__RaycastFaces` (tmd_model.c, passed to NextPrimitive before it
  is set; Clang deletes the function, so `LSD_CFI` builds without noundef)
  and `junk` in `StageMap__SetFootprintFromQuery` (stage_map.c).

## Setup

1. **Do not build from `~/git/lsddecomp`, and do not touch it.** Another
   session has it on `main` with uncommitted work. Make your own worktree
   of `host-64-bit` in your scratchpad:

   ```sh
   W=<your scratchpad>/lsddecomp
   git -C ~/git/lsddecomp worktree add $W host-64-bit
   cd $W
   for d in disk sdk lib .venv tools/binutils tools/splat tools/asm-differ \
            tools/m2c tools/maspsx tools/gcc263 tools/psyq-obj-parser include/psyq; do
       rm -rf $d; ln -s ~/git/lsddecomp/$d $d
   done
   git update-index --skip-worktree disk/.gitignore sdk/.gitkeep
   make extract
   ./build-and-verify.sh && tools/lint.sh
   ```

   Commit only files you mean to (`git add <file>`), never the symlinks.
2. **lsd-port builds**, each its own directory, all with
   `-DLSD_DECOMP_DIR=$W`:
   - `build-x64-dbg` and `build-i686-dbg`: `-DCMAKE_BUILD_TYPE=Debug`
     (`-DLSD_ARCH=x86_64` for the first): the lockstep pair. Lockstep needs
     `-O0`; at `-O2` gdb misses the pad and clock functions.
   - `build-x64-asan`: `RelWithDebInfo -DLSD_ARCH=x86_64
     -DLSD_SANITIZE=address,bounds`.
   - `build-x64-cfi`: `CC=clang`, `RelWithDebInfo -DLSD_ARCH=x86_64
     -DLSD_CFI=ON` (the CFI check has only run at i686).
   Some of these may already exist from task 10; reconfigure them to your
   worktree and rebuild.
3. **`/tmp` is near a per-user quota.** When it fills, commands' output
   silently disappears (`echo` exits 1). Lockstep runs use SDL's dummy
   audio by default; keep it that way. Delete each run's folder once its
   results are written down, and check `du -sh` of your scratchpad now
   and then. Do not delete other sessions' folders.
4. Use distinct debug-server ports for runs that overlap (78xx).

## The work, in priority order

Stop where the two hours run out; A alone is a useful result.

### A. Lockstep sweep: x86_64 against i686, ordinary days

Four runs at each width, two runs at a time (one per width), four days
per run, so the intro is paid once per run. Run the x86_64 and the i686
run of a set in parallel, then compare:

```sh
tools/lockstep.py build-x64-dbg  rN-x64  78a1 rN.json $OUT
tools/lockstep.py build-i686-dbg rN-i686 78a2 rN.json $OUT
diff <(grep '^STATE\|^SAVEBLK' $OUT/rN-x64.state) <(grep '^STATE\|^SAVEBLK' $OUT/rN-i686.state)
```

and the freeze screenshots (`$OUT/<tag>/f*.png`, pixel diff with PIL). The
days are 0-based and none is special (a day d is special when d + 1 is in
`sSpecialDays`, dream_sys.c; a special day plays a movie instead of the
dream, and its menu flow does not fit these steps):

| run | days | input |
|---|---|---|
| r1 | 3, 22, 30, 57 | walk |
| r2 | 88, 130, 150, 177 | wander |
| r3 | 210, 240, 250, 275 | walk |
| r4 | 290, 320, 340, 362 | wander |

Config for r1 (r2..r4 change `days` and, for "wander", `input`):

```json
{"seed": 1234, "days": [3, 22, 30, 57],
 "freeze": [300, 1500, 3000], "freeze_len": 10, "pktuse": true,
 "input": [[30, 150, "up"], [150, 170, "right"], [170, 300, "up"],
           [300, 330, "left"], [330, 900, "up"], [900, 1000, "right"],
           [1000, 2000, "up"]],
 "steps": [["menu"],
           ["day"], ["sleep", 3], ["press", "circle", 4],
           ["day"], ["sleep", 3], ["press", "circle", 4],
           ["day"], ["sleep", 3], ["press", "circle", 4],
           ["day"], ["sleep", 3], ["press", "circle", 4]],
 "day_timeout": 900}
```

"wander" input (turns more, backs up, so it meets walls and links):

```json
[[20, 200, "up"], [200, 230, "left"], [230, 500, "up"], [500, 530, "down"],
 [530, 560, "right"], [560, 900, "up"], [900, 960, "left"], [960, 1400, "up"],
 [1400, 1420, "right"], [1420, 2400, "up"], [2400, 2450, "left"], [2450, 3600, "up"]]
```

After `["press", "circle", 4]` the post-day chart closes and the title
menu comes up; the next `["day"]` presses START there. A day ends at its
timer (about 3650 ticks) or earlier (a fall, a link out). Both widths
must take the same path, so a divergence shows as differing STATE lines.

For each run record: the days, the stages seen (STATE `stage=`), the end
tick of each day, `SAVEBLK` equal or not, screenshots equal or not, and
PKT/POOL peaks at each width (x86_64's packet area is 230400 bytes per
buffer, i686's 153600; its pool is 8 MB against 4 MB).

**On a divergence**: find the first differing tick (rerun that day alone,
`"days": [d]`, with `"trace": [from, to]` around it: a STATE line every
tick), then use `"probe": {"ticks": [t, t+1], "funcs": [...]}` on the
functions that ran there, as task 10 did (handover, "How task 10
worked"). Fix it in your worktree only if the fix keeps the PS1 bytes
(`./build-and-verify.sh`, `tools/lint.sh`), one kind of fix per commit,
on a new branch `host-64-bit-soak` from `host-64-bit`. No new
`#ifdef HOST_BUILD`: that needs the operator, who is away; write it up
instead. On a crash, keep the gdb backtrace from the run's log.

### B. Sanitizer and CFI runs at x86_64

While A's pairs run, the CPU has room for one more process. Run
`tools/coverage_run.py` (15 days including special days, GRAPH,
FLASHBACK and SAVE, random walking) twice:

```sh
LSD_DISC=<abs .cue> COV_DAYS=1,13,14,41,80,119,125,201,258,266,283,307,327,357,363 \
    tools/coverage_run.py $OUT/asan build-x64-asan/lsd 7795 15
LSD_DISC=<abs .cue> COV_DAYS=0,1,10,14,50,42,100,119,200,201,300,357,363,6,20 \
    tools/coverage_run.py $OUT/cfi build-x64-cfi/lsd 7796 15
tools/cfi_triage.py build-x64-cfi/lsd $OUT/cfi/cov.log
```

The first list is mostly special days; the second is the list
`mistyped-calls.md` used at i686, so the CFI result compares with its
table. Expected from task 10: ASan reports only `&coord.m[3][0]`
(viewport_draw.c, harmless); CFI's int-pointer and lost-return rows that
mattered are fixed on `host-64-bit`. Report anything else, especially any
int-pointer or lost-return site that appears at x86_64 and not in the i686
table.

### C. The year's end

Day 364 (0-based) is the last of the year, and what follows it (a wrap to
day 0, the ending movie) has not run on the port at any width. One
lockstep pair with `"days": [363, null]` and two `["day"]` steps (the
second day continues from wherever the first left the count), plus
screenshots every few seconds after the first EndDay (`["shots", "e",
20, 2.0]`) to see what plays. Record what happens at each width; they
should agree. If the second day does not start (the ending runs instead),
that is a result too.

### D. Two uninitialized arguments (lsddecomp, small)

In your worktree, on `host-64-bit-soak`: set `p` before its first use in
`TmdModel__RaycastFaces` (NULL; NextPrimitive ignores it while `*count`
is 0) and `junk` in `StageMap__SetFootprintFromQuery`. Each must keep the
PS1 bytes; if one does not, leave it and write down what changed. If `p`
lands, `LSD_CFI`'s `-Xclang -no-enable-noundef-analysis` can go: rebuild
`build-x64-cfi` without it (edit CMakeLists.txt on lsd-port's
`task-10-64-bit`) and check a dream still walks.

### E. Loose ends from task 10

- The one differing screenshot: two spinning flowers on day 200 of a
  four-day lockstep run (days 0, 9, 99, 199). Check whether they turn while
  the dream clock is frozen: two screenshots a second apart during one
  freeze, at one width. If they move, the difference is the shot's timing,
  not the game.
- The GRAPH menu item: open it at x86_64 from the title menu (the cursor
  order is START, SAVE, LOAD, GRAPH, SHAKE; with FLASHBACK unlocked it
  comes second) and take a screenshot.

## Rules (in addition to task 01's)

- **No pushes**, to any repository and any branch.
- lsddecomp: commits only on `host-64-bit-soak` in your worktree, each with
  `./build-and-verify.sh` and `tools/lint.sh` green.
- lsd-port: commits on `task-10-64-bit` are fine (results, tools); do not
  move `main`.
- No windows on the operator's display: everything headless (offscreen,
  as lockstep and coverage_run already are). No Wine in this task.
- No game data, screenshots, recordings or save files in any repository.
- Kill every process you start before you finish: by pid, not `pkill -f`
  (it can kill your own shell); a gdb-run game is a child of gdb.

## Report back with

A table per part: what ran, at which widths, and what matched. Every
divergence or sanitizer/CFI report, with its first tick or call site and
whether it is fixed (commit) or written up. PKT and POOL peaks. What C
showed at the year's end. D's commits or why not. Then a short handover,
`docs/tasks/11-64-bit-soak-handover.md`, committed on `task-10-64-bit`.
