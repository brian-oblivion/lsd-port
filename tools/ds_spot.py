#!/usr/bin/env python3
r"""DuckStation at a chosen day and first spawn, so the console can be
compared with a lockstep run (tools/lockstep.py, config "days" and "spawn")
standing at the same spot (task 13).

    tools/ds_spot.py CFG OUT

The config (JSON):
  day        the day to set at DreamSys__StartDay; days 1 to 10 load only a
             stage's TEXA textures, so both sides draw the same set
  spawn      [stage, index]: the first spawn, sStageSpawnPoints[stage][index]
  wait       seconds from StartDay to the first screenshot (default 8)
  shots, gap how many screenshots, how far apart (default 4, 1.0)
  settle     seconds between DuckStation's window and attaching gdb (default 8)

The player stands still, so compare with a lockstep run without "input".
Screenshots go to OUT/ds<n>.png, gdb's output to OUT/gdb.log, DuckStation's
own files under OUT/root; never into a repository.

gdb attaches to DuckStation's stub with ~/git/lsddecomp/build/lsdde.elf,
which has symbols but no types, so the hooks write DreamSys by its offsets
(the i686 build's, which are the PS1's). The stub reports a breakpoint as a
plain SIGTRAP, so gdb would stop on it again at every continue: each
breakpoint is used once and deleted, and the hooks run in a loop of
`continue`s rather than in Breakpoint.stop(). OUT is made absolute:
DuckStation runs from its install directory.
"""
import json
import os
import shutil
import subprocess
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import ds_drive as ds  # noqa: E402

ELF = os.path.expanduser('~/git/lsddecomp/build/lsdde.elf')

# DreamSys offsets: currentStage, linkCoordinates, currentDay
HOOKS = r'''
import gdb, json, os
C = json.load(open(os.environ['DS_SPOT']))
OFF_STAGE, OFF_LINK, OFF_DAY = 356, 364, 384
w = lambda s: (gdb.write(s + '\n'), gdb.flush())
reg = lambda r: int(gdb.parse_and_eval('$' + r)) & 0xffffffff
addr = lambda s: int(gdb.parse_and_eval('(unsigned int)&' + s)) & 0xffffffff
rd = lambda a, n: gdb.selected_inferior().read_memory(a, n).tobytes()
wr = lambda a, b: gdb.selected_inferior().write_memory(a, b)
A_INIT = addr('DreamSys__InitSpawnLoc')
A_START = addr('DreamSys__StartDay')
A_MENU = addr('GameApplication__RunTitleMenu')
BP = {a: gdb.Breakpoint('*%d' % a) for a in (A_INIT, A_START, A_MENU)}
after = None
w('MARK hooks')
while True:
    gdb.execute('continue', to_string=True)
    pc = reg('pc')
    if pc in BP:
        BP.pop(pc).delete()
    if pc == A_MENU:
        w('MARK RunTitleMenu')
    elif pc == A_START:
        if C.get('day') is not None:
            wr(reg('a0') + OFF_DAY, int(C['day']).to_bytes(4, 'little'))
        w('MARK StartDay')
    elif pc == A_INIT and C.get('spawn'):
        after = (reg('ra'), reg('a0'))
        BP[after[0]] = gdb.Breakpoint('*%d' % after[0])
    elif after and pc == after[0]:
        ds, (stage, index) = after[1], C['spawn']
        wr(ds + OFF_STAGE, stage.to_bytes(4, 'little'))
        table = int.from_bytes(rd(addr('sStageSpawnPoints') + 4 * stage, 4), 'little')
        ent = rd(table + 6 * index, 6)  # StageSpawn: chunk, tile, adjustment, extra
        wr(ds + OFF_LINK, ent[:4])
        wr(ds + OFF_LINK + 4, rd(addr('sSpawnPosAdjust') + 6 * ent[4], 6))
        w('MARK Spawn stage=%d index=%d' % (stage, index))
        after = None
    else:
        w('MARK stop pc=%x' % pc)
'''


def main():
    cfgp, out = (os.path.abspath(a) for a in sys.argv[1:3])
    cfg = json.load(open(cfgp))
    os.makedirs(out, exist_ok=True)
    hooks = os.path.join(out, 'hooks.py')
    with open(hooks, 'w') as f:
        f.write(HOOKS)
    cue = [f for f in os.listdir(os.path.join(REPO, 'disc')) if f.lower().endswith('.cue')][0]
    d = ds.DS(os.path.join(out, 'root'), os.path.join(REPO, 'disc', cue), args=['-fastboot'])
    logp = os.path.join(out, 'gdb.log')
    g = None
    try:
        d.window(60)
        time.sleep(cfg.get('settle', 8))
        g = subprocess.Popen(
            ['stdbuf', '-o0', 'gdb', '-q', '-batch', '-ex', 'set architecture mips:3000',
             '-ex', 'set pagination off', '-ex', 'file ' + ELF,
             '-ex', 'target remote localhost:2345', '-x', hooks],
            stdout=open(logp, 'w'), stderr=subprocess.STDOUT,
            env=dict(os.environ, DS_SPOT=cfgp), start_new_session=True)

        def log():
            return open(logp).read()

        t0 = time.time()
        while 'MARK RunTitleMenu' not in log():
            if time.time() - t0 > 400 or g.poll() is not None:
                raise RuntimeError('no title menu')
            d.key('start', 0.15, 2.5)
        time.sleep(2)
        d.key('start', 0.15, 1.0)
        t0 = time.time()
        while 'MARK StartDay' not in log():
            if time.time() - t0 > 60:
                raise RuntimeError('no StartDay')
            time.sleep(0.5)
        time.sleep(cfg.get('wait', 8))
        for i in range(cfg.get('shots', 4)):
            p = d.screenshot()
            if p:
                shutil.copy(p, os.path.join(out, 'ds%02d.png' % i))
            time.sleep(cfg.get('gap', 1.0))
    finally:
        if g and g.poll() is None:
            os.killpg(g.pid, 15)
        d.stop()
        print(''.join(l + '\n' for l in open(logp).read().splitlines() if l.startswith('MARK')))


if __name__ == '__main__':
    main()
