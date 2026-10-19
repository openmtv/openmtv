"""Shared loader for the unpacked MTV image."""
import struct, os
HERE = os.path.dirname(os.path.abspath(__file__))
class MZ:
    def __init__(self, path):
        d = open(path, 'rb').read()
        h = struct.unpack_from('<2s13H', d, 0)
        self.nreloc, hpara = h[3], h[4]
        self.ss, self.sp, self.ip, self.cs = h[7], h[8], h[10], h[11]
        size = (h[2] - 1) * 512 + (h[1] or 512)
        self.image = d[hpara * 16:size]
        self.relocs = [struct.unpack_from('<HH', d, h[12] + 4 * i) for i in range(self.nreloc)]
        self.reloc_lin = sorted(s * 16 + o for o, s in self.relocs)
        self.reloc_set = set(self.reloc_lin)
    def w(self, lin): return struct.unpack_from('<H', self.image, lin)[0]
    def b(self, lin): return self.image[lin]
