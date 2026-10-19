#!/usr/bin/env python3
"""Mad TV resource files (SOURCE*.RSC) — format as implemented by the game.

Index (resource manager, 385D:0070): every *.rsc starts with 600 entries of
6 bytes: u32 file offset, u16 size (offset 0 = absent).  Later files override
earlier ones for the same id.  Ids <= 300 belong to disk 1 (SOURCE1), the
rest to disk 2 (SOURCE2).

Sprite banks (res_load -> 37B4:04AC), order-1 context Huffman:
  u8 N; N x (u8 width, u8 height);
  u16be tree-node count; u32be number of pixels;
  bit stream, LSB first:
    escape tree, then 256 context trees (one per previous byte).
    tree := 1 leaf | 0 tree tree ; a tree's first leaf is the escape code
    (0x100), other leaves carry an 8-bit symbol (LSB first).
  decode: walk escape tree for the first pixel; afterwards walk the tree of
  the previous pixel's value; the escape code switches to the escape tree,
  whose escape code means "8 literal bits follow".
  Pixels fill each sprite row by row; in memory rows are padded to a
  multiple of 8 pixels.

Palettes (ids 1, 299): u16 count (256) + count x 3 bytes of 6-bit RGB.

Usage:
  rsc.py list   SOURCE1.RSC SOURCE2.RSC
  rsc.py hash   GAMEDIR                       the files' hash a network game compares
  rsc.py extract OUTDIR SOURCE1.RSC SOURCE2.RSC     raw .bin + decoded sprite PNGs
"""
import sys, os, struct


def read_index(paths):
    idx = {}
    for p in paths:
        d = open(p, 'rb').read()
        for i in range(600):
            off, size = struct.unpack_from('<IH', d, 6 * i)
            if off:
                idx[i] = (p, off, size)
    return idx


def raw(idx, i):
    p, off, size = idx[i]
    with open(p, 'rb') as f:
        f.seek(off)
        return f.read(size)


class Bits:
    def __init__(self, data, pos):
        self.d, self.p, self.cur, self.left = data, pos, 0, 0

    def bit(self):
        if self.left:
            self.cur >>= 1
            self.left -= 1
        else:
            self.cur = self.d[self.p]
            self.p += 1
            self.left = 7
        return self.cur & 1

    def byte(self):
        v = 0
        for _ in range(8):
            v = (self.bit() << 7) | (v >> 1)
        return v


def decode_bank(data):
    """returns [(w, h, bytes)] or raises ValueError"""
    n = data[0]
    if n == 0:
        raise ValueError('empty bank')
    dims = [(data[1 + 2 * i], data[2 + 2 * i]) for i in range(n)]
    pos = 1 + 2 * n
    nodes = (data[pos] << 8) | data[pos + 1]
    total = int.from_bytes(data[pos + 2:pos + 6], 'big')
    if total != sum(w * h for w, h in dims):
        raise ValueError('pixel count mismatch')
    b = Bits(data, pos + 6)
    left, right = {}, {}
    next_id = [0x101]

    def tree():
        first = [True]

        def rec():
            if b.bit():
                if first[0]:
                    first[0] = False
                    return 0x100
                return b.byte()
            nid = next_id[0]
            next_id[0] += 1
            if nid - 0x101 >= nodes:
                raise ValueError('tree too large')
            left[nid] = rec()
            right[nid] = rec()
            return nid
        return rec()

    sys.setrecursionlimit(10000)
    escape = tree()
    ctx = [tree() for _ in range(256)]

    def walk(si):
        while si > 0x100:
            si = right[si] if b.bit() else left[si]
        return si

    out = bytearray()
    si = walk(escape)
    v = b.byte() if si == 0x100 else si
    out.append(v)
    while len(out) < total:
        si = walk(ctx[v])
        if si == 0x100:
            si = walk(escape)
            v = b.byte() if si == 0x100 else si
        else:
            v = si
        out.append(v)
    sprites, p = [], 0
    for w, h in dims:
        sprites.append((w, h, bytes(out[p:p + w * h])))
        p += w * h
    return sprites


def palette(data):
    n = struct.unpack_from('<H', data, 0)[0]
    pal = []
    for i in range(min(n, 256)):
        r, g, bl = data[2 + 3 * i:5 + 3 * i]
        pal.append((r * 255 // 63, g * 255 // 63, bl * 255 // 63))
    return pal + [(0, 0, 0)] * (256 - len(pal))


def save_png(path, w, h, pix, pal):
    from PIL import Image
    im = Image.new('P', (w, h))
    im.putpalette([c for rgb in pal for c in rgb])
    im.putdata(list(pix))
    im.save(path)


SAVE_SLOTS = range(0xD1, 0xDC)


def find_file(d, name):
    """name in directory d, whatever its case"""
    for f in os.listdir(d):
        if f.lower() == name.lower():
            return os.path.join(d, f)
    return None


def content_hash(game_dir):
    """the game's files as a network game compares them (runtime/dos.c,
    dos_game_files_hash): FNV-1a 64 over every resource of SOURCE1.RSC and
    SOURCE2.RSC -- its id and size (16 bits each, little-endian) and its
    bytes, in id order -- but the savegame slots D1h..DBh (saving appends
    to SOURCE1.RSC); None if a file is missing"""
    paths = [find_file(game_dir, n) for n in ('SOURCE1.RSC', 'SOURCE2.RSC')]
    if None in paths:
        return None
    idx = read_index(paths)
    h = 1469598103934665603
    for i in sorted(idx):
        if i in SAVE_SLOTS:
            continue
        p, off, size = idx[i]
        with open(p, 'rb') as f:
            f.seek(off)
            data = f.read(size)
        for b in struct.pack('<HH', i, size) + data:
            h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    cmd = sys.argv[1]
    if cmd == 'hash':
        h = content_hash(sys.argv[2])
        print('%016x' % h if h is not None else 'no SOURCE1.RSC/SOURCE2.RSC in %s' % sys.argv[2])
        return
    if cmd == 'list':
        idx = read_index(sys.argv[2:])
        for i in sorted(idx):
            p, off, size = idx[i]
            d = raw(idx, i)
            kind = 'raw'
            try:
                s = decode_bank(d)
                kind = 'sprites x%d (%s)' % (len(s), ' '.join('%dx%d' % (w, h) for w, h, _ in s[:4]) + (' ...' if len(s) > 4 else ''))
            except Exception:
                if size == 770 and d[:2] == b'\x00\x01':
                    kind = 'palette'
            print('%3d %-12s off %7d size %6d  %s' % (i, os.path.basename(p), off, size, kind))
    elif cmd == 'extract':
        out = sys.argv[2]
        idx = read_index(sys.argv[3:])
        os.makedirs(out, exist_ok=True)
        pal = palette(raw(idx, 1))
        nb = ns = 0
        for i in sorted(idx):
            d = raw(idx, i)
            open(os.path.join(out, 'res%03d.bin' % i), 'wb').write(d)
            try:
                s = decode_bank(d)
            except Exception:
                continue
            nb += 1
            for k, (w, h, pix) in enumerate(s):
                if w and h:
                    save_png(os.path.join(out, 'res%03d_%02d.png' % (i, k)), w, h, pix, pal)
                    ns += 1
        print('%d resources, %d sprite banks, %d sprites -> %s' % (len(idx), nb, ns, out))


if __name__ == '__main__':
    main()
