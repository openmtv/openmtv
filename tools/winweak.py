#!/usr/bin/env python3
"""Weak definitions for the Windows link (make win): ELF's rule by hand.

    python3 tools/winweak.py NM OBJECT... > build/win/weak.rsp

src/ overrides gen/ by weak symbols (README, "Layers"): every gen/ function is
a weak definition, the decompiled one a strong one, and the linker takes the
strong one where there is one, else the weak one.  On Windows (PE-COFF) gcc
makes a weak definition NAME into a "weak external" NAME whose fallback is a
plain symbol .weak.NAME.SUFFIX holding the code.  GNU ld 2.40 (Debian 12's
MinGW-w64) takes the strong definition fine, but where there is none it
leaves NAME undefined when another object refers to it ("undefined reference",
a known PE-COFF bug of GNU ld).  So: for each weak NAME that no object
defines strong, define it as its fallback (--defsym NAME=.weak.NAME.SUFFIX;
the first object's, in link order, like ELF).  Printed as gcc options, one per
line, for a response file (@build/win/weak.rsp).  Standard library only."""
import re
import subprocess
import sys

FALLBACK = re.compile(r'\.weak\.([A-Za-z_][A-Za-z0-9_$]*)\.(.*)$')


def main():
    nm, objs = sys.argv[1], sys.argv[2:]
    strong, weak, fallback = set(), [], {}
    for o in objs:
        out = subprocess.run([nm, o], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            f = line.split()
            if len(f) == 2 and f[0] == 'w':                 # a weak external (the weak definition's name)
                weak.append(f[1])
            elif len(f) == 3 and f[1] in 'TDBRC':          # a global definition
                m = FALLBACK.match(f[2])
                if m:
                    fallback.setdefault(m.group(1), f[2])   # the first one, in link order
                else:
                    strong.add(f[2])
    seen = set()
    for name in weak:
        if name in strong or name in seen or name not in fallback:
            continue
        seen.add(name)
        print('-Wl,--defsym=%s=%s' % (name, fallback[name]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
