#!/usr/bin/env python3
"""Drive a headless lsd run from Python: start it (optionally under gdb),
press buttons, take screenshots, change the pacing, wait for log lines.

    import lsd_drive as d
    r = d.Run('day1', 7710, out='/tmp/runs', gdb_script='menu.gdb', vsync='limitless')
    r.wait_log('RunTitleMenu', 1, every=lambda: r.press('start', 4, 0.2))
    r.press('start'); r.vsync('off'); r.shot('dream')
    r.stop()

Everything a run writes (log, raw audio, screenshots, memory card files)
goes to `out`, never the repository or the real per-user folder: the
default saves folder is `out/xdg-data/lsd-dream-emulator/` unless `saves` (or
LSD_SAVES in `env`) says otherwise. The window never opens: SDL renders
offscreen and plays audio into a raw file (S16LE stereo, 44.1 kHz) through
its `disk` driver. See docs/tasks/05-*.md for the pitfalls.
"""
import array
import os
import signal
import subprocess
import time
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Run:
    def __init__(self, tag, port, out, binary='build-i686/lsd', disc=None,
                 gdb_script=None, vsync='off', env=None, saves=None):
        self.tag, self.port, self.out = tag, port, out
        os.makedirs(out, exist_ok=True)
        self.log = os.path.join(out, tag + '.log')
        self.raw = os.path.join(out, tag + '.raw')
        e = dict(os.environ)
        e.pop('DISPLAY', None)
        e.pop('WAYLAND_DISPLAY', None)
        e.update(SDL_VIDEO_DRIVER='offscreen', SDL_AUDIO_DRIVER='disk',
                 SDL_AUDIO_DISK_OUTPUT_FILE=self.raw, LSD_VSYNC=vsync,
                 LSD_DEBUG_PORT=str(port),
                 # the default saves folder (SDL's per-user one), kept in `out`
                 XDG_DATA_HOME=os.path.join(out, 'xdg-data'))
        e.update(env or {})
        if disc is None:
            cues = [f for f in os.listdir(os.path.join(REPO, 'disc')) if f.lower().endswith('.cue')]
            disc = os.path.join(REPO, 'disc', cues[0])
        cmd = [os.path.join(REPO, binary), '--disc', disc]
        if saves:
            cmd += ['--saves', saves]
        if gdb_script:
            cmd = ['gdb', '-q', '-batch', '-x', gdb_script, '--args'] + cmd
        # The working directory is `out`, where builds before task 06 put
        # bu00/ and bu10/.
        self.proc = subprocess.Popen(cmd, cwd=out, env=e, stdout=open(self.log, 'w'),
                                     stderr=subprocess.STDOUT, start_new_session=True)
        self.wait_log('debug server on', timeout=120)

    def text(self):
        return open(self.log, errors='replace').read()

    def count(self, pattern):
        return self.text().count(pattern)

    def alive(self):
        return self.proc is None or self.proc.poll() is None

    def wait_log(self, pattern, n=1, timeout=600, poll=0.5, every=None):
        t0 = time.time()
        while self.count(pattern) < n:
            if not self.alive():
                raise RuntimeError(f'{self.tag}: exited waiting for {pattern!r}')
            if time.time() - t0 > timeout:
                raise TimeoutError(f'{self.tag}: {pattern!r} x{n} not seen in {timeout}s')
            if every:
                every()
            time.sleep(poll)

    def req(self, path, post=False):
        r = urllib.request.Request(f'http://127.0.0.1:{self.port}{path}',
                                   method='POST' if post else 'GET')
        return urllib.request.urlopen(r, timeout=30).read()

    def press(self, buttons, frames=4, gap=0.5):
        """Queue `buttons` (comma-separated debug-server names) for `frames`
        game frames, then sleep `gap` seconds. Frames are the game's: a dream
        runs at 20 fps, so give the input time to drain before the next one,
        or the queue fills and the server answers 400."""
        self.req(f'/input?buttons={buttons}&frames={frames}', post=True)
        time.sleep(gap)

    def vsync(self, mode):
        """'off' paces at 59.94 Hz; 'limitless' runs as fast as it can."""
        self.req(f'/config?vsync={mode}', post=True)

    def shot(self, name):
        path = os.path.join(self.out, name + '.png')
        open(path, 'wb').write(self.req('/screenshot'))
        return path

    def interrupt(self):
        """SIGINT the game (not gdb), so a gdb script's commands after `run` execute."""
        pid = subprocess.check_output(['pgrep', '-P', str(self.proc.pid)]).split()[0] \
            if self.proc.args[0] == 'gdb' else self.proc.pid
        os.kill(int(pid), signal.SIGINT)

    def stop(self):
        if self.proc is not None and self.alive():
            # gdb runs the game in a process group of its own, which killing
            # gdb's leaves running (and holding the debug server's port).
            if self.proc.args[0] == 'gdb':
                for pid in subprocess.run(['pgrep', '-P', str(self.proc.pid)],
                                          capture_output=True).stdout.split():
                    os.kill(int(pid), signal.SIGKILL)
            os.killpg(self.proc.pid, signal.SIGTERM)
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)


def attach(tag, port, out):
    """A Run for a game started earlier (by another script), to keep driving it."""
    r = Run.__new__(Run)
    r.tag, r.port, r.out, r.proc = tag, port, out, None
    r.log = os.path.join(out, tag + '.log')
    r.raw = os.path.join(out, tag + '.raw')
    return r


def classify(path, refdir, names, limit=18):
    """The name of the reference screenshot (refdir/<name>.png) `path` looks
    most like, or None. Make the references locally from your own runs; they
    are game imagery and never go in the repository."""
    from PIL import Image, ImageChops, ImageStat
    a = Image.open(path).convert('L').resize((80, 60))
    best, bestd = None, 1e9
    for n in names:
        b = Image.open(os.path.join(refdir, n + '.png')).convert('L').resize((80, 60))
        d = ImageStat.Stat(ImageChops.difference(a, b)).mean[0]
        if d < bestd:
            best, bestd = n, d
    return best if bestd < limit else None


def loudness(raw_path, start_byte=0):
    """(peak, rms) per second of a raw S16LE stereo 44.1 kHz file."""
    data = open(raw_path, 'rb').read()[start_byte:]
    a = array.array('h')
    a.frombytes(data[:len(data) // 2 * 2])
    out = []
    step = 44100 * 2
    for i in range(0, len(a), step):
        seg = a[i:i + step]
        out.append((max(abs(x) for x in seg), (sum(x * x for x in seg) / len(seg)) ** 0.5))
    return out
