#!/usr/bin/env python3
"""Sort the reports of an LSD_CFI build's runs by what the mismatch does.

    tools/cfi_triage.py build-i686-cfi/lsd run1.log [run2.log ...]

Each report names a call site, the type of the pointer it called through
and the function it reached. The function's own type comes from the
binary's debug info (through gdb), and the two are compared with typedefs
stripped, so a report reads as, for example, "the callee takes 3 arguments,
the call passes 2". Kinds, worst first:

  missing-args   the callee takes more arguments than the call passes: it
                 reads the missing ones from whatever is on the stack
                 (or in the registers, at 64 bits)
  lost-return    the call uses a return value the callee doesn't produce
  int-pointer    an int where the other side has a pointer (truncated at
                 64 bits; a long, which psyz's u_long is, is not counted)
  width          integers of different sizes (u8 against s32, say)
  unprototyped   one side is declared without a prototype, so the check
                 can't compare them; read by hand
  extra-args     the call passes arguments the callee doesn't take
                 (harmless on x86 and MIPS, undefined in C)
  return-ignored the callee returns a value the call drops (harmless)
  sign           signed against unsigned of the same size (harmless)
  long-pointer   only a pointer carried in a long (psyz's u_long, which is
                 pointer-sized on every target it builds for)

Pointer-only differences don't reach here: LSD_CFI builds with
-fsanitize-cfi-icall-generalize-pointers.
"""
import os
import re
import subprocess
import sys

KINDS = ['missing-args', 'lost-return', 'int-pointer', 'width', 'unprototyped',
         'extra-args', 'return-ignored', 'sign', 'long-pointer', 'other']

REPORT = re.compile(r"^(\S+?):(\d+):\d+: runtime error: control flow integrity check "
                    r"for type '(.*)' failed during indirect function call")
# "file:line: note: name defined here", or "ld-temp.o: note: ..." under LTO
NOTE = re.compile(r"^(?:(\S+?):(\d+)|\S+): note: (\S+) defined here")


def parse_logs(paths):
    """{(site, slot type, callee): count of logs it appears in}"""
    found = {}
    for path in paths:
        lines = open(path, errors='replace').read().splitlines()
        for i, line in enumerate(lines):
            m = REPORT.match(line)
            if not m:
                continue
            site = os.path.relpath(m.group(1)) + ':' + m.group(2)
            callee = '?'
            for nxt in lines[i + 1:i + 3]:
                n = NOTE.match(nxt)
                if n:
                    callee = n.group(3).split('.')[0]  # LTO's static suffix
                    if n.group(1):
                        # The name can be a function inlined at the
                        # callee's start; the line is the callee's own.
                        callee = '@%s:%s' % (os.path.abspath(n.group(1)), n.group(2))
                    break
            key = (site, m.group(3), callee)
            found[key] = found.get(key, 0) + 1
    return found


def in_gdb(reports):
    import gdb

    def kind(t):
        t = t.strip_typedefs()
        if t.code == gdb.TYPE_CODE_VOID:
            return ('void', 0, False)
        if t.code in (gdb.TYPE_CODE_PTR, gdb.TYPE_CODE_ARRAY, gdb.TYPE_CODE_FUNC):
            return ('ptr', 0, False)
        if t.code in (gdb.TYPE_CODE_INT, gdb.TYPE_CODE_ENUM, gdb.TYPE_CODE_CHAR,
                      gdb.TYPE_CODE_BOOL):
            if 'long' in str(t):  # as wide as a pointer, outside Windows
                return ('long', 0, False)
            return ('int', t.sizeof, t.is_signed if hasattr(t, 'is_signed') else None)
        return (str(t), t.sizeof, False)

    def compare(slot, callee):
        kinds = set()
        if str(slot).endswith('()') or str(callee).endswith('()'):
            kinds.add('unprototyped')
        sp = [f.type for f in slot.fields()]
        cp = [f.type for f in callee.fields()]
        if len(cp) > len(sp) and 'unprototyped' not in kinds:
            kinds.add('missing-args')
        if len(sp) > len(cp) and 'unprototyped' not in kinds:
            kinds.add('extra-args')
        sr, cr = kind(slot.target()), kind(callee.target())
        if sr[0] != 'void' and cr[0] == 'void':
            kinds.add('lost-return')
        elif sr[0] == 'void' and cr[0] != 'void':
            kinds.add('return-ignored')
        pairs = list(zip(sp, cp))
        if sr[0] != 'void' and cr[0] != 'void':
            pairs.append((slot.target(), callee.target()))
        for a, b in pairs:
            ka, kb = kind(a), kind(b)
            if ka == kb:
                continue
            if {ka[0], kb[0]} == {'ptr', 'long'}:
                continue
            if {ka[0], kb[0]} == {'ptr', 'int'}:
                kinds.add('int-pointer')
            elif ka[0] == kb[0] == 'int' and ka[1] != kb[1]:
                kinds.add('width')
            elif ka[0] == kb[0] == 'int':
                kinds.add('sign')
            else:
                kinds.add('other')
        return sorted(kinds, key=KINDS.index) or ['long-pointer']

    rows = []
    for (site, slot_str, callee_name), n in reports.items():
        try:
            slot = gdb.parse_and_eval('(%s)0' % re.sub(r'\(', '(*)(', slot_str, count=1)).type.target()
        except gdb.error as e:
            slot = None
        try:
            if callee_name.startswith('@'):
                pc = gdb.decode_line(callee_name[1:])[1][0].pc
                callee_name = gdb.execute('info symbol %d' % pc, to_string=True).split()[0]
                callee_name = callee_name.split('.')[0]
            callee = gdb.parse_and_eval(callee_name).type
        except gdb.error:
            callee = None
        if slot is None or callee is None:
            kinds = ['other']
            callee_str = '?'
        else:
            kinds = compare(slot, callee)
            callee_str = str(callee)
        rows.append((KINDS.index(kinds[0]), site, kinds, slot_str, callee_name, callee_str, n))
    rows.sort()
    counts = {}
    for _, site, kinds, slot_str, callee_name, callee_str, n in rows:
        counts[kinds[0]] = counts.get(kinds[0], 0) + 1
        print('%-15s %s\n    call:   %s\n    callee: %s %s\n    (%s, in %d log%s)'
              % (kinds[0], site, slot_str, callee_name, callee_str, ', '.join(kinds),
                 n, '' if n == 1 else 's'))
    print('\n' + ', '.join('%s %d' % (k, counts[k]) for k in KINDS if k in counts)
          + ' (%d call sites)' % len(rows))


if __name__ == '__main__':
    if 'gdb' in sys.modules or os.environ.get('CFI_TRIAGE_IN_GDB'):
        import json
        in_gdb({tuple(k): v for k, v in json.loads(os.environ['CFI_TRIAGE_REPORTS'])})
    else:
        if len(sys.argv) < 3:
            sys.exit(__doc__)
        import json
        reports = parse_logs(sys.argv[2:])
        env = dict(os.environ, CFI_TRIAGE_IN_GDB='1',
                   CFI_TRIAGE_REPORTS=json.dumps([[list(k), v] for k, v in reports.items()]))
        sys.exit(subprocess.call(['gdb', '-q', '-batch', '-nx', '-x', os.path.abspath(__file__),
                                  sys.argv[1]], env=env))
