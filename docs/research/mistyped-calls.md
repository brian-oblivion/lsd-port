# Calls through mistyped function pointers (2026-10-05)

The game's C calls almost every method through a table slot whose type
differs from the function in it. Compilers don't warn about that, since
the cast is written out. This records what a Clang build that checks
every indirect call found at i686 (lsd-port `main` `27f0b5a`, lsddecomp
`f3b9ccbc4`, psyz `597cd85`), which of it matters, and how to run it again.

## The build

`-fsanitize=function` was tried first and is not usable here: it compares
exact types, so a slot typed `void (*)(TitleMenu *, ...)` holding
`TaskCore__SetSlotCursor(TaskCore *, ...)` reports. That is the game's C
inheritance, and it was all of the first run's 155 reports. Each call site
reports only once, and a suppression still uses up the site, so a site
whose first call is such a pair could never report a real mismatch.
`-fsanitize-cfi-icall-generalize-pointers` (counting all pointer types as
one) is ignored for `-fsanitize=function` in Clang 22.

So the build is Clang's CFI indirect-call check with pointer types
generalized: `-DLSD_CFI=ON` (CMakeLists.txt). It needs Clang, ThinLTO
and lld, and covers the game's C and all of psyz:

    CC=clang cmake -S . -B build-i686-cfi -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSD_CFI=ON
    ninja -C build-i686-cfi lsd
    LSD_DISC=/abs/path.cue COV_DAYS=0,1,10,14,50,42,100,119,200,201,300,357,363,6,20 \
        tools/coverage_run.py /tmp/runs/cov build-i686-cfi/lsd 7795 15
    tools/cfi_triage.py build-i686-cfi/lsd /tmp/runs/cov/cov.log

`tools/cfi_triage.py` takes each report's slot type and the callee's
type from the debug info and sorts by what differs. `tools/coverage_run.py`
drives a headless run under gdb: the intro, 15 days (6 of them set to
special days), random walking in each, links, GRAPH, FLASHBACK and SAVE.
The game runs at full speed under the check.

## A Clang build crashed when the player walked

Not a mismatched call, but it stopped every Clang build: in
`TmdModel__RaycastFaces` (tmd_model.c:268) `p` is passed to
`TmdModel__NextPrimitive` before it is set. NextPrimitive ignores it on
that first call (`*count == 0`), so GCC's builds and the PS1 are fine.
Clang marks arguments `noundef`, so passing an uninitialized value is
undefined, and Clang compiled the whole function to its first two
instructions. Execution ran on into whatever followed, and walking
(`SceneNode__RaycastVertical`, ground contact) ended in a trap. Any
Clang build of the game has this (macOS, for example). `LSD_CFI` builds
with `-Xclang -no-enable-noundef-analysis` until lsddecomp sets `p`.

`-Wuninitialized -Wsometimes-uninitialized` on the game's C finds one
more argument passed uninitialized, `junk` in
`StageMap__SetFootprintFromQuery` (stage_map.c:1416). Its callee is
inlined and ignores it, so no code was lost there. Clang's other
warnings (bg_layer.c:116 and 125, tmd_model.c:612, task_objf.c:626 and
800, stage_map.c:1268) are on paths the code's own comments say can't
happen.

Neither can be set without changing the PS1 bytes (task 11). Retail never
initializes them: `p` lives in `s1`, and any assignment (`p = NULL` in
five places, three of them also with the declarations in four orders,
or `count == 0 ? NULL : p` as the argument) fills a
delay slot that retail leaves as a `nop` and swaps `s0` and `s1`
through the function (11 or 12 instructions differ, same length);
`junk` is `s1` passed on unset, the caller's value, and `junk = 0`
changes 12 instructions. So the noundef flag stays; a host-only
initialization would need `#ifdef HOST_BUILD`, which is the operator's
call.

## What the runs reported: 115 call sites

| kind | sites | |
|---|---|---|
| missing-args | 4 | all `TextRow__SetDisplay`, harmless |
| lost-return | 41 | 2 matter (below) |
| int-pointer | 12 | 3 truncate at 64 bits (below) |
| width | 4 | harmless |
| unprototyped | 17 | harmless |
| extra-args | 27 | harmless on x86 |
| return-ignored, sign, long-pointer | 10 | harmless |

### Matters now: a void callee whose result is tested

`ModelData__ModelData` (model_data.c:40) and `TriggerWorld__TriggerWorld`
(trigger_world.c:35), the ctors, call `onRequestDone` through `s32 (*)()` and fail
the ctor when it returns nonzero. The occupants, `ModelData__Load` and
`TriggerWorld__Load`, are declared `void`, but their last statement calls
`processBuffer` (`BuildResources`, 1 when it fails). On the PS1 that
result is still in `$v0` on return, so the functions really return it. On
the PC it works because nothing clobbers `eax` after the call, which no
compiler promises (inlining under LTO, for one). Fix in lsddecomp: return
`s32` and `return` the call; the MIPS code should be unchanged.

### Matters at 64 bits: pointers through int slots

Each of these is outside task 10's 71 cast warnings, because the
truncation happens in a function pointer's return type:

- `PlacementGrid__ResolveEntry` (placement_grid.c:100) returns
  `LinkResource__GetModel`'s `TmdModel *` through
  `PlacementGridGetModelFn`, which returns `s32`; `StageMap`
  (stage_map.c:981, 1000) stores it back as `(void *)model`. Every map
  cell's model would be truncated.
- `style_effect.c:236` and `:523` call `LinkResource__GetModel` through
  the `setBackClip` slot (`int`), and 523 casts the result back to
  `TmdModel *`.
- `UnprototypedCtorTable`'s `ctor` returns `s32`, but the ctors behind it
  (`LinkResource`, `ModelData`, `TodSet`, `TriggerWorld`) return the
  object. Only tested for zero, so it fails only when the low 32 bits of
  the address are zero; `void *` would be right at both widths.

`scene_node.c:113` passes `sender` (a pointer) into `DreamSys__OnPadEvent`'s
`s32`, which never reads it; harmless.

### Harmless

- **missing-args**: `TextRow__SetDisplay(self, on, result)` is called with
  two arguments (task_core.c:801, 807, 859, 868). `result` is a decomp
  artifact: the PS1 code returns `$a2` when the loop doesn't run. It only
  feeds the return value, which no caller reads.
- **lost-return**, the other 39: ctors (`void`) called through
  `void *(*)(...)` slots, and `attachToParent`/`detachFromParent` (`void`)
  through `SceneNode *` slots; every site is a statement and doesn't read
  the value. `VabStreamObj__Unmute` stores `SsSetMute`'s non-result, and
  `unmute`'s one caller ignores it. `StageMap__SetTargetAndLoadChunks`
  returns `LoadChunksAround`'s, and its one caller ignores it.
- **width**: `DrawSystem__MoveImage`'s `s16` coordinates passed as `int`
  (tim_image.c:136, 141, 146; the values fit). `TodSet__ScanPackets`'s
  `u8` read as `int` by `ModelData__ForwardScanPackets`, which returns
  `u8` itself.
- **unprototyped**: `s32 (*)()` calls in the game, and libsnd's MIDI
  handlers in psyz's decomp, which take what they're passed.
- **extra-args**: base methods taking fewer arguments than the slot
  (`TaskCore__OnInit`, `TitleMenu__Reset`, `SetNullDriverMode` at
  data_source.c:163, already commented there).
- psyz's `GPU_DataWrite` through `int (*)(u_long, u_long)`: psyz's
  `u_long` is pointer-sized on every target it builds for, Win64 included.

## Direct calls

CFI sees only indirect calls. For direct calls, a GCC LTO build with
`-Wlto-type-mismatch` compares each declaration with its definition across
files: none differ in the game's C. psyz has two: libcd's `CD_status`
declared `u_char` in `decomp/src/libcd/sys.c` against `int` in
`src/psyz/libcd.c` (reads the low byte; right on little-endian), and
`CD_getsector2`'s parameters (a `NOT_IMPLEMENTED` stub).

## x86_64

`-DLSD_ARCH=x86_64 -DLSD_CFI=ON` builds. It reaches task 10's
`BMemPMgrAlloc` crash with two reports before it, neither the cause;
past that crash it is the check to run, for the int-pointer kind above.

## Not covered

The runs see only the calls they make: not the ending, LOAD's file list,
every stage's links, or the gamepad. The report count per kind grows
with coverage; 40 sites after one day, 115 after 15.
