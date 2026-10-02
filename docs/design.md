# lsd-port design

The decisions in PLAN.md's "Decided" list are not repeated here. This file
holds the measurements behind the open ones, with a recommendation each.

## Pointer width: 32-bit first (*decided 2026-10-02*)

The operator took the recommendation below.

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
   on Windows). CI adds the multilib packages.
2. CI also builds x86_64 and records the width warnings (71 today), so
   the count can only go down. Fixes that are correct at both widths and
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
# 64-bit (the default build) and 32-bit
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake -S . -B build-m32 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32
ninja -C build -t clean lsd_game && ninja -C build lsd_game 2>&1 |
    grep -c 'pointer-to-int-cast\|int-to-pointer-cast'
tools/class-sizes.py build-m32
```
