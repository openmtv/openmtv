#!/usr/bin/env python3
"""Play a recorded game again, without a window: what the player did, frame by frame.

    python3 tools/replay.py LOG [--bin PATH] [--until FRAME] [--shots F,F,..] [--out DIR] [-v]

LOG: a game's folder in build/logs/ (the launcher keeps the last ten: game.log,
input.txt -- the game's --record --, settings.ini and game/ with the game files it
started from), or a recording alone (then with the original game files, $GAME).
The game runs with the recording's own options (its "# command:" line) on a copy
of those files, headless, its input from the recording; every 1000 frames the
recording holds the game state as the live game had it ("# frame N state H"),
and the replay prints its own there (a "hash" line): the first frame where they
differ is where the replay went its own way (another build, an input the
recording misses).  A game that stopped (a halt, a divide error) stops here at
the same frame, with the same message.

--bin: the game (default build/mtv-dec; it must be the build that played -- the
same commit: the logical clock counts its functions); --until: stop there
(default: 300 frames after the last input); --shots: pictures of the screen at
those frames, each as it shows (shot) and at 4x (shothr, with --filter hr or
hr-pixel), into --out (default build/replay/); -v: the game's messages.
Standard library only."""
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
GAME = os.environ.get('GAME', os.path.abspath(os.path.join(ROOT, '..')))
VALUED = ('--record', '--script', '--game')       # options of the recording left out or replaced
LEFT_OUT = ('--fullscreen', '--headless')


def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def options(command, game, settings):
    """the recording's command line -> the replay's options (program left out)"""
    out, i = [], 1
    while i < len(command):
        a = command[i]
        name = a.split('=', 1)[0]
        if name in VALUED:
            i += 1 if '=' in a else 2
            continue
        if a in LEFT_OUT:
            i += 1
            continue
        if settings and name == '--settings':
            a = '--settings=' + settings
        out.append(a)
        i += 1
    return ['--game', game, '--headless', '--nosound'] + out


def main():
    if len(sys.argv) < 2 or sys.argv[1].startswith('-'):
        print(__doc__)
        sys.exit(2)
    log = os.path.abspath(sys.argv[1])
    rec = os.path.join(log, 'input.txt') if os.path.isdir(log) else log
    files = os.path.join(log, 'game') if os.path.isdir(log) else GAME
    settings = os.path.join(log, 'settings.ini') if os.path.isdir(log) else None
    if settings and not os.path.exists(settings):
        settings = None
    binary = opt('--bin', os.path.join(ROOT, 'build', 'mtv-dec'))
    out_dir = os.path.abspath(opt('--out', os.path.join(ROOT, 'build', 'replay')))
    command, checks, lines = None, {}, []
    for l in open(rec, encoding='utf-8', errors='replace'):
        l = l.rstrip('\n')
        if l.startswith('# command:'):
            command = shlex.split(l[len('# command:'):])
        elif l.startswith('# frame '):          # "# frame N state H"
            w = l.split()
            checks[int(w[2])] = w[4]
        elif l.strip() and not l.startswith('#'):
            lines.append((int(l.split()[0]), 1, l))
    if not command:
        sys.exit('%s: no "# command:" line (a recording of an older build: replay it by hand)' % rec)
    last = max([f for f, _, _ in lines] + list(checks) + [0])
    until = int(opt('--until', str(last + 300)))
    script = lines + [(f, 0, '%d hash' % f) for f in checks]
    shots = [int(f) for f in opt('--shots', '').split(',') if f]
    if shots:
        os.makedirs(out_dir, exist_ok=True)
    hr = any(a in ('--filter=hr', '--filter=hr-pixel') for a in command) or \
        any(a == '--filter' and b in ('hr', 'hr-pixel') for a, b in zip(command, command[1:]))
    for f in shots:
        script.append((f, 2, '%d shot %s' % (f, os.path.join(out_dir, 'frame%d.png' % f))))
        if hr:
            script.append((f, 2, '%d shothr %s' % (f, os.path.join(out_dir, 'frame%d_4x.png' % f))))
    script = [s for s in script if s[0] <= until]
    script.sort(key=lambda s: (s[0], s[1]))
    tmp = tempfile.mkdtemp(prefix='replay_')
    try:
        game = os.path.join(tmp, 'game')
        os.makedirs(game)
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(files, f), game)
        path = os.path.join(tmp, 'script.txt')
        open(path, 'w').write('\n'.join(s[2] for s in script) + '\n%d quit\n' % until)
        args = [binary] + options(command, game, settings) + ['--script', path]
        if '-v' in sys.argv:
            print(' '.join(args))
        p = subprocess.run(args, capture_output=True, text=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    states = {}
    for l in p.stdout.split('\n'):
        w = l.split()
        if len(w) >= 4 and w[0] == 'STATE':
            states[int(w[2])] = w[3]
    same, first = 0, None
    for f in sorted(checks):
        if f not in states:
            break
        if states[f] != checks[f]:
            first = f
            break
        same += 1
    reached = max(states) if states else 0
    print('%d of %d checkpoints the same%s; the replay ran to frame %s' % (
        same, len(checks), ', the first difference at frame %d' % first if first is not None else '',
        reached if '[quit]' not in p.stderr else '%d (its end)' % until))
    err = [l for l in p.stderr.split('\n') if l.strip()]
    stop = [l for l in err if 'HALT' in l or 'Divide error' in l or '[DOS]' in l]
    if stop or p.returncode != 0:
        print('the game stopped (exit %d): %s' % (p.returncode, ' | '.join(stop or err[-3:])))
    if '-v' in sys.argv:
        print('\n'.join(err))
    for f in shots:
        print('frame %d: %s' % (f, os.path.join(out_dir, 'frame%d%s.png' % (f, '_4x' if hr else ''))))
    sys.exit(0 if first is None and same == len(checks) else 1)


if __name__ == '__main__':
    main()
