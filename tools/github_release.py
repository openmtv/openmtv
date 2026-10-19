#!/usr/bin/env python3
"""OpenMTV's source for GitHub: a clean snapshot of HEAD, checked -- never the private history.

    python3 tools/github_release.py [--build] [--keep-going]
    python3 tools/github_release.py --publish --repo URL --author "NAME <MAIL>"

1. The snapshot: the files of HEAD (git archive: what is committed, nothing else) that
   public/MANIFEST names, without the blocks between "public:skip" markers, with
   public/files/ laid over them (NOTICE.md, LICENSE) -> build/github/VERSION/tree/
2. The gates -- any finding stops it (--keep-going: every gate runs, for the report):
   paths     no content packs, IMDb data, legacy, release tooling, game files, build
             output, keys -- whatever MANIFEST says
   markers   every "public:skip" block closed
   internal  none of public/DENY's names, addresses, users, home directories, e-mail
             addresses, passwords, tokens (exceptions: public/ALLOW; "warn:" lines only
             report)
   secrets   gitleaks (pinned, in docker) over the snapshot
   original  no run of 47 bytes or more from the original's files ($GAME: MTV.EXE,
             INTRO.EXE, SOURCE1.RSC, SOURCE2.RSC, MCV.BIN, DATAV.PCK; the unpacked EXEs
             in build/)
   binary    binary files only of the kinds allowed (fonts)
   licences  LICENSE and NOTICE.md at the top, a licence in every third_party/ directory
   refs      (warnings only) a public Markdown text naming a file the snapshot leaves out
3. --build: the snapshot built and tested in a fresh container that has nothing but the
   original's files (make setup, make, make launcher; the launcher's go test, tools/modtest.py,
   buildtest.py, playtest.py, launchertest.py --no-chrome: the test's own content packs and
   expectations, tests/packs and tests/golden)
4. The report: build/github/VERSION/REPORT.md -- the gates, the files, what changed since
   the snapshot published last (build/github/last.json)
5. --publish, once the report is read and approved: the snapshot as ONE commit "OpenMTV
   VERSION" by --author on top of the public repository (its clone: build/github/repo),
   the tag release-VERSION, the GitHub release with docs/release-notes.md's "Details"
   (gh).  --replace: the release published last made anew instead (its commit amended
   and force-pushed, its tag moved, its notes renewed) -- for a correction of the same
   version (an old commit stays reachable on GitHub by its hash until GitHub collects
   it).  Nothing leaves this machine without --publish.

Markers: a line that is only a comment "public:skip" opens a block, one that is only
"/public:skip" closes it, in any comment style (<!-- public:skip -->, # public:skip,
// public:skip); both lines and everything between are left out.  docs/github-release.md: the whole process.
Standard library only (and git, docker, gh)."""
import argparse
import datetime
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
PUB = os.path.join(ROOT, 'public')
OUT = os.path.join(ROOT, 'build', 'github')
GAME = os.environ.get('GAME') or os.path.dirname(ROOT)
GITLEAKS = 'zricethezav/gitleaks:v8.21.2'
BUILD_IMAGE = 'golang:1.25-bookworm'
ORIGINALS = ('MTV.EXE', 'INTRO.EXE', 'SOURCE1.RSC', 'SOURCE2.RSC', 'MCV.BIN', 'DATAV.PCK')
UNPACKED = ('build/MTV_U.EXE', 'build/INTRO_U.EXE')
WINDOW, STRIDE = 32, 16         # every run of WINDOW + STRIDE - 1 = 47 common bytes is found
BINARY_OK = ('.woff2',)
FORBIDDEN = (r'(^|/)mods/', r'(^|/)docs/neue-', r'(^|/)tools/deprecated/', r'\.(rsc|exe|pck|dll|so|dylib)$',
             r'(^|/)mcv\.bin$', r'^(gen|gen_intro|build|run|run_intro|public)/', r'site-assets', r'(^|/)\.env$',
             r'\.(pem|key|p12|pfx)$', r'(^|/)id_(rsa|ed25519|ecdsa)', r'(^|/)docs/release(-notes)?\.md$')
SKIP_OPEN = re.compile(r'^\s*(<!--|#|//)\s*public:skip\s*(-->)?\s*$')        # (a marker is a line of its own)
SKIP_CLOSE = re.compile(r'^\s*(<!--|#|//)\s*/public:skip\s*(-->)?\s*$')


def say(msg):
    print('github: ' + msg, flush=True)


def git(*args, cwd=ROOT):
    return subprocess.run(['git', '-C', cwd] + list(args), capture_output=True, text=True, check=True).stdout.strip()


def glob_re(pattern):
    """a MANIFEST glob -> a regular expression for a whole path"""
    out, i = '', 0
    while i < len(pattern):
        if pattern.startswith('**', i):
            out, i = out + '.*', i + 2
        elif pattern[i] == '*':
            out, i = out + '[^/]*', i + 1
        elif pattern[i] == '?':
            out, i = out + '[^/]', i + 1
        else:
            out, i = out + re.escape(pattern[i]), i + 1
    return re.compile('^' + out + '$')


def lines_of(name):
    """public/NAME's lines without comments and blanks"""
    out = []
    for line in open(os.path.join(PUB, name), encoding='utf-8'):
        line = re.sub(r'(^|\s)#.*$', '', line).strip()
        if line:
            out.append(line)
    return out


def manifest():
    return [(not line.startswith('!'), glob_re(line.lstrip('!'))) for line in lines_of('MANIFEST')]


def wanted(path, rules):
    keep = False
    for include, rx in rules:            # (the last rule that matches decides)
        if rx.match(path):
            keep = include
    return keep


def strip_markers(text):
    """TEXT without its public:skip blocks -> (text, problem or None)"""
    out, open_at = [], None
    for n, line in enumerate(text.split('\n'), 1):
        if SKIP_CLOSE.search(line):
            if open_at is None:
                return text, 'line %d closes a public:skip block that is not open' % n
            open_at = None
        elif SKIP_OPEN.search(line):
            if open_at is not None:
                return text, 'line %d opens a public:skip block inside the one of line %d' % (n, open_at)
            open_at = n
        elif open_at is None:
            out.append(line)
    if open_at is not None:
        return text, 'the public:skip block of line %d is never closed' % open_at
    return '\n'.join(out), None


def is_text(data):
    if b'\0' in data:
        return False
    try:
        data.decode('utf-8')
        return True
    except UnicodeDecodeError:
        return False


def snapshot(base):
    """HEAD's public files into BASE/tree -> (their paths, the marker problems)"""
    tree, everything = os.path.join(base, 'tree'), os.path.join(base, 'head')
    for d in (tree, everything):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    subprocess.run('git -C %s archive HEAD | tar -x -C %s' % (shlex.quote(ROOT), shlex.quote(everything)),
                   shell=True, check=True)
    rules, paths, problems = manifest(), [], []
    for p in git('ls-tree', '-r', '-z', '--name-only', 'HEAD').split('\0'):
        if not p or not wanted(p, rules) or not os.path.isfile(os.path.join(everything, p)):
            continue
        src, dst = os.path.join(everything, p), os.path.join(tree, p)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        data = open(src, 'rb').read()
        if is_text(data) and b'public:skip' in data:
            text, problem = strip_markers(data.decode('utf-8'))
            if problem:
                problems.append('%s: %s' % (p, problem))
            else:
                open(dst, 'w', encoding='utf-8', newline='').write(text)
        paths.append(p)
    over = os.path.join(PUB, 'files')
    for dp, _, fns in os.walk(over):
        for fn in fns:
            rel = os.path.relpath(os.path.join(dp, fn), over).replace(os.sep, '/')
            os.makedirs(os.path.dirname(os.path.join(tree, rel)), exist_ok=True)
            shutil.copy2(os.path.join(dp, fn), os.path.join(tree, rel))
            if rel not in paths:
                paths.append(rel)
    shutil.rmtree(everything)
    return sorted(paths), problems


def read(tree, p):
    return open(os.path.join(tree, p), 'rb').read()


# ---------------------------------------------------------------- the gates: (findings, warnings)

def gate_paths(tree, paths):
    return ['%s: not for the public (%s)' % (p, rx) for p in paths for rx in FORBIDDEN if re.search(rx, p, re.I)], []


def deny_rules():
    out = []
    for line in lines_of('DENY'):
        name, rx = line.split(None, 1)
        warn = name.startswith('warn:')
        out.append((name[5:] if warn else name, warn, re.compile(rx, re.I)))
    return out


def gate_internal(tree, paths):
    deny = deny_rules()
    allow = [(glob_re(g), n) for g, n in (line.split() for line in lines_of('ALLOW'))]
    found, warned = [], []
    texts = [(p, read(tree, p)) for p in paths]
    texts.append(('(the GitHub release notes: docs/release-notes.md, "Details")', details_notes().encode('utf-8')))
    for p, data in texts:
        if not is_text(data):
            continue
        for n, line in enumerate(data.decode('utf-8').split('\n'), 1):
            for name, warn, rx in deny:
                m = rx.search(line)
                if m and not any(g.match(p) and a == name for g, a in allow):
                    (warned if warn else found).append('%s:%d: %s: %s' % (p, n, name, line.strip()[:140]))
    return found, warned


def gate_secrets(tree, paths):
    report = os.path.join(os.path.dirname(tree), 'gitleaks.json')
    if os.path.exists(report):
        os.remove(report)
    r = subprocess.run(['docker', 'run', '--rm', '-u', '%d:%d' % (os.getuid(), os.getgid()), '-v', tree + ':/repo:ro', '-v', os.path.dirname(tree) + ':/out', GITLEAKS,
                        'detect', '--source', '/repo', '--no-git', '--no-banner', '--report-format', 'json',
                        '--report-path', '/out/gitleaks.json', '--exit-code', '0'], capture_output=True, text=True)
    if r.returncode or not os.path.exists(report):
        return ['gitleaks did not run (%s): %s' % (r.returncode, (r.stderr or r.stdout).strip()[-300:])], []
    return ['%s:%s: %s (%s)' % (x.get('File', '?').removeprefix('/repo/'), x.get('StartLine', '?'), x.get('RuleID', '?'),
                                (x.get('Match') or '')[:60]) for x in json.load(open(report))], []


def gate_original(tree, paths):
    files = [os.path.join(GAME, f) for f in ORIGINALS] + [os.path.join(ROOT, f) for f in UNPACKED]
    missing = [f for f in files if not os.path.isfile(f)]
    if missing:
        return ['the original\'s files are needed for this gate, missing: %s (set GAME)' % ', '.join(missing)], []
    index = {}
    for f in files:
        data = open(f, 'rb').read()
        for i in range(0, len(data) - WINDOW + 1, STRIDE):
            w = data[i:i + WINDOW]
            if len(set(w)) >= 10:            # (no padding, no runs of one value, no plain tables)
                index.setdefault(w, '%s+0x%X' % (os.path.basename(f), i))
    found = []
    for p in paths:
        data, i = read(tree, p), 0
        while i <= len(data) - WINDOW:
            hit = index.get(data[i:i + WINDOW])
            if hit:
                line = data.count(b'\n', 0, i) + 1
                found.append('%s:%d: bytes of %s: %s' % (p, line, hit, data[i:i + WINDOW].decode('utf-8', 'replace')))
                i = data.find(b'\n', i) + 1 or len(data)      # (one per line)
            else:
                i += 1
    return found, []


def gate_refs(tree, paths):
    """only warnings: a public Markdown text that names a file of the repository the
    snapshot leaves out (a working document that stays private)"""
    have, warned = set(paths), []
    rx = re.compile(r'(?<![\w./-])((?:docs|src|names|tests|tools|launcher|runtime)/[\w./-]+\.md)\b')
    for p in paths:
        if not p.endswith('.md'):
            continue
        for n, line in enumerate(read(tree, p).decode('utf-8', 'replace').split('\n'), 1):
            for ref in rx.findall(line):
                if ref not in have:
                    warned.append('%s:%d: names %s, which is not public' % (p, n, ref))
    return [], warned


def gate_binary(tree, paths):
    return ['%s: a binary file of a kind not allowed (%s)' % (p, ', '.join(BINARY_OK)) for p in paths
            if not is_text(read(tree, p)) and not p.endswith(BINARY_OK)], []


def gate_licences(tree, paths):
    found = []
    if not any(p in paths for p in ('LICENSE', 'LICENSE.md', 'COPYING')):
        found.append('no LICENSE at the top (public/files/LICENSE: the licence is still to be chosen, docs/github-release.md)')
    if 'NOTICE.md' not in paths:
        found.append('no NOTICE.md at the top (public/files/NOTICE.md)')
    dirs = sorted({p.split('/')[1] for p in paths if p.startswith('third_party/') and p.count('/') >= 2})
    for d in dirs:
        if not any(re.match(r'third_party/%s/(LICEN[CS]E|COPYING)' % re.escape(d), p, re.I) for p in paths):
            found.append('third_party/%s: no licence file' % d)
    return found, []


GATES = (('paths', gate_paths), ('internal', gate_internal), ('secrets', gate_secrets), ('original', gate_original),
         ('binary', gate_binary), ('licences', gate_licences), ('refs', gate_refs))


def build_check(tree, base):
    """the snapshot in a fresh container with only the original's files: setup, build, tests"""
    game = os.path.join(base, 'game')
    shutil.rmtree(game, ignore_errors=True)
    os.makedirs(game)
    for f in ORIGINALS:
        shutil.copy2(os.path.join(GAME, f), game)
    steps = ('apt-get update -qq && apt-get install -y -qq --no-install-recommends libsdl2-dev python3 python3-capstone '
             '>/dev/null && cp -r /src /work && cd /work && make setup GAME=/game >/tmp/setup.log 2>&1 '
             '|| { tail -30 /tmp/setup.log; exit 1; }; make -j"$(nproc)" >/tmp/make.log 2>&1 || { tail -30 /tmp/make.log; '
             'exit 1; }; (cd launcher && go test ./... >/tmp/go.log 2>&1) || { tail -30 /tmp/go.log; exit 1; }; '
             'tail -3 /tmp/go.log; make launcher >/dev/null 2>&1; for t in modtest.py buildtest.py playtest.py "launchertest.py --no-chrome"; do '
             'python3 tools/$t >/tmp/t.log 2>&1 || { tail -30 /tmp/t.log; exit 1; }; tail -1 /tmp/t.log; done')
    r = subprocess.run(['docker', 'run', '--rm', '-e', 'GAME=/game', '-v', tree + ':/src:ro', '-v', game + ':/game:ro', BUILD_IMAGE,
                        'sh', '-c', steps], capture_output=True, text=True)
    shutil.rmtree(game, ignore_errors=True)
    out = (r.stdout + r.stderr).strip()
    return ([] if r.returncode == 0 else ['the build in a fresh container failed (%d)' % r.returncode]), out


# ---------------------------------------------------------------- the report

def file_hashes(tree, paths):
    return {p: hashlib.sha256(read(tree, p)).hexdigest() for p in paths}


def report(base, v12, results, paths, hashes, build_out):
    last = {}
    if os.path.exists(os.path.join(OUT, 'last.json')):
        last = json.load(open(os.path.join(OUT, 'last.json')))
    before = last.get('files', {})
    added = sorted(set(hashes) - set(before))
    removed = sorted(set(before) - set(hashes))
    changed = sorted(p for p in hashes if p in before and before[p] != hashes[p])
    size = sum(len(read(os.path.join(base, 'tree'), p)) for p in paths)
    lines = ['# OpenMTV %s -- the public snapshot' % v12[:7], '',
             '%s, HEAD %s (%s)' % (datetime.date.today().isoformat(), v12, git('log', '-1', '--format=%s')), '',
             '## Gates', '', '| gate | findings | warnings |', '|---|---|---|']
    for name, (found, warned) in results.items():
        lines.append('| %s | %s | %s |' % (name, len(found) or 'ok', len(warned) or ''))
    for title, k in (('Findings', 0), ('Warnings (no stop -- look at them)', 1)):
        items = [(name, x) for name, r in results.items() for x in r[k]]
        if items:
            lines += ['', '## ' + title, ''] + ['- %s: `%s`' % (name, x.replace('`', "'")) for name, x in items]
    if build_out is not None:
        lines += ['', '## The build in a fresh container', '', '```', build_out[-3000:], '```']
    lines += ['', '## Files', '', '%d files, %.1f MB; since %s: %d new, %d changed, %d gone'
              % (len(paths), size / 1e6, last.get('version', 'nothing published'), len(added), len(changed), len(removed))]
    for title, items in (('new', added), ('changed', changed), ('gone', removed)):
        if items and last:
            lines += ['', '### ' + title, ''] + ['- ' + p for p in items[:200]] + (['- ...'] if len(items) > 200 else [])
    path = os.path.join(base, 'REPORT.md')
    open(path, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    return path


def details_notes():
    """docs/release-notes.md's part "Details" (or, without it, the short one) for the GitHub release"""
    text = open(os.path.join(ROOT, 'docs', 'release-notes.md'), encoding='utf-8').read()
    parts = re.split(r'(?m)^## ', text)
    for want in ('Details', 'Für die Seite'):
        for part in parts:
            if part.startswith(want) and re.search(r'(?m)^- (neu|fehler):', part):
                return '\n'.join(line for line in part.split('\n')[1:] if line.startswith('- ')) + '\n'
    return ''


def publish(a, base, v12, hashes):
    if not a.repo or not a.author or not re.match(r'^[^<>]+ <[^<>@\s]+@[^<>\s]+>$', a.author):
        sys.exit('github: --publish needs --repo URL and --author "NAME <MAIL>" (a neutral identity, docs/github-release.md)')
    name, mail = re.match(r'^(.+?) <(.+)>$', a.author).groups()
    repo = os.path.join(OUT, 'repo')
    if not os.path.isdir(os.path.join(repo, '.git')):
        subprocess.run(['git', 'clone', '-q', a.repo, repo], check=True)
    if subprocess.run(['git', '-C', repo, 'rev-parse', '-q', '--verify', 'HEAD'], capture_output=True).returncode:
        git('symbolic-ref', 'HEAD', 'refs/heads/main', cwd=repo)       # (a new, empty repository: its first branch is main)
    for f in os.listdir(repo):
        if f != '.git':
            p = os.path.join(repo, f)
            shutil.rmtree(p) if os.path.isdir(p) and not os.path.islink(p) else os.remove(p)
    shutil.copytree(os.path.join(base, 'tree'), repo, dirs_exist_ok=True)
    git('add', '-A', cwd=repo)
    if not git('status', '--porcelain', cwd=repo):
        sys.exit('github: the public repository has this snapshot already')
    env = dict(os.environ, GIT_AUTHOR_NAME=name, GIT_AUTHOR_EMAIL=mail, GIT_COMMITTER_NAME=name, GIT_COMMITTER_EMAIL=mail)
    owner = re.sub(r'(\.git)?/*$', '', re.sub(r'^.*github\.com[:/]', '', a.repo))
    notes = os.path.join(base, 'notes.md')
    open(notes, 'w', encoding='utf-8').write(details_notes() or 'OpenMTV %s\n' % v12[:7])
    if a.replace:       # (the release published last, made anew: its commit, tag and notes -- no commit more)
        last = json.load(open(os.path.join(OUT, 'last.json')))
        v_pub = last['version']
        tag = 'release-' + v_pub
        subprocess.run(['git', '-C', repo, '-c', 'commit.gpgsign=false', 'commit', '-q', '--amend', '-m',
                        'OpenMTV %s' % v_pub[:7]], env=env, check=True)
        subprocess.run(['git', '-C', repo, 'tag', '-f', '-a', tag, '-m', 'OpenMTV %s' % v_pub[:7]], env=env, check=True)
        subprocess.run(['git', '-C', repo, 'push', '-q', '--force-with-lease', 'origin', 'HEAD:refs/heads/main'], check=True)
        subprocess.run(['git', '-C', repo, 'push', '-q', '--force', 'origin', 'refs/tags/' + tag], check=True)
        subprocess.run(['gh', 'release', 'edit', tag, '--repo', owner, '--notes-file', notes], check=True)
        json.dump({'version': v_pub, 'from': v12, 'date': datetime.date.today().isoformat(), 'files': hashes},
                  open(os.path.join(OUT, 'last.json'), 'w'), indent=1)
        say('replaced the release %s on %s with the snapshot of %s (tag %s moved)' % (v_pub, a.repo, v12, tag))
        return
    tag = 'release-' + v12
    subprocess.run(['git', '-C', repo, '-c', 'commit.gpgsign=false', 'commit', '-q', '-m', 'OpenMTV %s' % v12[:7]],
                   env=env, check=True)
    subprocess.run(['git', '-C', repo, 'tag', '-a', tag, '-m', 'OpenMTV %s' % v12[:7]], env=env, check=True)
    subprocess.run(['git', '-C', repo, 'push', '-q', 'origin', 'HEAD:refs/heads/main', tag], check=True)
    subprocess.run(['gh', 'release', 'create', tag, '--repo', owner, '--title', 'OpenMTV %s' % v12[:7],
                    '--notes-file', notes], check=True)
    json.dump({'version': v12, 'date': datetime.date.today().isoformat(), 'files': hashes},
              open(os.path.join(OUT, 'last.json'), 'w'), indent=1)
    say('published %s to %s (tag %s)' % (v12, a.repo, tag))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--build', action='store_true', help='also build and test the snapshot in a fresh container')
    ap.add_argument('--keep-going', action='store_true', help='run every gate even after a finding (for the report)')
    ap.add_argument('--publish', action='store_true', help='push the approved snapshot to the public repository')
    ap.add_argument('--repo', help='the public repository (e.g. https://github.com/OWNER/openmtv.git)')
    ap.add_argument('--author', help='"NAME <MAIL>" for the public commit')
    ap.add_argument('--replace', action='store_true', help='with --publish: make the release published last anew '
                    '(its commit amended, its tag moved) instead of adding a commit')
    a = ap.parse_args()
    v12 = git('rev-parse', '--short=12', 'HEAD')
    if git('status', '--porcelain', '--untracked-files=no'):
        say('note: the working tree has changes -- they are NOT in the snapshot (it is HEAD)')
    base = os.path.join(OUT, v12)
    os.makedirs(base, exist_ok=True)
    paths, problems = snapshot(base)
    tree = os.path.join(base, 'tree')
    say('%s: %d files in %s' % (v12, len(paths), os.path.relpath(tree, ROOT)))
    results, stop = {'markers': (problems, [])}, bool(problems)
    say('%-9s %s' % ('markers', '%d found' % len(problems) if problems else 'ok'))
    for name, gate in GATES:
        if stop and not a.keep_going:
            results[name] = (['(not run: an earlier gate found something)'], [])
            continue
        results[name] = gate(tree, paths)
        found, warned = results[name]
        say('%-9s %s%s' % (name, '%d found' % len(found) if found else 'ok', ', %d warnings' % len(warned) if warned else ''))
        stop = stop or bool(found)
    build_out = None
    if a.build and not stop:
        found, build_out = build_check(tree, base)
        results['build'] = (found, [])
        say('build     %s' % ('failed' if found else 'ok'))
        stop = stop or bool(found)
    hashes = file_hashes(tree, paths)
    say('the report: %s' % os.path.relpath(report(base, v12, results, paths, hashes, build_out), ROOT))
    if stop:
        sys.exit('github: not publishable -- see the report')
    if a.publish:
        if not a.build:
            sys.exit('github: --publish only together with --build (the snapshot must build on its own)')
        publish(a, base, v12, hashes)
    else:
        say('publishable -- read the report and the tree, then --publish --build --repo URL --author "NAME <MAIL>"')


if __name__ == '__main__':
    main()
