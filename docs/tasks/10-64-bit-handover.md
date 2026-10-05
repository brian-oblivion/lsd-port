# Task 10: 64-bit — where it ended

Handover from task 10 (`10-64-bit.md`). The design write-up is in
`docs/design.md` ("64-bit clean (task 10)"), the decisions in `PLAN.md`.

## Where things stand (2026-10-05)

- **lsddecomp** `host-64-bit`, 18 commits on `main` `1d4aa8be7`, each one
  byte-exact (`./build-and-verify.sh`) and lint-clean (`tools/lint.sh`) on
  its own:
  1. `bb8565dd3` bmem_pmgr, grid_cell: sizes from the pointer width
  2. `d8a824194` tmd: lists through `TMD_LIST_ADDR` (`#ifdef HOST_BUILD`)
  3. `1df62bcfc` integers that carry an address: `intptr_t`
  4. `14a295598` libgs mirrors: attribute and tmd as libgs types
  5. `fb4c674f9` SDK buffers: `u32 *` cast to `u_long *`
  6. `537edb437` style_layer: scene references as pointers
  7. `afa9cc133` addresses returned or kept as `s32`: pointer types
  8. `33349fde6` arguments typed for what they carry
  9. `5f9a52a2e` tim_image: step past the TIM's id word, not a `u_long`
  10. `45f5408a4` tod_set, trigger_world: `SUBBLOCK_OBJ` (`#ifdef HOST_BUILD`)
  11. `e06f47df1` task_objf: event callbacks with libapi's type
  12. `b0fb4083d` stage_map: SplitCoord2's prefix as GsCOORDINATE2's types
  13. `2d7322533` ModelData, TriggerWorld: Load returns its result
  14. `5768a2f88` static asserts (`COMPILE_ASSERT`)
  15. `cf8588b7c` main: the host pool grows with the pointer width
  16. `4cf46ad26` viewport: the packet area scales with the primitive size
  17. `ea2f2385e` tmd: `u_long *` for GsMapModelingData (Windows x86_64)
  18. `090be31e1` data_source: cast SetNullDriverMode
  (One hunk of 7 landed in 8: `TryDreamAuxTrigger`'s return.)
- **psyz fork**: three topic branches, one upstream PR each, merged into
  `lsd-64-bit` (fork `main` `597cd85` plus the three):
  - `libgs-tmd-offsets` `3564361` (on `libgs-3d`): GsMapModelingData maps
    to offsets from each object's entry, `GsTMDAddr`; tests updated (no
    more 4 GB skips).
  - `gpu-queue-full-keep-packet` `f55caeb` (on upstream `afed8f3`): the
    packet that filled GPU_Enqueue's queue was dropped. Upstream bug.
  - `gpu-drain-at-vsync` `c4bd824` (on upstream `afed8f3`): the 64-bit queue
    is drawn before a frame is presented. Upstream bug.
- **lsd-port** `task-10-64-bit`: `decomp/` at `090be31e1`, `psyz/` at
  `56aac8c`; CMake drops the `-Wno-error` downgrades (no site left); CI
  fails above 0 pointer-width warnings; `tools/lockstep.py` and
  `tools/lockstep_gdb.py`; README, PLAN, design.
- **Not pushed** anywhere yet; the pins must be on their remotes before
  CI on this branch can check them out.

## Results

- **x86_64 Linux**: boots, menu, whole days, links, flashbacks, a
  special day's movie, the post-day graph, SAVE, LOAD, music, the intro
  movie, widescreen. The GRAPH menu item itself was not opened. In lockstep with i686
  (same seed and pad input per tick) the game state matches on every tick
  compared and the screenshots match (one exception, `docs/design.md`).
  Release build: same dream speed as i686.
- **i686**: unchanged; STATE lines and screenshots equal `main`'s build.
- **Windows x86_64** (MinGW, under Wine with xvfb): builds without width
  warnings, plays the intro and a dream at 20 ticks a second, and LOADs a
  card saved by Linux x86_64.
- **Sanitizer** (`-DLSD_SANITIZE=address,bounds`, x86_64): a whole first
  day and day 200 report only task 08's harmless `&coord.m[3][0]`.
- **psyz tests**: x86_64 319 passed, 0 failed, 0 skipped; i686 318 passed,
  1 failed (`gte::read_rot_matrix...`, pre-existing).
- **Warnings**: pointer-width 0 at both widths (was 71); the game's C has
  two warnings left at every width, both calls through a cast function
  type that are harmless (dead code in scene_node.c, SsSetMute's result).
- **Cards**: the same day at both widths writes the same card but for 29
  reserved title bytes (pool leftovers, at every width); each width loads
  the other's card to the same save block.

## Open

- **Review and push**: lsddecomp `host-64-bit` to a reviewer, then push
  and merge; the psyz topic branches, then their upstream PRs (the
  operator's call); then lsd-port's pins and `main`.
- **64-bit release default** (*decision*): `LSD_ARCH` stays i686, and the
  release workflow builds i686 only. Switching would drop the multilib
  requirement on Linux.
- **macOS, ARM64 Linux**: not built. Nothing width-specific is known to be
  left; ARM64 would also test unaligned and strict-aliasing assumptions
  x86 tolerates.
- **Not driven at 64 bits**: the ending, a year's wrap, every stage's every
  link, the gamepad, real Windows.
- **Flowers**: the one differing freeze shot (spinning flowers, day 200 of
  a four-day run) was not tracked down.

## How task 10 worked (reuse it)

- **Lockstep** (`tools/lockstep.py`, docstrings): two Debug builds under
  gdb, rand() seeded at StartDay, the pad driven from a tick-indexed script
  through `Pad__DispatchEvents`, the dream FrameClock frozen at listed
  ticks for screenshots. Diff the STATE lines; the first differing tick
  narrows a 64-bit bug to one frame, and its PROBE option logs the return
  values of chosen functions there. This found SplitCoord2 (tick 73),
  psyz's GPU queue, and the packet-area overflow.
- The debug server's `/vram` reads back unreliably in the middle of a run;
  compare screenshots, uploads (break on LoadImage and hash the data) or
  the ordering table instead.
- Pool and packet use: lockstep_gdb's POOL and PKT lines.
- gdb: an inferior call (`call ...`) from a Python breakpoint's stop()
  hung the game; set variables only.
- `/tmp` has a per-user quota: when it is near, commands' output is lost
  (writes fail). Keep runs' raw audio off unless measuring sound
  (lockstep's default is SDL's dummy driver).
- The lsddecomp checkout in `~/git/lsddecomp` was switched to `main` by
  another session mid-task; task 10 then worked in a git worktree of
  `host-64-bit` (untracked tool and data directories symlinked in, then
  `make extract`).
- Rules unchanged.
