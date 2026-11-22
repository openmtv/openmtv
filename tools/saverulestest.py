#!/usr/bin/env python3
"""Checks that a single player's savegame carries its rules (src/saverules.c);
verify.sh step 21.

The original game files in a temporary directory, the decompiled build:
1. a game started with rules (fixes, 20 auction lots, 17 film offers, the
   fourth speed, the AI improvement, credit line, terror damage, Sammy prizes,
   the series topicality, the unfinished screenplays): the setup's "Load
   game", the shipped slot 1 (it has no rules: the start's stay), lot 11
   poked, the safe, "Save", slot 7 "Seven" -- the savegame's extension has an
   eighth count, the line of the rules after the lots;
2. --load-slot 7 in a game started bare (Classic, the original's rules): the
   savegame's rules come back -- the line on stderr, lot 11 (the lots' count
   was set before the extension's lots were read), the data fix live-aid
   (film 164: 2 hours);
3. the same from the setup's "Load game", slot 7 (the load in the game);
4. a game saved bare: slot 8 holds the original's bytes, no extension; loaded
   in a game started with the fixes it keeps them (no line on stderr);
5. one game, bare: slot 7 loaded, then slot 8 (game_load, as the safe loads)
   -- a tape without rules plays with the start's again (live-aid out again);
   and slot 7, then the start savegame (a new game: game over brings the
   setup back and loads it) -- the start's again too;
6. a line from another build: an unknown key passed over, an unknown fix not
   played (but kept in the choice: saved again it is there), a bad count, a
   credit line --credit-line would refuse and the keys missing the original's;
7. the largest line (every fix by name, every amount at its top) fits in
   SAVERULES_MAX, and the launcher counts that much into a savegame's largest
   (launcher/content.go, tools/content.py);
8. the rules are a network game's parameters (runtime/main.c): every key of
   those is a rule here or named as none.
"""
import os, re, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
DEC = os.path.join(ROOT, 'build', 'mtv-dec')
SAVE_BASE = 37605
NAMES_RES = 0xD1

RULES = ['--fixes=all', '--auction-lots=20', '--film-offers=17', '--fourth-speed', '--ai-improve=normal',
         '--credit-line=900000', '--terror-damage=0', '--sammy-prize=1000,2000,3000,4000', '--series-topicality',
         '--original-screenplays']
LINE = ('fixes=all speed4=1 aiimp=1 credit=900000 terror=0 sammy=1000,2000,3000,4000 offers=10 '
        'filmoffers=17 lots=20 sertop=1 scripts=1')
BARE = ('fixes=classic speed4=0 aiimp=0 credit=-1 terror=-1 sammy=0,0,0,0 offers=10 filmoffers=10 lots=10 '
        'sertop=0 scripts=0')
# a network game's parameters that are no rule of a saved game: who plays, the game's start, its state
NOT_RULES = {'humans', 'args', 'time', 'money', 'knock', 'start', 'difficulty', 'names', 'seed', 'files'}

failed = 0


def check(cond, what):
    global failed
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        failed += 1


def run(d, lines, extra=()):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write('\n'.join(lines) + '\n')
        script = f.name
    try:
        p = subprocess.run([DEC, '--headless', '--nosound', '--logical-time', '--game', d, '--args', 'e',
                            '--script', script] + list(extra), capture_output=True, text=True, timeout=300)
    finally:
        os.unlink(script)
    vals = {}
    for l in p.stdout.splitlines():
        parts = l.split()
        if len(parts) == 5 and parts[0] == 'PEEK':
            vals[parts[3]] = int(parts[4])
    rules = [l.split(': the savegame\'s rules: ', 1)[1] for l in p.stderr.splitlines() if l.startswith('[save-rules]') and 'the savegame\'s rules' in l]
    return p.returncode, vals, p.stderr, rules


# lot 11's programme, film 164's length, the room
PEEKS = ['peek16 EC80:0000', 'peek8 4DA5:0EC8', 'peek16 39D4:838C']


def peeks(frame):
    return ['%d %s' % (frame, x) for x in PEEKS]


def save_script(slot_click, name, poke=True):
    """the setup's "Load game", slot 1, (lot 11 poked,) the safe, "Save", the slot, the name"""
    keys = ['%d key %s' % (2100 + 10 * i, c) for i, c in enumerate(name)]
    return (['400 key esc', '1000 click 160 100', '1200 click 100 28']
            + (['1400 poke16 EC80:0000 31'] if poke else [])
            + ['1600 click 76 35', '1800 click 134 75', '2000 click %s' % slot_click] + keys
            + ['2250 key enter', '2300 peek16 39D4:838C', '2400 quit'])


def slot_bytes(d, slot):
    """slot's savegame resource (the last file that has it) -- None if none"""
    found = None
    for n in sorted(os.listdir(d), key=str.lower):
        if not n.upper().endswith('.RSC'):
            continue
        b = open(os.path.join(d, n), 'rb').read()
        off, size = struct.unpack_from('<IH', b, 6 * (NAMES_RES + slot))
        if off:
            found = (n, off, size, b[off:off + size])
    return found


def extension(save):
    """the "MTVX" counts and the rules' line of a savegame's bytes (None, None without one)"""
    x = save[SAVE_BASE:]
    if len(x) < 6 or x[:4] != b'MTVX':
        return None, None
    n = struct.unpack_from('<H', x, 4)[0]
    c = [struct.unpack_from('<H', x, 6 + 2 * k)[0] for k in range(n)] + [200, 50, 50, 0, 10, 10, 10, 0][n:]
    at = 6 + 2 * n + max(c[0] - 200, 0) * 0x12 + max(c[1] - 50, 0) * 0x29 + max(c[2] - 50, 0) + c[3] \
        + max(c[4] - 10, 0) * 7 + max(c[5] - 10, 0) * 8 + max(c[6] - 10, 0) * 9
    return c, x[at:at + c[7]].decode('ascii', 'replace') if c[7] else None


def put_slot(d, slot, save):
    """slot's savegame replaced: appended to SOURCE1.RSC, its index entry pointed at it"""
    p = os.path.join(d, 'SOURCE1.RSC')
    with open(p, 'r+b') as f:
        f.seek(0, 2); end = f.tell(); f.write(save)
        f.seek(6 * (NAMES_RES + slot)); f.write(struct.pack('<IH', end, len(save)))


def fresh():
    d = tempfile.mkdtemp(prefix='mtv_saverules_')
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), d)
    return d


def main():
    if not os.path.exists(DEC):
        print('build/mtv-dec is not built'); return 1
    d = fresh()
    try:
        # 1. saved with rules
        rc, v, err, _ = run(d, save_script('220 45', 'Seven'), RULES)
        s7 = slot_bytes(d, 7)
        counts, line = extension(s7[3]) if s7 else (None, None)
        check(rc == 0 and counts and len(counts) == 8 and line == LINE,
              'slot 7 saved with rules: the eighth count, the line after the lots (%s, %r)' % (counts, line))
        # 2. --load-slot 7, bare
        rc, a, err, rules = run(d, peeks(800) + ['900 quit'], ['--load-slot', '7'])
        check(rc == 0 and rules == [LINE], '--load-slot 7 in a bare game: the savegame\'s rules (%s)' % rules)
        check(a.get('EC80:0000') == 31 and a.get('4DA5:0EC8') == 2 and a.get('39D4:838C') == 1,
              '--load-slot 7: lot 11 back (the 20 lots set before they were read), live-aid in (2 hours), the office (%s)' % a)
        # 3. the same from the setup's "Load game"
        rc, b, err, rules = run(d, ['400 key esc', '1000 click 160 100', '1200 click 220 45'] + peeks(1300) + ['1400 quit'])
        check(rules == [LINE] and b == a, 'the setup\'s "Load game", slot 7: the same (%s, %s)' % (rules, b))
        # 4. saved bare: the original's bytes; loaded in a game with the fixes, they stay
        rc, _, err, _ = run(d, save_script('220 62', 'Eight', poke=False))
        s8 = slot_bytes(d, 8)
        check(s8 and s8[2] == SAVE_BASE and extension(s8[3]) == (None, None),
              'slot 8 saved bare: %s bytes, no extension -- the original\'s savegame' % (s8 and s8[2]))
        rc, c, err, rules = run(d, peeks(800) + ['900 quit'], ['--load-slot', '8', '--fixes=all'])
        check(rc == 0 and not rules and 'save-rules' not in err and c.get('4DA5:0EC8') == 2,
              '--load-slot 8 with --fixes: no rules in it, the start\'s stay (live-aid in: %s)' % c)
        # 5. 7, then 8 in one game: the start's again
        rc, e, err, rules = run(d, ['400 key esc', '1000 click 160 100', '1200 click 220 45', '1500 call 229E:1386,%d' % (NAMES_RES + 8)]
                                + peeks(1501) + ['1600 quit'])
        again = [l for l in err.splitlines() if 'the start\'s again' in l]
        check(rules == [LINE] and again and e.get('4DA5:0EC8') == 0,
              'bare: slot 7, then slot 8 from the safe -- the start\'s rules again, live-aid out (%s, %s)' % (again, e))
        # 5b. slot 7, then a new game (the start savegame, as after game over: building.c)
        rc, g, err, rules = run(d, ['400 key esc', '1000 click 160 100', '1200 click 220 45', '1500 call 229E:1386,%d' % 0x2B]
                                + peeks(1501) + ['1600 quit'])
        new = [l for l in err.splitlines() if 'a new game: the start\'s rules again' in l]
        check(rules == [LINE] and new and g.get('4DA5:0EC8') == 0,
              'bare: slot 7, then the start savegame (a new game after game over) -- the start\'s rules again, live-aid out (%s, %s)' % (new, g))
        # 6. a line from another build
        line6 = 'lots=12 fixes=intruder,nosuchfix newrule=5 offers=99 credit=123456'
        save = bytearray(s7[3])
        cn, _ = extension(save)
        x = SAVE_BASE
        struct.pack_into('<H', save, x + 6 + 2 * 7, len(line6))
        save = bytes(save[:len(save) - cn[7]]) + line6.encode()
        put_slot(d, 7, save)
        rc, f, err, rules = run(d, peeks(800) + ['900 quit'], ['--load-slot', '7', '--fixes=all'])
        want = 'fixes=intruder,nosuchfix speed4=0 aiimp=0 credit=-1 terror=-1 sammy=0,0,0,0 offers=10 filmoffers=10 lots=12 sertop=0 scripts=0'
        check(rc == 0 and rules == [want] and "fix 'nosuchfix' is not in this game" in err and f.get('4DA5:0EC8') == 0,
              'another build\'s line: an unknown key passed over, an unknown fix not played but kept for the next save, a bad count and the missing keys the original\'s (%s; live-aid %s)'
              % (rules, f.get('4DA5:0EC8')))
    finally:
        shutil.rmtree(d, ignore_errors=True)
    # 7. the largest line
    names = [l.split()[0] for l in subprocess.run([DEC, '--fixes=list'], capture_output=True, text=True).stdout.splitlines() if l.strip()]
    big = ['--fixes=' + ','.join(names), '--auction-lots=20', '--film-offers=17', '--ad-offers=17', '--fourth-speed',
           '--ai-improve=strong', '--credit-line=100000000', '--terror-damage=100000000', '--sammy-prize=100000000',
           '--series-topicality', '--original-screenplays']
    maxdef = int(re.search(r'#define SAVERULES_MAX (\d+)', open(os.path.join(ROOT, 'src', 'fixes.h')).read()).group(1))
    d = fresh()
    try:
        rc, _, err, _ = run(d, save_script('220 45', 'Seven', poke=False), big)
        s = slot_bytes(d, 7)
        counts, line = extension(s[3]) if s else (None, None)
        check(line and line.startswith('fixes=' + ','.join(names) + ' ') and len(line) <= maxdef and 'do not fit' not in err,
              'every fix by name, every amount at its top: a line of %s bytes, at most %d (%d to spare)'
              % (len(line or ''), maxdef, maxdef - len(line or '')))
    finally:
        shutil.rmtree(d, ignore_errors=True)
    go = open(os.path.join(ROOT, 'launcher', 'content.go')).read()
    py = open(os.path.join(ROOT, 'tools', 'content.py')).read()
    gm = re.search(r'saveRulesMax\s*=\s*(\d+)', go)
    pm = re.search(r'SAVE_RULES_MAX\s*=\s*(\d+)', py)
    check(gm and int(gm.group(1)) == maxdef and pm and int(pm.group(1)) == maxdef,
          'launcher/content.go and tools/content.py count %s / %s bytes of rules into a savegame\'s largest (SAVERULES_MAX %d)'
          % (gm and gm.group(1), pm and pm.group(1), maxdef))
    # 8. the network game's parameters
    main_c = open(os.path.join(ROOT, 'runtime', 'main.c')).read()
    params = re.search(r'"humans=%s args=[^"]*"', main_c).group(0)
    pkeys = set(re.findall(r'(\w+)=', params))
    rkeys = set(re.findall(r'(\w+)=', re.search(r'"fixes=%s speed4[^"]*"', open(os.path.join(ROOT, 'src', 'saverules.c')).read()).group(0)))
    check(pkeys - NOT_RULES == rkeys,
          'a network game\'s parameters: rules of a saved game %s, none %s, missing here %s'
          % (sorted(rkeys), sorted(pkeys & NOT_RULES), sorted((pkeys - NOT_RULES) ^ rkeys)))
    print('saverulestest: %s' % ('all ok' if not failed else '%d failed' % failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
