# The host link surface (2026-10-02)

What the linker reports missing when the game's C (lsddecomp `e5daad5c3`)
links against psyz (fork `main` `8030744`) on Linux x86_64; i686 and
MinGW report the same list. It replaces the regex estimate in
`psyz-coverage-2026-10-02.md` as the platform layer's to-do list. Each
symbol here has a temporary stand-in in `src/stubs.c`; deleting a stand-in
shows whether psyz now provides it.

To reproduce: remove `src/stubs.c` from the `lsd` target and build; the
undefined references are this list.

## SDK functions psyz lacks (38)

The game links against psyz's headers; none of these has a body in psyz's
PC build.

| library | functions |
| --- | --- |
| libgs (18) | `GsInit3D`, `GsMapModelingData`, `GsLinkObject4`, `GsInitCoordinate2`, `GsGetLs`, `GsGetLws`, `GsGetTimInfo`, `GsSetAmbient`, `GsSetFlatLight`, `GsSetLightMatrix`, `GsSetLightMode`, `GsSetLsMatrix`, `GsSetNearClip`, `GsSetProjection`, `GsSetRefView2`, `GsSortBg`, `GsSortBoxFill`, `GsSortSprite` |
| libgte (12) | `ApplyMatrixLV`, `ApplyMatrixSV`, `MulMatrix2`, `Square0`, `RCpolyF3`, `RCpolyF4`, `RCpolyFT3`, `RCpolyFT4`, `RCpolyG3`, `RCpolyG4`, `RCpolyGT3`, `RCpolyGT4` |
| libsnd (5) | `SsSeqPause`, `SsSeqReplay`, `SsSetMute`, `SsUtAutoVol`, `SsUtGetVabHdr` |
| libcd (2) | `StClearRing`, `StUnSetRing` |
| libapi (1) | `SetMem` |

## Linked, but only as psyz stubs

These link because psyz defines them, but the body only logs "not
implemented" (or does part of the job). The coverage snapshot lists them;
they are as much a to-do as the table above.

- libcd: `CdRead2`, `StSetRing`, `StSetStream`, `StGetNext`, `StFreeRing`
- libpress (MDEC): `DecDCTReset`, `DecDCTin`, `DecDCTout`,
  `DecDCToutCallback`, `DecDCTvlc`
- libgte: `SetFogNear`
- libsnd: `SsSeqOpen`
- libapi: `EnterCriticalSection`, `ExitCriticalSection` (psyz's
  `PS1_` names), `EnableEvent`, `DisableEvent` (partly)
- libcard: `_bu_init`, `_card_info`, `_card_load` (partly)

## The game's own data that is not in its C (37 + 18)

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
  them under `PLATFORM_PC` in their units, sized from the code; the nine
  are one `StyleEffectParams` there, as psyz's porting guide asks of
  overlapping symbols.
- **37 read-only objects** with contents, still assembly in lsddecomp:
  file paths (`sModelPathDreamE5`, `sDreamerTmdPath`, `sEtcTimPath`, the
  seven `sSoundBank*Path`, `sTitleTimPath`, `sTitleMenuFontPath`,
  `sTitleMenuSoundBankPath`, `sGraphTimPath`, `sGraphSoundBankPath`,
  `sLogoPathAsmk`, `sLogoPathOsd`, `sAsmkMoviePath`, `sSaveIconTimPath`),
  messages and `printf` formats (`sBMemPMgrInitFailFmt`,
  `sCdFileNotFoundFmt`, `sFileNotFoundMsg`, `sFileNotCreatedMsg`,
  `sSeqOpenErrorMsg`), the memory-card texts and icon names
  (`sCardFilePrefixText`, `sSaveFileNameText`, `sSaveTitleText`,
  `sSaveTitleBlanksText`, `sSaveTitleGlyphTable`, `sNoCardIconName`,
  `sSaveNoSpaceIconName`, `sLoadNotFoundIconName`,
  `sTitleMenuFlashbackName`), and the text-entry character sets
  (`sStrNameChars`, `sStrComInput`, `sStrFontIcon`,
  `sItemListStrFontIcon`). `src/stubs.c` zero-fills them at their PS1
  sizes, so the game cannot name any file until they are real. This
  needs a decision (see the task report): written as C in lsddecomp (as
  matched C where it can be byte-exact, else under `PLATFORM_PC`), or
  read from the user's own executable on the disc at startup.

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

With no disc image `lsd` exits with 2 and says so. With one (`--disc
game.cue`), the game runs `main` through `New_GameApplication` to its
first file, `sModelPathDreamE5`: `CdSearchFile` fails (the name is a
placeholder, and a blank image has no files either), the game prints its
"file not found" message, and `LinkResource__BuildModels` then reads the
missing buffer, a NULL pointer. On the PS1 that read does not fault; on
the host it does. Both widths stop there.
