#!/usr/bin/env python3
"""The game window's notice font: runtime/notice_font.h from a PSF console font.

    python3 tools/notice_font.py [PSF] > runtime/notice_font.h

Default PSF: /usr/share/consolefonts/Lat15-Terminus12x6.psf.gz (Debian's
console-setup).  Takes the Latin-1 characters (32..126, 160..255) through the
font's Unicode table, 6 pixels wide and 12 high, one byte a row (bit 7 the
leftmost pixel).  The glyphs are a subset of a font under the SIL Open Font
License 1.1 (runtime/notice_font_OFL.txt), whose Reserved Font Name the subset
does not carry.  Standard library only."""
import gzip
import struct
import sys

PSF = '/usr/share/consolefonts/Lat15-Terminus12x6.psf.gz'


def read_psf(path):
    data = gzip.open(path).read() if path.endswith('.gz') else open(path, 'rb').read()
    if data[:4] == b'\x72\xb5\x4a\x86':                       # PSF2
        _, _, hsize, flags, n, size, h, w = struct.unpack('<8I', data[:32])
        glyphs = [data[hsize + i * size:hsize + (i + 1) * size] for i in range(n)]
        table = {}
        if flags & 1:
            pos, i = hsize + n * size, 0
            while i < n and pos < len(data):
                end = data.index(b'\xff', pos)
                for ch in data[pos:end].decode('utf-8', 'replace').split('￾')[0]:
                    table.setdefault(ord(ch), i)
                pos, i = end + 1, i + 1
        return w, h, glyphs, table
    if data[:2] == b'\x36\x04':                               # PSF1
        mode, h = data[2], data[3]
        n = 512 if mode & 1 else 256
        glyphs = [data[4 + i * h:4 + (i + 1) * h] for i in range(n)]
        table = {}
        if mode & 2:
            pos = 4 + n * h
            for i in range(n):
                while True:
                    (u,) = struct.unpack('<H', data[pos:pos + 2])
                    pos += 2
                    if u == 0xFFFF:
                        break
                    if u != 0xFFFE:
                        table.setdefault(u, i)
        return 8, h, glyphs, table
    sys.exit('not a PSF font: %s' % path)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else PSF
    w, h, glyphs, table = read_psf(path)
    if w > 8:
        sys.exit('glyphs wider than 8 pixels: %d' % w)
    rows = []
    for c in list(range(32, 127)) + list(range(160, 256)):
        g = glyphs[table.get(c, table.get(ord('?')))]
        rows.append((c, g[:h]))
    print('/* The game window\'s notices (runtime/host.c host_notice): %dx%d glyphs of the Latin-1' % (w, h))
    print(' * characters 32..126 and 160..255 (0 where a font lacks one), one byte a row, bit 7 leftmost.')
    print(' * Made by tools/notice_font.py from %s.' % path.split('/')[-1])
    print(' * The glyphs are a subset of a font by Dimitar Toshkov Zhekov, Copyright (c) 2010, under the')
    print(' * SIL Open Font License 1.1 (runtime/notice_font_OFL.txt); a modified version, it does not')
    print(' * carry that font\'s Reserved Font Name. */')
    print('#define NOTICE_FONT_W %d' % w)
    print('#define NOTICE_FONT_H %d' % h)
    print('static const unsigned char notice_font[256][%d] = {' % h)
    for c, g in rows:
        print('    [%d] = {%s},' % (c, ', '.join('0x%02X' % b for b in g)))
    print('};')


if __name__ == '__main__':
    main()
