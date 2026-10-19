#!/usr/bin/env python3
"""The players' start in Go (build/openmtv, launcher/): the same arguments must give the
same output, messages, exit code, files and game as when tests/golden/playtest.json was
recorded (tools/golden.py; recorded 2026-10-19, after the Python start it replaced,
the former Python launcher (play.py), had given the same -- the developers' ways too).

    python3 tools/playtest.py [--exe PATH] [--stub PATH] [--no-mod] [-v] [--record]

Every case runs the Go program in a fresh copy of a small decomp directory under a
temporary directory (it finds its decomp directory from where it lies), with
stand-ins for the game in build/ (mtv-dec, mtv, mtv-intro: shell scripts that print
their directory and arguments and end with 3, so the start itself is compared too;
on Windows copies of build/playstub.exe, launcher/stub -- without it empty files,
and the cases that start a program are left out), a copy of the original game files
($GAME, default: one directory above decomp/) and a settings directory of its own
(XDG_CONFIG_HOME, on Windows APPDATA).  Compared: stdout and stderr (the copy's path
made ROOT), the exit code, and every file the run leaves (path, contents, and on
POSIX the executable bit).  The --mod cases build real packs from the test's
content packs (tests/packs; --no-mod leaves them out).  The developers' ways
(--lifted, --record, intro) get make setup's unpacked EXEs (build/MTV_U.EXE,
build/INTRO_U.EXE).  The Go program's own words -- its help, its errors -- are
launcher/cli_test.go's too (launcher/testdata/).  Recorded on Linux: other systems'
stand-ins print otherwise.

--exe: the Go program (default build/openmtv, on Windows build/openmtv.exe);
--stub: the stand-in on Windows (default build/playstub.exe: make playtest-win).
Standard library only (Python 3.8+)."""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import golden  # noqa: E402
ROOT = os.path.abspath(os.path.join(HERE, '..'))
WIN = os.name == 'nt'
EXE = '.exe' if WIN else ''
GAME_FILES = ('SOURCE1.RSC', 'SOURCE2.RSC', 'MTV.EXE', 'INTRO.EXE', 'MCV.BIN', 'DATAV.PCK')
STUBS = ('mtv-dec', 'mtv', 'mtv-intro')
STUB = '#!/bin/sh\necho "%s in $(pwd): $*"\nexit 3\n'

SETTINGS = {
    'none': None,
    'de': '[game]\nlanguage = german\n',
    'de-crlf': '# the player\'s\r\n[display]\r\nscale = 4\r\n[game]\r\nlanguage = english\r\n  language=german  \r\n',
    'other-section': '[display]\nlanguage = german\n[game]\n',
    'en-last': '[game]\nlanguage = german\n[game]\nlanguage = english\n',
}

# (name, [run, ...], options): a run is the argument list; options: start (a program is
# started: not on Windows), game ('full', 'no-source2', 'no-mtv', 'unset'), stubs
# (the stand-ins there), settings (SETTINGS), env (more variables), mod (copies tools/
# and mods/: a real build)
P = '--print'
CASES = [
    ('plain', [[P]], {}),
    ('twice', [[P], [P, 'd']], {}),
    ('direct', [[P, '--direct']], {}),
    ('languages', [[P, 'd'], [P, 'e', 'd']], {}),
    ('classic', [[P, 'e', '--classic']], {}),
    ('fixes', [[P, '--fixes=title-cache,case-corner'], [P, '--fixes'], [P, '--classic', '--fixes=a']], {}),
    ('display', [[P, '--scale', '4', '--aspect', 'square', '--filter', 'mmpx', '--fullscreen', '--integer',
                  '--mouse', 'system'], [P, '--scale=2', '--aspect=4:3']], {}),
    ('options', [[P, '--fourth-speed', '--fast-rooms=no', '--fourth-speed=yes', '--start-money=500000',
                  '--credit-line', '900000', '--terror-damage', '0', '--sammy-prize=1,2,3,4',
                  '--ai-improve', 'strong', '--ad-offers', '14', '--film-offers=16']], {}),
    ('network', [[P, '--mp', '127.0.0.1:7641', '--seat', '1', '--humans', '2', '--difficulty', '1',
                  '--names', 'Ann,Bo', '--seed', '5', '--new-game', '--mp-save=s', '--mp-load', 'x/y']], {}),
    ('settings', [[P, '--settings=/x/other.ini', 'e']], {}),
    ('game-files', [[P, '--game-files', 'build/mp/abc'], [P, '--game-files', '/elsewhere/g/', 'd']], {}),
    ('mod-path', [[P, '--game-files', 'x', '--mod', 'test-filme']], {}),
    ('no-source2', [[P], [P, '--game-files', 'x']], {'game': 'no-source2'}),
    ('game-unset', [[P]], {'game': 'unset'}),
    ('start', [['d', '--scale', '2']], {'start': True}),
    ('start-direct', [['--direct']], {'start': True}),
    ('start-classic', [['--classic', 'e']], {'start': True}),
    ('start-error', [['d']], {'start': True, 'game': 'no-source2'}),
    ('mod', [[P, '--mod', 'test-filme'], [P, '--mod=test-filme+test-serien', 'd']], {'mod': True}),
    ('mod-unknown', [[P, '--mod', 'gibtsnicht'], [P, '--mod', 'test-filme+test-filme'], [P, '--mod=bad name']], {'mod': True}),
    ('mod-start', [['--mod', 'test-werbung', 'e']], {'mod': True, 'start': True}),
    # the developers' ways: the machine-level build, a recording, the intro first
    ('lifted', [[P, '--lifted'], [P, '--lifted', 'd', '--scale', '2', '--classic']], {'unpacked': True}),
    ('lifted-missing', [[P, '--lifted']], {'stubs': ('mtv-dec',)}),
    ('record', [[P, '--record', 'session.txt', 'e']], {'unpacked': True}),
    ('record-mod', [[P, '--record', 'x.txt', '--mod', 'test-filme']], {'unpacked': True, 'mod': True}),
    ('intro-print', [[P, 'intro', 'd']], {'unpacked': True}),
    ('intro-start', [['intro', 'e']], {'unpacked': True, 'start': True}),
    ('lifted-start', [['--lifted', 'd']], {'unpacked': True, 'start': True}),
]


def original_dir():
    return os.path.abspath(os.path.join(ROOT, os.environ.get('GAME', '..')))


def make_game(base, kind):
    d = os.path.join(base, 'game-' + kind)
    if not os.path.isdir(d):
        os.makedirs(d)
        for f in GAME_FILES:
            if (kind, f) in (('no-source2', 'SOURCE2.RSC'), ('no-mtv', 'MTV.EXE')):
                continue
            shutil.copy(os.path.join(original_dir(), f), d)
    return d


def make_root(d, stubs, mod, stub_exe, unpacked=False):
    os.makedirs(os.path.join(d, 'build'))
    if unpacked:                            # (make setup's: the developers' ways need them)
        for f in ('MTV_U.EXE', 'INTRO_U.EXE'):
            shutil.copy(os.path.join(ROOT, 'build', f), os.path.join(d, 'build', f))
    if mod:                                 # (the Go program builds the packs itself: no tools/ there)
        shutil.copytree(os.path.join(ROOT, 'tests', 'packs'), os.path.join(d, 'mods'))      # (the test's content packs)
    for s in stubs:
        p = os.path.join(d, 'build', s + EXE)
        if stub_exe:
            shutil.copy(stub_exe, p)
            continue
        with open(p, 'w') as f:
            f.write('' if WIN else STUB % s)
        os.chmod(p, 0o755)


def tree(d, skip):
    """{relative path: (sha1, executable)} of the files below D (SKIP: top-level names)"""
    out = {}
    for top, dirs, files in os.walk(d):
        rel = os.path.relpath(top, d)
        if rel == '.':
            dirs[:] = [x for x in dirs if x not in skip]
        dirs[:] = [x for x in dirs if x != '__pycache__']
        for x in dirs:
            out[os.path.normpath(os.path.join(rel, x)).replace(os.sep, '/') + '/'] = None
        for x in files:
            p = os.path.join(top, x)
            r = os.path.normpath(os.path.join(rel, x))
            if r in skip or x == 'MTV_U.EXE':      # (play.py's and the editor's, for the machine-level build)
                continue
            with open(p, 'rb') as f:
                h = hashlib.sha1(f.read()).hexdigest()
            out[r.replace(os.sep, '/')] = (h, (not WIN) and bool(os.stat(p).st_mode & 0o111))
    return out


def norm(text, root, name='ROOT'):
    text = text.replace(root, name)
    if WIN:
        text = text.replace(root.replace('\\', '/'), name).replace('\r\n', '\n').replace('\\', '/')
    return text


def run_case(base, name, runs, opt, go_exe, stub_exe, verbose, gold):
    case = os.path.join(base, name)
    os.makedirs(case)
    stubs = opt.get('stubs', STUBS)
    d = os.path.join(case, 'go')
    make_root(d, stubs, opt.get('mod'), stub_exe, opt.get('unpacked'))
    shutil.copy(go_exe, os.path.join(d, 'build', 'openmtv' + EXE))
    env = {k: v for k, v in os.environ.items() if k not in ('GAME', 'MADTV_CMD', 'XDG_CONFIG_HOME', 'APPDATA', 'MADTV_MODS')}
    env.update(PYTHONIOENCODING='utf-8', PYTHONDONTWRITEBYTECODE='1')
    game = opt.get('game', 'full')
    if game != 'unset':
        env['GAME'] = make_game(base, game)
    cfg = os.path.join(case, 'cfg')
    os.makedirs(os.path.join(cfg, 'madtv'))
    text = SETTINGS[opt.get('settings', 'none')]
    if text is not None:
        with open(os.path.join(cfg, 'madtv', 'settings.ini'), 'w', newline='') as f:
            f.write(text)
    env['APPDATA' if WIN else 'XDG_CONFIG_HOME'] = cfg
    env.update(opt.get('env', {}))
    bad = []
    for k, args in enumerate(runs):
        r = subprocess.run([os.path.join(d, 'build', 'openmtv' + EXE)] + args, cwd=case, env=env, capture_output=True, timeout=300)
        got = [r.returncode] + [norm(norm(x.decode('utf-8', 'replace'), d), base, 'BASE') for x in (r.stdout, r.stderr)]
        #     (the copy: ROOT; the game files' directories: below BASE)
        ok, detail = gold.check('%s, run %d: %s' % (name, k + 1, ' '.join(args)), got)
        if not ok:
            bad.append('  %s:\n%s' % (' '.join(repr(a) for a in args), detail))
        elif verbose:
            print('  %r -> %d\n%s' % (args, got[0], ''.join('    | ' + l + '\n' for l in (got[1] + got[2]).splitlines()[:6])))
    skip = ('mods', 'run_intro', os.path.join('build', 'openmtv' + EXE),
            os.path.join('build', 'MTV_U.EXE'), os.path.join('run', 'MTV_U.EXE'), os.path.join('build', 'INTRO_U.EXE'))
    ok, detail = gold.check('%s: the files' % name, tree(d, skip))
    if not ok:
        bad.append('  the files differ:\n' + detail)
    return bad


def main(argv):
    go_exe = os.path.join(ROOT, 'build', 'openmtv' + EXE)
    if '--exe' in argv:
        go_exe = os.path.abspath(argv[argv.index('--exe') + 1])
    stub_exe = None
    if WIN:
        stub_exe = os.path.abspath(argv[argv.index('--stub') + 1]) if '--stub' in argv \
            else os.path.join(ROOT, 'build', 'playstub.exe')
        if not os.path.exists(stub_exe):
            print('playtest: no %s (make playtest-win): the starts are left out' % stub_exe)
            stub_exe = None
    verbose = '-v' in argv
    gold = golden.Golden('playtest', '--record' in argv)
    if not os.path.exists(go_exe):
        sys.exit('playtest: %s is not built (make launcher)' % go_exe)
    for f in GAME_FILES:
        if not os.path.exists(os.path.join(original_dir(), f)):
            sys.exit('playtest: %s not found in %s (set GAME)' % (f, original_dir()))
    base = os.path.realpath(tempfile.mkdtemp(prefix='playtest-'))
    failed = 0
    try:
        for name, runs, opt in CASES:
            if (opt.get('start') and WIN and not stub_exe) or (opt.get('mod') and '--no-mod' in argv):
                print('%-20s skipped' % name)
                continue
            bad = run_case(base, name, runs, opt, go_exe, stub_exe, verbose, gold)
            print('%-20s %s' % (name, 'ok' if not bad else 'DIFFERENT'))
            for b in bad:
                print(b)
            failed += bool(bad)
    finally:
        shutil.rmtree(base, ignore_errors=True)
    gold.save()
    print('playtest: %d of %d cases different' % (failed, len(CASES)) if failed else 'playtest: all the same')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
