# psyz coverage of LSD's SDK calls (2026-10-02)

Measured against psyz `388091e` (upstream, 2026-09-30); the fork's `main`
(`6bd06da`) adds no SDK functions on top of that.

**The surface (146 functions):** every symbol that lsddecomp's game objects (`build/src/**/*.o`,
excluding `src/psyq/`) leave undefined, and that is defined by Sony's library
objects (`lib/`) or by lsddecomp's carried SDK C (`src/psyq/`). Game data
symbols (`s*`, `gp*`) are excluded: they are the game's own small data, in C.
The plan's earlier "~290" also counted Sony code calling Sony code.

**Classification:** a function is *implemented* when psyz's PC sources
(`psyz/src/psyz`, `psyz/src/platform`, or a `decomp/src` file listed in
`psyz/CMakeLists.txt`) define it with a body that is not just
`NOT_IMPLEMENTED`. The check was a regex over the sources, so treat this as
a to-do list to confirm, not a verdict. Once the game's C links on the host,
the linker's undefined symbols are the real measurement and replace this
file.

Libc names (host libc covers them, but PS1 `rand` differs from glibc's and
`itoa` is not in glibc) and the kernel's file calls (memory card `bu00:`;
these clash with POSIX/CRT names) are marked.

## Absent from psyz (3)

| function | note | where in psyz |
| --- | --- | --- |
| `GsIDMATRIX` |  |  |
| `GsLIGHT_MODE` |  |  |
| `delete` | kernel file I/O |  |

## Stubbed in psyz (logs "not implemented") (12)

| function | note | where in psyz |
| --- | --- | --- |
| `CdRead2` |  | `psyz/src/psyz/libcd.c` |
| `DecDCTReset` |  | `psyz/src/psyz/libpress.c` |
| `DecDCTin` |  | `psyz/src/psyz/libpress.c` |
| `DecDCTout` |  | `psyz/src/psyz/libpress.c` |
| `DecDCToutCallback` |  | `psyz/src/psyz/libpress.c` |
| `DecDCTvlc` |  | `psyz/src/psyz/libpress.c` |
| `SetFogNear` |  | `psyz/src/psyz/libgte.c` |
| `SsSeqOpen` |  | `psyz/src/psyz/libsnd.c` |
| `StFreeRing` |  | `psyz/src/psyz/libcd.c` |
| `StGetNext` |  | `psyz/src/psyz/libcd.c` |
| `StSetRing` |  | `psyz/src/psyz/libcd.c` |
| `StSetStream` |  | `psyz/src/psyz/libcd.c` |

## Partly implemented in psyz (5)

| function | note | where in psyz |
| --- | --- | --- |
| `DisableEvent` |  | `psyz/src/psyz/libapi.c` |
| `EnableEvent` |  | `psyz/src/psyz/libapi.c` |
| `_bu_init` |  | `psyz/src/psyz/libcard.c` |
| `_card_info` |  | `psyz/src/psyz/libcard.c` |
| `_card_load` |  | `psyz/src/psyz/libcard.c` |

## Not in psyz's PC build: only assembly in psyz's PSY-Q decomp (53)

| function | note | where in psyz |
| --- | --- | --- |
| `ApplyMatrixLV` |  | `decomp/src/libgte/mtx_004.c` |
| `ApplyMatrixSV` |  | `decomp/src/libgte/mtx_06.c` |
| `EnterCriticalSection` |  | `decomp/src/libapi/a36.c` |
| `ExitCriticalSection` |  | `decomp/src/libapi/a37.c` |
| `GsGetLs` |  | `decomp/src/libgs/gs_134.c` |
| `GsGetLws` |  | `decomp/src/libgs/gs_135.c` |
| `GsGetTimInfo` |  | `decomp/src/libgs/gs_122.c` |
| `GsInit3D` |  | `decomp/src/libgs/gs_104.c` |
| `GsInitCoordinate2` |  | `decomp/src/libgs/matrix.c` |
| `GsLinkObject4` |  | `decomp/src/libgs/objt.c` |
| `GsMapModelingData` |  | `decomp/src/libgs/gs_105.c` |
| `GsSetAmbient` |  | `decomp/src/libgs/gs_110.c` |
| `GsSetFlatLight` |  | `decomp/src/libgs/gs_107.c` |
| `GsSetLightMatrix` |  | `decomp/src/libgs/matrix.c` |
| `GsSetLightMode` |  | `decomp/src/libgs/gs_108.c` |
| `GsSetLsMatrix` |  | `decomp/src/libgs/matrix.c` |
| `GsSetNearClip` |  | `decomp/src/libgs/gs_101.c` |
| `GsSetProjection` |  | `decomp/src/libgs/gs_106.c` |
| `GsSetRefView2` |  | `decomp/src/libgs/gs_131.c` |
| `GsSortBg` |  | `decomp/src/libgs/2d_bg0.c` |
| `GsSortBoxFill` |  | `decomp/src/libgs/2d_box0.c` |
| `GsSortSprite` |  | `decomp/src/libgs/2d_sp0.c` |
| `MulMatrix2` |  | `decomp/src/libgte/mtx_04.c` |
| `RCpolyF3` |  | `decomp/src/libgte/divf3a.c` |
| `RCpolyF4` |  | `decomp/src/libgte/divf4a.c` |
| `RCpolyFT3` |  | `decomp/src/libgte/divft3a.c` |
| `RCpolyFT4` |  | `decomp/src/libgte/divft4a.c` |
| `RCpolyG3` |  | `decomp/src/libgte/divg3a.c` |
| `RCpolyG4` |  | `decomp/src/libgte/divg4a.c` |
| `RCpolyGT3` |  | `decomp/src/libgte/divgt3a.c` |
| `RCpolyGT4` |  | `decomp/src/libgte/divgt4a.c` |
| `SetMem` |  | `decomp/src/libapi/c159.c` |
| `Square0` |  | `decomp/src/libgte/smp_00.c` |
| `SsSeqPause` |  | `decomp/src/libsnd/sspause.c` |
| `SsSeqReplay` |  | `decomp/src/libsnd/ssreplay.c` |
| `SsSetMute` |  | `decomp/src/libsnd/sssm.c` |
| `SsUtGetVabHdr` |  | `decomp/src/libsnd/ut_gvh.c` |
| `StClearRing` |  | `decomp/src/libcd/c_002.c` |
| `StUnSetRing` |  | `decomp/src/libcd/c_003.c` |
| `atoi` | libc | `decomp/src/libc2/atoi.c` |
| `close` | kernel file I/O | `decomp/src/libapi/a54.c` |
| `itoa` | libc | `decomp/src/libc2/itoa.c` |
| `lseek` | kernel file I/O | `decomp/src/libapi/a51.c` |
| `memcpy` | libc | `decomp/src/libc2/memcpy.c` |
| `open` | kernel file I/O | `decomp/src/libapi/a50.c` |
| `printf` | libc | `decomp/src/libc2/printf.c` |
| `rand` | libc | `decomp/src/libc2/rand.c` |
| `srand` | libc | `decomp/src/libc2/rand.c` |
| `strcat` | libc | `decomp/src/libc2/strcat.c` |
| `strcpy` | libc | `decomp/src/libc2/strcpy.c` |
| `strlen` | libc | `decomp/src/libc2/strlen.c` |
| `strstr` | libc | `decomp/src/libc2/strstr.c` |
| `write` | kernel file I/O | `decomp/src/libapi/a53.c` |

## Implemented in psyz (data) (2)

| function | note | where in psyz |
| --- | --- | --- |
| `GsOUT_PACKET_P` |  | `psyz/src/psyz/libgs.c` |
| `read` | kernel file I/O | `psyz/tests/test_libgte.c` |

## Implemented in psyz (71)

| function | note | where in psyz |
| --- | --- | --- |
| `CdControl` |  | `decomp/src/libcd/sys.c` |
| `CdControlB` |  | `decomp/src/libcd/sys.c` |
| `CdControlF` |  | `decomp/src/libcd/sys.c` |
| `CdFlush` |  | `decomp/src/libcd/sys.c` |
| `CdInit` |  | `psyz/src/psyz/libcd.c` |
| `CdIntToPos` |  | `decomp/src/libcd/sys.c` |
| `CdPosToInt` |  | `decomp/src/libcd/sys.c` |
| `CdRead` |  | `psyz/src/psyz/libcd.c` |
| `CdReadSync` |  | `psyz/src/psyz/libcd.c` |
| `CdSearchFile` |  | `decomp/src/libcd/iso9660.c` |
| `CdSetDebug` |  | `decomp/src/libcd/sys.c` |
| `CdSync` |  | `decomp/src/libcd/sys.c` |
| `CdSyncCallback` |  | `decomp/src/libcd/sys.c` |
| `ClearImage` |  | `decomp/src/libgpu/sys.c` |
| `CloseEvent` |  | `psyz/src/psyz/libapi.c` |
| `DrawSync` |  | `decomp/src/libgpu/sys.c` |
| `GetTPage` |  | `decomp/src/libgpu/prim.c` |
| `GsClearOt` |  | `psyz/src/psyz/libgs.c` |
| `GsDefDispBuff` |  | `psyz/src/psyz/libgs.c` |
| `GsDrawOt` |  | `psyz/src/psyz/libgs.c` |
| `GsGetActiveBuff` |  | `psyz/src/psyz/libgs.c` |
| `GsInitGraph` |  | `psyz/src/psyz/libgs.c` |
| `GsSetWorkBase` |  | `psyz/src/psyz/libgs.c` |
| `GsSortClear` |  | `psyz/src/psyz/libgs.c` |
| `GsSwapDispBuff` |  | `psyz/src/psyz/libgs.c` |
| `InitCARD` |  | `decomp/src/libcard/init.c` |
| `LoadImage` |  | `decomp/src/libgpu/sys.c` |
| `MoveImage` |  | `decomp/src/libgpu/sys.c` |
| `OpenEvent` |  | `psyz/src/psyz/libapi.c` |
| `OuterProduct0` |  | `psyz/src/psyz/libgte.c` |
| `PadInit` |  | `psyz/src/psyz/libetc.c` |
| `PadRead` |  | `psyz/src/psyz/libetc.c` |
| `PadStop` |  | `decomp/src/libetc/pad.c` |
| `ResetGraph` |  | `decomp/src/libgpu/sys.c` |
| `RotMatrix` |  | `psyz/src/psyz/libgte.c` |
| `SetFarColor` |  | `psyz/src/psyz/libgte.c` |
| `SpuSetCommonAttr` |  | `decomp/src/libspu/s_sca.c` |
| `SquareRoot0` |  | `psyz/src/psyz/libgte.c` |
| `SsEnd` |  | `decomp/src/libsnd/ssend.c` |
| `SsInit` |  | `decomp/src/libsnd/ssinit_c.c` |
| `SsQuit` |  | `decomp/src/libsnd/ssquit.c` |
| `SsSeqClose` |  | `decomp/src/libsnd/ssclose.c` |
| `SsSeqPlay` |  | `decomp/src/libsnd/ssplay.c` |
| `SsSeqSetCrescendo` |  | `decomp/src/libsnd/sscres.c` |
| `SsSeqSetVol` |  | `decomp/src/libsnd/ssvol.c` |
| `SsSeqStop` |  | `decomp/src/libsnd/ssstop.c` |
| `SsSetMVol` |  | `decomp/src/libsnd/sssmv.c` |
| `SsSetTableSize` |  | `decomp/src/libsnd/sstable.c` |
| `SsSetTickMode` |  | `decomp/src/libsnd/sstick.c` |
| `SsStart` |  | `decomp/src/libsnd/ssstart.c` |
| `SsUtAllKeyOff` |  | `decomp/src/libsnd/ut_ako.c` |
| `SsUtAutoVol` |  | `decomp/src/libsnd/ut_autov.c` |
| `SsUtGetProgAtr` |  | `decomp/src/libsnd/ut_gpa.c` |
| `SsUtGetVagAtr` |  | `decomp/src/libsnd/ut_gva.c` |
| `SsUtKeyOffV` |  | `decomp/src/libsnd/ut_keyv.c` |
| `SsUtKeyOn` |  | `decomp/src/libsnd/ut_key.c` |
| `SsVabClose` |  | `decomp/src/libsnd/vs_vab.c` |
| `SsVabOpenHead` |  | `decomp/src/libsnd/vs_vh.c` |
| `SsVabTransBody` |  | `decomp/src/libsnd/vs_vtb.c` |
| `SsVabTransCompleted` |  | `decomp/src/libsnd/vs_vtc.c` |
| `StartCARD` |  | `decomp/src/libcard/init.c` |
| `StoreImage` |  | `decomp/src/libgpu/sys.c` |
| `TestEvent` |  | `psyz/src/psyz/libapi.c` |
| `VSync` |  | `psyz/src/psyz/libapi.c` |
| `VSyncCallback` |  | `psyz/src/psyz/libetc.c` |
| `_card_clear` |  | `decomp/src/libcard/card.c` |
| `format` | kernel file I/O | `psyz/src/psyz/libapi.c` |
| `free` | libc | `psyz/tests/target/ps1/ps1_heap.c` |
| `malloc` | libc | `psyz/tests/target/ps1/ps1_heap.c` |
| `memset` | libc | `decomp/src/libgpu/sys.c` |
| `ratan2` |  | `decomp/src/libgte/ratan.c` |

