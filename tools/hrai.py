#!/usr/bin/env python3
"""AI pictures (AI upscaling) for --filter hr-pixel (docs/ki-bilder.md): every picture
sprite (rule mmpx in runtime/hr_rules.txt) upscaled 4x by a pixel-art ESRGAN
model, made into a picture set the game loads ([display] ai_pictures or HR_KI=FILE;
the format: the comment in src/hr.c).  The reference for src/ki_make.c (mtv-dec
--make-ki, the launcher's way; tools/kitest.py compares the two).  Everything here comes from the player's own game
files, so it stays outside git (build/hrai/).

  hrai.py export IN          the model's input: IN/NAME.png (colour; transparent
                             pixels filled from their neighbours, PAD around),
                             IN/NAME_m.png (the mask), IN/jobs.json
  (tools/hrai_up.py MODEL IN OUT on a machine with torch + spandrel: OUT/NAME.png)
  hrai.py palette IN OUT PAL  the 223 extra colours of a model (k-means over its
                             pictures), made once per model and kept: every
                             player's set then takes the same colours
  hrai.py import IN OUT SET [--pal PAL] [--model NAME]   the picture set: the
                             game's 31 colours, with --pal also the extra ones
  hrai.py list SET           what a set holds
  hrai.py parts OUT MAP...   runtime/ki_parts.txt (OUT) from the recordings of
                             HR_KIMAP=MAP (src/hr.c): where each animation part
                             sits on what the game draws beneath it, which
                             pictures it puts together from pieces, and where a
                             sharp picture always lies on one of the model's

The pictures:
  * rooms: one picture of both halves (room_draw_background 0054:1EEC:
    resource n at x 0, n-50 at x 160), so there is no seam;
  * groups: pictures the game always shows together, piece beside piece
    (the bottom panel's frames, the title, "THE END"; runtime/ki_parts.txt),
    one picture for the model too, cut apart afterwards;
  * animation parts: a sprite that the game draws over a larger one of its
    bank (a moving hand, a mouth; found by pixel match) is upscaled where it
    sits on that one, so the model sees the same surroundings;
  * where the original does not change -- a part on its picture, a frame
    against the previous and the first frame of its animation (sprites of one
    size in a row), the same pixels in a 5x5 neighbourhood -- the sub-pixels
    are taken from the other one, so they do not flicker or show an edge;
  * a sub-pixel within KEEP (CIELAB) of its original pixel's colour keeps that
    colour: the models shift flat areas a little, which shows next to the
    pictures drawn exactly;
  * around a sharp picture (no model) that always lies at one place on a
    picture of the model's (the TV's button frames), the ring of pixels just
    outside it keeps its colour where it is flat there: the model would
    spread an edge beneath it out into that ring (a seam beside the frame).
"""
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hrdata
import rsc

GAME = os.environ.get('GAME', os.path.join(os.path.dirname(HERE), 'run'))
PAD = 8                 # border of repeated edge pixels for the model
EXT = 223               # extra colours: indices 32..254
KEEP = 7.0              # a sub-pixel this close (CIELAB) to its original pixel's colour keeps it
STILL = 2               # radius of the neighbourhood that must be unchanged
SAME = 3.0              # a part's sub-pixel this close (CIELAB) to the picture without it shows that picture
MARGIN = 8              # pixels around a part in its pictures for the model
MAGIC = b'OMTVKI\x02\x00'
PARTS = os.path.join(os.path.dirname(HERE), 'runtime', 'ki_parts.txt')

_idx = _pal = None
_banks = {}


def game():
    global _idx, _pal
    if _idx is None:
        _idx = rsc.read_index([os.path.join(GAME, 'SOURCE1.RSC'), os.path.join(GAME, 'SOURCE2.RSC')])
        _pal = rsc.palette(rsc.raw(_idx, 1))
    return _idx, _pal


def bank(n):
    if n not in _banks:
        idx, _ = game()
        try:
            _banks[n] = rsc.decode_bank(rsc.raw(idx, n)) if n in idx else None
        except (ValueError, IndexError, KeyError, RecursionError):    # not a sprite bank
            _banks[n] = None
    return _banks[n]


def pix(res, k):
    if res == 155 and k > 100:
        return tapes()[(res, k)]
    w, h, p = bank(res)[k]
    return np.frombuffer(bytes(p), np.uint8).reshape(h, w)


def raw_rule(rules, res, k):
    for r, a, b, rule in rules:
        if r == res and a <= k <= b:
            return rule
    return 'HR_MMPX'


def still(a, b, r=STILL):
    """where a and b (same shape) agree, a is ink, in the whole neighbourhood of radius r"""
    eq = (a == b) & (a != 0)
    h, w = eq.shape
    p = np.pad(eq, r)
    s = np.ones_like(eq)
    for dy in range(2 * r + 1):
        for dx in range(2 * r + 1):
            s &= p[dy:dy + h, dx:dx + w]
    return s


def find_base(spr, k):
    """(base k, x, y) when sprite k sits on a larger sprite of its bank: the best
    pixel match (half its ink the same), and an eighth of its ink unchanged with
    the 3x3 around it"""
    from numpy.lib.stride_tricks import sliding_window_view
    o = spr[k]
    h, w = o.shape
    ink = o != 0
    if ink.sum() < 16:
        return None
    best = None
    for kb, b in spr.items():
        H, W = b.shape
        if kb == k or w > W or h > H or (w, h) == (W, H) or W * H < 400:
            continue
        score = ((sliding_window_view(b, (h, w)) == o) & ink).sum(axis=(2, 3))
        y, x = np.unravel_index(score.argmax(), score.shape)
        if score[y, x] >= 0.5 * ink.sum() and (best is None or score[y, x] > best[0]):
            best = (score[y, x], kb, int(x), int(y))
    if best is None:
        return None
    _, kb, x, y = best
    if still(o, spr[kb][y:y + h, x:x + w], 1).sum() < ink.sum() / 8:
        return None
    return kb, x, y


def read_parts(path=PARTS):
    """{(res, k): ('room', n, x, y) | ('sprite', r, k, x, y)}: where the game draws
    each animation part (runtime/ki_parts.txt); None: on nothing at one place, or one
    of a group's pictures"""
    out = {}
    if os.path.exists(path):
        for line in open(path):
            w = line.split('#')[0].split()
            if len(w) == 3 and w[2] == 'none' or len(w) == 7 and w[2] == 'group':
                out[(int(w[0]), int(w[1]))] = None
            elif len(w) == 6 and w[2] == 'room':
                out[(int(w[0]), int(w[1]))] = ('room', int(w[3]), int(w[4]), int(w[5]))
            elif len(w) == 7 and w[2] == 'sprite':
                out[(int(w[0]), int(w[1]))] = ('sprite', int(w[3]), int(w[4]), int(w[5]), int(w[6]))
    return out


def read_groups(path=PARTS):
    """{(r0, k0): [(res, k, x, y), ...]}: the groups (runtime/ki_parts.txt), each by
    its first sprite, its sprites in order with their places in its picture"""
    out = {}
    if os.path.exists(path):
        for line in open(path):
            w = line.split('#')[0].split()
            if len(w) == 7 and w[2] == 'group':
                out.setdefault((int(w[3]), int(w[4])), []).append((int(w[0]), int(w[1]), int(w[5]), int(w[6])))
    return {g: sorted(m) for g, m in out.items()}


def read_covers(path=PARTS):
    """{(r2, k2): [(res, k, x, y), ...]}: the sharp pictures that always lie on
    picture r2:k2 of the model's, and where (runtime/ki_parts.txt)"""
    out = {}
    if os.path.exists(path):
        for line in open(path):
            w = line.split('#')[0].split()
            if len(w) == 7 and w[2] == 'cover':
                out.setdefault((int(w[3]), int(w[4])), []).append((int(w[0]), int(w[1]), int(w[5]), int(w[6])))
    return {g: sorted(m) for g, m in out.items()}


def touching(a, b, x, y):
    """pictures a and b (pixels) with b's corner at (x, y) in a's: None apart, else
    whether they go together -- touch or overlap by at most a tenth of the smaller,
    no pixel in both of them in two colours"""
    ha, wa = a.shape
    hb, wb = b.shape
    x0, y0, x1, y1 = max(0, x), max(0, y), min(wa, x + wb), min(ha, y + hb)
    if x0 > x1 or y0 > y1:
        return None
    if (x1 - x0) * (y1 - y0) > 0.1 * min(a.size, b.size):
        return False
    sa, sb = a[y0:y1, x0:x1], b[y0 - y:y1 - y, x0 - x:x1 - x]
    return not ((sa != 0) & (sb != 0) & (sa != sb)).any()


def make_groups(seen, beside, ok):
    """the groups from the looks at the screen: two pictures of the model's go
    together when nearly always (9 of 10 looks) the one shows, the other lies
    beside it at one place, and they touch (touching); joined strongest first, a
    join that would put two of a group's pictures at odds is left out"""
    cand = []
    for (a, b), offs in beside.items():
        if not ok(*a) or not ok(*b):
            continue
        (x, y), n = min(offs.items(), key=lambda t: (-t[1], t[0]))
        if n >= 0.9 * sum(offs.values()) and n >= 0.9 * max(seen[a], seen[b]) and touching(pix(*a), pix(*b), x, y):
            cand.append((-n, a, b, x, y))
    group = {}                                  # sprite -> {sprite: (x, y)} (shared by its group)
    for _, a, b, x, y in sorted(cand):
        ga, gb = group.get(a, {a: (0, 0)}), group.get(b, {b: (0, 0)})
        if ga is gb:
            continue
        ax, ay = ga[a]
        bx, by = gb[b]
        dx, dy = ax + x - bx, ay + y - by       # b's group moved so that b lies at a + (x, y)
        moved = {s: (px + dx, py + dy) for s, (px, py) in gb.items()}
        if all(touching(pix(*s), pix(*t), tx - sx, ty - sy) is not False for s, (sx, sy) in ga.items() for t, (tx, ty) in moved.items()):
            ga.update(moved)
            for s in ga:
                group[s] = ga
    out = {}
    for g in {id(g): g for g in group.values()}.values():
        x0, y0 = min(x for x, _ in g.values()), min(y for _, y in g.values())
        out[min(g)] = sorted((s[0], s[1], x - x0, y - y0) for s, (x, y) in g.items())
    return out


def make_covers(seen, beside, groups):
    """(res, k, r2, k2, x, y): a sharp picture (rule nearest) inside picture r2:k2 of
    the model's at x, y in half its looks at the screen, and there in 9 of 10 looks at r2:k2"""
    rules = hrdata.rules()
    out = []
    for (a, b), offs in beside.items():
        for (x, y), n in offs.items():
            for s, p, ox, oy in ((a, b, -x, -y), (b, a, x, y)):     # s on p, its corner at (ox, oy) in p's pixels
                if not bank(s[0]) or s[1] >= len(bank(s[0])) or not bank(p[0]) or p[1] >= len(bank(p[0])):
                    continue
                if raw_rule(rules, *s) != 'HR_NEAREST' or raw_rule(rules, *p) != 'HR_MMPX':
                    continue
                (h, w), (H, W) = pix(*s).shape, pix(*p).shape
                if 0 <= ox and 0 <= oy and ox + w <= W and oy + h <= H and n >= 0.5 * seen[s] and n >= 0.9 * seen[p]:
                    out.append(s + p + (ox, oy))
    return sorted(out)


def make_parts(path, maps):
    """runtime/ki_parts.txt from HR_KIMAP files (src/hr.c): for every picture sprite
    the game drew, the picture under most of its pixels -- not itself, not a frame of
    its own animation (a sprite of its bank and size), not an equal picture it
    replaces -- if that is a fifth of them and the sprite sits at one place on it"""
    import collections
    rules = hrdata.rules()
    pixels, places, rel = collections.Counter(), collections.Counter(), collections.defaultdict(collections.Counter)
    seen, beside = collections.Counter(), collections.defaultdict(collections.Counter)
    for m in maps:
        for line in open(m):
            w = line.split()
            if w[:1] == ['seen']:
                seen[(int(w[1]), int(w[2]))] += int(w[3])
            elif w[:1] == ['beside']:
                r, k, r2, k2, x, y, n, _ = map(int, w[1:])
                beside[((r, k), (r2, k2))][(x, y)] += n
            elif w[:1] == ['sprite']:
                pixels[(int(w[1]), int(w[2]))] += int(w[6])
                places[(int(w[1]), int(w[2]))] = max(places[(int(w[1]), int(w[2]))], int(w[8]))
            elif w[:1] == ['under']:
                r, k, r2, k2, x, y, n = map(int, w[1:])
                if (r2, k2) == (r, k) or not bank(r2) or k2 >= len(bank(r2)):
                    continue
                if 101 <= r2 <= 127 and k2 == 0:
                    b = ('room', r2, x, y)
                elif 51 <= r2 <= 77 and k2 == 0:
                    b = ('room', r2 + 50, x + 160, y)
                else:
                    b = ('sprite', r2, k2, x, y)
                rel[(r, k)][b] += n
    ok = lambda r, k: (raw_rule(rules, r, k) == 'HR_MMPX' and not (51 <= r <= 77 or 101 <= r <= 127) and bank(r) and k < len(bank(r))
                       and (lambda w, h, p: w and h and w <= 255 and h <= 255 and any(p))(*bank(r)[k]))
    groups = make_groups(seen, beside, ok)
    members = {(r, k): (g, x, y) for g, m in groups.items() for r, k, x, y in m}
    rows = []
    for key in sorted(rel):
        r, k = key
        if raw_rule(rules, r, k) != 'HR_MMPX' or 51 <= r <= 77 or 101 <= r <= 127 or not bank(r) or k >= len(bank(r)) or key in members:
            continue
        shape = pix(r, k).shape
        # not itself, not a frame of its own animation, not a picture of its size it replaces
        c = collections.Counter({b: n for b, n in rel.get(key, {}).items() if not (b[0] == 'sprite' and pix(b[1], b[2]).shape == shape
                                                                           and (b[1] == r or b[3:] == (0, 0)))})
        if not c:
            continue
        b, n = c.most_common(1)[0]
        total = sum(c.values())
        same = sum(m for bb, m in c.items() if bb[:-2] == b[:-2])
        # one place on that picture: a moving sprite spreads over its picture
        fixed = n >= 0.9 * total or (n >= 0.5 * total and n >= 0.95 * same and places[key] <= 3)
        if fixed and n >= 0.2 * pixels[key]:
            rows.append('%d %d %s    # %.0f %% of its pixels, %.0f %% of those on another picture, %d places'
                        % (r, k, ' '.join(map(str, b)), 100 * n / pixels[key], 100 * n / total, places[key]))
    shown = {tuple(map(int, row.split()[:2])) for row in rows}
    rows += ['%d %d none' % key for key in sorted(pixels) if key not in shown and key not in members and raw_rule(rules, *key) == 'HR_MMPX'
             and not (51 <= key[0] <= 77 or 101 <= key[0] <= 127) and bank(key[0]) and key[1] < len(bank(key[0]))]
    nparts = len(rows)
    rows += ['%d %d group %d %d %d %d' % (key + (g[0], g[1], x, y)) for key, (g, x, y) in sorted(members.items())]
    covers = make_covers(seen, beside, groups)
    rows += ['%d %d cover %d %d %d %d' % c for c in covers]
    head = ['# AI pictures (docs/ki-bilder.md): where the game draws an animation part, from',
            '# HR_KIMAP over every scenario and the AI day (tools/hrai.py parts) -- numbers only:',
            '#   RES K room N X Y          sprite K of bank RES lies on room N (both halves) at X, Y',
            '#   RES K sprite R2 K2 X Y    ... on sprite K2 of bank R2 at X, Y',
            '#   RES K none                drawn, but on nothing at one place (it moves)',
            '#   RES K group R0 K0 X Y     one of the pictures the game shows together, piece beside',
            '#                             piece: at X, Y in their picture (R0:K0 the first of them)',
            '#   RES K cover R2 K2 X Y     a sharp picture (rule nearest) that always lies on picture',
            '#                             R2:K2 of the model\'s, at X, Y']
    open(path, 'w').write('\n'.join(head + rows) + '\n')
    print('%d parts, %d groups (%d pictures), %d covers -> %s' % (nparts, len(groups), len(members), len(covers), path))


# ---- the film dealer's new tapes (src/film_offers.c: --film-offers above 10) ----
# made from resource 155's ten tapes at run time, never a sprite of the game: the
# same rules here (Python's round, Pillow's nearest scaling, as the C), and in the
# set as sprites 101..107 of bank 155
TAPE_W = (17, 16, 19, 17, 15, 16, 16, 18, 16, 14)
TAPE_H = (36, 35, 33, 33, 32, 31, 30, 29, 27, 26)
TAPE_LEND = (2, 5, 3, 7, 1, 6, 4, 8, 9)
TAPES = 7
LINE, FACE = 2, 4


def _label_box(g):
    h, w = len(g), len(g[0])
    mid = g[h // 2]
    cols = [c for c in range(w) if mid[c] in (FACE, LINE)]
    if not cols:
        return None
    L, R = cols[0], cols[-1]
    full = [all(g[r][c] in (FACE, LINE) for c in range(L, R + 1)) for r in range(h)]
    T = 0
    while T < h and not full[T]:
        T += 1
    B = T
    while B + 1 < h and full[B + 1]:
        B += 1
    return (L, R, T, B) if T < h else None


def _scale_tab(src, dst):
    a, xo, out = src / dst, src / dst * 0.5, []
    for _ in range(dst):
        out.append(-1 if xo < 0 else int(xo))
        xo += a
    return out


def new_tape(t1, tk, j):
    """new tape j (1..) from tape 1 and tape k (lists of rows), as make_tape"""
    fixed = lambda v: v in (0, 1, 2, 4, 31)
    marks = lambda g: sorted({v for row in g for v in row if not fixed(v)})
    h1, w1 = len(t1), len(t1[0])
    H = h1 + j
    W = round(w1 * H / h1)
    cmap = list(range(256))
    for a, b in zip(marks(t1), marks(tk)):
        cmap[a] = b
    xt, yt = _scale_tab(w1, W), _scale_tab(h1, H)
    sc = [[cmap[t1[yt[r]][xt[c]]] for c in range(W)] for r in range(H)]
    L, R, T, B = _label_box(sc)
    Lk, Rk, Tk, Bk = _label_box(tk)
    lw = Rk - Lk + 1 + (1 if H >= 40 else 0)
    old, neu = W - L, lw + (W - 1 - R)
    g = [[sc[r][c] for c in range(L)] + [sc[r][L + round(i * (old - 1) / (neu - 1))] for i in range(neu)] for r in range(H)]
    R = L + lw - 1
    for r in range(T, B + 1):
        for c in range(L, R + 1):
            if g[r][c] in (FACE, LINE):
                g[r][c] = FACE
    n, nkr = B - T + 1, Bk - Tk + 1
    for c0 in range(Lk, Rk + 1):
        r = Tk
        while r <= Bk:
            if tk[r][c0] == LINE:
                st = r
                while r <= Bk and tk[r][c0] == LINE:
                    r += 1
                en, c = r - 1, L + (c0 - Lk)
                if c < R:
                    a = T + round((st - Tk) * n / nkr)
                    zz = round((Bk - en) * n / nkr) + 1
                    z = B - max(zz, 2)
                    for rr in range(a, max(a + 1, z) + 1):
                        if rr < H:
                            g[rr][c] = LINE
            r += 1
    return np.array(g, np.uint8)


_tapes = None


def tapes():
    """{(155, 100 + j): pixels} for the new tapes, if resource 155 is the original's"""
    global _tapes
    if _tapes is None:
        _tapes = _make_tapes()
    return _tapes


def _make_tapes():
    b = bank(155)
    if not b or len(b) < 10 or any(pix(155, k).shape[0] != TAPE_H[k] for k in range(10)):
        return {}
    orig = [pix(155, k)[:, :TAPE_W[k]].tolist() for k in range(10)]
    return {(155, 100 + j): new_tape(orig[0], orig[TAPE_LEND[j - 1]], j) for j in range(1, TAPES + 1)}


def room_pix(n):
    """room n's picture: both halves side by side"""
    l, r = pix(n, 0), pix(n - 50, 0)
    out = np.zeros((109, 160 + r.shape[1]), np.uint8)
    out[:, :l.shape[1]] = l
    out[:, 160:] = r
    return out


def chain(key, parts, fallback, seen=()):
    """(root, layers, (x, y)) for a part: root ('room', n) or ('sprite', r, k); the
    sprites beneath it [(r, k, x, y)] in drawing order and its place, all in the
    root's pixels; None for a sprite without a base (or a circle of them)"""
    b = parts.get(key) or fallback.get(key)
    if not b or key in seen:
        return None
    if b[0] == 'room':
        return ('room', b[1]), [], (b[2], b[3])
    _, r, k, x, y = b
    up = chain((r, k), parts, fallback, seen + (key,))
    if up is None:
        return ('sprite', r, k), [], (x, y)
    root, layers, (ux, uy) = up
    return root, layers + [(r, k, ux, uy)], (ux + x, uy + y)


def root_pix(root):
    return room_pix(root[1]) if root[0] == 'room' else pix(root[1], root[2])


def jobs():
    """(pictures, chains, refs).  pictures [(name, w, h, [(res, k, x, y, w, h)], mask)]
    for the model (a room's or a group's: its sprites drawn in this order, colour 0
    clear); chains {(res, k): (root, layers, at, window)} for the parts, each
    with two pictures of its window: ctxRRR_K (as on the screen) and undRRR_K
    (without it); refs {(res, k): [(res, k_ref, x, y), ...]} -- for the other
    sprites, the frames whose sub-pixels an unchanged part takes (a later one wins)"""
    idx, _ = game()
    rules = hrdata.rules()
    want = lambda res, k: (lambda w, h, p: w and h and w <= 255 and h <= 255 and any(p)
                           and raw_rule(rules, res, k) == 'HR_MMPX')(*bank(res)[k])
    out, refs, done = [], {}, set()
    for n in range(101, 128):
        left, right = bank(n), bank(n - 50)
        if not left or not right or len(left) != 1 or len(right) != 1:
            continue
        (wl, hl, _), (wr, hr, _) = left[0], right[0]
        if hl != 109 or hr != 109 or wl > 160 or not want(n, 0) or not want(n - 50, 0):
            continue
        done |= {n, n - 50}
        out.append(('room%03d' % n, 160 + wr, 109, [(n, 0, 0, 0, wl, hl), (n - 50, 0, 160, 0, wr, hr)], True))
    members = set()                             # the groups' sprites: no chain, no frame refs of their own
    for g, m in sorted(read_groups().items()):
        m = [(r, k, x, y) + pix(r, k).shape[::-1] for r, k, x, y in m if r not in done and bank(r) and k < len(bank(r)) and want(r, k)]
        if len(m) < 2:                          # (other game files: what is left of it on its own)
            continue
        out.append(('grp%03d_%d' % g, max(x + w for _, _, x, _, w, _ in m), max(y + h for _, _, _, y, _, h in m), m, True))
        members |= {(r, k) for r, k, _, _, _, _ in m}
    parts, fallback, sprites = read_parts(), {}, []
    for n in sorted(idx):
        b = None if n in done else bank(n)
        if not b:
            continue
        spr = {k: pix(n, k) for k in range(len(b)) if want(n, k)}
        for k, o in spr.items():
            if (n, k) in members:               # (in spr still: a part may sit on it)
                continue
            h, w = o.shape
            out.append(('%03d_%d' % (n, k), w, h, [(n, k, 0, 0, w, h)], True))
            sprites.append((n, k))
            if (n, k) not in parts:             # not seen in a scenario: the pixel match in its bank
                base = find_base(spr, k)
                if base:
                    fallback[(n, k)] = ('sprite', n) + base
    for key, o in tapes().items():            # the film dealer's new tapes (src/film_offers.c)
        out.append(('%03d_%d' % key, o.shape[1], o.shape[0], [key + (0, 0, o.shape[1], o.shape[0])], True))
    chains = {}
    for key in sprites:
        c = chain(key, parts, fallback)
        if c is None:
            continue
        root, layers, (x, y) = c
        H, W = root_pix(root).shape
        h, w = pix(*key).shape
        win = (max(0, x - MARGIN), max(0, y - MARGIN), min(W, x + w + MARGIN), min(H, y + h + MARGIN))
        if win[0] >= win[2] or win[1] >= win[3] or x >= W or y >= H or x + w <= 0 or y + h <= 0:
            continue
        chains[key] = (root, layers, (x, y), win)
        for kind in ('ctx', 'und'):
            out.append(('%s%03d_%d' % ((kind,) + key), win[2] - win[0], win[3] - win[1], [], False))
    pictured = set(sprites) | members           # (a frame takes from a group's sprite too: 192:2 from 192:1)
    for n, k in sprites:                        # the frames of a run of one size: the previous and the first
        if (n, k) in chains:
            continue
        o, first = pix(n, k), k
        while (n, first - 1) in pictured and pix(n, first - 1).shape == o.shape:
            first -= 1
        if first != k:
            r = [(n, f, 0, 0) for f in dict.fromkeys((k - 1, first)) if still(o, pix(n, f)).any()]
            if r:
                refs[(n, k)] = r
    return out, chains, refs


def layered(root, layers, extra=None):
    """the root's pixels with the layers (and extra = (r, k, x, y)) drawn over it,
    colour 0 transparent"""
    a = root_pix(root).copy()
    for r, k, x, y in layers + ([extra] if extra else []):
        o = pix(r, k)
        h, w = o.shape
        x0, y0, x1, y1 = max(0, x), max(0, y), min(a.shape[1], x + w), min(a.shape[0], y + h)
        if x0 >= x1 or y0 >= y1:
            continue
        sub = o[y0 - y:y1 - y, x0 - x:x1 - x]
        a[y0:y1, x0:x1] = np.where(sub != 0, sub, a[y0:y1, x0:x1])
    return a


def fill(rgb, opaque):
    """the transparent pixels get the mean colour of their filled neighbours, ring by
    ring (so the model sees no foreign colour at an outline)"""
    rgb, have = rgb.astype(np.float32), opaque.copy()
    if not have.any():
        return rgb
    while not have.all():
        p = np.pad(rgb * have[..., None], ((1, 1), (1, 1), (0, 0)))
        c = np.pad(have.astype(np.float32), 1)
        s = p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]
        n = c[:-2, 1:-1] + c[2:, 1:-1] + c[1:-1, :-2] + c[1:-1, 2:]
        new = ~have & (n > 0)
        rgb[new] = s[new] / n[new][:, None]
        have |= new
    return rgb


def export(dst):
    from PIL import Image
    _, pal = game()
    P = np.array(pal, np.uint8)
    os.makedirs(dst, exist_ok=True)
    pics, chains, refs = jobs()
    meta = []
    for name, w, h, parts, mask in pics:
        if name[:3] in ('ctx', 'und'):
            key = tuple(int(v) for v in name[3:].split('_'))
            root, layers, (x, y), (x0, y0, x1, y1) = chains[key]
            ix = layered(root, layers, key + (x, y) if name[:3] == 'ctx' else None)[y0:y1, x0:x1]
        else:
            ix = np.zeros((h, w), np.uint8)
            for res, k, x, y, pw, ph in parts:
                ix[y:y + ph, x:x + pw] = np.where(pix(res, k) != 0, pix(res, k), ix[y:y + ph, x:x + pw])
        opaque = ix != 0
        rgb = np.pad(fill(P[ix], opaque), ((PAD, PAD), (PAD, PAD), (0, 0)), mode='edge')
        Image.fromarray((rgb + 0.5).clip(0, 255).astype(np.uint8)).save(os.path.join(dst, name + '.png'))
        mask = mask and not opaque.all()
        if mask:
            m = np.pad(opaque.astype(np.uint8) * 255, PAD)
            Image.fromarray(np.repeat(m[..., None], 3, 2)).save(os.path.join(dst, name + '_m.png'))
        meta.append({'name': name, 'w': w, 'h': h, 'mask': mask, 'parts': [list(p) for p in parts]})
    json.dump({'pad': PAD, 'jobs': meta, 'refs': [list(k) + list(r) for k, v in sorted(refs.items()) for r in v],
               'chains': [list(k) + [list(c[0]), [list(l) for l in c[1]], list(c[2]), list(c[3])] for k, c in sorted(chains.items())]},
              open(os.path.join(dst, 'jobs.json'), 'w'))
    own = [j for j in meta if j['name'][:3] not in ('ctx', 'und')]
    print('%d pictures for %d sprites (%d parts on what lies beneath them, %d frames kept still against another) -> %s'
          % (len(meta), sum(len(j['parts']) for j in own), len(chains), len(refs), dst))


def lab(rgb):
    """CIELAB of (..., 3) sRGB 0..255"""
    c = np.asarray(rgb, np.float64) / 255
    c = np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)
    xyz = c @ np.array([[0.4124, 0.2126, 0.0193], [0.3576, 0.7152, 0.1192], [0.1805, 0.0722, 0.9505]])
    xyz /= np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 216 / 24389, np.cbrt(xyz), (24389 / 27 * xyz + 16) / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def lab_to_rgb(L):
    fy = (L[..., 0] + 16) / 116
    f = np.stack([fy + L[..., 1] / 500, fy, fy - L[..., 2] / 200], -1)
    xyz = np.where(f ** 3 > 216 / 24389, f ** 3, (116 * f - 16) / (24389 / 27)) * np.array([0.95047, 1.0, 1.08883])
    c = xyz @ np.linalg.inv(np.array([[0.4124, 0.2126, 0.0193], [0.3576, 0.7152, 0.1192], [0.1805, 0.0722, 0.9505]]))
    c = np.where(c > 0.0031308, 1.055 * np.clip(c, 0, None) ** (1 / 2.4) - 0.055, 12.92 * c)
    return (c * 255 + 0.5).clip(0, 255).astype(np.uint8)


def nearest(L, centres):
    """the index of the nearest centre (CIELAB) of each row of L"""
    out = np.empty(len(L), np.int64)
    for i in range(0, len(L), 16384):
        d = ((L[i:i + 16384, None, :] - centres[None]) ** 2).sum(-1)
        out[i:i + 16384] = d.argmin(1)
    return out


def kmeans(L, fixed, n, rounds=24, seed=1):
    """n centres beside the fixed ones (they stay), CIELAB"""
    rng = np.random.default_rng(seed)
    free = L[rng.choice(len(L), n, replace=False)]
    for _ in range(rounds):
        a = nearest(L, np.concatenate([fixed, free]))
        for i in range(n):
            sel = L[a == len(fixed) + i]
            free[i] = sel.mean(0) if len(sel) else L[rng.integers(len(L))]
    return free


def fixed_colours():
    """indices and CIELAB of the colours a sub-pixel may always take: 1..30 and 31
    (black, opaque)"""
    _, pal = game()
    ids = list(range(1, 32))
    return ids, lab(np.array([pal[i] for i in ids], np.float64)).astype(np.float32)


def load_out(out, j, pad):
    """a job's upscaled colour picture (4h x 4w x 3, CIELAB) and its mask (or None)"""
    from PIL import Image
    w, h, s = j['w'], j['h'], 4 * pad
    L = lab(np.asarray(Image.open(os.path.join(out, j['name'] + '.png')).convert('RGB'))[s:s + 4 * h, s:s + 4 * w]).astype(np.float32)
    m = None
    if j['mask']:
        m = np.asarray(Image.open(os.path.join(out, j['name'] + '_m.png')).convert('L'))[s:s + 4 * h, s:s + 4 * w] >= 128
    return L, m


def sprites_of(src, out):
    """(own, refs, chains, win): own {(res, k): (L, mask, orig)} -- each sprite's
    upscaled colours, its mask and the original's index under each sub-pixel; the
    frames' refs; the parts' chains and win {(res, k): (L as on the screen, L
    without it)} over their windows"""
    meta = json.load(open(os.path.join(src, 'jobs.json')))
    own, win = {}, {}
    for j in meta['jobs']:
        L, m = load_out(out, j, meta['pad'])
        if j['name'][:3] in ('ctx', 'und'):
            key = tuple(int(v) for v in j['name'][3:].split('_'))
            win.setdefault(key, [None, None])[j['name'][:3] == 'und'] = L
            continue
        for r, k, x, y, pw, ph in j['parts']:
            Lp = L[4 * y:4 * (y + ph), 4 * x:4 * (x + pw)]
            mp = m[4 * y:4 * (y + ph), 4 * x:4 * (x + pw)] if m is not None else np.ones(Lp.shape[:2], bool)
            own[(r, k)] = (Lp, mp, np.repeat(np.repeat(pix(r, k), 4, 0), 4, 1))
    refs = {}
    for a, b, c, d, x, y in meta['refs']:
        refs.setdefault((a, b), []).append((c, d, x, y))
    chains = {(a, b): (tuple(root), [tuple(l) for l in layers], tuple(at), tuple(w))
              for a, b, root, layers, at, w in meta.get('chains', [])}
    return own, refs, chains, win


def keep_mask(L, o, P):
    """the sub-pixels that keep their original pixel's colour"""
    return (o != 0) & (o < 32) & (((L - P[o]) ** 2).sum(-1) < KEEP ** 2)


def make_palette(src, out, path):
    _, pal = game()
    P = lab(np.array(pal, np.float64)).astype(np.float32)
    spr = sprites_of(src, out)[0]
    sample = np.concatenate([L[m & ~keep_mask(L, o, P)] for L, m, o in spr.values()])
    sample = sample[np.random.default_rng(0).choice(len(sample), min(len(sample), 300000), replace=False)]
    _, fixed = fixed_colours()
    free = kmeans(sample, fixed, EXT)
    full = np.array(pal, np.uint8)
    full[32:32 + EXT] = lab_to_rgb(free)
    open(path, 'wb').write(full.tobytes())
    print('%d extra colours -> %s' % (EXT, path))


def rle(data):
    """the set's run-length code: c < 128 + c+1 bytes as they are; c >= 128 + a byte
    repeated c-126 times (2..129)"""
    out, i, n = bytearray(), 0, len(data)
    lit = bytearray()
    while i < n:
        j = i
        while j < n and j - i < 129 and data[j] == data[i]:
            j += 1
        if j - i >= 2:
            while lit:
                out.append(min(len(lit), 128) - 1); out += lit[:128]; lit = lit[128:]
            out.append(j - i + 126); out.append(data[i])
            i = j
        else:
            lit.append(data[i]); i += 1
    while lit:
        out.append(min(len(lit), 128) - 1); out += lit[:128]; lit = lit[128:]
    return bytes(out)


def fnv(data):
    """FNV-1a, 32 bits: a set's sum of the pixels a picture was made from (src/hr.c ki_sum)"""
    v = 2166136261
    for b in data:
        v = ((v ^ b) * 16777619) & 0xFFFFFFFF
    return v


def unrle(data, n):
    out, i = bytearray(), 0
    while i < len(data) and len(out) < n:
        c = data[i]; i += 1
        if c < 128:
            out += data[i:i + c + 1]; i += c + 1
        else:
            out += bytes([data[i]]) * (c - 126); i += 1
    return bytes(out)


def do_import(src, out, path, pal_path=None, model=''):
    _, pal = game()
    P = lab(np.array(pal, np.float64)).astype(np.float32)
    ids, centres = fixed_colours()
    full = np.array(pal, np.uint8)
    if pal_path:
        full = np.frombuffer(open(pal_path, 'rb').read(), np.uint8).reshape(256, 3).copy()
        centres = np.concatenate([centres, lab(full[32:32 + EXT].astype(np.float64)).astype(np.float32)])
        ids = ids + list(range(32, 32 + EXT))
    ids = np.array(ids, np.uint8)
    spr, refs, chains, win = sprites_of(src, out)
    up4 = lambda a: np.repeat(np.repeat(a, 4, 0), 4, 1)

    def quant(L, o):
        v = ids[nearest(L.reshape(-1, 3), centres)].reshape(L.shape[:2])
        keep = keep_mask(L, o, P)
        v[keep] = o[keep]
        return v
    q = {}
    for key, (L, m, o) in spr.items():
        v = quant(L, o)
        v[~m] = 0
        q[key] = v
    def frame(key):                    # a frame: its still parts from its references (final by then)
        for r, kb, x, y in refs[key]:
            o, ob = pix(*key), pix(r, kb)
            h, w = o.shape
            st = up4(still(o, ob[y:y + h, x:x + w]))
            q[key][st] = q[(r, kb)][4 * y:4 * (y + h), 4 * x:4 * (x + w)][st]

    def final(key):                    # a sprite's cells as the display has them (not in the set: nearest)
        return q[key] if key in q else up4(pix(*key))

    def root_final(root):
        if root[0] == 'room':
            l, r = final((root[1], 0)), final((root[1] - 50, 0))
            a = np.zeros((4 * 109, 4 * 160 + r.shape[1]), np.uint8)
            a[:, :l.shape[1]] = l
            a[:, 640:] = r
            return a
        return final(root[1:]).copy()
    done = set()

    def part(key):                     # a part on what lies beneath it, after the parts beneath it
        if key in done:
            return
        done.add(key)
        root, layers, (x, y), (x0, y0, x1, y1) = chains[key]
        for r, k, _, _ in layers:
            if (r, k) in chains:
                part((r, k))
        bf = root_final(root)
        for r, k, lx, ly in layers:
            f = final((r, k))
            h, w = f.shape
            ax0, ay0, ax1, ay1 = max(0, 4 * lx), max(0, 4 * ly), min(bf.shape[1], 4 * lx + w), min(bf.shape[0], 4 * ly + h)
            if ax0 < ax1 and ay0 < ay1:
                sub = f[ay0 - 4 * ly:ay1 - 4 * ly, ax0 - 4 * lx:ax1 - 4 * lx]
                bf[ay0:ay1, ax0:ax1] = np.where(sub != 0, sub, bf[ay0:ay1, ax0:ax1])
        under = up4(layered(root, layers))
        Lc, Lu = win[key]
        L, m, o = spr[key]
        h, w = o.shape
        # the part's sub-pixels inside its window (in the part's and in the root's coordinates)
        px0, py0 = max(0, 4 * (x0 - x)), max(0, 4 * (y0 - y))
        px1, py1 = min(w, 4 * (x1 - x)), min(h, 4 * (y1 - y))
        if px0 >= px1 or py0 >= py1:
            return
        rx0, ry0 = 4 * x + px0, 4 * y + py0
        rs = (slice(ry0, ry0 + py1 - py0), slice(rx0, rx0 + px1 - px0))
        ws = (slice(ry0 - 4 * y0, ry0 - 4 * y0 + py1 - py0), slice(rx0 - 4 * x0, rx0 - 4 * x0 + px1 - px0))
        ps = (slice(py0, py1), slice(px0, px1))
        d = np.sqrt(((Lc[ws] - Lu[ws]) ** 2).sum(-1))
        v, beneath = o[ps], bf[rs]
        qc = quant(Lc[ws], np.where(v != 0, v, under[rs]))
        same = d < SAME
        q[key][ps] = np.where(v != 0, np.where(same & (beneath != 0), beneath, qc), np.where(m[ps] & ~same, qc, 0))
    covers = read_covers()

    def ring(key):                     # around the sharp pictures that always lie on it: the ring just outside
        o = pix(*key)                  # keeps the original's colour where that is flat beside what they cover
        H, W = o.shape                 # (no edge beneath them spread into it)
        covered, near = np.zeros((H, W), bool), np.zeros((H, W), bool)
        for r, k, x, y in covers.get(key, ()):
            if bank(r) and k < len(bank(r)):
                h, w = pix(r, k).shape
                covered[max(0, y):y + h, max(0, x):x + w] = True
                near[max(0, y - 1):y + h + 1, max(0, x - 1):x + w + 1] = True
        for py, px in zip(*np.nonzero(near & ~covered)):
            c = o[py, px]
            if 0 < c < 32 and all(o[ny, nx] == c for ny in range(max(0, py - 1), min(H, py + 2))
                                  for nx in range(max(0, px - 1), min(W, px + 2)) if not covered[ny, nx]):
                q[key][4 * py:4 * py + 4, 4 * px:4 * px + 4] = c
    finished, busy = set(), set()

    def finish(key):                   # a sprite's cells final: a frame after its references, a part after
        if key in finished or key in busy:     # what lies beneath it -- each of them final first (a frame
            return                     # may take a part's, a part may lie on a frame) --, then the rings
        busy.add(key)
        if key in refs:
            for r, kb, _, _ in refs[key]:
                finish((r, kb))
            frame(key)
        elif key in chains and key in q and key in win and all(L is not None for L in win[key]):
            root, layers = chains[key][:2]
            for k in ([(root[1], 0), (root[1] - 50, 0)] if root[0] == 'room' else [root[1:]]) + [(r, k) for r, k, _, _ in layers]:
                finish(k)
            part(key)
        if key in q:
            ring(key)
        busy.discard(key)
        finished.add(key)
    for key in sorted(q):
        finish(key)
    head = json.dumps({'model': model, 'colours': 254 if pal_path else 32, 'tool': 1}).encode()
    keys = sorted(q)
    blobs = [rle(q[k].tobytes()) for k in keys]
    off = 8 + 4 + len(head) + 768 + 8 + 20 * len(keys)
    index = bytearray()
    for (r, k), b in zip(keys, blobs):
        src = pix(r, k)
        h, w = src.shape
        index += struct.pack('<HHHHIII', r, k, w, h, off, len(b), fnv(src.tobytes()))
        off += len(b)
    data = MAGIC + struct.pack('<I', len(head)) + head + full.tobytes() + struct.pack('<II', 1 if pal_path else 0, len(keys)) + bytes(index) + b''.join(blobs)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    open(path, 'wb').write(data)
    print('%d sprites -> %s (%.1f MB)' % (len(keys), path, len(data) / 1e6))


def read_set(path):
    d = open(path, 'rb').read()
    if d[:8] != MAGIC:
        sys.exit('%s: not a picture set' % path)
    n, = struct.unpack_from('<I', d, 8)
    head = json.loads(d[12:12 + n])
    p = 12 + n
    pal = np.frombuffer(d[p:p + 768], np.uint8).reshape(256, 3)
    flags, count = struct.unpack_from('<II', d, p + 768)
    p += 776
    out = {}
    for i in range(count):
        r, k, w, h, o, ln, _ = struct.unpack_from('<HHHHIII', d, p + 20 * i)
        out[(r, k)] = np.frombuffer(unrle(d[o:o + ln], 16 * w * h), np.uint8).reshape(4 * h, 4 * w)
    return head, pal, flags, out


def main():
    a = sys.argv[1:]
    if a[:1] == ['export'] and len(a) == 2:
        export(a[1])
    elif a[:1] == ['palette'] and len(a) == 4:
        make_palette(a[1], a[2], a[3])
    elif a[:1] == ['import'] and len(a) >= 4:
        opts = dict(zip(a[4::2], a[5::2]))
        if set(opts) - {'--pal', '--model'} or len(a[4:]) % 2:
            sys.exit(__doc__)
        do_import(a[1], a[2], a[3], opts.get('--pal'), opts.get('--model', ''))
    elif a[:1] == ['parts'] and len(a) >= 3:
        make_parts(a[1], a[2:])
    elif a[:1] == ['list'] and len(a) == 2:
        head, pal, flags, spr = read_set(a[1])
        print(json.dumps(head), 'extra colours' if flags & 1 else '31 colours', len(spr), 'sprites')
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
