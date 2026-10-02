#!/usr/bin/env python3
"""Compare the game's class sizes on the host with their PS1 sizes.

lsddecomp's headers give each class's PS1 object size in a sentence like
"The object is 0x20 bytes (New_FlatLightObj)". This reads those, takes the
host sizes from the debug info of a build's lsd_game objects (with pahole),
and prints the classes whose size differs.

    tools/class-sizes.py [--decomp DIR] BUILD_DIR

BUILD_DIR must be a RelWithDebInfo or Debug build (the objects need -g).
"""
import argparse
import glob
import re
import subprocess


def ps1_sizes(decomp):
    sizes = {}
    for path in glob.glob(f"{decomp}/include/*.h"):
        with open(path, encoding="utf-8", errors="replace") as f:
            text = re.sub(r"\s*\n\s*\*\s*", " ", f.read())
        for m in re.finditer(
            r"(?:object|allocation) is (0x[0-9A-Fa-f]+) bytes \((?:the size )?New_(\w+)",
            text,
        ):
            sizes[m.group(2)] = int(m.group(1), 16)
    return sizes


def host_sizes(build):
    sizes = {}
    objs = glob.glob(f"{build}/CMakeFiles/lsd_game.dir/**/*.o", recursive=True)
    objs += glob.glob(f"{build}/CMakeFiles/lsd_game.dir/**/*.obj", recursive=True)
    for obj in objs:
        out = subprocess.run(["pahole", obj], capture_output=True, text=True).stdout
        name = None
        for line in out.splitlines():
            m = re.match(r"^struct (\w+) \{", line)
            if m:
                name = m.group(1)
                continue
            m = re.match(r"^\s*/\* size: (\d+)", line)
            if m and name:
                sizes[name] = int(m.group(1))
                name = None
    return sizes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("build")
    ap.add_argument("--decomp", default="decomp")
    args = ap.parse_args()
    ps1 = ps1_sizes(args.decomp)
    host = host_sizes(args.build)
    found = [k for k in sorted(ps1) if k in host]
    differ = [k for k in found if host[k] != ps1[k]]
    for k in differ:
        print(f"{k}: PS1 {ps1[k]:#x}, host {host[k]:#x}")
    print(
        f"{len(ps1)} classes with a PS1 size, {len(found)} in the build, "
        f"{len(found) - len(differ)} the same size, {len(differ)} not"
    )


if __name__ == "__main__":
    main()
