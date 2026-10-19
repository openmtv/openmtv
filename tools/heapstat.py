#!/usr/bin/env python3
"""How full is the game's far heap?  For the multiplayer plan (stage 0): can
it hold the graphics of two or three rooms at once?

    python3 tools/heapstat.py [NAME ...]     session1 and every scenario

Plays each test with build/mtv-dec, dumps the memory at every checkpoint
(the script's hash lines) and walks the Borland far heap there (block
header { u32 size | 1 used, u32 previous, u32 next free, u32 previous free },
src/rtl_heap.c) from _heapbase to the break: used and free bytes, the room
in view, and the size of the blocks the room's graphics pointers (DS:0100..
0123) and the screen buffers point at.  Prints, per test, the checkpoint
with the least memory left (free blocks + break to heap top) and the
largest room, then the overall minimum."""
import os, shutil, struct, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario

ROOT = scenario.ROOT
DS = (0x01B0 + 0x39D4) * 16
ROOM_PTRS = [('gfx%d' % i if i else 'gfx', 0x100 + 4 * i) for i in range(10)]   # 0100..0124 (slot 4 = actor texts, 9 = window buffer)
OTHER = [('screen_buf', 0x8E82), ('building_buf', 0x8EDA), ('building_bg', 0x00F4), ('figure_sprites', 0x00F8)]


def lin(fp):
    return ((fp >> 16) << 4) + (fp & 0xFFFF)


def walk(m):
    u32 = lambda a: struct.unpack_from('<I', m, a)[0]
    u16 = lambda a: struct.unpack_from('<H', m, a)[0]
    base, brk, top = lin(u32(DS + 0x87)), lin(u32(DS + 0x8B)), lin(u32(DS + 0x8F))
    used = free = n = 0
    sizes = {}
    a = base
    while base <= a < brk and n < 100000:
        sz = u32(a)
        if sz < 2 or a + (sz & ~1) > brk:
            break
        if sz & 1:
            used += sz & ~1
        else:
            free += sz
        sizes[a + 8] = sz & ~1
        a += sz & ~1
        n += 1
    blocks = {}
    for name, off in ROOM_PTRS + OTHER:
        p = lin(u32(DS + off))
        if p:
            blocks[name] = sizes.get(p, sizes.get(p - 8 + 8, 0))
    room = sum(v for k, v in blocks.items() if k.startswith('gfx'))
    return {'heap': brk - base, 'used': used, 'free_blocks': free, 'above_break': top - brk,
            'left': free + (top - brk), 'room_gfx': room, 'blocks': blocks, 'room': u16(DS + 0x838C),
            'walked': a == brk}


def run(name):
    script = os.path.join(ROOT, 'tests', 'session1.txt') if name == 'session1' else os.path.join(scenario.SCEN, name + '.txt')
    tmp = tempfile.mkdtemp(prefix='heap_')
    try:
        d = os.path.join(tmp, 'g')
        os.mkdir(d)
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(scenario.GAME, f), d)
        lines, dumps = [], []
        for l in open(script).read().split('\n'):
            lines.append(l)
            f = l.split()
            if len(f) >= 2 and f[1] == 'hash':
                dumps.append(os.path.join(tmp, f[0] + '.bin'))
                lines.append('%s dump %s' % (f[0], dumps[-1]))
        s = os.path.join(tmp, 's.txt')
        open(s, 'w').write('\n'.join(lines))
        lang = scenario.lang_of(open(script).read().split('\n'))
        subprocess.run([os.path.join(ROOT, 'build', 'mtv-dec'), '--headless', '--nosound', '--game', d,
                        '--script', s, '--args', lang], capture_output=True)
        res = []
        for p in dumps:
            if os.path.exists(p):
                r = walk(open(p, 'rb').read())
                r['frame'] = os.path.basename(p)[:-4]
                res.append(r)
        return name, res
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    names = sys.argv[1:] or ['session1'] + sorted(f[:-4] for f in os.listdir(scenario.SCEN) if f.endswith('.txt'))
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        out = list(ex.map(run, names))
    lo = big = None
    print('%-26s %8s %8s %8s %8s  %s' % ('test', 'heap', 'used', 'left', 'room', 'room in view at the minimum'))
    for name, res in out:
        if not res:
            print('%-26s no checkpoints' % name)
            continue
        m = min(res, key=lambda r: r['left'])
        g = max(res, key=lambda r: r['room_gfx'])
        print('%-26s %8d %8d %8d %8d  room %d frame %s; largest room %d: %d bytes%s' % (
            name, m['heap'], m['used'], m['left'], m['room_gfx'], m['room'], m['frame'], g['room'], g['room_gfx'],
            '' if all(r['walked'] for r in res) else '  (heap walk stopped early)'))
        lo = m if lo is None or m['left'] < lo['left'] else lo
        big = g if big is None or g['room_gfx'] > big['room_gfx'] else big
    if lo:
        print('\nleast left: %d bytes (room %d); largest room graphics: %d bytes (room %d, blocks %s)' % (
            lo['left'], lo['room'], big['room_gfx'], big['room'], big['blocks']))


if __name__ == '__main__':
    main()
