# psyz changes waiting to go upstream

Topic branches on the fork (`brian-oblivion/lsd-psyz`), each started from
`Xeeynamo/psyz` `main` and merged into the fork's `main`, which `psyz/`
pins. Each section is the pull-request description for when one is
wanted. Keep it current when a branch changes; delete a section once its
PR is merged.

Common to all of them: found while compiling LSD: Dream Emulator's
decompilation against psyz. Host tests (`psyz/tests`, Linux, SDL3
offscreen) pass on every branch, and the changed files are
clang-format clean. **Not tried on the PS1 target:** no `mipsel-none-elf`
toolchain or console was at hand, so anything that is also compiled into
`tests/target/ps1` is unbuilt there (noted per branch).

## `libgs-drawbuff`: draw into the current buffer

Already on the fork before task 01 (commit `686b409`). `GsSetDrawBuffClip`
and `GsSetDrawBuffOffset` were stubs, so the drawing area was never set
and everything drawn through libgs was clipped away. They now follow
Psy-Q. Tests in `tests/test_libgs.c`.

## `libc-host-headers`: rand(), itoa() and convert.h on hosts

- `rand.h` and `convert.h` redeclared `rand`, `srand`, `atoi`, `atol`,
  `strtol` and `strtoul` with PS1 prototypes, which conflict with the
  host's `<stdlib.h>`: a unit including both did not compile. `rand.h`
  also typedef'd `u_long` as `unsigned long`, wrong on 64-bit Windows.
- `convert.h` now takes the host's declarations.
- `rand()`/`srand()` cannot: the host's generator and `RAND_MAX` differ
  from libc2's, and games' random sequences should match the console's.
  They become `psyz_rand`/`psyz_srand` (as `open` and friends are
  `psyz_open`...), implemented as libc2 does: checked against libc2's
  `rand.o`, the same LCG, bits 16-30, and a seed that starts at 0 (it
  lives in `.sbss`), not the 1 the manual gives.
- `itoa(n)`: libc2's formats `n` into a 16-byte static buffer (checked
  against `itoa.o`). No Psy-Q header declares it, and MinGW's
  `itoa(int, char *, int)` conflicts with a game's own declaration, so it
  is `psyz_itoa`, declared in `libc.h` under the `itoa` name.
- New `tests/test_libc.c` (host only; not added to the PS1 test build).

## `gte-macros-more`: twelve more inline GTE macros

`gte_dpct`, `gte_ncds`, `gte_rtir`, `gte_llir`, `gte_stdp`, `gte_stsxy2`,
`gte_ReadRotMatrix`, and `gte_stsxy3_f3/f4/ft3/ft4/g4/gt4`, on both sides
of `__psyz`.

- psyz side: through the existing GTE operations (DPCT, NCDS, MVMVA) and
  data-register reads; the primitive stores write the vertex fields, so
  they suit psyz's primitive layouts.
- PS1 side: the COP2 encodings. `gte_rtir` (0x4A49E012) and `gte_llir`
  (0x4A4BE012) differ only in the MVMVA matrix, rotation or light.
- Tests in `tests/test_libgte.c`, with values worked out by hand or
  cross-checked (DPCT against three DPCS). That file is in the PS1 test
  build, so **the new PS1 macros and these tests are unbuilt and unrun on
  the PS1**; a run there is the real check of the hand-derived values.
- Found on the way, not changed: `NormalColorDpq()` runs NCDS with
  `sf=0` and saturates where `gte_ncds` gives the expected colour.

## `libgte-decls`: ONE, MulMatrix2, ApplyMatrixSV, ApplyMatrixLV

Psy-Q's libgte.h has them and games use them; without the declarations a
current GCC rejects the implicit declarations. Declarations only: the
functions have no body yet, so a caller gets an undefined symbol.

## `libgs-sdk-header`: Psy-Q's declarations and layouts

Two commits.

- Signatures: `GsSortSprite`, `GsSortBg`, `GsSortBoxFill`, `GsSortLine`,
  `GsSortGLine` take the OT position (`pri`); `GsInitCoordinate2(super,
  base)`; `GsLinkObject3/4/5` take the address, handler and object
  number; `GsSortObject3/4/5` take the shift (and a scratch area);
  `GsMulCoord2(m1, m2)`; `GsSetFlatLight`/`GsSetRefView2(L)`/`GsSetView2`
  return int.
- Layouts: `GsIMAGE` is what `GsGetTimInfo` fills (pmode, positions,
  sizes, pointers); `GsF_LIGHT`'s direction is three ints (games keep the
  light in their own structures and pass it cast); `GsFOGPARAM.dqa` is a
  short; `GsOBJTABLE2` is the object array with its counts.
- Added: the `TMD_P_*` packet types and `TMD_STRUCT`, `GsZCLIP`, the
  remaining attribute bits (`GsFOG`, `GsMATE`, `GsDIVn`, `GsROTOFF`, the
  `GsA*` rates, `GsALON`, with `GsAON` kept as an alias), `GsTMDFlagGRD`,
  `GPU_COM_*`, declarations of `GsSetLightMode`, `GsSetLsMatrix`,
  `GsSetNearClip`, `GsSetFarClip`, `GsGetTimInfo`; `GsIDMATRIX`,
  `GsLIGHT_MODE` and `GsOUT_PACKET_P` are global.
- None of the changed functions had a body, so only declarations change.
- New `tests/test_libgs_types.c`: TMD packet sizes against the file
  format, the light's layout, the identity matrix (host only).

## `libetc-scratchpad`: getScratchAddr() on psyz

On psyz the scratchpad is a 1 KiB array and `getScratchAddr()` indexes it
in 32-bit words, as on the console; the PS1 side keeps 0x1F800000. The
test is in `tests/test_libapi.c`, which is in the PS1 test build (unrun
there).

## `kernel-write-buffer`: write() takes any buffer

The kernel's `write()` takes a void pointer and games pass structures;
`psyz_write` took `char *` everywhere but Windows. Now `const void *` on
Unix and PSP too.

## `gpu-32bit-flush`: 32-bit targets drew nothing

On 32-bit targets `GPU_Enqueue`'s fast path (#107) dispatches primitives
straight into the draw buffer and leaves them there. `Psyz_GpuExeque`
began with `Draw_ResetBuffer`, so the next `DrawSync`, `LoadImage` or
`StoreImage` dropped them undrawn: on i686 Linux only block fills and
VRAM transfers reached the VRAM. Exeque now flushes the buffer before
resetting it. Upstream `main`'s host tests at i686: 211 passed, 41
failed before, 248/4 after (the four are the `bu`/`truncation` file
tests, which fail on x86_64 too); x86_64 unchanged at 248/4.

## `libcd-stream`: CdRead2 and the St* streaming calls

`CdRead2` was a stub returning 0 (games that retry it hang), and the
`St*` calls were stubs. `CdRead2(CdlModeStream)` now starts a stream at
`CD_pos`, its XA audio going through the existing `CdlModeRT` path.
`StGetNext` assembles each video frame from the stream's data sectors
(`StHEADER` + 2016 bytes each) in psyz's own buffer, and hands it out
once a drive started at `CdRead2` (single or double speed, by
`VSync(-1)`'s clock) would have read it, so movies play at their rate.
`StFreeRing`, `StClearRing`, `StUnSetRing`, `StSetStream` (start and end
frame, callbacks) and `StSetRing` go with it; `CdlPause`/`CdlStop` end
the stream. No host test: it needs a CD image with an STR file (checked
with LSD: Dream Emulator's movies).

## `libpress-mdec`: a software MDEC and the BS VLC decode

`DecDCTvlc` decodes BS version 1 and 2 frames (version 3 logs once and
is not decoded) into run-level codes; `DecDCTin`/`DecDCTout` decode
those into 15- or 24-bit macroblocks: the default quantization table,
the IDCT and the YCbCr conversion psx-spx documents, the AC codes being
MPEG-1's table B.14 (111 codes, prefix-free, checked). Transfers finish
before the call returns, and the `DecDCTout` callback runs before
`DecDCTout` returns, so the usual strip-by-strip loop driven from that
callback recurses once per strip. `DecDCTGetEnv`/`PutEnv`, the `Sync`
calls and `DecDCTBufSize` are filled in. No host test yet (a synthetic
BS frame would do); checked on LSD's 320x240 frames by eye.

## `libsnd-vabhdr`: SsUtGetVabHdr in C

Assembly only before. Copies an open VAB's header, as `SsUtGetProgAtr`
copies a program's attributes; -1 for a VAB that is not open.

## `libgs-2d`: the 2D sorts, GsGetTimInfo, GsInit3D, PSY-Q's GsClearOt

Stacked on `libgs-sdk-header` and `libgs-drawbuff` (it merges both: it
needs `GsIDMATRIX`/`GsLIGHT_MODE` and the draw-buffer offset). Written
from PSY-Q 3.3's LIBGS objects (`2d_sp0`, `2d_bg0`, `2d_com0`, `2d_box0`,
`gs_104`, `gs_113`, `gs_122`, `gs_013`) and the matching game.

- `GsClearOt` clears in reverse (`ClearOTagR`) with the work pointer on
  the last tag, so pri 0 is drawn last, in front.
- `GsInit3D` puts the origin at the screen's centre (and sets the 3D
  clip and light mode); `GsSetOrign` sets it.
- `GsSortSprite`: (x, y) is where the pivot (mx, my) goes; DR_TPAGE +
  SPRT unless rotated, scaled or flipped (or always with attribute bit
  27), else a POLY_FT4 through the GTE at the projection distance.
- `GsSortBg`: the scrolled, wrapping window of the map, one POLY_FT4 per
  visible cell, with PSY-Q's UV rules for plain and transformed BGs and
  the cell flips. `GsSortBoxFill`: DR_TPAGE + TILE.
- `GsGetTimInfo`; `ReadGeomScreen`/`ReadGeomOffset` in libgte.
- `GsCELL`'s u and v are bytes (PSY-Q's cell is 8 bytes).

Existing libgs tests pass; the new calls have no host test yet.

## `libapi-vsync-n`: VSync(n) waits n vertical blanks

`VSync(n)` with n > 1 presented and paced one frame, so a game running at
20 fps on `VSync(3)` ran three times too fast, and every timer counted in
its frames ran out early (LSD's title menu closed after about 3 s instead
of 10). `Psyz_VideoVSync(n)` now presents and waits until n blanks have
passed since the previous blocking call: the SDL3 limiter scales its
target by n; with driver VSync the timer covers the blanks after the
first. The PSP backend waits the extra blanks with
`sceDisplayWaitVblankStart` (**unbuilt**: no PSP toolchain at hand). New
gpu test `vsync_n_waits_n_vblanks` fails before, passes after.

## `libgte-matrix`: MulMatrix2, ApplyMatrixSV, ApplyMatrixLV, Square0

Written from Sony's `mtx.o`, `mtx_00.o` and `smp_00.o`: each loads the GTE
as Sony's does and runs psyz's MVMVA/SQR, so MAC, IR and FLAG come out the
same, with the rotation matrix left loaded. `ApplyMatrixLV` splits each
32-bit component into a 15-bit high and low part, as Sony's does
(exact below 2^30, wrapping above). `MulMatrix2` writes the IR3 sign into
the padding after m[2][2], as Sony's 32-bit `swc2` does. The three
declarations are the same hunks as `libgte-decls`, so the two merge
cleanly. 13 tests in `test_libgte.c`.

## `libgte-rcpoly`: RCpoly* polygon subdivision

`RCpolyF3/F4/FT3/FT4/G3/G4/GT3/GT4` in a new `src/psyz/libgte_div.c`,
from Sony's `div*a.o`: recursive division to `ndiv` levels with plain
`(a+b)>>1` midpoints (UV and colour too), new vertices projected with
RTPT, per-level near-Z and screen-window rejection, primitives linked at
the head of `divp->ot` with psyz's `addPrim`. `ndiv` 0 returns and
above 5 is clamped (Sony's would never end, or overrun `cr[]`). 8 tests.

## `libgs-3d`: coordinates, the reference view, flat lights, TMD linking

Stacked on `libgs-2d` and `libgte-matrix`. `GsInitCoordinate2`,
`GsGetLw`, `GsGetLs`, `GsGetLws`, `GsMulCoord2`, `GsSetLsMatrix`,
`GsSetLightMatrix`, `GsSetRefView2`, `GsSetProjection`, `GsSetNearClip`,
`GsSetAmbient`, `GsSetFlatLight`, `GsSetLightMode`, `GsMapModelingData`
and `GsLinkObject4`, from Sony's `gs_10x`/`gs_13x`/`matrix.o` (and
`GsLinkObject4`'s jump table): the coordinate walk caches `workm` per
frame (`flg == PSDCNT`), `GsSetRefView2` scales the two points to 15 bits
and wraps its squared distances as the console does, and `GsInitGraph`
now sets `GsIDMATRIX2` (aspect in m[1][1]) and clears the light
matrices. `GsMapModelingData` maps TMD offsets in place, so it needs
data below 4 GB: its two tests skip on 64-bit hosts. 9 tests (`gs3d`).

## `libgte-fog`: SetFogNear, and InitGeom's DQB

`InitGeom` set DQB to 0x140 where Sony's sets 0x1400000, and `SetFogNear`
was a stub; depth cueing saturated IR0 and raised FLAG bit 12 on every
vertex. `SetFogNear(a, h)`: DQA = -(a*320)/h, DQB = 0x1400000, as Sony's
`fog_01.o`. Two tests.

## `libgte-stflg4`: gte_stflg_4

Sony's `inline_c.h` has `gte_stflg` (the whole FLAG) and `gte_stflg_4`
(bit 18 alone, SZ3/OTZ saturated). psyz had only the first; added for
the C host macros and the PS1 inline-asm block (**unbuilt** on PS1). One
test.

## `libsnd-seq`: the SEQ sequencer and the voice manager's key-on paths

`SsSeqOpen`, the per-tick player and every MIDI event handler were
stand-ins, and so was `_SsVmAlloc`, which returned -1: no SEQ played and
`SsUtKeyOn` never found a voice. Host C for them, under `__psyz` beside
each file's `INCLUDE_ASM`, as the decompiled files already do:

- the voice manager: `vm_aloc1`, `vm_key`, `vm_seq`, `vm_vol`, `vm_prog`,
  `vm_pb`, `vm_no1`, `vm_no2`, `vm_noise`, `vm_autov`, `vm_don` and
  `SeAutoPan`;
- the sequencer: `seqinit`, `ssopenq`, `ssopenqj` (SsSeqOpen and the SEQ
  header), `midiread` (the tick and the event decoder, through
  `SsFCALL`), `midinote`, `midiprog`, `midibend`, `midimeta` (tempo, end
  of track), `midicc` and `cc_*` (the controllers, loops and the mark
  callback), `de_*` and `ccadsr` (the data entry's VAB attributes),
  `next`;
- the calls around them: `sspause`, `ssreplay`, `ssdecres`, `sssm`,
  `ssmark`, `ut_autov`, `ut_rfb`, `ut_sva`, and libspu's `SpuSetMute`.

Written from libsnd **3.3**, the build LSD: Dream Emulator links (its
decompilation carries most of it as C; the rest was read from the
game's code and Sony's 3.3 objects), and laid over this library's 4.x
`SeqStruct`, whose fields it names in comments: much of the 3.3 record
is the 4.x one 8 bytes lower. **Not checked against 4.7.** 3.3's own
quirks are kept where its code has them (`_SsVmSetVol` reads the tone's
volume at the voice's tone index alone; `vmNoiseOn` reads a score for
sound effects too, which here takes full volume instead of reading past
`_ss_score`). `_SsSndTempo` (accelerando, ritardando) stays a stand-in.

Checked in LSD: Dream Emulator: the dream's SEQ opens, plays and loops at
its end of track, recorded through SDL's `disk` audio driver (not yet
listened to). No new tests: the sequencer needs a SEQ and a VAB.

## `libetc-interrupt-callback`: InterruptCallback by interrupt number

`InterruptCallback` bound its handler to the counter `SetRCnt` last
programmed, and only a non-NULL one. `SsEnd` puts back the VBLANK
handler `SsStart` replaced, usually NULL, so the sequencer's stayed
installed; the next `SsStart` saved it as the handler to chain to, and
`_SsTrapIntrVSync` called itself until the stack ran out. A game that
closes its last VAB and later opens another crashed there (LSD: Dream
Emulator, starting a day). Interrupt 0 (VBLANK, which RCntCNT3 counts)
and 6 (root counter 2) now have their own slots and NULL empties one;
other numbers keep the old binding. `libetc.h` declares
`InterruptCallback`, which `ssstart.c` called undeclared, taking its
pointer result as an int. 4 tests (`interrupt_callback`, host only).

## `libsnd-keyonnow-pan`: _SsVmKeyOnNow pans the right way

The host `_SsVmKeyOnNow` set the left level from the right one for a pan
below 64 and the right from the left above it: every panned key-on came
out on the wrong side, at the other side's level. Sony's code (3.3's
`SpuVmKeyOnNow`, and `SetAutoPan` here) scales the right channel down
for a pan below 64 and the left above it, each from itself.

## `libapi-setmem`: SetMem

Games set the RAM size at boot (`SetMem(2)`); it had no definition. The
host has no such limit, so it does nothing.

## `kernel-open-flags`: open() on Unix sets the access mode

`psyz_open` (`plat_unix.c`) put `O_RDWR`/`O_RDONLY`/`O_WRONLY` into the
PS1 flag instead of the `open(2)` flags, so every file it opened without
`FCREAT` was read-only: a game that creates its save, closes it and
reopens it with `FWRITE` wrote nothing. `FCREAT` now implies `FWRITE`, as
`plat_win.c` has it, and creates through `open(2)`, still truncating as
`creat()` did. 1 test (`bu::write_to_existing_file`).

The host tests run in a Debug build: the `bu` group's teardown deletes
its files inside `assert()`, so a Release build leaves them behind and
the next run fails `bu` and `truncation`. With these five branches the
fork's `main` passes 313 tests at x86_64 (2 skipped) and 314 at i686,
where `gte::read_rot_matrix_reads_rotation_and_translation` still fails
as before (it compares uninitialised padding).
