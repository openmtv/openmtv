#!/usr/bin/env python3
"""LZEXE 0.91 unpacker (clean-room from the documented format).
Usage: unlzexe.py IN.EXE OUT.EXE"""
import struct, sys

def unpack(data):
    hdr = list(struct.unpack_from('<14H', data, 0))
    assert data[0:2] == b'MZ' and data[0x1c:0x20] == b'LZ91', 'not LZEXE 0.91'
    hdrsize = hdr[4] * 16
    ldr = hdrsize + hdr[11] * 16                   # loader at CS:0
    o_ip, o_cs, o_sp, o_ss, comp_paras, inc_paras, ldr_size, _ = struct.unpack_from('<8H', data, ldr)
    # relocations (compressed table at loader+0x158)
    p = ldr + 0x158
    relocs, seg, off = [], 0, 0
    while True:
        span = data[p]; p += 1
        if span == 0:
            span = struct.unpack_from('<H', data, p)[0]; p += 2
            if span == 0:
                seg += 0x0FFF; continue
            if span == 1:
                break
        off += span
        seg += (off & ~0x0F) >> 4
        off &= 0x0F
        relocs.append((off, seg))
    # decompress
    src = hdrsize + (hdr[11] - comp_paras) * 16
    pos = [src]
    def byte():
        b = data[pos[0]]; pos[0] += 1; return b
    bits = [struct.unpack_from('<H', data, src)[0], 16]; pos[0] += 2
    def bit():
        b = bits[0] & 1
        bits[0] >>= 1; bits[1] -= 1
        if bits[1] == 0:
            bits[0] = struct.unpack_from('<H', data, pos[0])[0]; pos[0] += 2; bits[1] = 16
        return b
    out = bytearray()
    while True:
        if bit():
            out.append(byte()); continue
        if not bit():
            ln = (bit() << 1) | bit()
            ln += 2
            span = byte() - 0x100
        else:
            lo = byte(); hi = byte()
            span = (lo | ((hi & ~7) << 5) | 0xE000) - 0x10000
            ln = (hi & 7) + 2
            if ln == 2:
                ln = byte()
                if ln == 0: break
                if ln == 1: continue
                ln += 1
        start = len(out) + span
        for i in range(ln):
            out.append(out[start + i])
    return dict(image=bytes(out), relocs=relocs, ip=o_ip, cs=o_cs, sp=o_sp, ss=o_ss,
                minalloc=hdr[5], maxalloc=hdr[6], inc_paras=inc_paras, comp_paras=comp_paras,
                ldr_cs=hdr[11], ldr_ss=hdr[7])

def build_exe(u):
    img, rel = u['image'], u['relocs']
    hsize = 0x1C + 4 * len(rel)
    hpara = (hsize + 0x1FF) // 0x200 * 0x20     # pad header to 512 like unlzexe
    total = hpara * 16 + len(img)
    # memory needed by the packed program, relative to its own image
    packed_mem_paras = u['ldr_cs'] * 16  # unused, kept for reference
    hdr = struct.pack('<2s13H', b'MZ', total % 512, (total + 511) // 512, len(rel), hpara,
                      0, 0xFFFF, u['ss'], u['sp'], 0, u['ip'], u['cs'], 0x1C, 0)
    hdr += b''.join(struct.pack('<HH', o, s) for o, s in rel)
    hdr = hdr.ljust(hpara * 16, b'\0')
    return hdr + img

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    u = unpack(d)
    print(f"image {len(u['image'])} bytes, {len(u['relocs'])} relocs, entry {u['cs']:04X}:{u['ip']:04X}, "
          f"stack {u['ss']:04X}:{u['sp']:04X}, packed minalloc {u['minalloc']} inc {u['inc_paras']}")
    open(sys.argv[2], 'wb').write(build_exe(u))
