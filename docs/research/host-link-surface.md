# The host link surface (2026-10-02)

What the linker reports missing when the game's C (lsddecomp `3127186d5`)
links against psyz (fork `main` `8030744`) on Linux x86_64; i686 and
MinGW report the same list. It replaces the regex estimate in
`psyz-coverage-2026-10-02.md` as the platform layer's to-do list. Each
symbol here has a temporary stand-in in `src/stubs.c`; deleting a stand-in
shows whether psyz now provides it.

To reproduce: remove `src/stubs.c` from the `lsd` target and build; the
undefined references are this list.

## SDK functions psyz lacks (30)

The game links against psyz's headers; none of these has a body in psyz's
PC build. Eight of the first count's 38 are done (task 02): `GsInit3D`,
`GsGetTimInfo`, `GsSortBg`, `GsSortBoxFill`, `GsSortSprite` (psyz branch
`libgs-2d`), `SsUtGetVabHdr` (`libsnd-vabhdr`), `StClearRing` and
`StUnSetRing` (`libcd-stream`).

| library | functions |
| --- | --- |
| libgs (13) | `GsMapModelingData`, `GsLinkObject4`, `GsInitCoordinate2`, `GsGetLs`, `GsGetLws`, `GsSetAmbient`, `GsSetFlatLight`, `GsSetLightMatrix`, `GsSetLightMode`, `GsSetLsMatrix`, `GsSetNearClip`, `GsSetProjection`, `GsSetRefView2` |
| libgte (12) | `ApplyMatrixLV`, `ApplyMatrixSV`, `MulMatrix2`, `Square0`, `RCpolyF3`, `RCpolyF4`, `RCpolyFT3`, `RCpolyFT4`, `RCpolyG3`, `RCpolyG4`, `RCpolyGT3`, `RCpolyGT4` |
| libsnd (4) | `SsSeqPause`, `SsSeqReplay`, `SsSetMute`, `SsUtAutoVol` |
| libapi (1) | `SetMem` |

## Linked, but only as psyz stubs

These link because psyz defines them, but the body only logs "not
implemented" (or does part of the job). The libcd streaming calls
(`CdRead2`, `StSetRing`, `StSetStream`, `StGetNext`, `StFreeRing`) and
libpress (`DecDCTReset`, `DecDCTin`, `DecDCTout`, `DecDCToutCallback`,
`DecDCTvlc`) are implemented now (branches `libcd-stream`,
`libpress-mdec`).

- libgte: `SetFogNear`
- libsnd: `SsSeqOpen`
- libapi: `EnterCriticalSection`, `ExitCriticalSection` (psyz's
  `PS1_` names), `EnableEvent`, `DisableEvent` (partly)
- libcard: `_bu_init`, `_card_info`, `_card_load` (partly)

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

Sound is unverified: SDL loads the audio libraries at run time, and on
this machine the 32-bit ALSA library (`lib32-alsa-lib`) is not
installed, so the SPU and XA output went nowhere.

The next stop is behind START, in the 3D world: the 13 libgs and 12
libgte functions above.

x86_64 stops earlier, in `BMemPMgrAlloc` called from
`LinkResource__BuildModels` for `ETC\DREAME5.TMD`: the heap and the
hand-sized layouts `docs/design.md` lists for 64-bit.
