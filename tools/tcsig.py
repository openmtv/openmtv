#!/usr/bin/env python3
"""Identify Turbo C runtime-library code in the MTV image.

Parses an OMF library (CL.LIB) / object (C0L.OBJ): for every module, the code
segment bytes with relocated (fixup) bytes masked, plus the public symbols.
Each module is then located in the unpacked EXE image by masked comparison.

Usage: tcsig.py CL.LIB [C0L.OBJ ...] -- MTV_U.EXE  -> JSON {lin: name}
"""
import sys, os, struct, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mzimage import MZ


def read_index(b, p):
    v = b[p]
    if v & 0x80:
        return ((v & 0x7F) << 8) | b[p + 1], p + 2
    return v, p + 1


class Module:
    def __init__(self):
        self.name = None
        self.lnames = [None]
        self.segs = [None]        # (name, class, length)
        self.data = {}            # seg idx -> bytearray
        self.mask = {}            # seg idx -> bytearray (1 = fixed byte)
        self.pubs = []            # (name, seg idx, offset)
        self.extdefs = [None]


def parse_records(blob, start=0, end=None):
    """Yield modules from an OMF stream (object file or library)."""
    end = len(blob) if end is None else end
    p = start
    mod = None
    last_led = None
    page = 0                                  # set by a library header record
    while p + 3 <= end:
        typ = blob[p]
        ln = struct.unpack_from('<H', blob, p + 1)[0]
        body = blob[p + 3:p + 3 + ln - 1]
        nxt = p + 3 + ln
        if typ == 0xF0:                       # library header
            page = ln + 3
            p = nxt
            continue
        if typ == 0xF1:                       # library end / dictionary
            break
        if typ == 0x80:                       # THEADR
            mod = Module()
            mod.name = body[1:1 + body[0]].decode('latin1')
        elif mod is None:
            p = nxt
            continue
        elif typ == 0x96:                     # LNAMES
            q = 0
            while q < len(body):
                n = body[q]; mod.lnames.append(body[q + 1:q + 1 + n].decode('latin1')); q += 1 + n
        elif typ in (0x98, 0x99):             # SEGDEF
            q = 0
            attr = body[q]; q += 1
            if (attr >> 5) == 0:
                q += 3
            if typ == 0x98:
                length = struct.unpack_from('<H', body, q)[0]; q += 2
            else:
                length = struct.unpack_from('<I', body, q)[0]; q += 4
            ni, q = read_index(body, q)
            ci, q = read_index(body, q)
            mod.segs.append((mod.lnames[ni], mod.lnames[ci], length))
            si = len(mod.segs) - 1
            mod.data[si] = bytearray(length)
            mod.mask[si] = bytearray(length)
        elif typ in (0x90, 0x91):             # PUBDEF
            q = 0
            gi, q = read_index(body, q)
            si, q = read_index(body, q)
            if si == 0:
                q += 2
            while q < len(body):
                n = body[q]; name = body[q + 1:q + 1 + n].decode('latin1'); q += 1 + n
                off = struct.unpack_from('<H' if typ == 0x90 else '<I', body, q)[0]
                q += 2 if typ == 0x90 else 4
                ti, q = read_index(body, q)
                mod.pubs.append((name, si, off))
        elif typ in (0xA0, 0xA1):             # LEDATA
            q = 0
            si, q = read_index(body, q)
            off = struct.unpack_from('<H' if typ == 0xA0 else '<I', body, q)[0]
            q += 2 if typ == 0xA0 else 4
            data = body[q:]
            seg = mod.data[si]
            if off + len(data) > len(seg):
                seg.extend(b'\0' * (off + len(data) - len(seg)))
                mod.mask[si].extend(b'\0' * (off + len(data) - len(mod.mask[si])))
            seg[off:off + len(data)] = data
            for k in range(off, off + len(data)):
                mod.mask[si][k] = 1
            last_led = (si, off)
        elif typ in (0xA2, 0xA3):             # LIDATA (rare in code); ignore
            last_led = None
        elif typ in (0x9C, 0x9D):             # FIXUPP
            q = 0
            while q < len(body):
                b0 = body[q]
                if b0 & 0x80 == 0:            # THREAD
                    method = (b0 >> 2) & 7
                    q += 1
                    if (b0 & 0x40) == 0 and method < 4 or (b0 & 0x40) and method < 3:
                        _, q = read_index(body, q)
                    continue
                locat = (b0 << 8) | body[q + 1]
                q += 2
                loc = (locat >> 10) & 0xF
                drec = locat & 0x3FF
                fixdat = body[q]; q += 1
                if not (fixdat & 0x80):       # frame not by thread
                    fm = (fixdat >> 4) & 7
                    if fm < 3:
                        _, q = read_index(body, q)
                if not (fixdat & 0x08):       # target not by thread
                    _, q = read_index(body, q)
                if not (fixdat & 0x04):       # displacement present
                    q += 2
                size = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2, 9: 2, 11: 4, 13: 2}.get(loc, 2)
                if last_led:
                    si, off = last_led
                    for k in range(off + drec, off + drec + size):
                        if k < len(mod.mask[si]):
                            mod.mask[si][k] = 0
        elif typ in (0x8A, 0x8B):             # MODEND
            yield mod
            mod = None
            last_led = None
            if page:                          # modules are page aligned in a LIB
                p = ((nxt + page - 1) // page) * page
                continue
        p = nxt


def code_segments(mod):
    for si in range(1, len(mod.segs)):
        name, cls, length = mod.segs[si]
        if cls and cls.upper() == 'CODE' and length:
            yield si, bytes(mod.data[si]), bytes(mod.mask[si])


def match(img, code, mask, lo, hi):
    """All linear positions in [lo,hi) where masked code matches."""
    # anchor: longest fully-fixed run
    best, run_s, cur = (0, 0), 0, 0
    for k in range(len(code) + 1):
        if k < len(code) and mask[k]:
            if cur == 0: run_s = k
            cur += 1
            if cur > best[1]: best = (run_s, cur)
        else:
            cur = 0
    a_off, a_len = best
    if a_len < 4:
        return []
    anchor = code[a_off:a_off + min(a_len, 24)]
    out = []
    p = img.find(anchor, lo)
    while p != -1 and p < hi:
        s = p - a_off
        if s >= 0 and s + len(code) <= len(img):
            if all(not mask[k] or img[s + k] == code[k] for k in range(len(code))):
                out.append(s)
        p = img.find(anchor, p + 1)
    return out


def main():
    args = sys.argv[1:]
    sep = args.index('--')
    libs, exe = args[:sep], args[sep + 1]
    m = MZ(exe)
    img = m.image
    found = {}
    stats = [0, 0]
    for lib in libs:
        blob = open(lib, 'rb').read()
        for mod in parse_records(blob):
            for si, code, mask in code_segments(mod):
                stats[0] += 1
                hits = match(img, code, mask, 0, len(img))
                if len(hits) != 1:
                    continue
                stats[1] += 1
                base = hits[0]
                for name, psi, off in mod.pubs:
                    if psi == si:
                        found[base + off] = name
                found.setdefault(base, mod.name + ':' + mod.segs[si][0])
    print('modules with code: %d, uniquely matched: %d, names: %d' % (stats[0], stats[1], len(found)),
          file=sys.stderr)
    json.dump({hex(k): v for k, v in sorted(found.items())}, sys.stdout, indent=0)


if __name__ == '__main__':
    main()
