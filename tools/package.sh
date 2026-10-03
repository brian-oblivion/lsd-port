#!/bin/sh
# package.sh BINARY NAME OUTDIR: a release archive of one build, OUTDIR/NAME.zip
# (Windows, BINARY ends in .exe) or OUTDIR/NAME.tar.gz (Linux).
#
# What goes in: the binary, the port's LICENSE, the README's sections for
# players as README.txt, and licences/ with the licence of everything linked
# into the binary and SOURCES.txt, the commits it was built from. No game
# data and no BIOS: the build never sees either. Run from the top of a
# checkout with its submodules; .github/workflows/release.yml uses it.
set -eu

bin=$1
name=$2
out=$(mkdir -p "$3" && cd "$3" && pwd)
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
    for section in "The game" "Saves" "Running a release" "Known differences from the console"; do
        awk -v s="## $section" '$0 == s {on = 1; print; next} /^## / {on = 0} on' README.md
    done
} > "$dir/README.txt"

l=$dir/licences
cp LICENSE "$l/lsd-port-MIT.txt"
cp decomp/LICENSE "$l/lsddecomp-CC0.txt"
cp psyz/LICENSE "$l/psyz-LICENSE-map.txt"
cp psyz/decomp/LICENSE "$l/psyz-decomp-MIT.txt"
cp psyz/external/SDL/LICENSE.txt "$l/SDL3-zlib.txt"
sed -n '/^This software is available under 2 licenses/,/^\*\//p' \
    psyz/psyz/src/dbgserver/stb_image_write.h > "$l/stb_image_write.txt"
if [ -n "${MPL_TEXT:-}" ]; then
    cp "$MPL_TEXT" "$l/MPL-2.0.txt"
else
    echo "package.sh: MPL_TEXT (the MPL 2.0 text) not set" >&2
    exit 1
fi

case $bin in
*.exe)
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
esac

{
    echo "Built from (the source of every part, MPL 2.0 section 3.2):"
    echo
    echo "lsd-port  https://github.com/brian-oblivion/lsd-port  $(git rev-parse HEAD)"
    echo "lsd-psyz  https://github.com/brian-oblivion/lsd-psyz  $(git -C psyz rev-parse HEAD)"
    echo "lsddecomp https://github.com/brian-oblivion/lsddecomp $(git -C decomp rev-parse HEAD)"
    echo "SDL       https://github.com/libsdl-org/SDL           $(git -C psyz/external/SDL rev-parse HEAD)"
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
    case $bin in
    *.exe) echo "- the MinGW-w64 runtime and winpthreads: mingw-w64-runtime.txt" ;;
    esac
} > "$l/SOURCES.txt"

cd "$stage"
case $bin in
*.exe) rm -f "$out/$name.zip"; python3 -m zipfile -c "$out/$name.zip" "$name" ;;
*) tar -czf "$out/$name.tar.gz" "$name" ;;
esac
echo "$out/$name: $(cd "$name" && find . -type f | sort | tr '\n' ' ')"
