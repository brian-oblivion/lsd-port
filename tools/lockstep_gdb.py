"""gdb half of tools/lockstep.py: makes a dream reproducible, tick for tick,
so two builds (i686 and x86_64, say) can be compared at the same moments.

Loaded by lockstep.py's gdb script; the run's JSON config is $LSD_LOCKSTEP.
At DreamSys__StartDay it seeds psyz's rand() and optionally sets the day;
during the dream it drives pad 1 from a script indexed by the dream
FrameClock's tick (Pad__DispatchEvents' masks), and at listed ticks it
freezes that clock for a while (for screenshots) and prints the game state.
Breakpoints are Python ones that print and continue: gdb's dprintf is
buffered, and an inferior call from a stop handler can deadlock the game.

Lines it prints, all prefixed for lockstep.py to collect:
  MARK <event> ...    StartDay, EndDay, FROZEN/THAWED <tick>, RunTitleMenu,
                      RunDayTask, FbUnlock
  STATE <tag> ...     tick, stage, day, the player's position and rotation,
                      psyz's rand state
  SAVEBLK ...         md5 of DreamSys's saved bytes at each StartDay
  POOL <tag> ...      bytes in the BMemPMgr pool's allocated blocks
  PKT ...             the dream's peak packet-area use (config "pktuse")
  PROBE ...           return values of the functions in config "probe"
  DRAW <tick> <kind> yaw=... pos=...
                      the camera (DreamSys) at each Viewport update in config
                      "draw_trace" [from, to): kind "tick" for the game's own,
                      "between" for src/pacing.c's in-between frames

Build with -O0 (CMAKE_BUILD_TYPE=Debug): at -O2 the pad and clock functions
can be inlined, and the script then misses them.
"""
import gdb
import hashlib
import json
import os

C = json.load(open(os.environ['LSD_LOCKSTEP']))
BITS = dict(up=1 << 12, down=1 << 14, left=1 << 15, right=1 << 13, triangle=1 << 4,
            cross=1 << 6, square=1 << 7, circle=1 << 5, start=1 << 11, select=1 << 8,
            l1=1 << 2, r1=1 << 3, l2=1 << 0, r2=1 << 1)


def mask(names):
    m = 0
    for n in names.split(','):
        if n:
            m |= BITS[n]
    return m


SCRIPT = [(a, b, mask(n)) for a, b, n in C.get('input', [])]
FREEZE = set()
FLEN = C.get('freeze_len', 40)
st = dict(active=False, frozen=0, prev=0, days=0)


def w(s):
    gdb.write(s + '\n')
    gdb.flush()


def ev(e):
    return gdb.parse_and_eval(e)


def tick():
    c = ev('sDreamAuxFrameClock')
    return int(c['frameCount']) if int(c) else -1


def pool_use():
    """Bytes in allocated blocks, the highest allocated byte, the pool size."""
    first = int(ev("(unsigned long)'bmem_pmgr.c'::sDefaultBMemPMgr->firstBlock"))
    size = int(ev("'bmem_pmgr.c'::sDefaultBMemPMgr->poolSize"))
    data = gdb.selected_inferior().read_memory(first, size).tobytes()
    off = used = top = 0
    while off < size:
        word = int.from_bytes(data[off:off + 4], 'little')
        bs = word & 0x0FFFFFFF
        if bs == 0:
            break
        if not word & 0x40000000:
            used += bs
            top = off + bs
        off += bs
    return used, top, size


def state(tag):
    if int(ev('sDreamAuxWorld')) == 0:
        w('STATE %s noworld' % tag)
        return
    ds = ev('sDreamAuxWorld')
    c2 = ds['coord2']
    t = c2['coord']['t']
    m = c2['coord']['m']
    w('STATE %s tick=%d stage=%d day=%d pos=%d,%d,%d m=%d,%d,%d,%d rand=%d' % (
        tag, tick(), int(ds['currentStage']), int(ds['currentDay']),
        int(t[0]), int(t[1]), int(t[2]), int(m[0][0]), int(m[0][2]), int(m[2][0]),
        int(m[2][2]), int(ev("'libc.c'::rand_next"))))
    try:
        u, top, size = pool_use()
        w('POOL %s used=%d top=%d size=%d' % (tag, u, top, size))
    except gdb.error as e:
        w('POOL %s error %s' % (tag, e))


class StartDay(gdb.Breakpoint):
    def stop(self):
        days = C.get('days', [])
        if st['days'] < len(days) and days[st['days']] is not None:
            gdb.execute('set var self->currentDay = %d' % days[st['days']])
        if 'seed' in C:
            gdb.execute("set var 'libc.c'::rand_next = %d" % C['seed'])
        st['days'] += 1
        st['active'] = True
        FREEZE.update(C.get('freeze', []))
        a = int(ev('(unsigned long)&self->saveMagic'))
        n = int(ev('(char *)&self->newGamePending - (char *)&self->saveMagic'))
        b = gdb.selected_inferior().read_memory(a, n).tobytes()
        w('SAVEBLK day=%d size=%d md5=%s' % (int(ev('self->currentDay')), n,
                                             hashlib.md5(b).hexdigest()))
        w('MARK StartDay %d' % int(ev('self->currentDay')))
        return False


class EndDay(gdb.Breakpoint):
    def stop(self):
        state('end')
        st['active'] = False
        if 'pkmax' in st:
            w('PKT max %d' % st['pkmax'])
        w('MARK EndDay %d' % st['days'])
        return False


class Tick(gdb.Breakpoint):
    """FrameClock__Tick on the dream's clock: freezes and the trace."""
    def stop(self):
        s = ev('self')
        if not st['active'] or int(s) != int(ev('sDreamAuxFrameClock')):
            return False
        if st['frozen'] > 0:
            st['frozen'] -= 1
            if st['frozen'] == 0:
                gdb.execute('set var self->paused = 0')
                w('MARK THAWED %d' % int(s['frameCount']))
            return False
        t = int(s['frameCount'])
        trace = C.get('trace')
        if trace and trace[0] <= t < trace[1]:
            state('tr')
        if t in FREEZE and int(s['paused']) == 0:
            FREEZE.discard(t)
            gdb.execute('set var self->paused = 1')
            st['frozen'] = FLEN
            state('t%d' % t)
            w('MARK FROZEN %d' % t)
        return False


class PadDispatch(gdb.Breakpoint):
    """The pad's masks from the script while a dream runs (none while frozen)."""
    def stop(self):
        if not st['active']:
            return False
        held = 0
        if not st['frozen']:
            t = tick()
            for a, b, m in SCRIPT:
                if a <= t < b:
                    held |= m
        prev = st['prev']
        gdb.execute('set var self->heldMask = %d' % held)
        gdb.execute('set var self->pressedMask = %d' % (held & ~prev))
        gdb.execute('set var self->releasedMask = %d' % (prev & ~held))
        st['prev'] = held
        return False


class Mark(gdb.Breakpoint):
    def __init__(self, fn, tag):
        super().__init__(fn, internal=True)
        self.tag = tag

    def stop(self):
        w('MARK ' + self.tag)
        return False


class FlashbackUnlock(gdb.Breakpoint):
    """Task 08's trick: the unlock score past 9999999, so FLASHBACK shows
    once a day has stored a flashback."""
    def stop(self):
        save = '((DreamSaveBlock *)self->saveBlock)'
        gdb.execute('set var %s->totalFlashbackUnlockScore = 10000000' % save)
        w('MARK FbUnlock flashbacks=%d' % int(ev(save + '->amountFlashbacksAvailable')))
        return False


class PacketUse(gdb.Breakpoint):
    """At Viewport__Flip, in a dream: the packet area the frame just used."""
    def stop(self):
        if not st['active']:
            return False
        try:
            vp = ev('self')
            cap = (int(vp['maxPackets']) * int(vp['packetSize']) *
                   ((int(ev('sizeof(P_TAG)')) + 7) // 8))
            p = int(ev('(char *)GsOUT_PACKET_P'))
            us = [p - int(ev('(char *)self->workBase[%d]' % i)) for i in (0, 1)]
            us = [u for u in us if 0 <= u <= 4 * cap]
            used = min(us) if us else -1
            st['pkmax'] = max(st.get('pkmax', 0), used)
            if used > cap * 0.9 and not st.get('pkwarned'):
                st['pkwarned'] = 1
                w('PKT near cap: used %d of %d at tick %d' % (used, cap, tick()))
        except gdb.error:
            pass
        return False


class Ret(gdb.FinishBreakpoint):
    def __init__(self, frame, name):
        super().__init__(frame, internal=True)
        self.name = name

    def stop(self):
        try:
            v = self.return_value
        except gdb.error:
            v = None
        w('PROBE t=%d %s -> %s' % (tick(), self.name, v))
        return False

    def out_of_scope(self):
        pass


class Probe(gdb.Breakpoint):
    def __init__(self, fn, a, b):
        super().__init__(fn, internal=True)
        self.fn, self.a, self.b = fn, a, b

    def stop(self):
        if st['active'] and self.a <= tick() < self.b:
            Ret(gdb.newest_frame(), self.fn)
        return False


class DrawTrace(gdb.Breakpoint):
    """Each Viewport update in a tick range: who drew, and the camera."""
    def stop(self):
        if not st['active']:
            return False
        a, b = C['draw_trace']
        t = tick()
        if not a <= t < b:
            return False
        caller = gdb.newest_frame().older()
        kind = 'between' if caller is not None and caller.name() == 'DrawInBetween' else 'tick'
        c2 = ev('sDreamAuxWorld')['coord2']
        r = c2['param']['rotate']
        p = c2['coord']['t']
        w('DRAW %d %s yaw=%d pitch=%d roll=%d pos=%d,%d,%d' % (
            t, kind, int(r['vy']), int(r['vx']), int(r['vz']), int(p[0]), int(p[1]), int(p[2])))
        return False


StartDay('DreamSys__StartDay', internal=True)
EndDay('DreamSys__EndDay', internal=True)
Tick('FrameClock__Tick', internal=True)
PadDispatch('Pad__DispatchEvents', internal=True)
Mark('GameApplication__RunTitleMenu', 'RunTitleMenu')
Mark('GameApplication__RunDayTask', 'RunDayTask')
if C.get('flashback'):
    FlashbackUnlock('UpdateFlashbackLock', internal=True)
if C.get('pktuse'):
    PacketUse('Viewport__Flip', internal=True)
if C.get('draw_trace'):
    DrawTrace('NodeGuardedViewport__Update', internal=True)
if C.get('probe'):
    a, b = C['probe']['ticks']
    for f in C['probe']['funcs']:
        Probe(f, a, b)
