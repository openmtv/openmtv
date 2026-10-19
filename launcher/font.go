package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	mbits "math/bits"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
)

// The game's 8-pixel font as a TrueType font, for the editor (it writes in the
// game's own letters) -- the former Python font tool (pixfont.py)'s, byte for byte.  Glyphs: resource 3
// (118 bitmaps, 8 rows); which character draws which glyph and how far it advances
// come from the game's tables in its data segment: g_font_map (DS:012E, char ->
// glyph, 0 = not in the font) and g_glyph_width (DS:022E): a character advances its
// width + 1 pixel, a space 3 + 1 (text_width_px 0E35:0989).  Characters are code
// page 437, as in the game.  Every run of ink pixels in a row becomes a rectangle
// outline, so the font is crisp at whole multiples of 8 px.

const (
	fontUnit    = 125        // font units per game pixel: 8 rows = 1000 = 1 em
	dgroup      = 0x39D4     // the data segment's paragraph in MTV.EXE's image
	fontCreated = 3849984000 // 2026-01-01 in seconds since 1904 (fixed: the same bytes on every build)
)

// dataSegment: the original's data segment -- from build/MTV_U.EXE (the unpacked
// MTV.EXE), else from the game, which carries it (build/mtv-dec --write-data
// build/mtv_data.bin, written again when the game is newer): no MTV.EXE needed
func dataSegment() ([]byte, error) {
	if d, err := os.ReadFile(filepath.Join(root, "build", "MTV_U.EXE")); err == nil && len(d) > 10 && string(d[:2]) == "MZ" {
		start := int(binary.LittleEndian.Uint16(d[8:]))*16 + dgroup*16
		if start < len(d) {
			return d[start:], nil
		}
	}
	out := filepath.Join(root, "build", "mtv_data.bin")
	bin := program("mtv-dec")
	bst, err := os.Stat(bin)
	if err != nil {
		return nil, newMsg("the game is not built yet (make build/mtv-dec)")
	}
	if st, err := os.Stat(out); err != nil || st.ModTime().Before(bst.ModTime()) {
		c := exec.Command(bin, "--write-data", out)
		hideWindow(c)
		if b, err := c.CombinedOutput(); err != nil {
			return nil, fmt.Errorf("%s --write-data: %v %s", bin, err, bytes.TrimSpace(b))
		}
	}
	return os.ReadFile(out)
}

// gameFont: the glyph bitmaps, the character map and the widths (256 each), the
// baseline row
type gameFont struct {
	glyphs       []sprite
	fmap, widths []byte
	base         int
}

func readGameFont(res resources) (*gameFont, error) {
	data, err := res.get(3)
	if err != nil {
		return nil, err
	}
	glyphs, err := decodeBank(data)
	if err != nil {
		return nil, err
	}
	ds, err := dataSegment()
	if err != nil {
		return nil, err
	}
	if len(ds) < 0x22E+256 {
		return nil, errors.New("the game's data segment is too short")
	}
	f := &gameFont{glyphs: glyphs, fmap: ds[0x12E : 0x12E+256], widths: ds[0x22E : 0x22E+256]}
	a := int(f.fmap['A']) - 1 // the baseline: below the capital A
	if a < 0 || a >= len(glyphs) {
		return nil, errors.New("the game's font has no A")
	}
	g := glyphs[a]
	for y := 0; y < g.h; y++ {
		for x := 0; x < g.w; x++ {
			if g.pix[y*g.w+x] != 0 {
				f.base = y + 1
			}
		}
	}
	return f, nil
}

// metrics: character -> advance in game pixels (absent: not in the font); glyphs:
// character -> sprite of resource 3 -- the editor's measure of a text
func (f *gameFont) metrics() (obj, obj) {
	m, gl := obj{}, obj{}
	for c := 1; c < 256; c++ {
		if f.fmap[c] != 0 {
			ch := string(cp437Dec[c])
			m[ch] = int64(max(int(f.widths[c]), 1) + 1)
			gl[ch] = int64(f.fmap[c]) - 1
		}
	}
	m[" "] = int64(4)
	return m, gl
}

type be struct{ bytes.Buffer }

func (b *be) u16(vs ...int) *be {
	for _, v := range vs {
		b.Write([]byte{byte(v >> 8), byte(v)})
	}
	return b
}

func (b *be) u32(vs ...int64) *be {
	for _, v := range vs {
		b.Write(binary.BigEndian.AppendUint32(nil, uint32(v)))
	}
	return b
}

func (b *be) u64(v int64) *be {
	b.Write(binary.BigEndian.AppendUint64(nil, uint64(v)))
	return b
}

func pad4(n int) int { return (4 - n%4) % 4 }

func ttfChecksum(data []byte) uint32 {
	d := append(append([]byte(nil), data...), make([]byte, pad4(len(data)))...)
	var s uint32
	for i := 0; i < len(d); i += 4 {
		s += binary.BigEndian.Uint32(d[i:])
	}
	return s
}

type px struct{ x, y int }

// glyphData: INK's pixels (y from the top) -> glyf bytes, bbox (nil: none), points,
// contours
func glyphData(ink []px, base int) ([]byte, []int, int, int) {
	if len(ink) == 0 {
		return nil, nil, 0, 0
	}
	rows := map[int][]int{}
	for _, p := range ink {
		rows[p.y] = append(rows[p.y], p.x)
	}
	var ys []int
	for y := range rows {
		ys = append(ys, y)
	}
	sort.Ints(ys)
	type rect struct{ x0, x1, y int }
	var rects []rect
	for _, y := range ys {
		xs := rows[y]
		sort.Ints(xs)
		start, prev := xs[0], xs[0]
		for _, x := range xs[1:] {
			if x == prev+1 {
				prev = x
				continue
			}
			rects = append(rects, rect{start, prev + 1, y})
			start, prev = x, x
		}
		rects = append(rects, rect{start, prev + 1, y})
	}
	var pts [][2]int
	var ends []int
	for _, r := range rects { // clockwise: top left, top right, bottom right, bottom left
		top, bot := (base-r.y)*fontUnit, (base-r.y-1)*fontUnit
		pts = append(pts, [2]int{r.x0 * fontUnit, top}, [2]int{r.x1 * fontUnit, top}, [2]int{r.x1 * fontUnit, bot}, [2]int{r.x0 * fontUnit, bot})
		ends = append(ends, len(pts)-1)
	}
	bb := []int{pts[0][0], pts[0][1], pts[0][0], pts[0][1]}
	for _, p := range pts {
		bb[0], bb[1], bb[2], bb[3] = min(bb[0], p[0]), min(bb[1], p[1]), max(bb[2], p[0]), max(bb[3], p[1])
	}
	var b be
	b.u16(len(ends), bb[0], bb[1], bb[2], bb[3]).u16(ends...).u16(0)
	for range pts {
		b.WriteByte(1) // on curve, 16-bit coordinates
	}
	last := 0
	for _, p := range pts {
		b.u16(p[0] - last)
		last = p[0]
	}
	last = 0
	for _, p := range pts {
		b.u16(p[1] - last)
		last = p[1]
	}
	b.Write(make([]byte, pad4(b.Len())))
	return b.Bytes(), bb, len(pts), len(ends)
}

func nameTable(names map[int]string) []byte {
	var ids []int
	for id := range names {
		ids = append(ids, id)
	}
	sort.Ints(ids)
	var recs, strs be
	for _, id := range ids {
		var s be
		for _, r := range names[id] { // (UTF-16 BE; these are ASCII)
			s.u16(int(r))
		}
		recs.u16(3, 1, 0x409, id, s.Len(), strs.Len())
		strs.Write(s.Bytes())
	}
	var b be
	b.u16(0, len(ids), 6+recs.Len())
	b.Write(recs.Bytes())
	b.Write(strs.Bytes())
	return b.Bytes()
}

func cmapTable(codes map[int]int) []byte {
	var cs []int
	for c := range codes {
		if c < 0xFFFF {
			cs = append(cs, c)
		}
	}
	sort.Ints(cs)
	type seg struct{ s, e, d int }
	var segs []seg
	for _, c := range cs {
		segs = append(segs, seg{c, c, (codes[c] - c) & 0xFFFF})
	}
	segs = append(segs, seg{0xFFFF, 0xFFFF, 1})
	n := len(segs)
	p := 1
	for p*2 <= n {
		p *= 2
	}
	var sub be
	sub.u16(n*2, p*2, mbits.Len(uint(p))-1, n*2-p*2)
	for _, s := range segs {
		sub.u16(s.e)
	}
	sub.u16(0)
	for _, s := range segs {
		sub.u16(s.s)
	}
	for _, s := range segs {
		sub.u16(s.d)
	}
	for range segs {
		sub.u16(0)
	}
	var t be
	t.u16(0, 1, 3, 1).u32(12).u16(4, 6+sub.Len(), 0)
	t.Write(sub.Bytes())
	return t.Bytes()
}

// ttf: the font file (pixfont.build_ttf)
func (f *gameFont) ttf(family string) []byte {
	type glyph struct {
		adv int
		ink []px
	}
	order := []glyph{{4, nil}, {4, nil}} // glyph 0 .notdef, 1 space, then one per mapped character
	codes := map[int]int{0x20: 1}
	for c := 1; c < 256; c++ {
		g := int(f.fmap[c])
		if g == 0 || g > len(f.glyphs) {
			continue
		}
		s := f.glyphs[g-1]
		var ink []px
		for y := 0; y < s.h; y++ {
			for x := 0; x < s.w; x++ {
				if s.pix[y*s.w+x] != 0 {
					ink = append(ink, px{x, y})
				}
			}
		}
		codes[int(cp437Dec[c])] = len(order)
		order = append(order, glyph{max(int(f.widths[c]), 1) + 1, ink})
	}
	var glyf, hmtx be
	loca := []int64{0}
	maxpts, maxcont, advmax := 0, 0, 0
	bb := []int{0, (f.base - 8) * fontUnit, 0, f.base * fontUnit}
	for _, g := range order {
		data, bbox, npts, ncont := glyphData(g.ink, f.base)
		glyf.Write(data)
		loca = append(loca, int64(glyf.Len()))
		lsb := 0
		if bbox != nil {
			lsb = bbox[0]
			bb = []int{min(bb[0], bbox[0]), min(bb[1], bbox[1]), max(bb[2], bbox[2]), max(bb[3], bbox[3])}
		}
		hmtx.u16(g.adv*fontUnit, lsb)
		advmax = max(advmax, g.adv*fontUnit)
		maxpts, maxcont = max(maxpts, npts), max(maxcont, ncont)
	}
	ng := len(order)
	asc, desc := f.base*fontUnit, (f.base-8)*fontUnit
	tabs := map[string][]byte{}
	var head be
	head.u32(0x10000, 0x10000, 0, 0x5F0F3CF5).u16(0x000B, 8*fontUnit).u64(fontCreated).u64(fontCreated)
	head.u16(bb[0], bb[1], bb[2], bb[3], 0, 8, 2, 1, 0)
	tabs["head"] = head.Bytes()
	var hhea be
	hhea.u32(0x10000).u16(asc, desc, 0, advmax, 0, 0, bb[2], 1, 0, 0, 0, 0, 0, 0, 0, ng)
	tabs["hhea"] = hhea.Bytes()
	var maxp be
	maxp.u32(0x10000).u16(ng, maxpts, maxcont, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
	tabs["maxp"] = maxp.Bytes()
	sum := 0
	for _, g := range order[1:] {
		sum += g.adv
	}
	avg := sum * fontUnit / max(1, ng-1)
	lo, hi := 0xFFFF, 0
	for c := range codes {
		lo, hi = min(lo, c), max(hi, c)
	}
	var os2 be
	os2.u16(4, avg, 400, 5, 0)
	os2.u16(5*fontUnit, 4*fontUnit, 0, fontUnit, 5*fontUnit, 4*fontUnit, 0, 3*fontUnit, fontUnit, 3*fontUnit)
	os2.u16(0)
	os2.Write(make([]byte, 10))
	os2.u32(3, 0, 0, 0)
	os2.WriteString("RAIN")
	os2.u16(0xC0, lo, min(hi, 0xFFFF), asc, desc, 0, asc, -desc).u32(1, 0).u16(5*fontUnit, 7*fontUnit, 0, 32, 1)
	tabs["OS/2"] = os2.Bytes()
	tabs["hmtx"] = hmtx.Bytes()
	var lb be
	lb.u32(loca...)
	tabs["loca"] = lb.Bytes()
	tabs["glyf"] = glyf.Bytes()
	tabs["cmap"] = cmapTable(codes)
	ps := ""
	for _, r := range family {
		if r != ' ' {
			ps += string(r)
		}
	}
	tabs["name"] = nameTable(map[int]string{1: family, 2: "Regular", 3: ps + "-Regular-1.0", 4: family + " Regular",
		5: "Version 1.0", 6: ps + "-Regular"})
	var post be
	post.u32(0x30000, 0).u16(-fontUnit, fontUnit).u32(0, 0, 0, 0, 0)
	tabs["post"] = post.Bytes()
	var tags []string
	for t := range tabs {
		tags = append(tags, t)
	}
	sort.Strings(tags)
	n := len(tags)
	p := 1
	for p*2 <= n {
		p *= 2
	}
	var font be
	font.u32(0x10000).u16(n, p*16, mbits.Len(uint(p))-1, n*16-p*16)
	offset, hoff := 12+16*n, 0
	var body bytes.Buffer
	for _, t := range tags {
		d := tabs[t]
		if t == "head" {
			hoff = offset + body.Len()
		}
		font.WriteString(t)
		font.u32(int64(ttfChecksum(d)), int64(offset+body.Len()), int64(len(d)))
		body.Write(d)
		body.Write(make([]byte, pad4(len(d))))
	}
	font.Write(body.Bytes())
	out := font.Bytes()
	binary.BigEndian.PutUint32(out[hoff+8:], 0xB1B0AFBA-ttfChecksum(out))
	return out
}
