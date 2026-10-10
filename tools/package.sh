#!/bin/sh
# package.sh BINARY NAME OUTDIR [FORMAT]: a release package of one build.
# FORMAT is
#   tar       OUTDIR/NAME.tar.gz (Linux; the default)
#   zip       OUTDIR/NAME.zip (Windows; the default when BINARY ends in .exe)
#   appimage  OUTDIR/NAME.AppImage (Linux), made with appimagetool and
#             AppImage's type 2 runtime at the versions pinned below,
#             downloaded and checked unless $APPIMAGETOOL and
#             $APPIMAGE_RUNTIME name those files already here
#   macos     OUTDIR/NAME.zip (macOS) holding "LSD Dream Emulator.app";
#             $VERSION goes in its Info.plist (default 0.0)
#
# What goes in: the binary, the port's LICENSE, the README's sections for
# players as README.txt, and licences/ with the licence of everything linked
# into the binary (and, for an AppImage, its runtime) and SOURCES.txt, the
# commits it was built from. An AppImage and an .app also get the icon and,
# for the AppImage, the .desktop file (packaging/). No game data and no BIOS:
# the build never sees either. Run from the top of a checkout with its
# submodules; .github/workflows/release.yml uses it.
set -eu

bin=$1
name=$2
out=$(mkdir -p "$3" && cd "$3" && pwd)
case $bin in
*.exe) format=${4:-zip} ;;
*) format=${4:-tar} ;;
esac
top=$(pwd)

# The AppImage tools. The runtime's licences (packaging/appimage-runtime/)
# are those of the libraries this tag links in: a new tag means checking them.
# The checksums are GitHub's for the release files; the runtime is also
# signed (runtime-x86_64.sig, signing-pubkey.asc in its repository).
appimagetool_tag=1.9.1
appimagetool_sha256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
runtime_tag=20251108
runtime_sha256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
dir=$stage/$name
mkdir -p "$dir/licences"

cp "$bin" "$dir/"
cp LICENSE "$dir/LICENSE.txt"

# The README's sections for players.
{
    echo "LSD: Dream Emulator, native port ($name)"
    echo "https://github.com/brian-oblivion/lsd-port"
    echo
    for section in "The game" "Saves" "Controls" "Picture" "Pace" "The settings menu" "Running a release" "Known differences from the console"; do
        awk -v s="## $section" '$0 == s {on = 1; print; next} /^## / {on = 0} on' README.md
    done
} > "$dir/README.txt"

l=$dir/licences
cp LICENSE "$l/lsd-port-MIT.txt"
cp decomp/LICENSE "$l/lsddecomp-CC0.txt"
cp psyz/LICENSE "$l/psyz-LICENSE-map.txt"
cp psyz/decomp/LICENSE "$l/psyz-decomp-MIT.txt"
cp psyz/external/SDL/LICENSE.txt "$l/SDL3-zlib.txt"
imgui=psyz/external/cimgui/imgui
{
    cat "$imgui/LICENSE.txt"
    echo
    echo "Its built-in fonts (imgui_draw.cpp), ProggyClean and ProggyForever, are MIT too:"
    grep -h -E '^// (MIT License|MIT license|Based on Proggy)' "$imgui/imgui_draw.cpp"
} > "$l/dear-imgui-MIT.txt"
sed -n '/^This software is available under 2 licenses/,/^\*\//p' \
    psyz/psyz/src/dbgserver/stb_image_write.h > "$l/stb_image_write.txt"
if [ -n "${MPL_TEXT:-}" ]; then
    cp "$MPL_TEXT" "$l/MPL-2.0.txt"
else
    echo "package.sh: MPL_TEXT (the MPL 2.0 text) not set" >&2
    exit 1
fi

case $format in
zip)
    # MinGW-w64's runtime and winpthreads are linked in (-static).
    for f in /usr/share/licenses/mingw-w64-crt/COPYING.MinGW-w64-runtime.txt \
        /usr/share/doc/mingw-w64-common/copyright; do
        if [ -f "$f" ]; then
            cp "$f" "$l/mingw-w64-runtime.txt"
            break
        fi
    done
    if [ ! -f "$l/mingw-w64-runtime.txt" ]; then
        echo "package.sh: no MinGW-w64 runtime licence found" >&2
        exit 1
    fi
    ;;
appimage)
    # The AppImage's runtime, the program at its head that mounts the rest,
    # links its libraries statically (AppImage/type2-runtime, built in Alpine).
    mkdir "$l/appimage-runtime"
    cp packaging/appimage-runtime/*.txt "$l/appimage-runtime/"
    ;;
esac

{
    echo "Built from (the source of every part, MPL 2.0 section 3.2):"
    echo
    echo "lsd-port  https://github.com/brian-oblivion/lsd-port  $(git rev-parse HEAD)"
    echo "lsd-psyz  https://github.com/brian-oblivion/lsd-psyz  $(git -C psyz rev-parse HEAD)"
    echo "lsddecomp https://github.com/brian-oblivion/lsddecomp $(git -C decomp rev-parse HEAD)"
    echo "SDL       https://github.com/libsdl-org/SDL           $(git -C psyz/external/SDL rev-parse HEAD)"
    echo "Dear ImGui https://github.com/ocornut/imgui           $(git -C "$imgui" rev-parse HEAD)"
    echo
    echo "What is linked in, and under which licence:"
    echo
    echo "- the port (src/): MIT, lsd-port-MIT.txt"
    echo "- the game's C, from lsddecomp (src/, include/): CC0 1.0, lsddecomp-CC0.txt;"
    echo "  the game itself is not covered (see that file), and no game data is included"
    echo "- psyz's platform layer (psyz/src/platform/, psyz/src/psyz/): MPL 2.0, MPL-2.0.txt"
    echo "- psyz's reconstructed Psy-Q SDK functions (decomp/src/): MIT, psyz-decomp-MIT.txt"
    echo "- psyz's SDK headers (psyz/include/): unlicensed, psyz-LICENSE-map.txt"
    echo "- stb_image_write (psyz's debug server): MIT or public domain, stb_image_write.txt"
    echo "- SDL 3, linked statically: zlib, SDL3-zlib.txt"
    echo "- Dear ImGui (the settings menu) and its fonts, ProggyClean and"
    echo "  ProggyForever: MIT, dear-imgui-MIT.txt"
    case $format in
    zip)
        echo "- the MinGW-w64 runtime and winpthreads: mingw-w64-runtime.txt"
        echo "- GCC's libgcc and libstdc++: GPL 3 with the GCC Runtime Library Exception,"
        echo "  which leaves the program's own terms as they are"
        ;;
    *)
        echo "- GCC's libstdc++, linked statically: GPL 3 with the GCC Runtime Library"
        echo "  Exception, which leaves the program's own terms as they are"
        ;;
    esac
    case $format in
    appimage)
        echo "- the AppImage runtime (the head of the .AppImage file, not linked"
        echo "  into lsd): AppImage/type2-runtime, MIT, with libfuse 3 (LGPL 2.1),"
        echo "  squashfuse (BSD 2-clause), zstd (BSD 3-clause), zlib, mimalloc (MIT)"
        echo "  and musl (MIT) linked in statically: appimage-runtime/. Their source:"
        echo "  https://github.com/AppImage/type2-runtime/tree/$runtime_tag,"
        echo "  whose scripts/ name the version of each library; libfuse's is"
        echo "  https://github.com/libfuse/libfuse/releases/tag/fuse-3.15.0 with"
        echo "  that repository's patches/libfuse/mount.c.diff. To relink the"
        echo "  runtime with another libfuse, build it from that source"
        echo "  (BUILD.md there) and pass it to appimagetool --runtime-file."
        ;;
    esac
} > "$l/SOURCES.txt"

cd "$stage"
case $format in
zip) rm -f "$out/$name.zip"; python3 -m zipfile -c "$out/$name.zip" "$name" ;;
tar) tar -czf "$out/$name.tar.gz" "$name" ;;
appimage)
    # The AppDir: lsd and its .desktop file and icon where appimagetool and
    # desktop integration look for them; README.txt and the licences under
    # usr/share/doc/lsd/. AppRun is lsd itself.
    app=$stage/AppDir
    mkdir -p "$app/usr/bin" "$app/usr/share/applications" \
        "$app/usr/share/icons/hicolor/256x256/apps" "$app/usr/share/doc"
    cp "$dir/$(basename "$bin")" "$app/usr/bin/lsd"
    cp "$top/packaging/lsd.desktop" "$app/usr/share/applications/"
    cp "$top/packaging/lsd-256.png" "$app/usr/share/icons/hicolor/256x256/apps/lsd.png"
    ln -s usr/share/applications/lsd.desktop "$app/lsd.desktop"
    ln -s usr/share/icons/hicolor/256x256/apps/lsd.png "$app/lsd.png"
    ln -s lsd.png "$app/.DirIcon"
    ln -s usr/bin/lsd "$app/AppRun"
    rm "$dir/$(basename "$bin")"
    mv "$dir" "$app/usr/share/doc/lsd"
    tool=${APPIMAGETOOL:-$stage/appimagetool}
    runtime=${APPIMAGE_RUNTIME:-$stage/runtime}
    if [ -z "${APPIMAGETOOL:-}" ]; then
        curl -fsSL -o "$tool" \
            "https://github.com/AppImage/appimagetool/releases/download/$appimagetool_tag/appimagetool-x86_64.AppImage"
    fi
    if [ -z "${APPIMAGE_RUNTIME:-}" ]; then
        curl -fsSL -o "$runtime" \
            "https://github.com/AppImage/type2-runtime/releases/download/$runtime_tag/runtime-x86_64"
    fi
    printf '%s  %s\n%s  %s\n' "$appimagetool_sha256" "$tool" "$runtime_sha256" "$runtime" | sha256sum -c -
    chmod +x "$tool"
    rm -f "$out/$name.AppImage"
    # appimagetool is an AppImage too: extract-and-run needs no FUSE.
    APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 "$tool" --no-appstream \
        --runtime-file "$runtime" "$app" "$out/$name.AppImage"
    dir=$app
    ;;
macos)
    # The bundle, with README.txt and licences/ both inside it (they go
    # wherever the .app is copied) and beside it in the zip.
    bundle="$dir/LSD Dream Emulator.app"
    mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources"
    mv "$dir/$(basename "$bin")" "$bundle/Contents/MacOS/lsd"
    sed "s/@VERSION@/${VERSION:-0.0}/g" "$top/packaging/Info.plist" > "$bundle/Contents/Info.plist"
    printf 'APPL????' > "$bundle/Contents/PkgInfo"
    iconset=$stage/lsd.iconset
    mkdir "$iconset"
    for size in 16 32 128 256 512; do
        sips -z $size $size "$top/packaging/lsd-1024.png" --out "$iconset/icon_${size}x${size}.png" > /dev/null
        sips -z $((size * 2)) $((size * 2)) "$top/packaging/lsd-1024.png" --out "$iconset/icon_${size}x${size}@2x.png" > /dev/null
    done
    iconutil -c icns -o "$bundle/Contents/Resources/lsd.icns" "$iconset"
    cp -R "$dir/README.txt" "$dir/LICENSE.txt" "$dir/licences" "$bundle/Contents/Resources/"
    # Unsigned, but ad-hoc signed as a whole, so the bundle's seal covers
    # Info.plist and the resources: Apple Silicon runs nothing without a
    # signature (README, "Running a release").
    codesign --force --sign - "$bundle"
    rm -f "$out/$name.zip"
    ditto -c -k --keepParent "$name" "$out/$name.zip"
    ;;
*)
    echo "package.sh: unknown format $format" >&2
    exit 1
    ;;
esac
echo "$out/$name ($format): $(cd "$dir" && find . \( -type f -o -type l \) | sort | tr '\n' ' ')"
