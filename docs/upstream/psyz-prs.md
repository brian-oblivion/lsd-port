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
