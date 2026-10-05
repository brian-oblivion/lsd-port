#!/usr/bin/env python3
r"""Run lsd headless under gdb with tools/lockstep_gdb.py, so that a dream is
reproducible tick for tick, and drive it through a list of steps. Running the
same config on two builds (i686 and x86_64, say) and diffing their STATE
lines and screenshots shows whether they play alike (task 10).

    tools/lockstep.py build-x64-dbg day1-x64 7811 day1.json OUT
    tools/lockstep.py build-i686-dbg day1-i686 7812 day1.json OUT
    diff <(grep '^STATE\|^SAVEBLK' OUT/day1-x64.state) \
         <(grep '^STATE\|^SAVEBLK' OUT/day1-i686.state)

(POOL and PKT lines measure memory, which differs with the width.)

The config (JSON):
  seed       psyz's rand() state, set at every DreamSys__StartDay
  days       the day to set at each StartDay, in order (null: keep it)
  input      [[from_tick, to_tick, "up,circle"], ...]: pad 1 while a dream
             runs, by the dream FrameClock's tick (button names as the
             debug server's)
  freeze     ticks at which the dream clock stops for freeze_len ticks
             (default 40); a screenshot is taken then, f<tick>_<n>.png
  steps      [["menu"], ["day"], ...] (default: menu, then one day):
               ["menu"]                   press START until the title menu
               ["day"] / ["day", "circle"] press START (or the button) and
                                          play the dream to its EndDay
               ["press", "down circle", gap]
               ["shot", name], ["shots", prefix, count, gap], ["sleep", s]
  cards      a saves folder (bu00/ ...) to start from
  audio      true: SDL's disk audio driver into OUT/<tag>/<tag>.raw;
             otherwise the dummy driver
  env        more environment for the game (LSD_ASPECT, ...)
  flashback, pktuse, probe, trace: see tools/lockstep_gdb.py

Everything goes to OUT/<tag>/ and OUT/<tag>.state: game imagery and save
files, so never into the repository. The build should be -O0.
"""
import json
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lsd_drive as d  # noqa: E402

TOOLS = os.path.dirname(os.path.abspath(__file__))


def main():
    build, tag, port, cfgp, outroot = sys.argv[1:6]
    cfg = json.load(open(cfgp))
    out = os.path.join(outroot, tag)
    shutil.rmtree(out, ignore_errors=True)
    saves = os.path.join(out, 'saves')
    os.makedirs(saves)
    if cfg.get('cards'):
        shutil.copytree(cfg['cards'], saves, dirs_exist_ok=True)
    gdbscript = os.path.join(out, 'lockstep.gdb')
    with open(gdbscript, 'w') as f:
        f.write('set pagination off\nset debuginfod enabled off\n'
                'handle SIGPIPE nostop noprint\nsource %s\nrun\n'
                'echo MARK STOPPED\\n\nbt 25\n' % os.path.join(TOOLS, 'lockstep_gdb.py'))
    env = {'LSD_LOCKSTEP': os.path.abspath(cfgp)}
    if not cfg.get('audio'):
        env['SDL_AUDIO_DRIVER'] = 'dummy'
    env.update(cfg.get('env', {}))
    r = d.Run(tag, int(port), out, binary=os.path.join(build, 'lsd'), gdb_script=gdbscript,
              env=env, saves=saves)
    shots = [0]

    def take_freeze_shots():
        lines = [l for l in r.text().splitlines() if l.startswith('MARK FROZEN')]
        while shots[0] < len(lines):
            time.sleep(0.8)  # a frame drawn while frozen
            r.shot('f%05d_%d' % (int(lines[shots[0]].split()[2]), shots[0]))
            shots[0] += 1

    days = 0
    try:
        for step in cfg.get('steps', [['menu'], ['day']]):
            k = step[0]
            if k == 'menu':
                n = r.count('MARK RunTitleMenu') + 1
                r.wait_log('MARK RunTitleMenu', n, timeout=600, poll=1,
                           every=lambda: (r.press('start', 4, 3)
                                          if r.count('MARK RunTitleMenu') < n else None))
                time.sleep(1.5)
            elif k == 'day':
                r.press(step[1] if len(step) > 1 else 'start', 4, 1.0)
                days += 1
                r.wait_log('MARK StartDay', days, timeout=60)
                t0 = time.time()
                while r.count('MARK EndDay') < days:
                    take_freeze_shots()
                    if 'MARK STOPPED' in r.text() or not r.alive():
                        raise RuntimeError('the game stopped')
                    if time.time() - t0 > cfg.get('day_timeout', 900):
                        raise RuntimeError('day timeout')
                    time.sleep(0.2)
                take_freeze_shots()
            elif k == 'press':
                for b in step[1].split(' '):
                    r.press(b, 4, step[2] if len(step) > 2 else 1.0)
            elif k == 'shot':
                r.shot(step[1])
            elif k == 'shots':
                for i in range(step[2]):
                    r.shot('%s%02d' % (step[1], i))
                    time.sleep(step[3])
            elif k == 'sleep':
                time.sleep(step[1])
            else:
                raise ValueError('unknown step %r' % (step,))
    except Exception as e:  # noqa: BLE001 -- report and keep the state file
        print('%s: %s' % (tag, e))
    finally:
        time.sleep(0.5)
        txt = r.text()
        r.stop()
        keep = ('STATE', 'MARK', 'SAVEBLK', 'POOL', 'PKT', 'PROBE')
        with open(out + '.state', 'w') as f:
            f.write('\n'.join(l for l in txt.splitlines() if l.startswith(keep)) + '\n')
        i = txt.find('MARK STOPPED')
        if i >= 0:
            print(txt[i:i + 3000])


if __name__ == '__main__':
    main()
