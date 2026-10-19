#!/usr/bin/env python3
"""The AI improvement's scenario tests (src/ai_improve.c, docs/ki-verbesserung.md).

    python3 tools/aiimptest.py [NAME ...]      all tests (or the named ones)

Each test plays a new game (--new-game, English, the human on station 1, the
game speed 60, the human's money held >= 0) with build/mtv-dec on fresh game
files; the human buys transmitters at fixed game times (the script's day
lines "@DAY.HOUR txbuy 1 100000", runtime/host.c), and the computer players'
diagnostics ("[ai-improve] ..." on stderr) are checked:

  follow_normal, follow_strong
      day 1 18:00 two transmitters (5.0 -> 13.0 M), 20:00 a third (17.0 M),
      during the wait.  Each computer player was made with reference 5.0 M
      and a personality; it notices 13.0 M at 18:00 -- not the third --,
      its wait and factor lie in the level's range and in its personality's
      half; the reaction is due when the wait is over; every purchase of the
      site rule's first kind brings 80-130 % of the gap; after the reaction
      it notices 17.0 M (one step behind).
  satellite_normal, satellite_strong
      the human buys the west satellite on day 1 (5.0 -> ~110 M).  Strong:
      every computer player buys one satellite in its reaction (as many as
      the human has), never more, and one of at most 130 % of the gap.
      Normal (one fewer than the human): none; one transmitter a reaction
      leaves the gap open, so it follows up -- after half a wait of the
      level, more than 15 % of the target still missing, the same target.
  off
      the same without the option and with --ai-improve=off: no diagnostics,
      the same game (STATE lines) both ways.
  save_load
      strong (its targets lie above the computer players' own reach, so both
      buy): the game saved at day 1 20:00 (during the wait, game_save
      229E:200D by the harness's "call") and loaded at once (game_load
      229E:1386): the state loaded is the state saved, and the reaction goes
      on as in the game without the load -- the same due time, target and
      first site.
  old_save
      a savegame made without the option, loaded with it: the state made at
      the game's start (5.0 M) starts anew, its reference the human's reach
      in the savegame (13.0 M: no reaction to what the human had bought
      before).
  three_humans
      tests/mp/ai_improve3 (three humans, no computer player; --humans
      locally) with and without the option: the same game, no diagnostics.

Game files come from $GAME (default: the directory above decomp/)."""
import os, re, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
FRAMES_PER_DAY = 34750           # at game speed 60 (tools/ai_run.py: 69 500 at 30)
SLOT = 0xD1                      # the first savegame slot (room_saveload_click)
LEVEL = {'normal': ((480, 1200), (500, 800)), 'strong': ((240, 720), (800, 1020))}

T = r'\[ai-improve\] day (\d+) (\d\d):(\d\d) '
RE_MADE = re.compile(T + r'player (\d): (cautious|ambitious), level (\w+), reference ([\d.]+) M')
RE_NOTICE = re.compile(T + r'player (\d) \((\w+)\): noticed ([\d.]+) M \(reference ([\d.]+) M\), waits (\d+):(\d\d) h until day (\d+) (\d\d):(\d\d), factor ([\d.]+)')
RE_DUE = re.compile(T + r'player (\d): due, target ([\d.]+) M \(([\d.]+) x ([\d.]+) M\), own ([\d.]+) M, gap ([\d.]+) M')
RE_BUY = re.compile(T + r'player (\d): bought (\d+),(\d+) rule (\d), new ([\d.]+) M for a gap of ([\d.]+) M')
RE_SAT = re.compile(T + r'player (\d): bought the (west|east) satellite rule (\d), new ([\d.]+) M for a gap of ([\d.]+) M')
RE_FOLLOW = re.compile(T + r'player (\d): gap still ([\d.]+) M \((\d+) % of the target\), follows up after (\d+):(\d\d) h at day (\d+) (\d\d):(\d\d)')
RE_OVER = re.compile(T + r'player (\d): reaction over \(([^)]*)\), reference ([\d.]+) M')
RE_STATE = re.compile(T + r'(saved|loaded) player (\d): (.*)$')


def minutes(day, hh, mm):
    """played minutes of "day D HH:MM" (the broadcast day 17:00-01:00)"""
    h = int(hh)
    return int(day) * 480 + ((h if h >= 17 else h + 24) - 17) * 60 + int(mm)


def events(err):
    """the diagnostics as (minute, kind, player, match) in their order"""
    out = []
    for line in err.split('\n'):
        for kind, rx in (('made', RE_MADE), ('notice', RE_NOTICE), ('due', RE_DUE), ('buy', RE_BUY), ('sat', RE_SAT),
                         ('follow', RE_FOLLOW), ('over', RE_OVER), ('state', RE_STATE)):
            m = rx.search(line)
            if m:
                out.append((minutes(*m.groups()[:3]), kind, int(m.group(4) if kind != 'state' else m.group(5)), m))
                break
    return out


def script(days, extra=(), lines=()):
    """a new game's script: Esc for the boss's dialogs, speed 60, the day lines"""
    end = 800 + days * FRAMES_PER_DAY
    body = [(600, 'poke16 39D4:72A2 60')] + [(f, 'key esc') for f in range(900, end, 300)] + list(extra) + [(end - 2, 'hash'), (end, 'quit')]
    return ['%d %s' % l for l in sorted(body)] + list(lines)


def run(lines, opts=(), game=None):
    d = game or tempfile.mkdtemp(prefix='mtv_aiimp_')
    try:
        if not game:
            for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
                shutil.copy(os.path.join(GAME, f), d)
        s = os.path.join(d, 'test.txt')
        with open(s, 'w') as f:
            f.write('\n'.join(lines) + '\n')
        p = subprocess.run([BIN, '--headless', '--fast', '--nosound', '--game', d, '--args', 'e', '--script', s,
                            '--new-game', '--fixes', '--clamp', '39D4:8BFD:0'] + list(opts),
                           capture_output=True, text=True, timeout=1800)
        return p.stdout, p.stderr
    finally:
        if not game:
            shutil.rmtree(d, ignore_errors=True)


PLAN = ['@1.1 txbuy 1 100000', '@1.1 txbuy 1 100000', '@1.3 txbuy 1 100000']


def check_follow(level, err, bad):
    ev = events(err)
    (wlo, whi), (flo, fhi) = LEVEL[level]
    for p in (2, 3):
        mine = [e for e in ev if e[2] == p]
        made = [e for e in mine if e[1] == 'made']
        if len(made) != 1 or made[0][3].group(6) != level or made[0][3].group(7) != '5.0':
            bad.append('player %d: not made once with reference 5.0 M at level %s: %s' % (p, level, [e[3].group(0) for e in made]))
            continue
        amb = made[0][3].group(5) == 'ambitious'
        notices = [e for e in mine if e[1] == 'notice']
        if not notices or notices[0][3].group(6) != '13.0' or notices[0][0] != minutes(1, 18, 0):
            bad.append('player %d: the first notice is not 13.0 M at day 1 18:00: %s' % (p, notices[0][3].group(0) if notices else '-'))
            continue
        n = notices[0][3]
        wait, f = int(n.group(8)) * 60 + int(n.group(9)), round(float(n.group(13)) * 1000)
        wmid, fmid = (wlo + whi) // 2, (flo + fhi) // 2
        if not (wlo <= wait <= whi and (wmid <= wait if not amb else wait <= wmid)):
            bad.append('player %d (%s): wait %d min outside %d..%d or its half' % (p, made[0][3].group(5), wait, wlo, whi))
        if not (flo <= f <= fhi and (fmid <= f if amb else f <= fmid)):
            bad.append('player %d (%s): factor %d outside %d..%d or its half' % (p, made[0][3].group(5), f, flo, fhi))
        due = minutes(n.group(10), n.group(11), n.group(12))
        if due != notices[0][0] + wait:
            bad.append('player %d: due %d is not the notice + the wait' % (p, due))
        dues = [e for e in mine if e[1] == 'due']
        if not dues or not 0 <= dues[0][0] - due <= 15:
            bad.append('player %d: the reaction not due at %d: %s' % (p, due, dues[0][3].group(0) if dues else '-'))
        overs = [e for e in mine if e[1] == 'over']
        if not overs:
            bad.append('player %d: the reaction never ended' % p)
            continue
        between = [e for e in notices[1:] if e[0] < overs[0][0] or (e[0] == overs[0][0] and mine.index(e) < mine.index(overs[0]))]
        if between:
            bad.append('player %d: noticed again during its reaction: %s' % (p, between[0][3].group(0)))
        if overs[0][3].group(6) != '13.0':
            bad.append('player %d: reference after the reaction %s, not 13.0 M' % (p, overs[0][3].group(6)))
        if len(notices) < 2 or notices[1][3].group(6) != '17.0':
            bad.append('player %d: no second step (17.0 M) after the reaction' % p)
        for e in mine:
            if e[1] == 'buy' and e[3].group(7) == '1':
                r = float(e[3].group(8)) / max(float(e[3].group(9)), 0.01)
                if not 0.79 <= r <= 1.31:
                    bad.append('player %d: rule 1 bought %.0f %% of the gap: %s' % (p, r * 100, e[3].group(0)))
    return ev


def test_follow(level):
    out, err = run(script(5, lines=PLAN), ['--ai-improve=' + level])
    bad = []
    ev = check_follow(level, err, bad)
    buys = [e for e in ev if e[1] == 'buy']
    if not buys:
        bad.append('no follow-up purchase in five days')
    return bad, '%d purchases, %s' % (len(buys), ', '.join('p%d %s' % (e[2], e[3].group(0).split(': ', 1)[1][:40]) for e in buys[:3]))


def test_satellite(level):
    """the human buys the west satellite on day 1 (5.0 -> ~110 M): strong, every computer player
    buys one satellite (as many as the human has) in its reaction, never a second; normal (one
    fewer than the human), none -- its single transmitters leave the gap open, and it follows
    up: each follow-up after half a wait of the level, to the same target"""
    out, err = run(script(5, lines=['@1.1 txbuy 1 -1']), ['--ai-improve=' + level])
    ev = events(err)
    (wlo, whi), _ = LEVEL[level]
    bad, info = [], []
    for p in (2, 3):
        mine = [e for e in ev if e[2] == p]
        sats = [e for e in mine if e[1] == 'sat']
        follows = [e for e in mine if e[1] == 'follow']
        targets = {e[3].group(5) for e in mine if e[1] == 'due'}
        if level == 'strong' and len(sats) != 1:
            bad.append('player %d: %d satellites bought, not one: %s' % (p, len(sats), [e[3].group(0) for e in sats]))
        if level == 'normal' and sats:
            bad.append('player %d: bought a satellite on normal (the human has one): %s' % (p, sats[0][3].group(0)))
        for e in sats:
            r = float(e[3].group(7)) / max(float(e[3].group(8)), 0.01)
            if r > 1.31:
                bad.append('player %d: the satellite brought %.0f %% of the gap (at most 130 %%)' % (p, r * 100))
        if level == 'normal':
            if not follows:
                bad.append('player %d: no follow-up in five days' % p)
            for e in follows:
                w = int(e[3].group(7)) * 60 + int(e[3].group(8))
                if not wlo // 2 - 1 <= w <= whi // 2 or int(e[3].group(6)) <= 15:
                    bad.append('player %d: follow-up wait %d min outside %d..%d or gap %s %%' % (p, w, wlo // 2, whi // 2, e[3].group(6)))
            if len(targets) != 1:
                bad.append('player %d: the follow-ups changed the target: %s' % (p, sorted(targets)))
        info.append('p%d %d satellite(s), %d follow-up(s)' % (p, len(sats), len(follows)))
    return bad, ', '.join(info)


def test_off():
    a = run(script(3, lines=PLAN))
    b = run(script(3, lines=PLAN), ['--ai-improve=off'])
    bad = []
    if '[ai-improve]' in a[1] + b[1]:
        bad.append('diagnostics without the option')
    sa, sb = [l for l in a[0].split('\n') if l.startswith('STATE')], [l for l in b[0].split('\n') if l.startswith('STATE')]
    if not sa or sa != sb:
        bad.append('another game with --ai-improve=off: %s / %s' % (sa, sb))
    return bad, 'the same game both ways (%s)' % (sa[0].split()[3] if sa else '-')


def first_buys(ev):
    return {p: next((e[3].groups()[4:7] for e in ev if e[1] == 'buy' and e[2] == p), None) for p in (2, 3)}


def test_save_load():
    at = 800 + FRAMES_PER_DAY * 3 // 8    # (day 1, ~20:00; the day lines' purchases at 18:00 and 20:00 before)
    save = (at, 'call 229E:200D,%d' % SLOT)
    a = run(script(4, [save], PLAN), ['--ai-improve=strong'])
    b = run(script(4, [save, (at + 5, 'call 229E:1386,%d' % SLOT)], PLAN), ['--ai-improve=strong'])
    bad = []
    ea, eb = events(a[1]), events(b[1])
    saved = [e[3].group(6) for e in eb if e[1] == 'state' and e[3].group(4) == 'saved']
    loaded = [e[3].group(6) for e in eb if e[1] == 'state' and e[3].group(4) == 'loaded']
    if not saved or saved != loaded:
        bad.append('the state loaded is not the state saved: %s / %s' % (saved, loaded))
    if not any('waiting 1' in s for s in saved):
        bad.append('nobody waiting when saved: %s' % saved)
    dues = lambda ev: [(e[2], e[3].group(5)) for e in ev if e[1] == 'due']
    if len(dues(ea)) < 2 or None in first_buys(ea).values():
        bad.append('the reactions did not come in four days: %s, %s' % (dues(ea), first_buys(ea)))
    if dues(ea)[:2] != dues(eb)[:2]:
        bad.append('other targets after the load: %s / %s' % (dues(ea)[:2], dues(eb)[:2]))
    if first_buys(ea) != first_buys(eb):
        bad.append('other first purchases after the load: %s / %s' % (first_buys(ea), first_buys(eb)))
    return bad, 'saved and loaded %d computer players, the same reaction (%s)' % (len(saved), first_buys(eb))


def test_old_save():
    d = tempfile.mkdtemp(prefix='mtv_aiimp_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(GAME, f), d)
        at = 800 + FRAMES_PER_DAY * 3 // 8
        run(script(1, [(at, 'call 229E:200D,%d' % SLOT)], PLAN[:2]), [], game=d)       # without the option
        out, err = run(script(2, [(6000, 'call 229E:1386,%d' % SLOT)]), ['--ai-improve=normal'], game=d)
    finally:
        shutil.rmtree(d, ignore_errors=True)
    bad = []
    ev = events(err)
    made = [e for e in ev if e[1] == 'made']
    refs = [(e[2], e[3].group(7)) for e in made]
    for p in (2, 3):                     # made at the new game's start (5.0 M), made anew after the load (13.0 M)
        mine = [r for q, r in refs if q == p]
        if mine != ['5.0', '13.0']:
            bad.append('player %d: not made at the start and anew with the savegame\'s reach (13.0 M): %s' % (p, mine))
    late = [e for e in ev if e[1] == 'notice']
    if late:
        bad.append('a reaction to what was bought before the save: %s' % late[0][3].group(0))
    return bad, 'made anew after the load: %s' % refs


def test_three_humans():
    """tests/mp/ai_improve3 (three humans, no computer player) played locally with and without the option"""
    path = os.path.join(ROOT, 'tests', 'mp', 'ai_improve3.txt')
    lines = [l for l in open(path).read().split('\n') if l.strip() and not l.startswith('#')]
    opts = []
    for l in open(path):
        if l.startswith('# options'):
            opts = [w for w in l.split()[2:] if not w.startswith('--ai-improve')]
    runs = [run(lines, ['--humans', '1,2,3'] + opts + extra) for extra in ([], ['--ai-improve=strong'])]
    got = [[l for l in out.split('\n') if l.startswith(('HASH', 'STATE', 'PEEK'))] for out, _ in runs]
    bad = []
    if not got[0] or got[0] != got[1]:
        bad.append('another game with the option: %s / %s' % (got[0][:2], got[1][:2]))
    if '[ai-improve]' in runs[1][1]:
        bad.append('diagnostics without a computer player')
    return bad, 'the same %d lines with and without the option' % len(got[0])


TESTS = {'follow_normal': lambda: test_follow('normal'), 'follow_strong': lambda: test_follow('strong'),
         'satellite_normal': lambda: test_satellite('normal'), 'satellite_strong': lambda: test_satellite('strong'),
         'off': test_off, 'save_load': test_save_load, 'old_save': test_old_save, 'three_humans': test_three_humans}


def main():
    names = sys.argv[1:] or list(TESTS)
    with ThreadPoolExecutor(max_workers=len(names)) as ex:
        fut = {n: ex.submit(TESTS[n]) for n in names}
        failed = 0
        for n in names:
            bad, info = fut[n].result()
            print('%-14s %s  (%s)' % (n, 'ok' if not bad else 'FAILS', info))
            for b in bad:
                print('    ' + b)
            failed += bool(bad)
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
