# lsd-port design

The decisions in PLAN.md's "Decided" list are not repeated here. This file
holds the measurements behind the open ones, with a recommendation each.

## Pointer width: 32-bit first (*decided 2026-10-02*)

The operator took the recommendation below. Superseded on 2026-10-05:
once task 10 made x86_64 play as i686 does and task 11's soak found no
difference, x86_64 became the default build and the release; i686 stays in
CI.

Measured 2026-10-02 on the game's C as it compiles for the host
(task 01: lsddecomp `e5daad5c3`, psyz fork `main` `8030744`), GCC 16.2,
Arch Linux.

### What 64-bit costs

**Pointers and integers.** Built 64-bit (x86_64), the game's C gives 71
warnings where an address passes through a 32-bit integer
(`-Wpointer-to-int-cast` 29, `-Wint-to-pointer-cast` 42), in 16 files
(75 when first measured; lsddecomp `5993e36f7` fixed StyleLayer's spawn
colours). StyleLayer has 29 of them: it keeps its scene references (`sStyleSceneRefs`,
`sStyleDecorObj`, `sStyleGrid`) as `s32` and casts them back at each use.
The rest are one or a few per file: method arguments typed `s32` that carry
an object (`DayTask`'s `init`, `ObjM`'s `RegisterStyleConfig` and
`TryDreamAuxTrigger`, TodActor's `setDisplay`), the CD driver's request
queue (`param0` is a buffer address), the viewport's OT buffer, model lists
kept as `s32` arrays (TriggerWorld, DreamAux). Each is a real truncation at
64 bits. Most are a type change in a header plus the casts at its uses;
none was attempted beyond StyleConfig, whose static initializer did not
compile at all (lsddecomp `751c5a592`), and the spawn colours that review
caught.

**Layouts.** Every class's layout moves: of 38 classes whose PS1 object
size lsddecomp documents, none has it on x86_64 (`tools/class-sizes.py`;
for example Viewport 0xBC becomes 0x100). Matching member names against
the `/* +0xNNN */` offsets in lsddecomp's headers, about 3000 of 3300
members land elsewhere, in 150 of 228 structures. This is harmless while
the code reaches every field by name and sizes every allocation with
`sizeof`, and almost all of it does (59 of the 101 `BMemPMgrAlloc` calls
take a `sizeof`, the rest byte buffers). The exceptions are where a layout
is a file's:

- **TMD data relocated in place.** `GsMapModelingData` turns the offsets in
  a TMD's object table into addresses, in the file's own 32-bit words.
  The game then reads that table as `TmdObject` (pointer fields) and as
  libgs's `TMD_STRUCT` (`u_long *` fields): TmdRenderer walks `primtop` and
  `vertop` directly. At 64 bits the words cannot hold an address and both
  structures misread the file. psyz's libgs (`GsMapModelingData`,
  `GsLinkObject4`, `GsSortObject4`, none implemented yet) and the game's
  TMD code would both need another representation, such as offsets
  resolved at each use.
- **Hand-sized objects.** `GRIDCELL_SIZE` (60) allocates every GridCell;
  at 64 bits the object no longer fits it. A method table reached through
  a hand-padded view did the same: `UnprototypedCtorTable` skipped eight
  bytes to the ctor slot, so at 64 bits `New_LinkResource` called the
  release slot and crashed at startup (fixed in lsddecomp `e5daad5c3`;
  others of the kind may remain).
- **The heap.** BMemPMgr's one pool is 1435 KiB, sized for the PS1's
  objects. Larger objects make it fill sooner; the pool size would have to
  grow with the width.

**Other.** 14 calls pass a `u32 *` buffer where the SDK takes `u_long *`
(`LoadImage`, `StoreImage`, `DecDCTin/out/vlc`, `StSetRing`, `StGetNext`,
`StFreeRing`); psyz's `u_long` is pointer-sized, so at 64 bits these are
type errors waiting for the function bodies to read 8-byte words. On
Windows (LLP64) three casts to `unsigned long *` add to them.

### What 32-bit costs

Built with `-m32` (i686), the same C has 0 errors and 17 warnings, none of
them about pointer width: the 14 `u32 *`/`u_long *` mismatches above
(`u_long` is `unsigned long` there, the same width but another type), two
calls through a cast function type, and one integer passed as a pointer
(`Entity__GetOrCreateFadeBox`'s `step`, 10; a warning on the PS1 too). All
38 classes keep their PS1 size; the one the tool reports, GridCell, is
0x44 on the PS1 as well (its header notes that `sizeof` overstates the
allocation by a trailing pad). The TMD tables, the hand sizes and the pool
work as they do on the console. psyz and the static SDL3 build for i686
too, and `lsd` runs offscreen to the same point as on x86_64, its first
file read (`host-link-surface.md`).

The cost is in the toolchain and the platforms:

- **Linux.** It needs the 32-bit C library and, for a window, the 32-bit
  X11/Wayland, audio and Vulkan or GL libraries. Arch has them in
  `multilib` (`lib32-glibc`, `lib32-gcc-libs`, `lib32-libx11`,
  `lib32-vulkan-*`); this machine had them already, as most Steam machines
  do. SDL3 loads those libraries at run time, so building needs only the
  headers and the 32-bit link libraries its configure step probes.
- **CI.** Ubuntu needs `gcc-multilib` and the i386 development packages
  SDL3 configures against (multiarch `apt` with `:i386`), or an SDL3
  configured without X11 and Wayland for the headless smoke test.
- **Windows.** `i686-w64-mingw32` exists on Ubuntu (`gcc-mingw-w64-i686`),
  and 32-bit programs run on 64-bit Windows. Windows on ARM runs them
  emulated.
- **Elsewhere.** macOS has no 32-bit at all, and arm64 Linux has no i686.
  A Mac or ARM build would have to wait for 64-bit.

### Recommendation

**32-bit first**, with the 64-bit build kept compiling. The game's C
already compiles both ways; what 64-bit adds is a redesign of how libgs and
the game treat TMD data, the hand sizes, the heap, and some 71 typed-as-
`s32` addresses, each before the game can run correctly, and none of it
testable until psyz's libgs draws. At 32 bits the game's data layouts are
the console's, so the platform layer can be brought up against the same
memory picture the decompilation describes, and every difference from the
PS1 is psyz's to explain.

Concretely:

1. The port's default build is i686 (`-m32` on Linux, `i686-w64-mingw32`
   on Windows). CI adds the multilib packages. *Done*: `LSD_ARCH`
   (default `i686`) picks `cmake/linux-i686.cmake`; Windows i686 is
   `cmake/windows-i686.cmake`.
2. CI also builds x86_64 and records the width warnings (71 today), so
   the count can only go down. *Done*: the Linux and MinGW jobs build
   both widths, and the Linux job writes the count to the run summary. Fixes that are correct at both widths and
   byte-exact on the PS1, like the StyleConfig change, land in lsddecomp
   as they are found.
3. PLAN.md's "Later: 64-bit clean" becomes a measured list: the 71
   sites, `GRIDCELL_SIZE`, the pool, and TMD relocation (in psyz and in
   the game). Static asserts on the layouts the PS1 data fixes
   (`TmdObject`, the TMD header, the memory-card header) come first, so
   that 64-bit work cannot silently break them.

The case for 64-bit from the start is that psyz and sotn-decomp's PC
build are 64-bit first, that a Mac build would need it, and that the
fixes get no cheaper later. If the operator wants a Mac build early,
64-bit should come first; otherwise it costs the most at the point where
nothing can test it yet.

### Reproducing

```sh
# 32-bit and 64-bit (the default since 2026-10-05)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSD_ARCH=i686
cmake -S . -B build-x86_64 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLSD_ARCH=x86_64
ninja -C build-x86_64 -t clean lsd_game && ninja -C build-x86_64 lsd_game 2>&1 |
    grep -c 'pointer-to-int-cast\|int-to-pointer-cast'
tools/class-sizes.py build
```

## 64-bit clean (task 10, 2026-10-05)

The x86_64 build now plays as i686 does. Measured with the two Debug
builds in lockstep (`tools/lockstep.py`: the same rand() seed at each
StartDay and the same pad input on the same dream ticks): every STATE line
(tick, stage, position, rotation, rand state) is identical, over a first
day with links through five stages, days 10, 100, 200 and 300 (played in
one session, and day 200 as the first), a flashback session, SAVE and LOAD,
and with `--aspect 16:9 --resolution 2`; so is every freeze screenshot but
one, where two spinning flowers stand at different angles (the shot is
taken by wall clock while the dream clock is stopped, and task 11 found
creatures, sparkles and fades still moving during a freeze: they run on
frame time, so the shot's timing decides what it catches).
The i686 build is unchanged: its STATE lines and screenshots equal those of
`main`'s i686 build. Release builds run the dream at the same speed (about
415 frames a second uncapped at both widths).

What it took, by kind (lsddecomp `host-64-bit`, psyz fork `lsd-64-bit`;
every lsddecomp commit byte-exact on the PS1):

- **Sizes written as the PS1's numbers**: BMemPMgr's pool header (28) and
  block layout (header word, links, footer), GridCell's allocation (60),
  the host pool (now 4 MB per 4-byte pointer: 1.4 MB peak at i686, 1.9 to
  2.1 MB at x86_64) and the draw buffers' packet areas (now scaled by
  `(sizeof(P_TAG) + 7) / 8`: psyz's tag is two words; day 200 used 82552
  bytes at x86_64 where the console's budget is 76800).
- **Addresses in 32-bit integers**: the 71 warnings, now 0 (CI fails above
  0). Most became pointer types (StyleLayer's scene references, DreamSys's
  link rotations, RegisterStyleConfig, TryDreamAuxTrigger, PlacementGrid's
  model); integer slots that carry either an address or a number became
  `intptr_t` (the CD queue's param0, clutBase). Some were not warned about:
  calls through cast function types (PlacementGrid's getModel returned
  `s32`), a descriptor built as `s32[4]`, an object passed as an `s32`
  argument. An audit of all 58 method tables (2146 slots) and the casts
  found no other live truncation.
- **Views of libgs's structs with PS1 types**: SceneNode's GsDOBJ2,
  SpriteGs, BoxFill and BgLayer had `u32 attribute` where libgs has
  `u_long` (8 bytes in psyz at 64 bits); SplitCoord2 skipped GsCOORDINATE2's
  `flg` as 4 bytes, which made every chunk origin read 4 bytes early and let
  the player walk through floors (the first divergence the lockstep runs
  found, at tick 73). `(unsigned long *)buffer + 1` stepped 8 bytes past a
  TIM's id word.
- **File data that receives addresses in its own 32-bit words** (needed
  `#ifdef HOST_BUILD`, approved by the operator): see below.
- **psyz at 64 bits**: GPU_Enqueue queues packets on a 64-bit host and
  dropped the packet that filled the queue; and the queue was drawn only on
  DrawSync, which this game never calls per frame, so frames were drawn in
  lumps (`gpu-queue-full-keep-packet`, `gpu-drain-at-vsync`, both upstream
  bugs).

### TMD data and sub-block tables: offsets on every host (*decided*)

`GsMapModelingData` turns a TMD object table's offsets into addresses in
the file's own words, and TodSet and TriggerWorld replace each offset in
their sub-block table with the object built over it. A 64-bit address does
not fit. The options were a separate pointer-sized table (the game reaches
the table through the file in several places, so each would need a lookup),
keeping offsets and resolving them at each use, or a host-only parallel
structure. The choice: keep 32-bit offsets on every host, resolved at
each use, at both widths so that i686 runs the same code:

- TMD: psyz's `GsMapModelingData` maps the offsets to offsets from each
  object's own table entry, read with `GsTMDAddr` (psyz `libgs-tmd-offsets`;
  it previously refused TMDs above 4 GB, which is every TMD on macOS and
  most on 64-bit Linux). The game reads the three lists through
  `TMD_LIST_ADDR` (`tmd_model.h`), an address on the PS1. The readers are
  few (TmdModel's bounds, hull and raycast, SortTmdObject, GsLinkObject4),
  and the table keeps the file's layout, which a static assert pins.
- Sub-blocks: `SUBBLOCK_SET_OBJ` / `SUBBLOCK_OBJ` (`tod_set.h`) store the
  object's offset from the table word (both live in the BMemPMgr pool; 0 is
  NULL), an address on the PS1; `ReleaseSubBlockObjects` replaces
  ReleaseBasicClassArray there on a host.

Pointer compression (all objects in the low 4 GB) was rejected: macOS maps
nothing below 4 GB for a 64-bit process.

### Static asserts

`COMPILE_ASSERT` (lsddecomp `common.h`) pins, on the PS1 and every host:
TmdObject and TmdFile, TOD files, frames and packets, the sub-block table,
LBD headers, placement records, the TIM block and TIM array headers, the
MOM header, the memory card's header and icon TIM, DreamSys's saved range
(0x700 bytes from saveMagic, laid out as DreamSaveBlock), and the views
handed to libgs as its own structs, against libgs's layout.

### Memory cards

The saved range of DreamSys has the same layout at both widths, so cards
are exchangeable: after the same day in lockstep the two widths write the
same file except bytes 67 to 95 (the title field's reserved tail, filled
from whatever the pool held, at every width and on the console too), and
each width loads the other's card to the same save block. Windows x86_64,
under Wine, loads a card written by Linux x86_64.

### What is left

- 64-bit is the release default since 2026-10-05 (the operator's call,
  after task 11). A macOS or ARM64 build starts from it; neither has been
  built yet.
- Not run at 64 bits: real Windows. (Task 11 ran the ending and a year's
  wrap, `docs/tasks/11-64-bit-soak-handover.md`.)

## Pace and smooth (task 12, 2026-10-06)

Two settings (README, "Pace"): `pace = N`, the dream's ticks a second,
and `smooth = on`, frames drawn between them; 14 and on by default since
2026-10-06, the operator's choice after playing them. Both live in `src/pacing.c`, which, like `widescreen.c`,
changes method tables rather than lsddecomp: it replaces
`gDrawSystemMethods.runLoop` and wraps DayTask's `onInit` and `onDeinit`
to know when a dream runs. At `pace = 20, smooth = off` it installs
nothing: the game as its code runs it.

### How the game paces itself

Everything, menus and movies too, runs in `DrawSystem__RunLoop`:
`VSync(3)`, the DrawSystem's callback, then `notifyParents(VSYNC)`. In a
dream one pass is, in order: the Viewport's flip (it draws the ordering
table the last pass built and swaps the display), the dream FrameClock's
tick, which first has the Viewport build the next table (`Viewport__Update`
draws the world as the last tick left it) and then runs the dream's logic
(`DreamSys__TimerTick`, the entities), then the pad's dispatch. So the
picture a pass builds is one tick behind the logic of that pass, and
`VSync(3)` gives one tick per three blanks: 19.98 a second at 59.94 Hz.

psyz's `VSync(n)` presents, waits n blanks, reads the pads and runs the
`VSyncCallback` functions n times. In a dream that is the CD driver's
service (`ServiceCdDriver`); the music is not among them. psyz runs
libsnd's sequencer (`SsSeqCalledTbyT`) from the audio thread, through its
root-counter emulation (`audio_callback` → `Psyz_SpuPullSamples` →
`Psyz_RcntAdd` → `_SsTrapIntrVSync`), so the music keeps its tempo
whatever the game's loop does.

### Pace

In a dream, `PacedRunLoop` makes one pass per blank: `Psyz_VideoVSync(0)`
presents and waits one blank, and every 60 / pace blanks (an integer
phase: `pace` added per blank, a tick at 60) the pass is a tick: the psyz
fork's `Psyz_VSyncRunCallbacks(3)` (what `VSync(3)` does once its wait is
over: the pads, the debug server's hook once, the VSyncCallbacks three
times), then RunLoop's callback and notification. Each tick sees exactly
what it sees at 20, the CD service three times included, only further
apart, so a pace changes nothing but time. At a pace that doesn't divide
60 the ticks come every 4 or 5 blanks (14: 13.99 a second).

The debug server's frames (`/input`) and `--frames` stay ticks. Outside a
dream (the title menu, the graph, movies, the diary) the loop is RunLoop's.

### Smooth

On the blanks between ticks `DrawInBetween` draws a frame as a tick's
pass does, `flip` then `update`, with the world placed i / n of the way
from where the last tick drew it to where its logic has put it since, n
being the passes from the last tick to the next (even steps, so a tick that
comes a blank late doesn't show).

- What it blends: before each tick (`RecordDrawn`) the pose (coord.t,
  `param->rotate`, `param->scale` and the parent coordinate) of every node
  in the trees `Viewport__Update` draws, about 2900 in a dream, most of
  them grid cells, which are skipped outside a scale ramp (below). Between
  ticks every node whose pose has changed is blended, the rotation as an
  angle the short way round, and marked dirty (`flg = 0`) so
  `Viewport__DrawNode` rebuilds its matrix from `param`, as every mover in
  the game's code already does. The camera is DreamSys's coordinate (the
  view's parent), so it is one of them. A TodActor's parts are blended as
  well (task 15; before, their TOD animation kept the tick rate, as on the
  console): `ApplyTodPacket` writes the same `param->rotate`,
  `param->scale` and coord.t, and a parent packet that moves a part under
  another is a jump. GridCells are blended in scale only, and only while
  the StageMap's scale ramp runs (the ground rising or sinking, 1/64 or
  1/4 a tick; its `scaleRampTicks` non-zero when the tick is recorded;
  recording all ~2800 cells every tick cost 50 to 100 µs a frame); their
  moves are not, since the StageMap moves them by whole cells (2048, with
  quarter turns) to reuse them on the other side as the player walks.
- Jumps: a node that moved more than 4096 along an axis or 45 degrees in
  one tick, changed a scale by more than 1.0 or changed parent is drawn
  where the tick drew it; when that node is DreamSys the
  frame isn't drawn at all, so the tick's own picture stays up. A
  threshold rather than DreamSys's link state, because it catches links,
  respawns and anything else that teleports, objects too. Measured over
  four days of every tick: walking moves DreamSys up to ~130 a tick, some
  stages carry it 256 or 512 a tick for a while, a turn is 68 (6
  degrees), and every link or respawn was 9824 or more.
- The game's state: the draw writes every drawn node's `GsCOORDINATE2`
  (matrix, `workm` cache, `flg`) and psyz's libgs globals. `BlendTree`
  copies every coordinate before the draw and `Restore` puts them and
  the blended rotations and scales back, so the logic finds what it left; libgs's
  frame counter (PSDCNT, which only tells caches apart), the display
  buffer and the ordering tables move on, as they do every frame. The draw
  steps no animation, timer or `rand()`: the lockstep runs below show it.
- Cached matrices (task 18): an in-between frame marks changed (`flg =
  0`) every coordinate the tick's draw computed, its `flg` the tick's
  frame stamp (libgs's PSDCNT, read by computing a root coordinate of our
  own: `FrameStamp`), so that it computes them again rather than reuse
  their `workm`. The game's `SortTmdObject` multiplies an object's `workm`
  by its parent's in place once it has its matrices; with one draw a
  tick, the next tick's logic marks every moving model changed before it
  is drawn again (`TodActor__Tick` marks an Entity, whose parts then follow),
  so the multiplied matrix is never read back. Drawn between ticks
  without that, an Entity's parts got their parent's rotation and scale
  twice: the fish at Natural World (an Entity at scale 6.0), torn into a
  flat sheet in every frame but the tick's. A coordinate the tick's draw
  took from the cache is left to the cache, as the console's next draw
  would.
- 2D drawn in the dream (fades, the pause text, sprites) is in the same
  trees and is redrawn as it is. Things the game moves on frame time (the
  DrawSystem's VSYNC event: fades, sparkles) still move once a tick.
- Frames are presented at psyz's 59.94 Hz, the rate `VSync` counts in,
  unless `frame_rate` says otherwise (below).

### Frame rate (task 16, 2026-10-07)

On a 120 or 144 Hz display psyz's auto VSync can't use the driver's VSync
(it matches 59.94 only), so the smooth dream went out at 59.94 through the
limiter. `frame_rate = display` (or a number, 30 to 360) runs the dream's
passes at that rate instead:

- psyz (fork, `present-rate`): `Psyz_VideoPresent(fps)` presents and paces
  the next frame at `fps`: the driver's VSync when the display refreshes at
  about that rate (or VSync is forced on), else the limiter; no VSync
  callbacks. `Psyz_VideoVSync` switches the driver VSync back for the
  59.94 pacing of menus and movies. `Psyz_VideoGetDisplayRate` reports the
  window's display.
- `src/pacing.c` (`TimedPass`): each pass presents at that rate, a tick
  runs when its time is due (`pace * 59.94 / 60` a second, as on the blank
  grid), and the passes between draw the world at the fraction of the tick
  their time is (in 4096ths). A late tick is not caught up on.
  Outside the dream, and with smooth off, nothing changes.

Measured (RelWithDebInfo x86_64, headless, the limiter): at 144, 144.1
frames and 14.00 ticks a second, tick gaps 69 to 77 ms (67 to 84 on the
blank grid, whose ticks fall on 4 or 5 blanks); a turn draws the camera's
yaw 6 to 7 units a frame, 68 a tick. Lockstep at 144 (x86_64 Debug, pace
14): days 22 and 340 and two of task 14's spots, every STATE and SAVEBLK
line the same as `main`'s. The driver VSync path (a real 120/144 Hz
display) is not tested here.

### Measured

- Lockstep (`tools/lockstep.py`, x86_64 Debug, task 11's configs): pace 20
  with smooth on, pace 14, and pace 14 with smooth on, each against pace
  20 with smooth off, over r1 to r4 (16 days) and `rt` (four days with a
  STATE line every tick, 12 955 lines): every STATE and SAVEBLK line
  identical.
- The music (`SDL_AUDIO_DRIVER=disk`, day 3, standing, the last 60 s):
  the onset envelope's beat period is 2.79 s at pace 20 and at 14, and the
  time stretch that best maps one onto the other is 1.00 (correlation
  0.83; 0.04 at 0.70, 0.85, 0.95 and 1.05).
- In-between frames (`draw_trace` in `tools/lockstep_gdb.py`, day 22,
  pace 14): through a turn the camera's yaw runs 2048, 2028, 2012, 1996,
  1980 (tick), ...; at the link on tick 118 → 119 (33 344 units) that
  interval has the tick's picture only. Entities moving 50 or 512 a tick
  (days 340 and 22) are blended; the state lines with smooth on and off
  are the same.
- Speed (Release x86_64, headless on a Ryzen 9 7950X and a Radeon RX 9070, 59.94 Hz with
  `LSD_VSYNC=off`): pace 14 with smooth holds 59.9 frames and 13.99
  ticks a second, a frame's work 349 µs on average (644 µs at most) of
  its 16 683; pace 20 with smooth 339 µs (1.6 ms at most); the game's own
  pacing 338 µs per tick frame. Uncapped, pace 14 with smooth runs 3700
  frames a second in a dream. i686 Release, pace 14 with smooth: 59.9 and
  13.99, 488 µs.

## Draw distance (task 14, 2026-10-06)

`draw_distance = N` (README, "Picture"; `--draw-distance`,
`LSD_DRAW_DISTANCE`), 1 to 4, default 1: the dream's fog N times further
away, never past 26624, the clearest fog the game uses. `src/draw_distance.c`
wraps `gNodeGuardedViewportMethods.setFogNear`, as `widescreen.c` and
`pacing.c` wrap methods; at 1 it installs nothing. Measurements and the
other limits are in `docs/tasks/14-draw-distance-handover.md`.

### What ends the view

- **The fog.** A stage's style config picks `fogNear` from
  `sStyleFogNears` (26624, 20480, 14336, 8192, 4096; the sixth, 2048, is
  never picked). `SetFogNear(fogNear, h)` sets the GTE's depth cue so that
  dp is 0 at fogNear and ONE at five times it; textured faces take
  `dp >> 9` palette rows of the stage's fog colours, the others are cued
  by the GTE, and `TransformAndCullPoly` drops a face at dp = ONE. Five
  times 26624, 20480 and 14336 is past the GTE's depth range (SZ
  saturates at 65535, where dp is still below ONE), so only levels 3
  (8192: culled at 40960) and 4 (4096: at 20480) cull anything.
- **The footprint.** The StageMap draws 20 x 20 cells (2048 each,
  `gridSpan` 40960) from the player's cell forward, axis-aligned by
  quadrant and shifted toward the look direction. Its far edge is
  38912 to 40960 ahead: exactly where level 3's fog culls, so on the
  console a footprint edge shows only on levels 0 to 2, as a straight
  horizon against the sky.
- Not limits: the OT (8192 tags, `otShift` 3, covers all of 0..65535),
  the near clip (10), the projection (h 266), faces past 65535 (flagged
  SZ-saturated and subdivided, not dropped), the chunk ring (seven
  chunks of 20 x 20 cells around the player's, which the footprint never
  leaves).

Which level a day gets: the stages with a fixed config (5 to 12) have
level 0 or 2. The others (Bright Moon Cottage, Pit & Temple, Kyoto,
Natural World, Happy Town, Monument Park) pick by day + stage
(`PickStyleFallbackConfig`): about 30 % of days level 3, 3 % level 4,
the rest 0 to 2.

### Why 26624

`SetFogNear` puts -320 * fogNear / h into DQA, a 16-bit register; at the
dream's h (266) fogNear above 27238 wraps it. 26624 is the largest the game
itself uses, so the farthest setting looks like its clearest stages: the
ground runs to the footprint's edge, partly fogged. Drawing beyond that
edge means a wider footprint (handover, "The footprint"), which is not
cheap.

### Measured

- Lockstep (x86_64 Debug, pace 14, smooth on, seed 4321): eight spots
  (Natural World, Kyoto, Happy Town and Monument Park, each on a level-3
  and a level-4 day) walking for 700 ticks, through links into four more
  stages: `main`, the branch at 1 and the branch at 4 print the same 50
  STATE and 8 SAVEBLK lines, and the 48 freeze screenshots at 1 are
  byte-identical to `main`'s. Two of them with a STATE line every tick
  (688 each, six stages): the same at 4 as on `main`.
- Frame times (RelWithDebInfo x86_64, 59.94 Hz with `LSD_VSYNC=off`,
  psyz's draw time, 12 s standing and 12 s turning): medians 200 to
  400 µs per frame at 1 and at 4 alike, out of 16 683; the differences
  between the two (-80 to +60 µs) are within what one run differs from the
  next. The GTE work is the same: the footprint's cells are transformed
  either way, and the fog only decides whether a face is submitted.

## Widescreen edges (task 17, 2026-10-08)

At 16:9 the ground and buildings at one side could end in a straight
line well inside the picture (the operator's Happy Town screenshot), and
some sparkles sat away from their effect: the map's footprint and plain
world sprites.

### Why the sides ran out

The StageMap shows only the cells in a 20 x 20 window
(`StageMap__ComputeFootprintFromRotation`): from the player's cell 20
ahead along the axis nearest the look direction, and 20 across, shifted
toward the look direction by `gridSpan · sin` of the angle off that axis
(at most 9 cells). Every other cell is GsDOFF. The shift suits the
console's view, half-width 160 / 266 ≈ 0.6 of the depth; at 16:9 it is
0.8, and the window's side, on the side it was shifted away from, comes
into view much sooner. Over positions in a chunk and all headings (a
model of the window against the view cone, scratch):

| | the side seen | nearest | 5 % of cases nearer than |
|---|---|---|---|
| 4:3, the console | always, before the far edge | 11.6 cells | 13.3 |
| 16:9, the game's window | always | 4.9 cells (10 000 units) | 6.4 |
| 16:9, widened (below) | 80 % of cases | 8.4 cells | 13.9 |

A cell is 2048 units; the far edge is 19 to 20 cells ahead. The worst
headings are 20 to 30 degrees off an axis, where the window is shifted
furthest. No fog but level 4's hides 10 000 units, so it showed at every
draw distance; above 1 more of it shows.

### The widening

`src/widescreen.c` wraps `gStageMapMethods.refreshFootprint` (called
every tick by UpdateFootprintTracking) when the aspect is wider than 4:3:
after the game's own refresh it shows every cell of the seven loaded
chunks that lies within the window's extent along its ahead axis (so the
horizon stays where it is) and in the wider view cone, at this tick's yaw
or the last one's (the in-between frames of `smooth` turn from one to the
other), with a margin of about two cells for models that reach past
their cell. Before the next refresh it hides them again; a slot whose
chunk changed in between is left alone (the reload reset its cells).

What it can't do: show a cell that isn't loaded. The ring is the
player's chunk and six neighbours; the rows before and after it cover
only x -10 to 30 cells of the centre chunk's 0 to 20. Near a chunk's
corner, looking diagonally, the cone still reaches past them at 8.4
cells. Going further needs a second ring (task 14, "The footprint": 19
chunks, their loads timed ahead), not attempted.

At 16:9 the centre can now show a little more than the console's 4:3:
at headings off an axis the window's lateral side also crosses the
middle of the view, far out, and those cells are shown too. Limiting the
extra cells to the side strips would draw a seam at the 4:3 edge.

It is drawing only: the commands StageMap hands to cells
(`ApplyToSenderFootprint`, `DispatchToRectCells`) use their own 3 x 3
window around the sender and restore `rects`; nothing in the game reads a
cell's GsDOFF but the renderer and `SceneNode__RaycastVertical`, which
for a hidden cell rebuilds its world position from the translations
instead of reading the one the draw left (the lockstep runs below show
the same state either way). At 4:3 nothing is installed.

### Sprites

Viewport__DrawNode projects a world sprite itself and hands GsSortSprite
the screen position; psyz's GTE path squeezes it, but a plain sprite
(scale 1, no rotation, no flip) is drawn as a SPRT where it is, 4/3 too
far from the centre at 16:9 (task 09 had seen none). A gdb counter on
GsSortSprite over the walks below found one kind: StyleEffect's
VariantSprites (class 0x1F44, a 16x16 cell of `sStyleEffectTim`). The
plain kind (`StyleEffect__SpawnPlainSprites`) leaves all five at scale 1,
the jitter kind its first. Seen at Kyoto, Monument Park and Natural
World; at Kyoto one sat at x -109 for thousands of frames.

`src/widescreen.c` wraps VariantSprite's reset (the ctor's last step)
and updateScale: a sprite left at exactly ONE x ONE gets scalex ONE + 1,
which sends it down the GTE path with its siblings, 1/4096 wider. Nothing
but GsSortSprite reads `sprite.scalex` (updateScale writes it, or
multiplies the separate accumScale while accumulateScale is set). The
pause text and other ScreenSprites keep the SPRT path (task 09: the GTE
path garbles their 8x8 glyphs).

The ±512 clamp on a world sprite's projected position is in unsqueezed
units; 512 · 3/4 is still off the 16:9 screen (±213), so it hides
nothing.

### Checked

- Lockstep (x86_64 Debug, pace 14, smooth on, seed 4321, draw distance
  4, 16:9): task 14's eight spots (Natural World, Kyoto, Happy Town,
  Monument Park, two days each) walking for 700 ticks, through links into
  stages 0 to 6, 12 and 13, with a STATE line every tick: the same 646 or
  647 STATE lines and SAVEBLK with and without the widening and the
  sprite wrap. Turning on the spot at Happy Town (three spawns, 35
  freezes each): the same STATE; the shots differ only in the side strips
  (and once, far out, inside the 4:3 part, above).
- At 4:3 (Happy Town and Kyoto walks): the same STATE and byte-identical
  freeze shots; nothing is installed.
- Frame times (RelWithDebInfo x86_64, 16:9, draw distance 4, psyz's draw
  time, 12 s standing and 12 s turning at Kyoto, Natural World and Happy
  Town): medians 240 to 440 µs per frame either way; the widened build's
  differ by -8 to +55 µs, within run-to-run noise (task 14: -80 to +60).
- Builds: x86_64 Debug and RelWithDebInfo, i686 RelWithDebInfo, Windows
  x86_64 (MinGW), with only the two old "function called through a
  non-compatible type" warnings.

## Dithering and colour depth (task 19, 2026-10-09)

At `resolution 6` the PS1's 4x4 dither pattern, drawn per scaled pixel,
shows as a fine crosshatch over every lit texture; with dithering off,
the 15-bit rounding shows instead. Two settings, both defaulting to the
console's look: `dither = on|off` and `colour = console|full`
(`src/settings.c`; psyz's `Psyz_VideoSetDitheringMode` and
`Psyz_VideoSetColorDepth`).

### What dithers

The game turns dithering on once, for everything (`GsInitGraph`'s dither
argument in `DrawSystem__InitGraph`), so every primitive it sends carries
the bit; which ones the GPU then dithers depends on the kind. A counter in
psyz's `Draw_PushPrim` (scratch) over Natural World day 5 (spawn 3/30,
walking 3200 ticks) and the menus:

| kind | in the dream | dithered |
|---|---|---|
| textured, lit (flat colour) | 5 526 000 | yes |
| Gouraud, untextured | 90 800 | yes |
| flat, untextured | 8 300 | no |
| tiles | 149 600 | no |
| sprites | 3 700 | no |
| textured Gouraud, raw textured, lines | 0 | |

The world is flat-lit textured polygons: GTE lighting and fog give each
polygon one colour, which modulates its texture, so the dither falls on
the texture times that colour. The intro movies, the title menu and the
graph are lit textured polygons as well (370 800 before the first menu,
all dithered). The sky's gradient is flat strips the game steps itself,
so neither setting touches it.

### Dither off

`dither = off` sets `PSYZ_DITHER_OFF` for the whole game, menus and movies
included. On the console a texture drawn at brightness 128 still dithers:
the pattern's offsets (-4 to +3 on 8 bits) put half its pixels one 5-bit
step down, so the 2D screens get the same grain; turning it off there gives
their texels exactly. Without dithering the texture-times-colour product is
truncated to 5 bits, as the PS1 does, and the steps show on darker lit
walls, in fog, and on the Gouraud walls of the first dream's rooms.

### Full colour

`colour = full` (`PSYZ_COLOR_DEPTH_24`) keeps what the shader computes at
8 bits per channel: the texture times the colour, as `tex5 / 31 · col / 128`
(so brightness 128 draws the texel exactly, as at 15 bits), and Gouraud
colour as interpolated. It implies no dithering. Each primitive carries the
depth in a spare TPAGE bit (`TPAGE_FULLCOLOR`, 0x1000), which the vertex
shader folds into its `dither` value (2: keep 8 bits). The render target is
already RGBA8, so nothing else changes; untextured primitives without
dithering were already drawn at 8 bits.

Where 8 bits can't reach, and why it doesn't matter here:

- **Readbacks.** VRAM read as 15-bit (texture pages, CLUT lookups,
  StoreImage, MoveImage) rounds a 24-bit pixel to the nearest 15-bit value
  rather than truncating it as the PS1 would. The game never reads what it
  drew: it textures only from loaded TIMs and the tile atlas (VRAM x 640 to
  960), draws only into the two display buffers (`GsDefDispBuff`), and its
  StoreImage and MoveImage touch the fade CLUTs (`CLUT_FADE_Y`, from the
  disc) and texture strips (`StyleScrollVramStrips`).
- **Semi-transparency** blends in the RGBA8 target at 8 bits either way;
  with full colour its inputs simply keep their low bits.
- **Images and movies** are 15-bit on the disc. Drawn at brightness 128
  they look the same at either depth; the title menu, drawn a little under
  128, comes out about two levels (of 255) brighter than with dither off,
  where the truncation drops a step.

### Checked

- Lockstep (x86_64 Debug, resolution 6, seed 1234, day 5, a STATE line
  every tick, four freezes): Natural World spawn 3/30 and Kyoto spawn 2/0,
  `main` (psyz `ad64361`) against this branch with the defaults, dither off
  and full colour: the same STATE and SAVEBLK lines over all 3 191 to 3 199
  ticks the runs reached. OpenGL (`sdl3_gl`, on Xvfb): `main`, defaults and
  full colour at Natural World, the same STATE (2 850 ticks and more).
- Screenshots with the defaults, `main` against the branch: byte-identical
  at 320x240 (Vulkan, all five), and at the full 1920x1440 (a scratch
  capture of the scaled target) the menu and two freezes identical; the
  other two differ only on the water, whose texture scrolls on frame time
  while the dream clock is frozen. OpenGL alike (about 91 % of the
  differing pixels water-coloured, the rest its pink reflections).
- Colours in a frame (1920x1440, Natural World): about 4 000 with
  dithering, 2 500 with it off, 5 300 at full colour.
- Frame times (RelWithDebInfo, resolution 6, psyz's draw time over 30 s
  standing in the first dream's room): medians 314 µs (defaults), 313
  (dither off), 307 (full colour), within run-to-run noise.

## Precise geometry (task 20, 2026-10-09)

The PS1's GTE hands the game each projected vertex as a 16-bit screen X and
Y, and its GPU draws every vertex on a whole pixel of the 320x240 screen and
maps textures affinely. At `resolution 6` the rounding shows as the ground
and walls wobbling while the view moves (a vertex jumps a whole console
pixel, six screen pixels, at a time), and the affine mapping as textures
bending across large polygons near the camera. `geometry = precise` draws
the vertices the GTE projected where they really fall; `perspective` also
maps their textures in perspective. `console` is the default, and a build
with `-DLSD_PRECISE_GEOMETRY=OFF` (psyz's `PSYZ_PRECISE_GEOMETRY`) leaves
the machinery out.

### Carrying a precise vertex from the GTE to the GPU

DuckStation's PGXP keeps a precise value beside every memory word the CPU
stores an SXY to, and follows the word through the CPU's loads and stores.
psyz has no CPU to watch: the game is C, and its copies are plain C
assignments. The options:

- **A cache keyed by the 32-bit SXY value.** Survives any copy, but two
  vertices that land on the same pixel in a frame share an entry. Their
  positions differ by under a pixel, but their depths need not: a near and
  a far vertex on one pixel would give a polygon the wrong w, and a texture
  bent the other way. Rejected.
- **Re-projecting at draw time** from the model vertex. The packets don't
  carry it. Rejected.
- **A table keyed by the address the SXY was stored to** (chosen): the GTE
  keeps a precise vertex beside each entry of its SXY FIFO, and each of
  psyz's SXY stores (`gte_stsxy*`, the `StoreSxyPoly*` macros,
  `RotTransPers*`/`RotAverage*`) files it under the destination address,
  with the 32-bit word it wrote and the frame. When a primitive is queued
  (`GPU_Enqueue`, while the packet is still at the address the game built
  it), each word is looked up; a hit counts only if the word still holds
  the stored value and was stored this frame or the last. A copy through
  C, or a value the game changed, misses and is drawn from its 16-bit
  value, as the console draws it.

What the game does between the two, and how it fares:

- `SortTmdObject`'s faces (`ProjectTriFace`/`ProjectQuadFace`) store the
  GTE's FIFO straight into the POLY_xx packet: precise.
- Copies: `TransformAndCullPoly` stores the SXYs into its context as well,
  for `FlagLargePolyForDivide` and the subdivider; those entries are only
  read by the game. `FillRVectors` copies a packet's SXYs into the
  DIVPOLYGON in C, so psyz's `RCpoly*` takes each corner's precise vertex
  from the packet at `s` (the same words, checked for equality) and
  carries it, and the midpoints it projects itself, into every primitive
  it emits.
- Subdivision decisions (`FlagLargePolyForDivide`'s 256-pixel box, the
  near and window rejection in `RCpoly*`), back-face culling (`nclip`),
  the OT slot (`avsz3`) and the depth cue all read the 16-bit values, which
  are unchanged.
- Sprites: world sprites are projected by the game on the CPU
  (`Viewport__DrawNode`, an integer divide); psyz's `GsSortSprite` corners
  go through locals. Both stay on whole pixels.

Each vertex decides for itself: a polygon draws the vertices that have a
precise value there and the others on their whole pixels, so a vertex
shared by two polygons is in the same place in both (deciding per polygon
left cracks where one neighbour fell back). The precise position is the
true projection even where the GTE's is not a rounding of it (SZ saturated
far away, IR or SX/SY clamped), for the same reason; only a vertex at or
behind the eye (or a projection past the GPU's ±32768) has none.
Perspective needs a depth at every vertex, so a polygon with any vertex
missing is mapped affinely.

### On the GPU

The vertex format carries x, y as floats and a w. A primitive drawn with
perspective has a spare TPAGE bit (`TPAGE_PRECISE`, 0x0800); its vertex
shader outputs `(x·w, y·w, 0, w)`, so the UV is interpolated with
perspective, while its colour comes through a `noperspective` copy, as the
console's Gouraud shading is (GLSL ES has no `noperspective`; there colour
is interpolated with perspective too). The texel rules (texture window,
CLUT lookup, `resolveTexel`'s sample point) are applied to the
interpolated UV as before. Without the bit w is 1 and every vertex is on
a whole pixel, which draws exactly what it did.

### In motion

With `console`, walking and turning at resolution 6, the ground's polygon
edges step a console pixel (six screen pixels) at a time, the grass folds
into chevrons along each quad's diagonal, and thin seams of sky open and
close between ground polygons from one tick to the next. `precise` puts the
edges where they belong and closes the seams, but the texture still bends
at the diagonals and shifts where a polygon switches between subdivided and
whole (the 256-pixel box). `perspective` removes the bend and the switching
too; Natural World's water shows even squares of texels instead of skewed
ones, and Kyoto's raked gravel, a zig-zag of folds under `console`, runs
in straight lines.

What still moves in steps with `perspective`: sprites (the game rounds a
world sprite's position itself, and psyz's `GsSortSprite` corners pass
through locals), the 2D, and any vertex at or behind the eye. Slits where
the game's own models don't meet (under Natural World's cliffs) are there
in every mode. The in-between frames of `smooth` project again, so they
are precise too, and widescreen's X squeeze applies to the precise X as to
the GTE's.

### Checked

- Coverage over Natural World day 5 (a scratch counter in `Draw_PushPrim`):
  4.80 M quads and 273 k triangles with every vertex precise, 129
  triangles partly, and 407 k quads with none: the intro and menu's 2D.
- Lockstep (x86_64 Debug, resolution 6, seed 1234, a STATE line every
  tick): Natural World day 5 spawn 3/30, `main` against this branch at
  `console` and `perspective`: identical STATE and SAVEBLK over 3 199
  ticks; a turn-and-walk run at `console`, `precise` and `perspective`:
  identical. Kyoto day 5 spawn 2/0, `console` against `perspective`: identical
  over 3 200 ticks. OpenGL (on Xvfb), `main`, `console` and
  `perspective`: identical over 2 064 ticks.
- Defaults against `main`, 1920x1440 (task 19's scratch capture), Vulkan:
  the menu and two freezes byte-identical; the other two differ only in
  water-blue pixels (the water scrolls on frame time while the dream clock
  is frozen, task 19). OpenGL at 320x240: the menu and one freeze
  identical, the other three differ only on the water and its reflections
  (75 to 95 % water-blue).
- Frame time (RelWithDebInfo, resolution 6, psyz's draw time over 30 s in
  the first dream's room): medians `main` 247 µs, `console` 256,
  `perspective` 329, `precise` 346.
- Builds: x86_64 Vulkan and OpenGL, `LSD_PRECISE_GEOMETRY=OFF`, i686,
  with only the two old warnings; psyz's tests 319 passed.

## Settings menu (task 21, 2026-10-09)

F1, or a gamepad's Guide button or both sticks pressed in, or SETTINGS in
the title menu, opens a menu over the game (`src/menu.cpp`) with every
setting of `settings.ini` and the keyboard's keys.

### Drawing it

Dear ImGui, through the overlay hooks psyz already had: after psyz blits
the PlayStation's display into the window, it calls the overlay's frame
callback, and with SDL GPU a render callback inside a render pass on the
swapchain texture; with OpenGL the frame callback draws straight into the
default framebuffer before the swap. So the menu is drawn into the window's
own buffer and never into the game's VRAM, and the debug server's
screenshots and VRAM dumps (which read the VRAM) don't show it. The
backends are Dear ImGui's own (`imgui_impl_sdl3`, `imgui_impl_sdlgpu3`,
`imgui_impl_opengl3`), from the copy in psyz's `external/cimgui`
submodule; cimgui's C bindings are not used, as they have no SDL GPU
backend, so `src/menu.cpp` is the port's one C++ file. SDL GPU wants a
frame's vertices uploaded outside a render pass: the menu does that in a
command buffer of its own, submitted ahead of psyz's. The alternative,
drawing the menu with psyz's 2D primitives, would have put it in the VRAM
(and in screenshots) and needed a font and widgets of our own.

While the menu is shut no Dear ImGui frame is built and no events are
passed to it, so it costs nothing and its event queue stays empty.

### The game's look

The menu is drawn in the title menu's font, `CDI\ETC\FONTICON.TIM`, read
from the disc image when the menu is set up (`src/disc.c`: the .cue's
first track, ISO 9660, through a file handle of its own, so psyz's libcd
and its streaming are left alone). It is a 4-bit TIM of 8x8 glyphs by
character code, 32 to a row, white with a dark drop shadow (0x2421) that
hardly shows on the title menu's blue; the menu takes the white pixels.
Dear ImGui gets them through an `ImFontLoader` of its own, which draws a
glyph at the whole multiple of 8 nearest the font's size, every glyph as
wide as it is high, as the game spaces them; the menu picks a font size of
16 pixels up to a window 1199 high, then 24, 32... (half the size of the
game's own text, which is 8 of 240 lines). The colours are the title
menu's as drawn: the backdrop's blue (16, 0, 62), its grey entries (156),
yellow for the row under the cursor (label and value, as the title menu
lights its entry) and the pink of "Day 001" for headings; no rounded
corners. Without the font (a disc without it) the menu uses Dear ImGui's.

### SETTINGS in the title menu

`src/title_settings.c` (built with the game's C) gives the title menu a
seventh entry. The menu is a TaskCore built from a table,
`sTitleMenuTarget`: names, positions, the entries the cursor skips, and the
item list each opens; TaskCore makes a TextRow per name and sizes its
per-entry arrays by their count. The port points the table at copies one
entry longer, SETTINGS 12 below SHAKE as the entries are spaced, and wraps
`confirmSlot` (SETTINGS opens the menu; every other entry, and START from
any entry, goes on as before) and `update` (while the menu is open, from
SETTINGS or F1, the frame count stays at 0, so the title menu doesn't time
out to the intro behind it). The decomp is not changed. When the menu
opens with a pad's button still down (circle, from SETTINGS), Dear ImGui's
gamepad navigation waits until every button is up, since it reads the
buttons' state rather than presses.

### Input

`Psyz_PadsHold` (psyz): while held, the pads the keyboard and gamepads
drive read as connected with nothing pressed and the sticks centred, and
Escape doesn't quit; once released, a button still held reads as released
until it is let go. The game keeps running: a dream's clock and its music
go on (the game's own pause is START; pausing for it would mean changing
the game's state). The debug server's injected input is not held, and
lockstep drives the pads above psyz, so neither is affected.

### What changes when

The settings were applied once at start; now each module can take a
change while the game runs, and the menu's changes reach them from a VSync
callback, between the game's frames, not inside psyz's present (where the
menu runs, and where a new internal resolution would recreate the render
targets of the frame being presented).

| setting | when | how |
| --- | --- | --- |
| resolution, scale, dither, colour, geometry | at once | psyz's setters |
| pace, smooth, frame_rate | at once, also mid-dream | `Pacing_Set`; `PacedRunLoop` is now always installed and runs the game's own loop at pace 20 without smooth, as `DrawSystem__RunLoop` does |
| draw_distance | at once, also mid-dream | the wrapper keeps the game's fog distance and the dream's Viewport, and gives it the new one; its update hands it on |
| aspect | next dream | `Widescreen_Set`; the ratio is taken up at DayTask's onInit (the GTE squeeze, the display stretch, the footprint widening), the window keeps its shape |
| keys | at once | `Psyz_PadsSetKeyboardMap` |

Aspect waits for the next dream because the dream's VariantSprites are
made un-plain when they are created (`src/widescreen.c`), and the StageMap's
extra cells follow the ratio of the dream they were shown in.

The wrappers for pace, aspect and draw distance are installed whatever the
settings say, so they must leave the game alone at the console's values:
lockstep (x86_64 Debug, Natural World day 5 spawn 3/30, seed 1234,
resolution 6, walking and turning) gives the same STATE and SAVEBLK lines
as `main` at the defaults, at pace 20 without smooth, and at 16:9 with
draw distance 3, over the whole day (3 622 lines each).

### Saving

`Ini_Update` (`src/ini.c`) rewrites only the lines that set what changed
(a comment after the value stays), uncomments a `# name = ...` line when
the setting is set nowhere else, appends otherwise, and leaves every other
line as it is; it writes a `.new` file and renames it over the old one. A
setting given on the command line or in the environment is shown greyed
with its option, can't be changed, and is never written.
