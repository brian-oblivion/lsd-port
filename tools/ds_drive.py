#!/usr/bin/env python3
"""DuckStation as the console reference, driven headless: its own settings
and data under `root` (never the operator's), its own Xvfb, pad keys and
hotkeys through xdotool. The BIOS is read where it is installed.

    import ds_drive as ds
    d = ds.DS('/scratch/ref', disc_cue, args=['-fastboot'])
    d.window(); d.key('F9')            # start media capture (audio + video)
    d.key('start'); png = d.screenshot()
    d.stop()                           # capture: root/config/duckstation/videos/

The capture runs in emulated time: its audio is the SPU's output at 44.1 kHz
(PCM in an mp4, or mkv with VideoCodec ffv1 for lossless frames), so it does
not depend on the host keeping up. EmulationSpeed 0 (or holding Tab, with
FastForwardSpeed 0) runs ~11x. Screenshots are the native frame (320x240).
The GDB stub (port 2345) takes the decomp's build/lsdde.elf for symbols, but
reports late and slows emulation per breakpoint hit: use it for what was
called, not when. Vulkan cannot present on Xvfb, so Vulkan is hidden and
the presenter is OpenGL. No recording or screenshot goes in a repository.
"""
import glob
import os
import re
import signal
import subprocess
import time

DS_APPRUN = '/opt/duckstation-qt/AppRun'
BIOS_DIR = os.path.expanduser('~/.local/share/duckstation/bios')

SETTINGS = {
    'Main': dict(ConfirmPowerOff='false', SaveStateOnExit='false', InhibitScreensaver='false',
                 PauseOnFocusLoss='false', DisableBackgroundInput='false', SetupWizardIncomplete='false',
                 StartPaused='false'),
    'AutoUpdater': dict(CheckAtStartup='false'),
    'BIOS': dict(SearchDirectory=BIOS_DIR),
    'GPU': dict(Renderer='Software', ResolutionScale='1', DitheringMode='Unscaled'),
    'Display': dict(VSync='false', ScreenshotMode='UncorrectedInternalResolution', CropMode='BordersUncorrected',
                    OSDMessageLocation='None', ShowOSDMessages='false'),
    'Audio': dict(Backend='Null', StretchMode='None', OutputVolume='100'),
    'MediaCapture': dict(VideoCapture='false', AudioCapture='true', AudioContainer='wav',
                         AudioCodec='pcm_s16le'),
    'MemoryCards': dict(Card1Type='PerGameTitle', Card2Type='None'),
    'Pad1': dict(Type='DigitalController', Up='Keyboard/UpArrow', Down='Keyboard/DownArrow',
                 Left='Keyboard/LeftArrow', Right='Keyboard/RightArrow', Cross='Keyboard/K',
                 Circle='Keyboard/L', Square='Keyboard/J', Triangle='Keyboard/I', L1='Keyboard/Q',
                 R1='Keyboard/E', L2='Keyboard/1', R2='Keyboard/3', Select='Keyboard/Backspace',
                 Start='Keyboard/Enter'),
    'Hotkeys': dict(Screenshot='Keyboard/F10', ToggleMediaCapture='Keyboard/F9',
                    TogglePause='Keyboard/F8', FastForward='Keyboard/Tab'),
    'Logging': dict(LogToFile='true', LogLevel='Info'),
    'Debug': dict(EnableGDBServer='true', GDBServerPort='2345'),
}
# xdotool key names for the pad bindings above
PAD = dict(up='Up', down='Down', left='Left', right='Right', cross='k', circle='l', square='j',
           triangle='i', l1='q', r1='e', l2='1', r2='3', select='BackSpace', start='Return')


def patch_ini(path, settings):
    text = open(path).read() if os.path.exists(path) else ''
    sections = {}
    order = []
    cur = None
    for line in text.splitlines():
        m = re.match(r'^\[(.+)\]$', line)
        if m:
            cur = m.group(1); order.append(cur); sections[cur] = []
        elif cur is not None and '=' in line:
            k, v = line.split('=', 1)
            sections[cur].append([k.strip(), v.strip()])
    for sec, kv in settings.items():
        if sec not in sections:
            sections[sec] = []; order.append(sec)
        for k, v in kv.items():
            for pair in sections[sec]:
                if pair[0] == k:
                    pair[1] = v; break
            else:
                sections[sec].append([k, v])
    with open(path, 'w') as f:
        for sec in order:
            f.write(f'[{sec}]\n')
            for k, v in sections[sec]:
                f.write(f'{k} = {v}\n')
            f.write('\n')


class DS:
    def __init__(self, root, disc, extra=None, args=()):
        self.root = root
        self.cfg = os.path.join(root, 'config', 'duckstation')
        os.makedirs(self.cfg, exist_ok=True)
        os.makedirs(os.path.join(root, 'home'), exist_ok=True)
        os.makedirs(os.path.join(root, 'data'), exist_ok=True)
        s = {k: dict(v) for k, v in SETTINGS.items()}
        for sec, kv in (extra or {}).items():
            s.setdefault(sec, {}).update(kv)
        patch_ini(os.path.join(self.cfg, 'settings.ini'), s)
        r, w = os.pipe()
        self.xvfb = subprocess.Popen(['Xvfb', '-displayfd', str(w), '-screen', '0', '1280x1024x24',
                                      '-nolisten', 'tcp'], pass_fds=(w,), start_new_session=True,
                                     stderr=subprocess.DEVNULL)
        os.close(w)
        self.display = ':' + os.read(r, 16).decode().strip()
        os.close(r)
        env = dict(os.environ, DISPLAY=self.display, HOME=os.path.join(root, 'home'),
                   XDG_CONFIG_HOME=os.path.join(root, 'config'),
                   XDG_DATA_HOME=os.path.join(root, 'data'), QT_QPA_PLATFORM='xcb', VK_DRIVER_FILES='/nonexistent.json', VK_ICD_FILENAMES='/nonexistent.json')
        env.pop('WAYLAND_DISPLAY', None)
        self.env = env
        self.log = open(os.path.join(root, 'stdout.log'), 'w')
        self.t0 = time.time()
        self.proc = subprocess.Popen([DS_APPRUN, '-batch', '-nogui', *args, '--', disc],
                                     cwd='/opt/duckstation-qt', env=env, stdout=self.log,
                                     stderr=subprocess.STDOUT, start_new_session=True)
        self.win = None

    def xdo(self, *a):
        return subprocess.run(['xdotool', *a], env=self.env, capture_output=True, text=True).stdout

    def window(self, timeout=30):
        t = time.time()
        while time.time() - t < timeout:
            ids = self.xdo('search', '--onlyvisible', '--name', '.').split()
            # the biggest visible window is the display
            best = None
            for i in ids:
                g = self.xdo('getwindowgeometry', i)
                m = re.search(r'Geometry: (\d+)x(\d+)', g)
                if m:
                    a = int(m.group(1)) * int(m.group(2))
                    if a > 10000 and (best is None or a > best[0]):
                        best = (a, i)
            if best:
                self.win = best[1]
                self.xdo('windowfocus', '--sync', self.win)
                return self.win
            time.sleep(0.5)
        raise TimeoutError('no DuckStation window')

    def key(self, name, hold=0.1, gap=0.3):
        name = PAD.get(name, name)
        self.xdo('keydown', name); time.sleep(hold); self.xdo('keyup', name); time.sleep(gap)

    def screenshot(self):
        before = set(glob.glob(os.path.join(self.cfg, 'screenshots', '*')))
        self.key('F10', 0.05, 0.0)
        t = time.time()
        while time.time() - t < 10:
            new = set(glob.glob(os.path.join(self.cfg, 'screenshots', '*'))) - before
            if new:
                time.sleep(0.3)
                return new.pop()
            time.sleep(0.1)
        return None

    def stop(self):
        for p in (self.proc, self.xvfb):
            if p.poll() is None:
                os.killpg(p.pid, signal.SIGTERM)
                try:
                    p.wait(10)
                except subprocess.TimeoutExpired:
                    os.killpg(p.pid, signal.SIGKILL)
