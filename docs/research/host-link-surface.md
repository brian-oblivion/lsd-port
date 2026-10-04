# The host link surface (2026-10-02)

What the linker reports missing when the game's C (lsddecomp `3127186d5`)
links against psyz (fork `main` `8030744`) on Linux x86_64; i686 and
MinGW report the same list. It replaces the regex estimate in
`psyz-coverage-2026-10-02.md` as the platform layer's to-do list. Each
symbol had a temporary stand-in in `src/stubs.c` until psyz provided it;
task 04 removed the last ones, and the file with them.

## SDK functions psyz lacks (0)

The game links against psyz's headers; none of these had a body in
psyz's PC build. Of the first count's 38, task 02 did eight: `GsInit3D`,
`GsGetTimInfo`, `GsSortBg`, `GsSortBoxFill`, `GsSortSprite` (psyz branch
`libgs-2d`), `SsUtGetVabHdr` (`libsnd-vabhdr`), `StClearRing` and
`StUnSetRing` (`libcd-stream`). Task 03 did 25: the 13 libgs 3D calls
(`libgs-3d`), `ApplyMatrixLV`, `ApplyMatrixSV`, `MulMatrix2` and `Square0`
(`libgte-matrix`), and the eight `RCpoly*` (`libgte-rcpoly`). Task 04 did
the last five: `SsSeqPause`, `SsSeqReplay`, `SsSetMute` and `SsUtAutoVol`
(`libsnd-seq`), and `SetMem` (`libapi-setmem`).

## Linked, but only as psyz stubs

These link because psyz defines them, but the body only logs "not
implemented" (or does part of the job). The libcd streaming calls
(`CdRead2`, `StSetRing`, `StSetStream`, `StGetNext`, `StFreeRing`) and
libpress (`DecDCTReset`, `DecDCTin`, `DecDCTout`, `DecDCToutCallback`,
`DecDCTvlc`) are implemented now (branches `libcd-stream`,
`libpress-mdec`), and so is libgte's `SetFogNear` (`libgte-fog`).

- libsnd: `_SsSndTempo` (accelerando and ritardando; the game uses
  neither). `SsSeqOpen` and the rest of the sequencer are implemented now
  (`libsnd-seq`).
- libapi: `EnterCriticalSection`, `ExitCriticalSection` (psyz's
  `PS1_` names), `EnableEvent`, `DisableEvent` (partly)
- libcard: `_card_info`, `_card_load` (partly: they log "not implemented"
  and answer as a formatted card that is always there, which is enough
  for SAVE and LOAD)

## The game's own data that was not in its C (37 + 18)

lsddecomp builds the PS1 executable partly from data that splat extracts
from the retail executable at build time (`asm/data`, never committed),
and places some uninitialised globals by address
(`config/undefined_syms_auto.slps01556.lsdde.txt`). The host has neither.

- **18 BSS globals** placed by address: `sButtonMasks`, `sSortLightOff`,
  `sSortNdiv`, `sSortUseGlobalLightMode`, `sSortLightMode`,
  `sSsSizeTableBuf`, `sTmdModelBoundsBuf`, `sStyleDecorSlots`,
  `sStyleEffectSlots`, `sStyleCueSlotPool`, and the nine words
  `sStyleSpawnOffsetX` .. `sStyleSpawnColors` that the code lays one
  `StyleEffectParams` over. They have no contents, so lsddecomp now defines
  them under `HOST_BUILD` in their units, sized from the code; the nine
  are one `StyleEffectParams` there, as psyz's porting guide asks of
  overlapping symbols.
- **37 read-only objects** with contents: file paths, messages and
  `printf` formats, the memory-card texts and icon names, the text-entry
  character sets and the save title's glyphs. They were assembly that
  splat extracts from the retail executable; lsddecomp now has them as C
  in the units that use them, byte-exact on the PS1 (lsddecomp
  `348f38606`). The save file's name and title, which the game writes,
  are writable on the host (`IMAGE_CONST`).

## Against the coverage snapshot

The snapshot's 146 functions, as the linker sees them:

- Its three "absent" are resolved: `GsIDMATRIX` and `GsLIGHT_MODE` are
  psyz data now (branch `libgs-sdk-header`), and `delete` is psyz's
  `erase` (lsddecomp maps the name).
- Of its 53 "only assembly in psyz's decomp", the libc names are the
  host's (`atoi`, `memcpy`, `printf`, `strcat`, `strcpy`, `strlen`,
  `strstr`) or psyz's under a `psyz_` name (`rand`, `srand`, `itoa`:
  branch `libc-host-headers`, matching libc2's results); the kernel file
  calls (`open`, `close`, `lseek`, `write`) are psyz's `psyz_` wrappers;
  `EnterCriticalSection`/`ExitCriticalSection` are psyz stubs. The rest
  are the 38 above.
- `SsUtAutoVol`, counted as implemented, is not: its C is in psyz's
  decomp but not in psyz's CMake source list.
- The 12 stubbed and 5 partly implemented are unchanged (the list above).

## What happens at run time

With no disc image `lsd` exits with 2 and says so.

With the real disc (2026-10-02, lsddecomp `32f27c495`, psyz fork `main`
`e65b823`), the i686 build plays the boot sequence as the console does:
the Asmik Ace logo, the ASMK movie, the OSD logo, one of the seven
opening movies (picked at random) and the title menu, where it waits for
input. On the way it registers its ~1000 files with `CdSearchFile`.
What it took, in boot order:

1. TaskCore read an image task's NULL target (lsddecomp `32f27c495`).
2. `CdRead2` was a stub returning 0, and the movie loop retried it
   forever: psyz's libcd streaming and a software MDEC (`libcd-stream`,
   `libpress-mdec`).
3. `SsUtGetVabHdr` returned -1, so the title menu's first sound read an
   empty tone table (`libsnd-vabhdr`).
4. Nothing was drawn: the libgs 2D sorts and `GsGetTimInfo` were stubs,
   and psyz's `GsClearOt` cleared the OT forwards (`libgs-2d`).
5. Still nothing on i686: psyz's 32-bit fast path dropped every
   primitive at the next `DrawSync` (`gpu-32bit-flush`; on upstream
   `main` 37 GPU host tests fail at i686).

Sound was unverified then: SDL loads the audio libraries at run time, and
the 32-bit ALSA library (`lib32-alsa-lib`) was not installed. Task 04
checked it headless instead (below).

### Into the dream (task 03)

Pressing START at the title menu now starts a day and plays the dream:
the first stage's room, textured, lit and fogged, links to the next area
when walked into, and the day ends in the dream graph. What it took, in
order (lsddecomp branch `task-03-host`, psyz fork `main`):

1. The title menu timed out before START could be pressed, after about
   3 s instead of 10: psyz's `VSync(n)` paced one frame for any n, and the
   game runs at `VSync(3)` (psyz `libapi-vsync-n`).
2. Starting the day wrote 1 over a return address: `CdDriver`'s
   retail store through a pointer it never sets (lsddecomp `71ea45019`).
3. `GsMapModelingData` and the rest of libgs 3D were stand-ins
   (`libgs-3d`, stacked on `libgte-matrix`; `libgte-rcpoly` alongside).
4. The end of the dream flushed an empty cue slot through NULL, which
   the PS1 survives (lsddecomp `f42cd5a12`).
5. The dream was a screen of fog colour, from three causes:
   - psyz's `InitGeom` set DQB to 0x140, not 0x1400000, and `SetFogNear`
     was a stub (`libgte-fog`);
   - `gte.h` named Sony's masked `gte_stflg_4` `gte_stflg`, so on the
     host the TMD renderer saw the whole FLAG and culled every face
     (lsddecomp `9f0a26978`, psyz `libgte-stflg4`);
   - the game's one memory pool, the console's 1435 KB, ran out on the
     host (larger primitives and OT entries), so the stage's textures
     never loaded (lsddecomp `a5746a4d6`: 4 MB on the host).

`GsSetFlatLight` warns of a negative `SquareRoot0` on the way in:
`StageMap` sets a light's colour before its direction, so the first call
sees an uninitialised direction. The next call corrects it, on the PS1
too.

x86_64 stops earlier, in `BMemPMgrAlloc` called from
`LinkResource__BuildModels` for `ETC\DREAME5.TMD`: the heap and the
hand-sized layouts `docs/design.md` lists for 64-bit.

### A whole day (task 04)

The i686 build now plays a whole day: the dream with its
music, links into other stages, the end of the day, the dream graph and
the title menu at the next day; SAVE, and LOAD after a restart, which
brings the day back; FLASHBACK; the graph scoring and the special-day
movie that follows. Sound was checked headless, through SDL's `disk`
audio driver (`SDL_AUDIO_DRIVER=disk`, raw S16LE stereo at 44.1 kHz in
`SDL_AUDIO_DISK_OUTPUT_FILE`): the movies' XA audio and the dream's SEQ
music are there; nobody has listened to them yet. What it took, in
order (all psyz; lsddecomp is unchanged):

1. The dream's music was not played: psyz had no SEQ sequencer
   (`SsSeqOpen`, the tick, the MIDI events) and its voice allocator
   always failed, so not even `SsUtKeyOn` could sound. lsddecomp carries
   libsnd 3.3's sequencer as C, but the port compiles none of `src/psyq/`,
   23 of its functions are still assembly there, and its score record is
   not psyz's. psyz `libsnd-seq` writes the missing 4.x functions from
   that C and Sony's 3.3 code, over psyz's own records; it also has the
   four libsnd calls `src/stubs.c` stood in for.
2. Starting a day crashed in `_SsTrapIntrVSync`, calling itself: psyz's
   `InterruptCallback` could not remove a handler, so the title menu's
   `SsEnd` left the sequencer's installed and the dream's `SsStart` chained
   to it (`libetc-interrupt-callback`).
3. Seen on the way: psyz's `_SsVmKeyOnNow` swapped left and right for
   panned notes (`libsnd-keyonnow-pan`), and `SetMem` had no body
   (`libapi-setmem`). With that, `src/stubs.c` was empty and is gone.
4. SAVE wrote an empty file: psyz's Unix `open()` opened everything
   without `FCREAT` read-only, and the game writes its save after
   reopening it with `FWRITE` (`kernel-open-flags`).

FLASHBACK needs a save past the unlock score with a flashback stored; the
test edited the score into a save made after six days, as it edited the
four moods the graph scores into one. Memory card files are `bu00/` and
`bu10/` in the working directory, psyz's default.

Three days walked with scripted input (forward, with random turns) ran
to their own time-up, through links into other stages, without a crash:
184, 33 and 79 seconds, as each day set its limit. Headless runs note:
START in a dream is the pause, which mutes the SPU (`SsSetMute`); a
START still held when the dream begins pauses it at once.

x86_64 still stops at boot, in `BMemPMgrAlloc` for `ETC\DREAME5.TMD`,
as before.

### Against the console (task 05)

The reference is DuckStation (0.1-11752), run headless by
`tools/ds_drive.py`: its own settings and data in a scratch directory,
its own Xvfb, the OpenGL presenter (Vulkan cannot present on Xvfb), the
software renderer, keys through xdotool, the media capture for audio
(PCM, in emulated time) and the native 320x240 screenshot. The only BIOS
installed is SCPH-1001 (US), which boots the Japanese disc with a region
warning. Its GDB stub takes lsddecomp's `build/lsdde.elf` for symbols
(SDK functions by address), but reports breakpoints late and slows
emulation per hit, so it answers what was called, not when. The same
moments were recorded on both: the title menu's tones, day 1's dream,
the attract movie the menu times out into, the dream graph.
`tools/snd_compare.py` holds the measurements.

Sound, after the fixes (psyz fork `main`):

| | console | port |
|---|---|---|
| menu tones: pitch, harmonics, decay | 51.7 Hz sweep | same |
| menu timeout (last press to fade) | 10.11 s | 10.07 s |
| dream music RMS | -30.24 dB | -30.20 dB |
| dream music pitch | | ratio 1.0000 |
| dream SEQ loop | 8.043 s | 8.006-8.017 s |
| XA movie gain, pitch, tempo | | -0.07 dB, 1.0000, 1.00000 |
| stereo balance (music and XA) | | equal to 0.01 dB |

What it took:

1. Whole layers of the dream music were missing: psyz left ENVX at its
   level when a one-shot sample ended, and libsnd frees a voice only after
   ENVX reads 0, so voices filled up and later notes were dropped (psyz
   `spu-end-mute-envx`).
2. XA played 0.85 dB louder, flat across the spectrum: the console's
   decoder resamples through zigzag tables whose gain is 0.906, psyz
   through a unity Hermite interpolator (psyz `xa-zigzag`). The waveforms
   now agree to -26..-37 dB.
3. Movie frames were ~3.8/255 darker in every channel: psyz's MDEC
   truncated its 15-bit output, which DuckStation rounds (psyz
   `mdec-15bit-round`; now within 0.15/255).

Still different:

- The port's SEQ runs ~0.3-0.4% fast: psyz paces NTSC at 59.94 Hz, a
  240p PS1 at 53.693175 MHz / (263 x 3413) = 59.826 Hz. Inaudible, but
  it is every psyz game's pacing, so it is the operator's (and upstream's)
  call.
- In the first ~6 s of a dream the console's mix drops to digital
  silence for 30-140 ms three times, at the same places in every run and
  never later; the port, which loads instantly, does not. No mute call is
  made on the port there; the console side was not traced to a cause.
- Loading: the console shows the title menu ~2 s after
  `GameApplication__RunTitleMenu` and starts the dream's music ~4.8 s
  after START; the port takes under 1 s for both.

Picture: the title menu is identical at 5 bits per channel (the 8-bit
values differ by 1 where psyz expands 5 to 8 bits by rounding and
DuckStation by bit replication). The first dream room has the same
textures, dithering, fog and light; 31% of pixels differ, by one 5-bit
step in the shading or by about a texel at edges and in the window's
texture, since psyz rasterises on the host GPU. The dream graph after
day 1 has the same layout and the same point. Movie frames match by eye;
their levels, before the MDEC fix, did not (above).

### Release (task 06)

Saves: the cards are `bu00/` and `bu10/` in SDL's per-user folder
(`~/.local/share/lsd-dream-emulator/`, `%APPDATA%\lsd-dream-emulator\`), or in
`--saves DIR` / `LSD_SAVES`, through `Psyz_AdjustPathCB` in `src/main.c`;
psyz's `_bu_init` now makes the card directories where they are mapped
(psyz `libcard-bu-init-path`). Checked headless: SAVE on day 2, LOAD after
a restart brings day 2 back, from the default folder (with
`XDG_DATA_HOME` in scratch), from `LSD_SAVES` (a folder that did not
exist yet) and with `--saves` overriding `LSD_SAVES`; an old `bu00/` in
the working directory gets a note on stderr and is left alone.

The card model (psyz `libcard-new-card`): `_card_info` and `_card_load`
deliver their SwCARD events through the kernel's event states
(`EnableEvent`, `DisableEvent`, `TestEvent` resetting, `DeliverEvent`). A
card answers EvSpNEW until written, as after power-on, but `_bu_init`
leaves both known: on DuckStation, LSD's first `_card_info` after boot
gets no EvSpNEW (its `_card_clear` is never called, no "memory card was
swapped" message). With EvSpNEW on the first access the port showed that
message before the first SAVE or LOAD, which the console does not; now
SAVE, LOAD and LOAD on an empty card ("no file on the memory card") look
as on DuckStation.

Windows: the i686 MinGW `lsd.exe` runs under Wine 11.18 (staging) inside
`xvfb-run`, with a scratch prefix. Nothing in the port or psyz needed a
fix. SDL_GPU picks Direct3D 12 (vkd3d); Xvfb gives it no swapchain, so
presentation is off and the debug server's screenshots are the picture.
It reaches the title menu, plays a dream with its music (-30.3 dB RMS,
console -30.24 dB, in real time through SDL's disk audio driver), SAVEs,
and LOADs the save after a restart. Two traps, both Wine's: a fresh
prefix waits on the Mono/Gecko installer (`WINEDLLOVERRIDES=
"mscoree,mshtml="`), and Wine does not pass `SDL_*` variables to Windows
programs, so `SDL_AUDIO_DRIVER` has to be set in the prefix's
`HKCU\Environment`. The release `lsd.exe` links MinGW's runtime
statically; on Windows, startup errors also show in a message box, and
`disc/` is also looked for beside the executable.

### Controls and speed (task 07)

Speed (2026-10-04). The dream's FrameClock (`sDreamAuxFrameClock`,
`frameCount` at +0x0C, paused flag at +0x10) counts one per game tick.
On DuckStation (no overclock, fast boot only) it was read through the
GDB stub, with no breakpoints: interrupt, read it with libetc's `Vcount`
and `sDreamAuxStage`, continue, every half second while a script walked
at random through about 25 minutes of dreams (seven stages). Emulated
time is `Vcount` / 59.94, so the stub's pauses do not count. On the port
the same variables were read from `/proc/<pid>/mem` against the wall
clock (`LSD_VSYNC=off`). Ticks per second:

| stage | 0 | 1 | 2 | 3 | 4 | 5 | 6 | all |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DuckStation | 14.3 | 16.7 | 13.6 | 13.8 | 12.8 | 13.5 | 17.8 | 13.8 |
| port | 20.0 | 19.8 | 20.0 | 20.0 | 20.0 | 20.0 | | 20.0 |

Most console ticks take 4 or 5 vblanks instead of the 3 that `VSync(3)`
asks for. A cross-check with no stub traffic at all: DuckStation's
lossless capture (mkv, ffv1) while turning in the first room of day 1
has a new picture every 5 or 6 vblanks, 10.5 ticks per second. The
operator chose to keep 20 for now (PLAN "Later", `speed`).

Move bug. On the way, walking back from the first room's start moved the
player forward on the port, and forward and back moved about 280 units a
tick where the console moves 64 (cottage walk; 128 outdoors, 384
running). `Actor__MoveLocalZ` wrote the step into `sActorLocalMoveZ`, a
static separate from `sActorLocalMove[2]`, while `addLocalTranslation`
reads x, y and z through `&sActorLocalMove[0]`: adjacent in retail's
small data, not on the host. lsddecomp `host-actor-local-move` makes it
one `s16[3]` (the PS1 build still matches). Afterwards forward, back,
strafe and run step 64, 64, 64 and 384 per tick in the cottage, as on
DuckStation.

Controls. psyz `pads-keyboard-map` adds `Psyz_PadsSetKeyboardMap`; the
port's `src/controls.c` reads `controls.ini` from the saves folder (README,
"Controls"), writes it with the defaults when missing, and reports lines
it does not understand. The default layout, chosen by the operator: WASD
or arrows for the d-pad, Q/E L2/R2, Shift cross, R/F triangle/square, Z/C
L1/R1, Space or Enter circle, Backspace cross, Tab SELECT, Escape START.
Checked with real key events (xdotool) on Linux (GL build on Xvfb) and
under Wine (`lsd.exe`): menu cursor, START from the menu, walk, back,
turn, strafe, run (moveMode 4 only while Shift is held), look, glance,
Escape pauses and resumes without quitting, pause + Tab + R back to the
title, a whole day to the graph, SAVE with Space on the comment entry,
the close button (WM_DELETE_WINDOW, exit 0); `layout = classic` (arrows
move, Escape quits); a hand-edited file (an override, unknown names
reported by line). Gamepads were not tested (none attached); their code
is unchanged. The dream's SELECT + triangle only ends a dream from the
pause screen (`ObjM__UpdateCloseReadyFlag` needs `pauseSetupStep`).

Mouse look, for later: `DreamSys`'s turn is a fixed 6 degrees per tick
(`sTurnRotations`, `turnCommand`), the glance (`lookYaw`) steps 45
degrees a tick up to 180 and springs back, and look up/down
(`lookOffset`) moves 600 a tick up to 9000; all are set from held pad
buttons in
`DreamSys__OnPadEvent`. A mouse would need a hook that adds an arbitrary
yaw through `updateRotation` (as `StepLookYaw` does with its patched
row of `sTurnRotations`).

### Neighbouring statics (task 08)

The decomp is byte-exact on the PS1, where every static sits in retail's
order. Code that reaches one static through another's address works there
and breaks silently on the PC, which places statics as GCC likes: task
07's move bug was one. Task 08 looked for the rest.

The tool is an ASan and UBSan build, `-DLSD_SANITIZE=address,bounds`
(`build-i686-asan`): global redzones turn a read past a static's end
into a report, and the run goes on (`ASAN_OPTIONS=halt_on_error=0`).
psyz's SDK files have to be instrumented too, because the game hands its
vectors to libgte and libgs: with only the game's C instrumented, the old
move bug went unreported. Instrumenting all of psyz (renderer, movie
decoder, SPU) slows the game so much that the debug server stops
answering in the intro movies, so those stay out. Checked first against
lsddecomp `863ea0777^`: the first step forward reports the move bug
(`SceneNode__RotateLocalVector`, reading `sActorLocalMoveZ`'s
neighbour).

Found and fixed (lsddecomp `host-neighbour-statics`, PS1 bytes unchanged):

- `StyleBuildDecorSet` and `StyleUpdateDecorSet` copy the decoration
  set's position and size whole, through `sStyleDecorPosX` and
  `sStyleDecorSizeW`, but y and h were separate statics. On the PC GCC
  made the two never-written firsts `.rodata` constants and dropped the
  seconds: v0.1 reads the position as (-100, 773874725) and the size as
  (320, -100), against the console's (-100, -60) and (320, 144). Now a
  `BoxFillPos` and a `BoxFillSize`.
- `StyleFillEffectKind0` picks a spawn height by `rand() % 5` from index
  1, so a pick of 4 reads the word after `sStyleSpawnYChoices[4]`: on the
  PS1 the first of `sStyleStage05Configs` (0x0A0A0200), on the PC
  another symbol's word (GCC reverses `.data`). Now one
  `StyleSpawnYBlock` holds both, and the pick reads through it.

Seen and harmless: `Viewport__DrawNode` forms `&coord.m[3][0]` as its
loop's end (UBSan's only other report).

Coverage, all on the fixed build with no other report: boot, intro,
title menu; 15 days walked with random input (forward, run, turn, strafe,
look, link button); 25 more with the day set from gdb at
`DreamSys__StartDay`, 15 of them special days (their movies); pause and
resume; SAVE with the comment entry; LOAD; GRAPH; FLASHBACK, unlocked
from gdb, walked for 28 minutes at full speed (stopped by the script's
timeout, not at its end). Read in the source:
every cast of a static's address to another type (the `(LongVec3 *)` sprite
and box positions are only the method slot's type; the receivers take two
words), the comments that mention neighbours or address order, and the
symbols `HOST_BUILD` defines that the PS1 places by address (each used
alone). GCC's `-Warray-bounds=2`, `-Wstringop-overflow=4` and
`-Wstringop-overread` on the game's C find only the harmless
`Viewport__DrawNode` one.

Headless driving notes: a gdb `dprintf` writes to gdb's buffered stdout,
so a marker can show up seconds late; the `call` style (`fprintf` in the
game) deadlocks against psyz's logging. Python breakpoints that
`gdb.write` and `gdb.flush` are prompt and safe. The pause text blinks:
one screenshot can miss it. The title menu falls into the attract movie
after about 10 s, so menu steps belong in one script that reacts to the
screen (`lsd_drive.classify` against local references).

### Widescreen (task 09)

On branch `widescreen` (lsd-port and the psyz fork), not on `main`.

How: anamorphic. The PS1's VRAM keeps its layout (the framebuffers sit
beside the texture pages, so a wider framebuffer would overlap them).
psyz's software GTE multiplies every projected X by a 16.16 factor
around OFX (`Psyz_GteSetScreenXScale`, in `RTP_VERTEX`, so RTPS, RTPT and
the libgte/libgs calls built on them), and the SDL3 backends multiply the
presented aspect ratio by a stretch (`Psyz_VideoSetDisplayStretch`); a
third call (`Psyz_VideoSetWindowAspect`) opens the window at 16:9
(1280x720). The port (`src/widescreen.c`, built with the game's C) wraps
`gDayTaskMethods.onInit`/`onDeinit` and turns both on for the life of a
DayTask, which is every dream; everything else (title menu, graph,
movies, TIM images) is drawn while they are off, so it is 4:3 with bars.
No change in lsddecomp.

What draws where at 16:9:

- 3D (TMDs through `TransformAndCullPoly`/`DIVPOLYGON`): squeezed, so
  right. A 360-degree turn in the first room, pose for pose against
  4:3, shows the same centre and more at the sides.
- Sprites: psyz's `GsSortSprite` draws a scaled or rotated sprite as a
  POLY_FT4 through the GTE, so it is squeezed too (position and size);
  `Viewport__DrawNode` projects world sprites itself (x * projH / z),
  but hands GsSortSprite that as the pivot, which the GTE path squeezes.
  A plain sprite (scale 1, no rotation) is a SPRT and is not squeezed:
  the pause text (CharSprite) is stretched by 4/3, and a world sprite
  at scale 1 would sit 4/3 too far from the centre (StyleEffect's
  VariantSprites set their scale every frame, so they take the GTE
  path; no plain world sprite was seen). Sending plain sprites
  through the GTE as well was tried and dropped: at 1x its 8x8 glyphs
  lose texel columns, and even at 4x the POLY_FT4 path's texel rounding
  garbles them ("Pause" reads "False").
- Box fills and fades are 2D across 320: they fill the 16:9 screen.
- Edges: the map draws a footprint of 20x20 cells (2048 units each)
  ahead of the player, axis-aligned by quadrant and shifted toward the
  look direction (`StageMap__ComputeFootprintFromRotation`); per polygon
  the game culls only on GTE flags, winding and depth cue, and
  `DIVPOLYGON` clips to 320x240 in screen space, which the squeeze
  keeps. Walking six stages at 16:9 (the first room, the town, Kyoto's
  fences, the desert, Violence District at night, a red stage) showed no
  missing ground or walls at the sides: fog ends the view before the
  footprint's sides. A stage with little fog, looked at diagonally,
  might still show the footprint's corners; not seen.

`--resolution N` is psyz's internal resolution. At 4 the dream is sharp,
movies and the menu keep their pixels, and the debug server's
screenshots stay 320x240 (capture the window to see it).

Checked: psyz host tests (317 passed, 2 skipped, Debug, with a new
`rot_trans_pers_screen_x_scale`); the GL build in Xvfb at 16:9, window
captures of the intro movies, menu, dream, pause and links, at 1x and
4x; the Windows build under Wine at 16:9 reaches the dream with the
squeeze on and a 1280x720 display. The default (no `--aspect`) runs the
same code as before: the scale is 0x10000 (the multiply is skipped),
the stretch 1 and the window 1280x960.
