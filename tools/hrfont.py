#!/usr/bin/env python3
"""The 4x font of --filter hr (docs/hochaufloesend.md, section 3b; stage 3).

The game's two fonts -- resource 3 (g_font_2) and resource 152 (the small
one, g_font = 1) -- drawn at 4 times their size: each glyph's 8 x 8 box is a
32 x 32 grid of sub-pixels.  The source is a text file per font,
tools/hrfont/font003.txt and font152.txt, one block per glyph:

    glyph K 'c' WxH        (K = sprite index in the bank, c = a char drawn with it)
    32 rows of 32 characters: '.' clear, '#' colour 31 (the ink), or a colour
    named in the file's header ("colour X NN": char X = palette index NN)

Glyphs not in the file (the #f gauge cells, the #v boxes: runtime/hr_rules.txt)
stay as the original, every pixel 4x4.  Advance widths, wrapping and click
areas stay the game's own: only the shapes are finer.

  hrfont.py draft             the first draft: the original's strokes drawn
                              again with a round pen 3.5 sub-pixels wide (MMPX
                              alone changes next to nothing in this font: it
                              has hardly any diagonals); overwrites the text files
  hrfont.py sheet OUT.png     original | MMPX | the font, every glyph, for review
  hrfont.py pack OUT.c        the C table the game uses (Makefile: build/hr_font.c)
  hrfont.py sample OUT.png    a line of text in all three, as in the game
  hrfont.py ttf NAME FONT [VARIATION|- [INK]]   a trial font from a TrueType
                              file: tools/hrfont/try/NAME/ (HR_FONT=NAME)
"""
import os, sys, struct, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import rsc

GAME = os.environ.get('GAME', os.path.join(ROOT, 'run'))
FONTS = (3, 152)
# sprites that stay nearest (runtime/hr_rules.txt): the #v boxes
KEEP = set(range(98, 103)) | {111}
# glyphs drawn by hand instead of from the original's strokes: centre lines in the original's
# pixel coordinates (pixel centres at whole numbers), drawn with the same pen, in the style of
# 0, 3 and 8 (straight strokes, corners cut at 45 degrees); ('dot', p) is a round dot.  The
# user's choice from the variants of 2026-10-17 (https://claude.ai/artifact/XKzyEzDi334hsc25RKbk8R):
# 4 as the original (45 degrees, a short upright into the bar), 6 with its left side up to a
# short bow, 9 with a flat foot, ? a bow of cut corners -- and its turned twin, glyph 116, the
# same turned round
DRAWN = {
    3: {61: [[(3, 0), (3, 6)], [(3, 0), (0, 3), (0, 4), (3, 4)]],
        63: [[(2.5, 0), (1, 0), (0, 1), (0, 5), (1, 6), (2, 6), (3, 5), (3, 3), (2, 2), (0, 2)]],
        66: [[(0, 3), (0, 1), (1, 0), (2, 0), (3, 1), (3, 5), (2, 6), (0, 6)], [(0, 3), (1, 4), (3, 4)]],
        72: [[(0, 1), (0.5, 0), (1.5, 0), (2, 0.5), (2, 2), (1, 3), (1, 4)], ('dot', (1, 6))],
        116: [[(2, 5), (1.5, 6), (0.5, 6), (0, 5.5), (0, 4), (1, 3), (1, 2)], ('dot', (1, 0))]},
    152: {61: [[(3, 0), (3, 5)], [(3, 0), (2, 0), (0, 2), (0, 3), (3, 3)]],
          63: [[(3, 0.5), (2.5, 0), (1, 0), (0, 1), (0, 4), (1, 5), (2, 5), (3, 4), (3, 3), (2, 2), (0, 2)]],
          66: [[(0, 1), (1, 0), (2, 0), (3, 1), (3, 4), (2, 5), (1, 5), (0, 4.5)], [(0, 1), (1, 2), (3, 2)]],
          72: [[(0, 1), (0.5, 0), (1.5, 0), (2, 0.5), (2, 2), (1, 3)], ('dot', (1, 5))],
          116: [[(2, 4), (1.5, 5), (0.5, 5), (0, 4.5), (0, 3), (1, 2)], ('dot', (1, 0))]},
}
INK = 31
# the #f gauge's cells (chars 1..7, 1 x 8 pixels: frame rows 1 and 5, colour
# rows 2..4, a quarter mark the bottom colour row, a half mark the bottom
# two): their own drawing, the frame 3 sub-pixels (the original's 4), the
# marks 2 wide, the end caps whole (stage 5)
GAUGE = {88: ('cap', None), 89: ('cell', 'r'), 90: ('cell', 'b'),
         93: ('quarter', 'r'), 94: ('quarter', 'b'), 95: ('half', 'r'), 96: ('half', 'b')}
GAUGE_COLOURS = {'r': 19, 'b': 7}


def gauge_art(kind, col):
    """32 x 32 rows of characters for a gauge cell; the pixel is column 0 (sub-columns 0..3)"""
    rows = [['.'] * 32 for _ in range(32)]
    def put(y0, y1, x0, x1, ch):
        for y in range(y0, y1):
            for x in range(x0, x1):
                rows[y][x] = ch
    if kind == 'cap':                      # the bar's ends: the whole pixel, the frame's height
        put(5, 23, 0, 4, '#')
        return [''.join(r) for r in rows]
    put(5, 8, 0, 4, '#')                   # the frame: the inner three quarters of rows 1 and 5
    put(20, 23, 0, 4, '#')
    put(8, 20, 0, 4, col)                  # the colour
    if kind == 'quarter':
        put(16, 20, 1, 3, '#')             # a mark two sub-pixels wide, as tall as the game's
    elif kind == 'half':
        put(12, 20, 1, 3, '#')
    return [''.join(r) for r in rows]


def path_of(res, name=None):
    return os.path.join(TRY, name, 'font%03d.txt' % res) if name else os.path.join(HERE, 'hrfont', 'font%03d.txt' % res)


def game_data():
    idx = rsc.read_index([os.path.join(GAME, 'SOURCE1.RSC'), os.path.join(GAME, 'SOURCE2.RSC')])
    pal = rsc.palette(rsc.raw(idx, 1))
    exe = open(os.path.join(ROOT, 'build', 'MTV_U.EXE'), 'rb').read()
    ds = int.from_bytes(exe[8:10], 'little') * 16 + 0x39D4 * 16
    fmap = exe[ds + 0x12E:ds + 0x12E + 256]
    widths = exe[ds + 0x22E:ds + 0x22E + 256]
    chars = {}                                   # sprite index -> a char drawn with it
    for c in range(32, 256):
        if fmap[c] and fmap[c] - 1 not in chars:
            chars[fmap[c] - 1] = c
    return idx, pal, fmap, widths, chars


def mmpx4(pix, w, h, pal):
    """palette indices (0 = clear) -> 4w x 4h indices by MMPX (build/mmpx4)"""
    tool = os.path.join(ROOT, 'build', 'mmpx4')
    if not os.path.exists(tool):
        subprocess.run(['make', '-s', '-C', ROOT, 'build/mmpx4'], check=True)
    # index in the top byte (kept distinct), the colour below it for MMPX's brightness tests
    code = [(v << 24) | (pal[v][0] << 16) | (pal[v][1] << 8) | pal[v][2] for v in pix]
    data = b'%d %d 0\n' % (w, h) + struct.pack('<%dI' % len(code), *code)
    out = subprocess.run([tool], input=data, capture_output=True, check=True).stdout
    return [v >> 24 for v in struct.unpack('<%dI' % (16 * w * h), out)]


PEN = 3.5      # the draft's stroke width in sub-pixels (the original's: 4; 3.0 until 2026-10-16: too thin)
SS = 8         # supersampling of the draft's drawing


TOL = 0.6      # how far a straightened stroke may leave the original pixels' centres (pixels;
               # 0.55 until 2026-10-17 left the small 2's jog, from 0.65 on the round corners go)
ORTH = {(1, 0), (-1, 0), (0, 1), (0, -1)}


def _strokes(ink):
    """the ink pixels' strokes: edges between orthogonal neighbours, and a
    diagonal step where no orthogonal pixel joins the two"""
    adj = {p: set() for p in ink}
    for (x, y) in ink:
        for q in ((x + 1, y), (x, y + 1)):
            if q in ink:
                adj[(x, y)].add(q); adj[q].add((x, y))
        for dx in (1, -1):
            q = (x + dx, y + 1)
            if q in ink and (x + dx, y) not in ink and (x, y + 1) not in ink:
                adj[(x, y)].add(q); adj[q].add((x, y))
    return adj


def _stair(p, adj):
    """p is a step of a staircase: the stroke moves A, B, A around it (B one
    pixel across A), so its pixels only approximate a slant (the foot of the
    small font's V, the arms of its Y)"""
    n = list(adj[p])
    if len(n) != 2:
        return False
    for o, q in (n, n[::-1]):
        d_in, d = (p[0] - o[0], p[1] - o[1]), (q[0] - p[0], q[1] - p[1])
        nxt = [r for r in adj[q] if r != p]
        if d_in in ORTH and d in ORTH and d_in[0] * d[0] + d_in[1] * d[1] == 0 \
                and len(nxt) == 1 and (nxt[0][0] - q[0], nxt[0][1] - q[1]) == d_in:
            return True                          # (B across A: a straight run is no step)
    return False


def _anchors(ink, adj):
    """where a stroke must keep its point: ends, forks, right-angle (or
    sharper) corners that are no step of a staircase, and every pixel of a
    2x2 block (thick parts)"""
    a = set()
    for p in ink:
        n = list(adj[p])
        if len(n) != 2:
            a.add(p); continue
        (ax, ay), (bx, by) = [(q[0] - p[0], q[1] - p[1]) for q in n]
        if ax * bx + ay * by > 0 or ax * bx + ay * by == 0 and not _stair(p, adj):
            a.add(p)                             # the two strokes meet at 90 degrees or less
        x, y = p
        for dx in (0, -1):
            for dy in (0, -1):
                if all((x + dx + i, y + dy + j) in ink for i in (0, 1) for j in (0, 1)):
                    a.add(p)
    return a


def _tips(adj):
    """fork -> point: a fork whose two arms step in sideways, opposite each
    other, and then turn away from its third stroke, a single pixel across
    them (the small font's V): the arms run on to that pixel, the stub goes.
    (A bar going on sideways with a pixel hanging from it is no point.)"""
    tips = {}
    for f in adj:
        stubs = [t for t in adj[f] if len(adj[t]) == 1]
        if len(adj[f]) != 3 or len(stubs) != 1:
            continue
        t = stubs[0]
        sd = (t[0] - f[0], t[1] - f[1])
        arms = [q for q in adj[f] if q != t]
        if sd in ORTH and all((a[0] - f[0], a[1] - f[1]) in ORTH and (a[0] - f[0]) * sd[0] + (a[1] - f[1]) * sd[1] == 0
                              and (a[0] - sd[0], a[1] - sd[1]) in adj[a] for a in arms):
            tips[f] = t
    return tips


def _dp(pts, tol):
    """Douglas-Peucker: the points that stay when every run within tol
    pixels of a straight line becomes that line"""
    if len(pts) < 3:
        return pts
    (x0, y0), (x1, y1) = pts[0], pts[-1]
    L = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
    ds = [abs((x1 - x0) * (y0 - y) - (x0 - x) * (y1 - y0)) / L for x, y in pts[1:-1]]
    i = max(range(len(ds)), key=ds.__getitem__)
    if ds[i] <= tol:
        return [pts[0], pts[-1]]
    return _dp(pts[:i + 2], tol)[:-1] + _dp(pts[i + 1:], tol)


def _simplify(pts, tol, held=(False, False)):
    """a stroke straightened wherever its pixels only approximate a line
    (the steps of a slanted stroke, the jogs in the 2's diagonal), keeping
    the first and last point of every run at a local extreme of x or y (the
    shoulder of the 2, the sides of an O) and of a straight run leaving an
    end that is a corner or goes on straight through it (held: start, end;
    the side of the s's bowl, the e's left side through its bar); a loop
    keeps its points"""
    if len(pts) < 3 or pts[0] == pts[-1]:
        return pts
    keep = {0, len(pts) - 1}
    for ax in (0, 1):
        i = 0
        while i < len(pts):
            j = i
            while j + 1 < len(pts) and pts[j + 1][ax] == pts[i][ax]:
                j += 1
            if 0 < i and j < len(pts) - 1 and (pts[i - 1][ax] - pts[i][ax]) * (pts[j + 1][ax] - pts[i][ax]) > 0 \
                    or i == 0 and held[0] or j == len(pts) - 1 and held[1]:
                keep.update((i, j))
            i = j + 1
    ks = sorted(keep)
    out = [pts[0]]
    for a, b in zip(ks, ks[1:]):
        out += _dp(pts[a:b + 1], tol)[1:]
    return out


def _paths(ink, adj, anchors):
    """the strokes as polylines from anchor to anchor (a closed loop without
    any anchor starts at its first pixel)"""
    seen, out = set(), []
    def walk(a, b):
        path, prev, cur = [a], a, b
        while True:
            seen.add(frozenset((prev, cur)))
            path.append(cur)
            if cur in anchors or cur == a:
                return path
            nxt = [q for q in adj[cur] if q != prev]
            if not nxt:
                return path
            prev, cur = cur, nxt[0]
    for a in sorted(anchors):
        for b in sorted(adj[a]):
            if frozenset((a, b)) not in seen:
                out.append(walk(a, b))
    for p in sorted(ink):                      # loops with no anchor (an O made of diagonals)
        for b in sorted(adj[p]):
            if frozenset((p, b)) not in seen:
                anchors.add(p)
                out.append(walk(p, b))
    return out


def restroke(b, h, pen=PEN, tol=TOL):
    """an 8 x h glyph (0 clear) -> 32 x 4h, 1 = ink: the original's strokes,
    a staircase read as the slant it stands for, straightened where the
    pixels only approximate a line (within tol pixels), drawn with a round
    pen; a pixel on its own is a round dot 4 sub-pixels across"""
    ink = {(x, y) for y in range(h) for x in range(8) if b[y * 8 + x]}
    adj = _strokes(ink)
    anchors = _anchors(ink, adj)
    tips = _tips(adj)
    strokes = []
    for path in _paths(ink, adj, anchors):
        if tips.get(path[0]) == path[-1] or tips.get(path[-1]) == path[0]:
            continue                           # the stub under a point
        # an end held: a corner, or a fork the stroke goes on straight through -- read on the
        # stroke's own pixels, before its steps go (an end that becomes a point is none)
        held = [e not in tips and (len(adj[e]) == 2 or (2 * e[0] - n[0], 2 * e[1] - n[1]) in adj[e])
                for e, n in ((path[0], path[1]), (path[-1], path[-2]))]
        if path[0] != path[-1]:                # the steps go; a point's arms run on to it straight (the V)
            arm = path[0] in tips or path[-1] in tips
            path = [tips.get(path[0], path[0])] + [p for p in path[1:-1] if p in anchors or not (arm or _stair(p, adj))] \
                + [tips.get(path[-1], path[-1])]
        strokes.append(_simplify(path, tol, held))
    strokes += [('dot', p) for p in ink if not adj[p]]
    return draw(strokes, h, pen)


def draw(strokes, h, pen=PEN):
    """polylines (original pixel coordinates, the centres at whole numbers) and ('dot', p) -> the
    32 x 4h glyph, 1 = ink: the round pen, joints round, a dot 4 sub-pixels across"""
    from PIL import Image, ImageDraw
    im = Image.new('L', (32 * SS, 4 * h * SS), 0)
    d = ImageDraw.Draw(im)
    c = lambda p: ((4 * p[0] + 2) * SS, (4 * p[1] + 2) * SS)
    r = pen * SS / 2
    for s in strokes:
        if s[0] == 'dot':
            cx, cy = c(s[1])
            d.ellipse([cx - 2 * SS, cy - 2 * SS, cx + 2 * SS, cy + 2 * SS], fill=255)
            continue
        for p, q in zip(s, s[1:]):
            d.line([c(p), c(q)], fill=255, width=int(round(pen * SS)))
        for p in s:
            cx, cy = c(p)
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    small = im.resize((32, 4 * h), Image.BOX)
    return [1 if small.getpixel((i % 32, i // 32)) >= 128 else 0 for i in range(32 * 4 * h)]


def boxed(w, h, pix):
    """the glyph in its 8-pixel-wide box (rows padded, as in the game's memory)"""
    return [pix[y * w + x] if x < w else 0 for y in range(h) for x in range(8)]


def draft():
    idx, pal, fmap, widths, chars = game_data()
    os.makedirs(os.path.join(HERE, 'hrfont'), exist_ok=True)
    for res in FONTS:
        glyphs = rsc.decode_bank(rsc.raw(idx, res))
        out = ['# OpenMTV: the 4x font for resource %d (tools/hrfont.py; docs/hochaufloesend.md, 3b).' % res,
               '# Each glyph: its 8-pixel box at 4x, 32 columns; "." clear, "#" colour %d,' % INK,
               '# and the colours below.']
        out += ['colour %s %d' % kv for kv in sorted(GAUGE_COLOURS.items())]
        out += [
               '# First draft: the original strokes, round pen %.1f sub-pixels (hrfont.py draft);' % PEN,
               '# edited by hand since; make builds build/hr_font.c from it.', '']
        for k, (w, h, pix) in enumerate(glyphs):
            if k in KEEP or not w or not h:
                continue
            if k in DRAWN.get(res, {}):
                c = chars.get(k)
                m = draw(DRAWN[res][k], h)
                out.append("glyph %d %s %dx%d   # drawn (hrfont.py DRAWN)" % (k, repr(chr(c)) if c and 32 < c < 127 else '-', w, h))
                out += [''.join('.' if v == 0 else '#' for v in m[y * 32:(y + 1) * 32]) for y in range(4 * h)]
                out.append('')
                continue
            if k in GAUGE and h == 8:
                c = chars.get(k)
                out.append("glyph %d - %dx%d   # the #f gauge (chars 1..7): drawn, not from MMPX" % (k, w, h))
                out += gauge_art(*GAUGE[k])
                out.append('')
                continue
            b = boxed(w, h, pix)
            if not any(b):
                continue                          # an empty glyph (the small font's gauge cells)
            if any(v not in (0, INK) for v in b):
                continue                          # coloured glyphs stay as they are
            m = restroke(b, h)
            c = chars.get(k)
            out.append("glyph %d %s %dx%d" % (k, repr(chr(c)) if c and 32 < c < 127 else '-', w, h))
            for y in range(4 * h):
                out.append(''.join('.' if v == 0 else '#' for v in m[y * 32:(y + 1) * 32]))
            out.append('')
        open(path_of(res), 'w').write('\n'.join(out))
        print('%s: %d glyphs' % (path_of(res), sum(1 for l in out if l.startswith('glyph'))))


# Trial fonts (hrfont.py ttf): the same glyphs from a TrueType font, in
# tools/hrfont/try/NAME/; the game picks one with HR_FONT=NAME
TRY = os.path.join(HERE, 'hrfont', 'try')
SYMBOLS = {91, 97}        # '[' draws the game's left arrow, '*' three lines: the draft's own
LIFT = 4                  # how much higher the bowl of g p q y j ends (sub-pixels: the original's pixel)


def _ink_box(b, w, h):
    ys = [y for y in range(h) for x in range(w) if b[y * 8 + x]]
    return (min(ys), max(ys) + 1) if ys else None


def ttf(name, fontfile, variation=None, thresh=128):
    """tools/hrfont/try/NAME/font*.txt from a TrueType font: each glyph's
    char drawn with it, its x-height, ascender, baseline and descender
    moved onto the original's (piecewise, so the small font's short
    ascenders stay as short), as wide as the original glyph at most
    (narrower ones centred); sub-pixels with at least thresh of 255 ink are
    ink.  The ascenders (b d h k l), not the capitals, reach the box's top:
    where they rise above the capitals, the capitals end a little lower
    instead of the ascenders being cut.  As in the original font, the bowl
    of a lowercase letter with a descender (g p q y j) ends LIFT sub-pixels
    above the baseline, so its descender gets two pixels, not one.  Symbols,
    the gauge and glyphs the font lacks: the draft's (tools/hrfont/font*.txt)"""
    from PIL import Image, ImageDraw, ImageFont
    idx, pal, fmap, widths, chars = game_data()
    E = 400
    f = ImageFont.truetype(fontfile, E)
    if variation:
        f.set_variation_by_name(variation)
    def render(ch):
        im = Image.new('L', (3 * E, 2 * E), 0)
        ImageDraw.Draw(im).text((E // 2, E), ch, font=f, fill=255, anchor='ls')
        return im, im.getbbox()
    capTop, base = render('H')[1][1], E
    xTop, descBot = render('x')[1][1], render('p')[1][3]
    top_ref = min([capTop] + [render(c)[1][1] for c in 'bdhkl'])   # the ascenders' top (or the capitals')
    out_dir = os.path.join(TRY, name)
    os.makedirs(out_dir, exist_ok=True)
    for res in FONTS:
        glyphs = rsc.decode_bank(rsc.raw(idx, res))
        draft_g = load(res)
        def rows_of(c):
            k = fmap[ord(c)] - 1
            w, h, pix = glyphs[k]
            return _ink_box(boxed(w, h, pix), w, h)
        T_cap, T_base = [4 * v for v in rows_of('H')]
        T_x, T_desc = 4 * rows_of('x')[0], 4 * rows_of('p')[1]
        s_main = (T_base - T_x) / (base - xTop)
        out = ['# OpenMTV: a trial 4x font for resource %d (tools/hrfont.py ttf; docs/hochaufloesend.md, 8).' % res,
               '# From %s%s, ink from %d of 255; "." clear, "#" colour %d, and the colours below.'
               % (os.path.basename(fontfile), ' (%s)' % variation if variation else '', thresh, INK)]
        out += ['colour %s %d' % kv for kv in sorted(GAUGE_COLOURS.items())]
        out.append('')
        n_ttf = 0
        for k in sorted(draft_g):
            w, h, px = draft_g[k]
            c = chars.get(k)
            uc = bytes([c]).decode('cp437') if c else None
            m = None
            if uc and k not in SYMBOLS and k not in GAUGE and f.getmask(uc).getbbox():
                im, (sx0, sy0, sx1, sy1) = render(uc)
                tw = min(4 * w, (sx1 - sx0) * s_main)
                tx0 = (4 * w - tw) / 2
                # a lowercase letter with a descender: its bowl a pixel shorter, as in the original
                xb = T_base - LIFT if uc.islower() and sy1 > base + 0.05 * E else T_base
                if uc.isupper() and sy0 < capTop - 0.04 * E:    # an accent over a capital: the whole glyph lower
                    knots = [(sy0, T_cap), (base, xb)]
                elif uc.islower() and sy0 < top_ref - 0.02 * E:  # an accent above the ascenders: squeezed in
                    knots = [(sy0, T_cap), (xTop, T_x), (base, xb)]
                else:
                    top = min(sy0, top_ref)
                    knots = [(top, T_cap - (top_ref - top) * (T_x - T_cap) / (xTop - top_ref)),
                             (top_ref, T_cap), (xTop, T_x), (base, xb)]
                bot = max(sy1, descBot)
                knots += [(descBot, T_desc), (bot, T_desc + (bot - descBot) * (T_desc - xb) / (descBot - base))]
                big = Image.new('L', (32 * SS, 4 * h * SS), 0)
                for (a, ta), (b2, tb) in zip(knots, knots[1:]):
                    ya, yb = int(round(ta * SS)), int(round(tb * SS))
                    if b2 <= a or yb <= ya:
                        continue
                    band = im.crop((sx0, int(a), sx1, int(b2))).resize(
                        (max(1, int(round(tw * SS))), yb - ya), Image.LANCZOS)
                    big.paste(band, (int(round(tx0 * SS)), ya))
                small = big.resize((32, 4 * h), Image.BOX)
                m = [INK if small.getpixel((i % 32, i // 32)) >= thresh else 0 for i in range(32 * 4 * h)]
                n_ttf += 1
            out.append("glyph %d %s %dx%d%s" % (k, repr(uc) if uc and 32 < c < 127 else '-', glyphs[k][0], h,
                                                 '' if m else '   # the draft\'s'))
            inv = {INK: '#', 0: '.'}
            inv.update({v: ch for ch, v in GAUGE_COLOURS.items()})
            g = m or px
            for y in range(4 * h):
                out.append(''.join(inv[v] for v in g[y * 32:(y + 1) * 32]))
            out.append('')
        open(path_of(res, name), 'w').write('\n'.join(out))
        print('%s: %d glyphs from the font' % (path_of(res, name), n_ttf))


def load(res, name=None):
    """sprite index -> (w, h, list of 32*4h palette indices)"""
    colours = {'.': 0, '#': INK}
    g, cur, rows = {}, None, []
    for line in open(path_of(res, name)).read().split('\n') + ['']:
        if line.startswith('colour '):
            _, ch, n = line.split()
            colours[ch] = int(n)
        elif line.startswith('glyph '):
            p = line.split('#')[0].split()
            w, h = map(int, p[-1].split('x'))
            cur, rows = int(p[1]), []
            g[cur] = (w, h, rows)
        elif cur is not None and line:      # (inside a glyph '#' is ink, not a comment)
            rows.append([colours[ch] for ch in line.ljust(32, '.')[:32]])
        elif not line and cur is not None:
            w, h, r = g[cur]
            if len(r) != 4 * h:
                sys.exit('%s: glyph %d has %d rows, not %d' % (path_of(res), cur, len(r), 4 * h))
            g[cur] = (w, h, [v for row in r for v in row])
            cur = None
    return g


def pack(out):
    lines = ['/* generated by tools/hrfont.py pack from tools/hrfont/font*.txt -- do not edit */',
             '#include "hr_font.h"', '']
    entries = []
    for res in FONTS:
        for k, (w, h, px) in sorted(load(res).items()):
            name = 'g%d_%d' % (res, k)
            # 4h rows of 32 sub-pixels, one byte each (palette index, 0 = clear)
            lines.append('static const unsigned char %s[%d] = {%s};' % (name, len(px), ','.join(map(str, px))))
            entries.append('{%d, %d, %d, %s}' % (res, k, h, name))
    lines.append('const HrGlyph hr_font_glyphs[] = {\n    %s\n};' % ',\n    '.join(entries))
    lines.append('const int hr_font_count = %d;' % len(entries))
    open(out, 'w').write('\n'.join(lines) + '\n')


def sheet(out):
    from PIL import Image, ImageDraw
    idx, pal, fmap, widths, chars = game_data()
    S = 3                                         # each sub-pixel 3 screen pixels: a glyph 96 wide
    cols = 8
    for res in FONTS:
        glyphs = rsc.decode_bank(rsc.raw(idx, res))
        font = load(res)
        ks = sorted(font)
        cw, ch = 3 * 32 * S // 2 + 3 * 8, 32 * S // 2 + 16
        rows_n = (len(ks) + cols - 1) // cols
        im = Image.new('RGB', (cols * (3 * 64 + 30), rows_n * (64 + 20) + 10), (255, 255, 255))
        d = ImageDraw.Draw(im)
        for i, k in enumerate(ks):
            w, h, pix = glyphs[k]
            b = boxed(w, h, pix)
            m = mmpx4(b, 8, h, pal)
            x0, y0 = (i % cols) * (3 * 64 + 30) + 5, (i // cols) * (64 + 20) + 5
            c = chars.get(k)
            d.text((x0, y0), '%d %s' % (k, chr(c) if c and 32 < c < 127 else ''), fill=(200, 0, 0))
            for j, (grid, n) in enumerate(((b, 8), (m, 32), (font[k][2], 32))):
                s = 64 // n
                gx = x0 + j * 66
                d.rectangle([gx - 1, y0 + 11, gx + 64, y0 + 12 + 2 * h * 4 * (64 // 32) // 2], outline=(220, 220, 220))
                for y in range(len(grid) // n):
                    for x in range(n):
                        if grid[y * n + x]:
                            d.rectangle([gx + x * s, y0 + 12 + y * s, gx + x * s + s - 1, y0 + 12 + y * s + s - 1], fill=(0, 0, 0))
        base, ext = os.path.splitext(out)
        im.save('%s_%03d%s' % (base, res, ext))
        print('%s_%03d%s' % (base, res, ext))


def sample(out, text='Mad TV: The Quick Brown Fox jumps over 1234567890 lazy dogs!'):
    """one line of text as the game sets it (advance = width + 1, space 3) in
    the original, MMPX and the 4x font, each 4 times its size"""
    from PIL import Image
    idx, pal, fmap, widths, chars = game_data()
    lines = []
    for res in FONTS:
        glyphs = rsc.decode_bank(rsc.raw(idx, res))
        font = load(res)
        x, pos = 0, []
        for ch in text.encode('latin-1'):
            if ch == 32:
                x += 3
                continue
            g = fmap[ch] or fmap[0x2E]
            pos.append((x, g - 1))
            x += widths[ch if fmap[ch] else 0x2E] + 1
        W = x + 8
        for kind in ('orig', 'mmpx', 'font'):
            im = Image.new('RGB', (4 * W, 32), (222, 222, 222))
            px = im.load()
            for gx, k in pos:
                w, h, pix = glyphs[k]
                b = boxed(w, h, pix)
                grid = [v for v in b for _ in (0,)]
                if kind == 'orig':
                    g4 = [b[(y // 4) * 8 + x // 4] for y in range(4 * h) for x in range(32)]
                elif kind == 'mmpx':
                    g4 = mmpx4(b, 8, h, pal)
                else:
                    g4 = font[k][2] if k in font else [b[(y // 4) * 8 + x // 4] for y in range(4 * h) for x in range(32)]
                for y in range(4 * h):
                    for xx in range(32):
                        v = g4[y * 32 + xx]
                        if v and 4 * gx + xx < 4 * W:
                            px[4 * gx + xx, y] = pal[v]
            lines.append(im)
    W = max(i.width for i in lines)
    im = Image.new('RGB', (W, sum(i.height + 6 for i in lines)), (255, 255, 255))
    y = 0
    for i in lines:
        im.paste(i, (0, y)); y += i.height + 6
    im.save(out)


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'draft':
        draft()
    elif cmd == 'pack' and len(sys.argv) == 3:
        pack(sys.argv[2])
    elif cmd == 'sheet' and len(sys.argv) == 3:
        sheet(sys.argv[2])
    elif cmd == 'sample' and len(sys.argv) == 3:
        sample(sys.argv[2])
    elif cmd == 'ttf' and 4 <= len(sys.argv) <= 6:
        ttf(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] != '-' else None,
            int(sys.argv[5]) if len(sys.argv) > 5 else 128)
    else:
        sys.exit(__doc__)
