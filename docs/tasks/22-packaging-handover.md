# Task 22 handover: packaging

Branch `task-22-packaging` (lsd-port), not merged or pushed. `decomp/` and
`psyz/` unchanged (`95f67e0fb`, the fork's `main` `9b1227a`).

## What was done

- **Linux: an AppImage** (the operator's choice; no Flatpak), made by
  `tools/package.sh BINARY NAME OUTDIR appimage` and built in
  `.github/workflows/release.yml` beside the `.tar.gz`, which stays. The
  AppDir: `usr/bin/lsd` (also `AppRun`), `lsd.desktop`, the icon,
  `usr/share/doc/lsd/` with `README.txt`, `LICENSE.txt` and `licences/`.
  Nothing else is bundled: the binary already needs only glibc and
  libgcc_s, and what SDL opens at run time (Vulkan loader and driver,
  X11/Wayland, ALSA/PulseAudio) must be the system's anyway. The tools are
  pinned with checksums in `package.sh`: appimagetool 1.9.1 (GitHub's
  sha256 for the release file) and type2-runtime `20251108` (also checked
  against its GPG signature here). The runtime is static, so the player
  needs no libfuse2.
- **Saves and settings**: already in SDL's per-user folder since task 06
  (`~/.local/share/lsd-dream-emulator/`; macOS `~/Library/Application
  Support/lsd-dream-emulator/`), with `settings.ini` and `controls.ini`
  there too, so a read-only AppImage or `.app` changes nothing and no
  existing saves move. Nothing to change in the code; the README now says
  so per platform.
- **The disc** (`src/main.c`, `FindDefaultDisc`): after the working
  directory and beside the binary (which for an AppImage is inside its
  mount, and for an `.app` is `Contents/Resources/`), lsd now also looks
  in `disc/` beside the `.AppImage` (`$APPIMAGE`) or beside the `.app`,
  then in `disc/` in the saves folder. The saves folder is now set up
  before the disc is looked for. The "no disc image" message names the
  saves folder.
- **Start errors in a box** when stderr isn't a terminal (started from a
  desktop, Steam or Finder), as on Windows; not when `SDL_VIDEO_DRIVER` is
  `offscreen` or `dummy`, so headless runs and CI never block on a box.
- **macOS: `LSD Dream Emulator.app`**, zipped with `ditto`, made by
  `package.sh ... macos` in a new `macos` job of the release workflow
  (macos-latest, arm64, `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`): `Info.plist`
  (`packaging/Info.plist`, version from the tag), `lsd.icns` made with
  `sips`/`iconutil` from `packaging/lsd-1024.png`, README and licences
  inside `Contents/Resources/` and beside the app in the zip. Ad-hoc
  signed as a whole (`codesign -s -`), as Apple Silicon needs some
  signature. The job checks `otool -L` (system libraries only), the
  minimum macOS (11.0), `codesign --verify --deep --strict`, `plutil
  -lint`, and runs the unzipped app's binary with no disc (exit 2).
- **Icon and `.desktop`**: `packaging/lsd.svg`, drawn for the port (a dusk
  over a low-poly field; nothing from the game), with 256 and 1024 px
  renders; `packaging/lsd.desktop` (`desktop-file-validate` clean).
- **Release workflow**: also runs on a push to any branch that changes
  `release.yml`, `tools/package.sh` or `packaging/` (artifacts only; the
  draft release still needs a tag). The Linux job smoke-tests the AppImage
  with no disc (`APPIMAGE_EXTRACT_AND_RUN=1`), and reports the newest glibc
  symbol and the macOS minimum as annotations, readable through the public
  API.
- **Licences**: the AppImage's runtime (the head of the file) statically
  links libfuse 3.15.0 (LGPL 2.1), squashfuse 0.5.2 (BSD 2), zstd, zlib,
  mimalloc and musl; their texts are vendored in
  `packaging/appimage-runtime/` and go into the AppImage's
  `licences/appimage-runtime/`, with `SOURCES.txt` pointing at the
  runtime's source and how to relink it (LGPL 2.1 section 6). The macOS
  app adds nothing beyond the Linux binary's list (libc++ is the system's).
- README: "The game" (where `disc/` can go), "Saves" (macOS; nothing
  written beside the program), "Running a release" (AppImage, macOS and
  opening an unsigned app, Steam Deck), "Layout", "Licence". PLAN.md's
  macOS line.

## Why AppImage over Flatpak (for the record)

One file, built in the existing workflow, nothing to install; the disc can
sit beside it and the saves go where the archives' do. A Flatpak would
need a manifest, a runtime (org.freedesktop.Platform) and a repository or
Flathub submission, and inside its sandbox `disc/` beside the program
doesn't exist: the disc would need `--filesystem` permissions or a file
portal, and the saves would move to `~/.var/app/<id>/data/`, away from
existing players'. The Deck runs AppImages as non-Steam games fine.

## Checked here

- x86_64 RelWithDebInfo built and stripped as CI does; AppImage made
  locally with the pinned tools (2.9 MB).
- AppImage, no disc, both through FUSE and with extract-and-run: exit 2,
  the message names the per-user folder.
- AppImage with `disc/` beside it, started from another folder, headless:
  intro movies, then the title menu at ~84 s (screenshots in scratch only).
- AppImage with the disc only in `$XDG_DATA_HOME/lsd-dream-emulator/disc/`:
  runs (`--frames 300`, exit 0).
- `--aspect 16:10`: the window opens at 1280x800; in a dream the 320x240
  picture is squeezed by 1.2 and, stretched back to 16:10, the room looks
  right.
- `--frame-rate display` headless falls back to 59.94 (no display rate);
  `--frame-rate 90` (the OLED Deck's) holds 11111 µs frames in a dream.
- Windows x86_64 (MinGW) still builds; the macOS branch of `main.c`
  syntax-checked with stub Apple headers. The `.tar.gz` unchanged.

Not checked here: the macOS bundle (needs CI; there's no Mac here), and
CI itself, which runs once the branch is pushed. Read the jobs and the
annotations ("newest glibc symbol", "macOS minimum") through the public
API afterwards.

## Steam Deck checklist (for the operator)

1. Desktop Mode: download the AppImage, Properties → "Is executable".
   Put `disc/` with the `.cue`/`.bin` beside it.
2. Run it from Dolphin once: it should reach the title menu. (If it says
   there's no disc image, the box says where it looked.)
3. Steam → Add a Non-Steam Game → the AppImage (choose "All files").
4. Game Mode: start it. Expect fullscreen, sound, the Deck's controls as
   a pad: d-pad moves, A (south) is cross, B (east) circle; Start pauses
   in a dream.
5. Press both sticks in: the settings menu opens (the Steam button is
   Steam's). Set aspect 16:10 and frame rate `display`; close it, start
   a day: no black bars, smooth motion.
6. OLED Deck: set the screen to 90 Hz (Performance overlay) and check
   the dream still moves at the right speed (a day lasts as at 60).
7. Save at the end of a day, quit, start again: the save is there
   (`~/.local/share/lsd-dream-emulator/bu00/`).
8. Suspend and resume the Deck mid-dream: the game carries on (sound too).
9. Note the frame time and battery draw in the performance overlay, and
   anything odd with the controls (Steam Input's layout is the default
   gamepad one).

## macOS signing and notarisation (left to the operator)

Needs an Apple Developer Program membership (99 USD/year).

1. Make a "Developer ID Application" certificate (Xcode or the developer
   site), export it as `.p12`, and store it and its password as repository
   secrets; an app-specific password (or an App Store Connect API key) for
   `notarytool` as well, with the Team ID.
2. In the `macos` job, before zipping: import the `.p12` into a temporary
   keychain (`security create-keychain`, `security import`,
   `security set-key-partition-list`), then
   `codesign --force --options runtime --timestamp --sign "Developer ID Application: NAME (TEAMID)" "LSD Dream Emulator.app"`
   in place of the ad-hoc `codesign --sign -` in `package.sh`. The hardened
   runtime may need no entitlements (no JIT, no unsigned libraries); check
   that Vulkan/MoltenVK isn't involved (SDL GPU uses Metal directly).
3. Zip with `ditto -c -k --keepParent`, then
   `xcrun notarytool submit lsd.zip --apple-id ... --team-id ... --password ... --wait`.
4. `xcrun stapler staple "LSD Dream Emulator.app"`, zip again, and that
   zip is the release file. `spctl -a -vv "LSD Dream Emulator.app"`
   should then say "Notarized Developer ID".
5. The README's "Open Anyway" paragraph can then go.

## Questions for the operator

- The `.tar.gz` for Linux is kept beside the AppImage: drop it?
- The release build runs on ubuntu-24.04. If the "newest glibc symbol"
  annotation says 2.34 or older, older distributions run it too and the
  README could say so; building on ubuntu-22.04 would be the way to go
  lower.
- An x86_64 macOS build (Intel Macs) isn't made; a universal binary would
  need SDL and the game built for both (`CMAKE_OSX_ARCHITECTURES`).
