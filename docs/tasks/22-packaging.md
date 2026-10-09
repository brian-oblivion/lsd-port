# Task 22: packaging (Linux AppImage or Flatpak, Steam Deck, macOS bundle)

Releases today are archives made by `tools/package.sh`, built by
`.github/workflows/release.yml`: a Linux `.tar.gz` and a Windows `.zip`,
each with the binary, `README.txt` and licences. On Linux the binary
needs the system's libraries at the versions it was built against. macOS
builds in CI, but there is no app bundle. The Steam Deck has never been
tried. Make the port easy to hand to someone else.

Read first: `tools/package.sh`, `.github/workflows/release.yml` and
`build.yml`, the README's "Running a release" and "Building" sections,
`docs/PLAN.md` ("Later": macOS), the memory note on CI (`gh` is not
logged in here; read CI through GitHub's public API). Task 01's Rules
apply.

## Where things stand (2026-10-09)

- lsd-port `main` `6d1d2b6`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `main` `ad64361`.
- The release workflow builds on `ubuntu-24.04` (Linux x86_64 and
  Windows via MinGW), checks what the binary links ("Check what the
  binary needs"), and drafts a release. `ubuntu-latest` moves to 26.04
  from 2026-10-19.
- The port needs the player's own disc image (`--disc`, `LSD_DISC`, or
  `disc/` beside the binary) and writes saves and `settings.ini` to a
  saves folder. Nothing of the game's goes in a package.

## Questions, in order

1. **Linux: AppImage or Flatpak**, or both. An AppImage is one file
   built in CI. A Flatpak suits the Steam Deck's Discover store and
   sandboxing, but has to find the disc image and the saves folder
   through its sandbox (portals, `--filesystem`). Recommend one, with
   what each means for the disc and the saves, and build it in the
   release workflow.
2. **Where saves and settings go** when packaged: beside the binary
   doesn't work for a read-only AppImage or Flatpak. Use the XDG data
   directory (`SDL_GetPrefPath`) when the folder beside the binary isn't
   writable, without moving existing players' saves.
3. **The Steam Deck**: it can't be tested here. Prepare what can be:
   - the pad works through SDL as a PlayStation pad;
   - 1280x800 is 16:10, so check `--aspect 16:10` is accepted and looks
     right headless;
   - `frame_rate = display` at the Deck's 60 or 90 Hz;
   - a `.desktop` file and icon.
   Write a short checklist for the operator to run on a Deck.
4. **macOS**: an `.app` bundle (Info.plist, the binary, an icon) made in
   CI and zipped; unsigned, and the README says how to open an unsigned
   app. Signing and notarisation need the operator's Apple account: out
   of scope, but describe the steps.
5. **Check**: each package from CI unpacked and run headless, to the
   title menu with a disc image (Linux here; macOS through CI's smoke
   test). The licences are complete for what each package bundles.
6. README "Running a release", and handover
   `docs/tasks/22-packaging-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes. Tags and published releases are the operator's.
- No game data, BIOS, screenshots or recordings in any repository or
  package.
- Headless only; kill what you start, by pid.

## Report back with

The Linux format chosen and why; where saves go in each package; what
was built in CI and checked here; the Deck checklist; the macOS bundle
and the signing steps left to the operator.
