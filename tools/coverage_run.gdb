# gdb side of tools/coverage_run.py. Prints LSDMARK lines into the run's
# log: STARTDAY as each day starts (setting the day from COV_DAYS), MENU as
# the title menu comes up, where it also drops back to normal speed (the
# menu times out in ~10 s of game time). Every TitleMenu refresh raises the
# flashback unlock score, so FLASHBACK shows once a day has stored one.
set pagination off
set confirm off
handle SIGPIPE nostop noprint pass
python
import gdb, os
DAYS = [int(x) for x in os.environ.get('COV_DAYS', '').split(',') if x]
class StartDay(gdb.Breakpoint):
    n = 0
    def stop(self):
        # under CFI each function also has a jump-table entry, without
        # debug info: skip that hit, the body's comes next
        try:
            gdb.parse_and_eval('self')
        except gdb.error:
            return False
        d = DAYS[StartDay.n] if StartDay.n < len(DAYS) else None
        if d is not None:
            gdb.execute('set var self->currentDay = %d' % d)
        gdb.write('LSDMARK STARTDAY %d %s\n' % (StartDay.n, gdb.parse_and_eval('self->currentDay')))
        gdb.flush()
        StartDay.n += 1
        return False
class Refresh(gdb.Breakpoint):
    def stop(self):
        try:
            gdb.parse_and_eval('self')
        except gdb.error:
            return False
        gdb.execute('set var ((DreamSaveBlock *)self->saveBlock)->totalFlashbackUnlockScore = 10000000')
        return False
StartDay('DreamSys__StartDay')
Refresh('TitleMenu__RefreshMenu')
end
break GameApplication__RunTitleMenu
commands
silent
call (int)Psyz_VideoSetVsyncMode(2)
python gdb.write('LSDMARK MENU\n'); gdb.flush()
continue
end
run
