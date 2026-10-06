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
  spawn      [stage, index]: each day starts at sStageSpawnPoints[stage][index]
             (tools/ds_spot.py puts the console at the same spot)
  input      [[from_tick, to_tick, "up,circle"], ...]: pad 1 while a dream
             runs, by the dream FrameClock's tick (button names as the
             debug server's)
  freeze     ticks at which the dream clock stops for freeze_len ticks
             (default 40); a screenshot is taken then, f<tick>_<n>.png
  freeze_shots [count, gap]: more shots per freeze, gap seconds apart,
             f<tick>_<n>_<k>.png (k from 1), to see what moves while the
             dream clock is stopped; freeze_len must cover them
  steps      [["menu"], ["day"], ...] (default: menu, then one day):
               ["menu"]                   press START until the title menu
               ["day"] / ["day", "circle"] press START (or the button) and
                                          play the dream to its EndDay
               ["press", "down circle", gap]
               ["shot", name], ["shots", prefix, count, gap], ["sleep", s]
               ["wait_tick", t]           wait until a STATE or DRAW line
                                          (config "trace", "draw_trace")
                                          reports dream tick t
               ["tick_shots", prefix, count, gap]
                                          like "shots", each named by the
                                          last tick reported when it returns
  cards      a saves folder (bu00/ ...) to start from
  audio      true: SDL's disk audio driver into OUT/<tag>/<tag>.raw;
             otherwise the dummy driver
  env        more environment for the game (LSD_ASPECT, ...)
  flashback, pktuse, probe, trace, draw_trace: see tools/lockstep_gdb.py
  gdb_extra  more gdb Python files to source after lockstep_gdb.py, for a
             one-off tracer (they see its globals through `import __main__`)

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
                'handle SIGPIPE nostop noprint\nsource %s\n' % os.path.join(TOOLS, 'lockstep_gdb.py'))
        for extra in cfg.get('gdb_extra', []):
            f.write('source %s\n' % os.path.abspath(extra))
        f.write('run\necho MARK STOPPED\\n\nbt 25\n')
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
            name = 'f%05d_%d' % (int(lines[shots[0]].split()[2]), shots[0])
            r.shot(name)
            more = cfg.get('freeze_shots', [1, 0])
            for k in range(1, more[0]):
                time.sleep(more[1])
                r.shot('%s_%d' % (name, k))
            shots[0] += 1

    def last_tick():
        for l in reversed(r.text().splitlines()):
            if l.startswith('STATE tr tick='):
                return int(l.split()[2].split('=')[1])
            if l.startswith('DRAW '):
                return int(l.split()[1])
        return -1

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
            elif k == 'wait_tick':
                t0 = time.time()
                while last_tick() < step[1]:
                    if time.time() - t0 > cfg.get('day_timeout', 900) or not r.alive():
                        raise RuntimeError('tick %d not reached' % step[1])
                    time.sleep(0.05)
            elif k == 'tick_shots':
                for i in range(step[2]):
                    r.shot('%s%02d_t%05d' % (step[1], i, max(last_tick(), 0)))
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
        keep = ('STATE', 'MARK', 'SAVEBLK', 'POOL', 'PKT', 'PROBE', 'DRAW')
        with open(out + '.state', 'w') as f:
            f.write('\n'.join(l for l in txt.splitlines() if l.startswith(keep)) + '\n')
        i = txt.find('MARK STOPPED')
        if i >= 0:
            print(txt[i:i + 3000])


if __name__ == '__main__':
    main()
