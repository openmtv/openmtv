#!/usr/bin/env python3
"""Checks of the MMPX display filter (runtime/mmpx.c, --filter mmpx); verify.sh step 7.

On the boss's welcome (tests/scenarios/room_boss_credit.txt, frame 1850) and
a later frame, with the decompiled build on the logical clock:
1. presentation only: memory, screen and game-state hashes are the same
   with and without --filter mmpx;
2. the 4x picture has only the frame's own colours and matches the
   recorded hash, and the second picture, computed from the first by
   redoing only the rows that changed, equals a picture computed from
   scratch.
"""
import os, shutil, struct, subprocess, sys, tempfile, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
F1, F2 = 1850, 2160
# hash of the MMPX picture at F1 (FNV-1a 64 over its RGB bytes); update after an
# intended change of the filter
MMPX_F1 = '0507f6a786493a3c'


def read_png(path):
    """the RGB rows of a PNG written by runtime/host.c (8-bit RGB, filter 0)"""
    d = open(path, 'rb').read()
    pos, w, h, idat = 8, 0, 0, b''
    while pos < len(d):
        n, kind = struct.unpack('>I4s', d[pos:pos + 8])
        body = d[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h = struct.unpack('>II', body[:8])
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    row = 1 + 3 * w
    return w, h, [raw[y * row + 1:(y + 1) * row] for y in range(h)]


def fnv(rows):
    x = 1469598103934665603
    for r in rows:
        for b in r:
            x = ((x ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % x


def script(tmp, name, lines):
    src = os.path.join(ROOT, 'tests', 'scenarios', 'room_boss_credit.txt')
    out = []
    for l in open(src):
        p = l.split()
        if not p or p[0].startswith('#') or p[1] in ('hash', 'peek8', 'peek16', 'peek32', 'call', 'quit'):
            continue
        if int(p[0]) < F2:
            out.append((int(p[0]), 0, l.strip()))
    out += [(f, 1, '%d %s' % (f, cmd)) for f, cmd in lines]
    out.sort(key=lambda t: (t[0], t[1]))
    path = os.path.join(tmp, name + '.txt')
    open(path, 'w').write('\n'.join(t[2] for t in out) + '\n%d quit\n' % (max(t[0] for t in out) + 1))
    return path


def run(tmp, name, lines, opts):
    g = os.path.join(tmp, 'game_' + name)
    os.makedirs(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), g)
    shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), g)
    p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', g, '--args', 'e',
                        '--script', script(tmp, name, lines)] + opts, capture_output=True, text=True)
    if p.returncode:
        sys.exit('%s: exit %d\n%s' % (name, p.returncode, p.stderr[-2000:]))
    return [l for l in p.stdout.splitlines() if l.startswith(('HASH', 'STATE'))]


def main():
    fails = []
    tmp = tempfile.mkdtemp(prefix='mtv_mmpx_')
    try:
        t = lambda n: os.path.join(tmp, n)
        plain = run(tmp, 'plain', [(F1, 'shot ' + t('f1.png')), (F1, 'hash'), (F2, 'hash')], [])
        mm = run(tmp, 'mmpx', [(F1, 'shothd ' + t('m1.png')), (F1, 'hash'), (F2, 'shothd ' + t('m2.png')), (F2, 'hash')],
                 ['--filter', 'mmpx'])
        run(tmp, 'mmpx2', [(F2, 'shothd ' + t('m2full.png'))], ['--filter', 'mmpx'])
        # 1. presentation only
        if not (plain == mm and len(plain) == 4):
            fails.append('hashes differ with --filter mmpx:\n  %s\n  %s' % (plain, mm))
        # 2. MMPX
        _, _, f1 = read_png(t('f1.png'))
        _, _, m1 = read_png(t('m1.png'))
        colours = {bytes(r[i:i + 3]) for r in f1 for i in range(0, len(r), 3)}
        extra = {bytes(r[i:i + 3]) for r in m1 for i in range(0, len(r), 3)} - colours
        if extra:
            fails.append('MMPX made %d colours that are not in the frame' % len(extra))
        h = fnv(m1)
        if h != MMPX_F1:
            fails.append('MMPX picture at frame %d: hash %s, recorded %s' % (F1, h, MMPX_F1))
        if read_png(t('m2.png'))[2] != read_png(t('m2full.png'))[2]:
            fails.append('MMPX: the picture redone row by row differs from one computed from scratch')
        print('mmpx: hashes equal with and without (%d lines); picture %s, %d colours, row updates == from scratch'
              % (len(plain), h, len(colours)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for f in fails:
        print('FAIL', f)
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
