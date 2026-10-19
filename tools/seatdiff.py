#!/usr/bin/env python3
"""Where does a second seat change seat 0's game?  (multiplayer stage 2)

    python3 tools/seatdiff.py NAME [--seats N]     a scenario or session1

Plays the test without seats and with --seats N (default 2: a spectator),
finds the first checkpoint whose HASH/STATE line differs, dumps the memory
there in both runs and lists the differing bytes outside the stack
segment: data-segment bytes by name (src/game_globals.h etc.), others by
the far-heap block they lie in and the data-segment pointer that points
at that block.  tools/seattest.py says which tests differ."""
import os, shutil, struct, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario, mpinventory

ROOT = scenario.ROOT
DS = (0x01B0 + 0x39D4) * 16


def script_of(n):
    return os.path.join(ROOT, 'tests', 'session1.txt') if n == 'session1' else os.path.join(scenario.SCEN, n + '.txt')


def play(n, extra, dump_frame=None, tmp=None, hashes=()):
    """-> (HASH/STATE lines, memory at dump_frame or None); hashes: more checkpoints"""
    d = tempfile.mkdtemp(prefix='seatdiff_', dir=tmp)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(scenario.GAME, f), d)
    head = [l for l in open(script_of(n)).read().split('\n') if l.startswith('#')]
    body = [l for l in open(script_of(n)).read().split('\n') if l.strip() and not l.startswith('#')]
    body += ['%d hash' % f for f in hashes]
    path = None
    if dump_frame:
        path = os.path.join(d, 'dump.bin')
        body.append('%s dump %s' % (dump_frame, path))
    body.sort(key=lambda l: int(l.split()[0]))
    s = os.path.join(d, 's.txt')
    open(s, 'w').write('\n'.join(head + body) + '\n')
    args = [os.path.join(ROOT, 'build', 'mtv-dec'), '--headless', '--game', d, '--script', s] + extra
    if n != 'session1':
        args += ['--nosound', '--args', scenario.lang_of(open(script_of(n)).read().split('\n'))]
    p = subprocess.run(args, capture_output=True, text=True)
    out = [l for l in p.stdout.split('\n') if l.startswith(('HASH', 'STATE'))]
    return out, (open(path, 'rb').read() if path and os.path.exists(path) else None)


def heap_blocks(m):
    u32 = lambda a: struct.unpack_from('<I', m, a)[0]
    lin = lambda fp: ((fp >> 16) << 4) + (fp & 0xFFFF)
    base, brk = lin(u32(DS + 0x87)), lin(u32(DS + 0x8B))
    out, a = [], base
    while base <= a < brk:
        sz = u32(a) & ~1
        if sz < 8:
            break
        out.append((a + 8, a + sz, u32(a) & 1))
        a += sz
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    n = sys.argv[1]
    seats = sys.argv[sys.argv.index('--seats') + 1] if '--seats' in sys.argv else '2'
    tmp = tempfile.mkdtemp(prefix='seatdiff_')
    try:
        a, _ = play(n, [], tmp=tmp)
        b, _ = play(n, ['--seats', seats], tmp=tmp)
        k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), None)
        if k is None:
            print('%s: the same game with --seats %s (%d lines)' % (n, seats, len(a)))
            return
        frame = a[k].split()[2]
        prev = int(a[k - 1].split()[2]) if k else 0
        if int(frame) - prev > 10:                 # narrow down: a checkpoint every 10 frames
            grid = range(prev + 10, int(frame) + 1, 10)
            a2, _ = play(n, [], tmp=tmp, hashes=grid)
            b2, _ = play(n, ['--seats', seats], tmp=tmp, hashes=grid)
            k2 = next((i for i in range(min(len(a2), len(b2))) if a2[i] != b2[i]), None)
            if k2 is not None:
                a, b, k, frame = a2, b2, k2, a2[k2].split()[2]
        print('%s: first difference at frame %s:\n  %s\n  %s' % (n, frame, a[k], b[k]))
        _, ma = play(n, [], frame, tmp)
        _, mb = play(n, ['--seats', seats], frame, tmp)
        anc = mpinventory.anchors()
        starts = [x[0] for x in anc]
        blocks = heap_blocks(ma)
        ptrs = {}
        for off in range(0, 0xA730 - 3, 2):
            fp = struct.unpack_from('<I', ma, DS + off)[0]
            l = ((fp >> 16) << 4) + (fp & 0xFFFF)
            ptrs.setdefault(l, []).append(off)
        diff = [i for i in range(0x500, 0xA0000) if ma[i] != mb[i] and not (0x45F70 <= i < 0x47F70)]
        print('%d bytes differ outside the stack' % len(diff))
        grp = []
        for i in diff:
            if grp and i <= grp[-1][1] + 8:
                grp[-1][1] = i
            else:
                grp.append([i, i])
        grp.sort(key=lambda g: not (DS <= g[0] < DS + 0xA730))   # the data segment first: the cause
        for lo, hi in grp[:40]:
            if DS <= lo < DS + 0xA730:
                lab, _ = mpinventory.label_of(lo - DS, anc, starts)
                where = 'DS:%04X %s' % (lo - DS, lab or '(unnamed)')
            else:
                blk = next(((s, e, u) for s, e, u in blocks if s - 8 <= lo < e), None)
                if blk:
                    owners = ', '.join('DS:%04X' % o for o in ptrs.get(blk[0], [])) or 'no pointer in DS'
                    where = 'heap block %05X..%05X (%s, pointed at by %s) +%X' % (blk[0], blk[1], 'used' if blk[2] else 'free', owners, lo - blk[0])
                else:
                    below = max((l for l in ptrs if l <= lo and lo - l < 0x10000), default=None)
                    where = 'linear %05X (nearest pointer below: %05X from %s, +%X)' % (
                        lo, below, ', '.join('DS:%04X' % o for o in ptrs[below][:4]), lo - below) if below is not None else 'linear %05X' % lo
            print('  %05X..%05X (%d) %s  without %s  with %s' % (lo, hi, hi - lo + 1, where,
                                                          ma[lo:hi + 1][:8].hex(), mb[lo:hi + 1][:8].hex()))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    main()
