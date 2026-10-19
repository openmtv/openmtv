#!/usr/bin/env python3
"""Per-day statistics from memory dumps written by `--dump-on 39D4:71E4:DIR`
(one dump shortly after each change of the day counter).

    tools/stats.py DIR [DIR2 ...] > stats.csv

One CSV line per dump: day, human player, difficulty, human credit, and per
player 1..3 money, image, Betty love, out flag, reach (viewers), contracts,
library size, yesterday's average rating and programme quality, and the AI
budget pots.  Field addresses come from names/symbols.txt, so the columns
follow the names (and their uncertainty -- see the descriptions there).
"""
import os, sys, struct, csv

HERE = os.path.dirname(os.path.abspath(__file__))
SYMS = os.path.join(HERE, '..', 'names', 'symbols.txt')
VIEWERS_PER_UNIT = 2464          # reach unit -> viewers (FINDINGS.md)


def load_names():
    g, rec = {}, {}
    for line in open(SYMS):
        p = line.split()
        if len(p) >= 4 and p[0] == 'g':
            g[p[3]] = (int(p[1], 16), int(p[2]))
        elif len(p) >= 6 and p[0] == 'rec':
            rec[p[5]] = (int(p[1], 16), int(p[2], 16), int(p[3], 16), int(p[4]))
    return g, rec


class Dump:
    def __init__(self, path, loadseg):
        self.m = open(path, 'rb').read()
        self.dg = (loadseg + 0x39D4) * 16

    def rd(self, off, width, signed=False):
        a = self.dg + (off & 0xFFFF)
        fmt = {8: 'b', 16: 'h', 32: 'i'}[width]
        return struct.unpack_from('<' + (fmt if signed else fmt.upper()), self.m, a)[0]


def main():
    g, rec = load_names()

    def G(d, name, signed=False):
        off, w = g[name]
        return d.rd(off, w, signed)

    def R(d, name, i, extra=0, signed=False):
        base, size, off, w = rec[name]
        return d.rd(base + i * size + off + extra, w, signed)

    cols = ['dir', 'day', 'frame', 'human', 'difficulty', 'human_credit']
    per = ['money', 'image', 'betty_love', 'out', 'viewers', 'contracts', 'library',
           'avg_rating', 'avg_quality', 'ai_gifts', 'ai_films', 'ai_news', 'ai_production', 'ai_transmitters']
    cols += ['p%d_%s' % (p, c) for p in (1, 2, 3) for c in per]
    w = csv.writer(sys.stdout)
    w.writerow(cols)
    for dr in sys.argv[1:]:
        idx = os.path.join(dr, 'index.txt')
        for line in open(idx):
            f = line.split()
            fn, frame, loadseg = f[0], int(f[4]), int(f[8], 16)
            d = Dump(os.path.join(dr, fn), loadseg)
            row = [dr, G(d, 'g_day'), frame, G(d, 'g_player'), G(d, 'g_difficulty'), G(d, 'g_credit', True)]
            for p in (1, 2, 3):
                lib_base = rec['count'][0] + p * rec['count'][1] + rec['count'][2]
                row += [
                    R(d, 'pl_money', p, signed=True),
                    R(d, 'pl_image', p, signed=True),
                    R(d, 'pl_betty_love', p, signed=True),
                    R(d, 'pl_out', p),
                    # transmitter entry 0 of the player holds the total reach
                    d.rd(rec['tx_reach'][0] + p * 0x1CE + rec['tx_reach'][2], 32) * VIEWERS_PER_UNIT,
                    d.rd(g['g_player_ad_count'][0] + 2 * p, 16),
                    sum(d.rd(lib_base + genre, 8) for genre in range(1, 12)),
                    d.rd(g['g_avg_rating'][0] + 2 * p, 16, True),
                    d.rd(g['g_avg_quality'][0] + 2 * p, 16, True),
                ] + [R(d, n, p, signed=True) for n in ('aibudget_gifts', 'aibudget_films', 'aibudget_news',
                                                       'aibudget_production', 'aibudget_transmitters')]
            w.writerow(row)


if __name__ == '__main__':
    main()
