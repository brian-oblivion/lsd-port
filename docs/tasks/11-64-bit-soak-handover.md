# Task 11: 64-bit soak — where it ended

Handover from task 11 (`11-64-bit-soak.md`), run unattended on
2026-10-05. Nothing pushed, no lsddecomp commits (D below), no new
`#ifdef HOST_BUILD`.

## Where things stand

- **lsd-port** `task-10-64-bit`: this handover, the `freeze_shots`
  option in `tools/lockstep.py` (several shots per freeze, to see what
  moves while the dream clock is stopped), a paragraph in
  `docs/research/mistyped-calls.md` (D), a sentence in `docs/design.md`
  (the flowers). Pins unchanged (`decomp/` `090be31e1`, `psyz/` `56aac8c`).
- **lsddecomp**: a local branch `host-64-bit-soak` at `host-64-bit`
  `090be31e1`, no commits (in this task's scratchpad worktree; nothing
  in `~/git/lsddecomp`'s checkout was touched).
- Every build was from that worktree: `build-x64-dbg`, `build-i686-dbg`,
  `build-x64-asan`, `build-x64-cfi`, and a new `build-i686-cfi` (for B).
  They point at a scratchpad path; reconfigure `LSD_DECOMP_DIR` before
  reusing them.

## A. Lockstep sweep, x86_64 against i686

Config as in the task doc (seed 1234, freezes at 300/1500/3000, 10 ticks).
"Equal" means every STATE and SAVEBLK line.

| run | days | input | stages | end ticks | state | saves | shots differing | PKT max x64 / i686 | POOL peak x64 / i686 |
|---|---|---|---|---|---|---|---|---|---|
| r1 | 3, 22, 30, 57 | walk | 3, 5, 12 | 683, 243, 182, 182 | equal | equal | 0 of 1 | 53216 / 41460 | 2.12 / 1.47 MB |
| r2 | 88, 130, 150, 177 | wander | 1, 2, 4, 5, 6, 12 | 521, 369, 7203, 2882 | equal | equal | 1 of 7 | 69424 / 54604 | 2.21 / 1.54 MB |
| r3 | 210, 240, 250, 275 | walk | 0, 1, 3, 4, 5, 12 | 1582, 3628, 273, 3602 | equal | equal | 3 of 8 | 56824 / 44344 | 2.26 / 1.57 MB |
| r4 | 290, 320, 340, 362 | wander | 0, 2, 6, 12 | 551, 454, 182, 182 | equal | equal | 1 of 2 | 44440 / 34516 | 2.14 / 1.49 MB |
| rt | 22, 150, 240, 320 | wander, `trace` every tick | 0–6, 9, 11, 12 | 1609, 7203, 520, 3654 | **12955 lines equal** | equal | 2 of 9 | 69152 / 54256 | 2.32 / 1.67 MB |

- No divergence anywhere. The four planned runs end many days early
  (falls, links out), so they compare few STATE lines (9–16 each). `rt`
  was added for that: the same pair with a STATE line on every tick.
- Packet use peaks at 69424 bytes at x86_64 (budget 230400 per buffer)
  and 54604 at i686 (153600). Pool peaks at 2.32 MB of 8 MB and
  1.67 MB of 4 MB. (`PKT max` is the session's running maximum.)
- The differing shots are not the game: the state at those ticks is
  equal, the differences are fades caught at different brightness
  (r2 and r4 at tick 300, just after a link) or small areas of animated
  texture or sky. See E: things keep moving while the dream clock is
  frozen.
- The fade at a link runs on frame time, not dream ticks, and how long
  the dream clock stays frozen changes the path: day 88 with a 120-tick
  freeze at tick 300 ends at tick 505, with a 10-tick one at 521. Both
  widths take the same path for the same config, so lockstep holds, but
  configs with different `freeze_len` aren't comparable with each other.
- Day 150 ends at tick 7203 at both widths (r2 and rt): not a bug. Each
  stage has its own limit (`sStageTimeLimits`, 60 to 600 seconds at 15
  ticks a second); 7203 is a 480-second stage's, and the common ~3650 is
  the 240-second one's.

## B. Sanitizer and CFI at x86_64

**ASan** (`build-x64-asan`, days 1, 13, 14, 41, 80, 119, 125, 201, 258,
266, 283, 307, 327, 357, 363; the run went on to play 364 and then 0):
three reports.

- `viewport_draw.c:106` `&coord.m[3][0]`: known, harmless (task 08).
- **New, `dream_sys.c:2232` and `:2233`**: `moodPreviousDays[-1]` in
  `DreamSys__GetPreviousDayMood`, on the first day after the year wraps
  (day 0, `currentYear` 1). The `lastDayOnly` branch reads
  `moodPreviousDays[currentDay - 1]` whenever the year or the day is
  nonzero, so on day 0 of year 1 it reads the 2 bytes before the array
  (the high half of `instanceFlashbackUnlockScore`) instead of day 364.
  A retail bug, the same on the PS1 and at both widths (the fields
  before the ring are all `s32`, and the save block's layout is
  asserted). Not fixed: the fix changes the PS1 bytes, and a host-only one
  would need `#ifdef HOST_BUILD` (operator's call). Where the wrong
  mood value goes from there was not traced.

**CFI** (`build-x64-cfi`, the i686 run's day list): 114 call sites,
against 113 for an i686 CFI build from the same tree (`build-i686-cfi`,
same list), and 115 in `mistyped-calls.md`'s table (an older tree).

| kind | x86_64 | i686 (same tree) |
|---|---|---|
| missing-args | 4 | 4 |
| lost-return | 37 | 39 |
| int-pointer | 4 | 5 |
| width | 4 | 4 |
| unprototyped | 22 | 24 |
| extra-args | 27 | 27 |
| return-ignored, sign, long-pointer | 11 | 10 |
| other (callee not resolved by the triage) | 5 | 0 |

- The site lists match except for: `placement_grid.c:101` is extra-args
  at x86_64 and int-pointer at i686 (host-64-bit's slot returns `long`,
  which the triage counts as a pointer: task 10's fix shows);
  five sites where gdb couldn't name an inlined callee at x86_64
  (`other`; the same sites are lost-return, unprototyped or extra-args at
  i686); `entity.c:2981` (i686 only) and `entity.c:1428` (x86_64 only),
  from random walking.
- No int-pointer or lost-return site at x86_64 that isn't at i686. The two
  lost-returns that mattered (ModelData, TriggerWorld Load) don't appear.
- int-pointer at both widths: `scene_node.c:113` (known, harmless) and
  `TodActor__ApplyTodFrame` (entity.c:2775, tod_actor.c:337, 395): the slot
  passes `s32 flag`, the function takes `void *extra`. Harmless: every
  caller passes 0, and `extra` only goes on to `ApplyTodPacket`, which
  never reads it.
- Pitfall: piping `cfi_triage.py` through `tee … | head` cuts its output
  short at the head's limit without an error; write it to a file.

## C. The year's end

Lockstep pair, `"days": [364, null]` (the doc's 363 is the second-last
day: `currentDay` runs 0..364, and the ASan run went 363, 364, 0).
Same at both widths: every STATE and SAVEBLK line, through the wrap.

1. Day 364 plays as any other (stages 3, 5, 3; ends at its timer, tick
   3628).
2. Post-day chart; circle closes it, and **the ending movie** plays
   (Mt. Fuji, the LSD logo, a line-drawing animation, "...Dream."),
   80 to 100 seconds of wall time.
3. The title menu comes back at **Day 001**. Left alone, it times out into
   the intro movies as usual.
4. START: **day 0 of the next year** starts (SAVEBLK day=0, equal at
   both widths), plays, and the menu then shows Day 002.

So both happen: the ending, then a wrap to day 0. (Day 0 of year 1 is
where B's `moodPreviousDays[-1]` read happens.) A first attempt with day
363 never got a second day, because its START landed during a chart or
menu transition: a fault in that config, not the game.

## D. Two uninitialized arguments: not fixable byte-exactly

Neither lands; `host-64-bit-soak` has no commits, and `LSD_CFI` keeps
`-Xclang -no-enable-noundef-analysis`.

- `p` in `TmdModel__RaycastFaces`: retail never sets it (it lives in
  `s1`). `p = NULL` in five places (three of them also with the
  declarations in four orders), or `count == 0 ? NULL : p` as the
  argument: every variant keeps the length but fills a delay slot retail
  leaves as `nop` and swaps `s0`/`s1` through the function (11–12
  instructions in each variant measured).
- `junk` in `StageMap__SetFootprintFromQuery`: retail passes `s1`
  unset, the caller's value. `junk = 0` in three places: 12 instructions
  differ each time.

Written up in `docs/research/mistyped-calls.md`. A host-only
initialization (`#ifdef HOST_BUILD`) would let the flag go; that's the
operator's call.

## E. Loose ends from task 10

- **Flowers**: things move while the dream clock is frozen. In one
  x86_64 run (task 10's four-day sequence with three shots a second apart
  in each freeze), a creature walks, a sparkle turns and a fade carries on
  between shots of the same freeze. These animations run on frame time,
  so a freeze shot's content depends on when it's taken: the flower
  difference is timing, not the game. (The exact flower scene didn't
  recur: longer freezes change the path, see A.)
- **GRAPH**: opened at x86_64 from a new game's first menu (Day 001, no
  FLASHBACK yet: START, SAVE, LOAD, GRAPH; three downs and circle). It
  shows the empty dream chart; cross doesn't leave it (coverage_run uses
  triangle). Before any day is played, so it can't be the post-day chart.
  The CFI and ASan runs also opened GRAPH after days had been played.

## Open

- `moodPreviousDays[-1]` on day 0 of year 1: retail bug, unfixed.
- `p` and `junk`: unfixable byte-exactly; a host-only init needs approval.
- Not driven at 64 bits: every stage's every link, the gamepad, real
  Windows, macOS/ARM64.

## How task 11 worked

- The scratchpad worktree of lsddecomp was made on a new branch
  (`git worktree add -b host-64-bit-soak <dir> host-64-bit`) because
  task 10's worktree still has `host-64-bit` checked out.
- Lockstep pairs ran two at a time alongside a coverage run and finished
  faster than the doc expected: a four-day pair took 3 to 11 minutes,
  intro included, and the whole of A about 32.
- coverage_run writes raw audio (`cov.raw`, 170–250 MB a run): delete it
  once the run is done.
- Old `build-i686/lsd --frames …` processes from three days earlier are
  still running on this machine; not this task's, left alone.
