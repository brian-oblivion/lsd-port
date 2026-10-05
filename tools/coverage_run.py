#!/usr/bin/env python3
"""A long headless run for a sanitizer build: days (some set to special
days from gdb), walking about in each, and the menu's GRAPH, FLASHBACK and
SAVE between them. Reports go to <out>/cov.log.

    COV_DAYS=0,1,10,14,50,42,100,119,200,201,300,357,363,6,20 \\
        tools/coverage_run.py /tmp/runs/cov build-i686-cfi/lsd 7795 15

COV_DAYS sets the day each StartDay begins (coverage_run.gdb); a day d
is special when d + 1 is in sSpecialDays. FLASHBACK is unlocked from the
first menu on, which moves the items below it down one: the "save" tour
opens FLASHBACK and the "load" tour SAVE. Walking is random but seeded.
Takes about 11 minutes for 15 days, intro included.
"""
import os
import random
import sys
import time
import urllib.error

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lsd_drive as d

if len(sys.argv) < 5:
    sys.exit(__doc__)
out, binary, port, ndays = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
disc = os.environ.get('LSD_DISC')  # the .cue; lsd_drive finds one in disc/ otherwise
r = d.Run('cov', port, out, binary=binary, vsync='limitless', disc=disc,
          gdb_script=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'coverage_run.gdb'),
          env={'UBSAN_OPTIONS': 'halt_on_error=0', 'ASAN_OPTIONS': 'halt_on_error=0:detect_leaks=0',
               'COV_DAYS': os.environ.get('COV_DAYS', '')})
rnd = random.Random(1)


def press(b, frames=4, gap=0.4):
    try:
        r.press(b, frames, gap)
    except urllib.error.HTTPError:  # the input queue is full
        time.sleep(0.5)
    except urllib.error.URLError:
        if not r.alive():
            raise


def menus():
    return r.count('LSDMARK MENU')


def starts():
    return r.count('LSDMARK STARTDAY')


def log(*a):
    print('%6.0f' % (time.time() - t0), *a, flush=True)


t0 = time.time()
try:
    # START skips the intro movies, until the first menu
    while menus() == 0 and time.time() - t0 < 300:
        press('start', 2, 1.0)
    for day in range(ndays):
        time.sleep(1.5)  # the graph or the menu settles
        tour = day % 4
        if day > 0 and tour == 1:  # GRAPH, the last item, and back
            log('graph')
            for _ in range(4):
                press('down', 2, 0.4)
            press('circle', 2, 3.0)
            r.shot('graph%d' % day)
            press('cross', 2, 2.0)
            press('triangle', 2, 2.0)
        if day > 0 and tour == 2:  # the second item, and circle through it
            log('second item')
            press('down', 2, 0.4)
            press('circle', 2, 2.5)
            press('circle', 2, 2.5)
            press('circle', 2, 2.5)
            r.shot('second%d' % day)
        if day > 0 and tour == 3:  # the third item
            log('third item')
            press('down', 2, 0.4)
            press('down', 2, 0.4)
            press('circle', 2, 2.5)
            press('circle', 2, 2.5)
            press('circle', 2, 2.5)
            r.shot('third%d' % day)
        # back to the top of the menu, then START until the day starts
        s = starts()
        tries = 0
        while starts() == s and tries < 40 and r.alive():
            if tries % 8 == 7:
                press('triangle', 2, 0.5)
            for _ in range(5):
                press('up', 2, 0.15)
            press('start', 2, 1.5)
            tries += 1
        log('day', day, r.text().split('LSDMARK STARTDAY')[-1].split('\n')[0])
        time.sleep(3)  # no START held as the dream begins: that pauses it
        r.vsync('limitless')
        m = menus()
        t1 = time.time()
        while menus() == m and time.time() - t1 < 240 and r.alive():
            b = rnd.choice(['up', 'up', 'up', 'up,left', 'up,right', 'left', 'right', 'down',
                            'up,l1', 'up,r1', 'cross'])
            press(b, rnd.randint(10, 40), 0.3)
        if menus() == m:
            log('no menu after 240 s; START once (paused?)')
            press('start', 2, 1)
        r.shot('end%d' % day)
    log('done, alive', r.alive())
finally:
    r.stop()
