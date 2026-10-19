package main

import (
	"bytes"
	"errors"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"sync"
)

// The game's pictures for the launcher page (tools/rsc.py decode_bank, palette;
// the former Python editor (editor.py) sprite_png, room_png): the logo, the room behind the start.

// bits: the sprite banks' bit stream, LSB first
type bits struct {
	d          []byte
	p, cur, nl int
}

func (b *bits) bit() int {
	if b.nl > 0 {
		b.cur >>= 1
		b.nl--
	} else {
		b.cur = int(b.d[b.p])
		b.p++
		b.nl = 7
	}
	return b.cur & 1
}

func (b *bits) byte() int {
	v := 0
	for k := 0; k < 8; k++ {
		v = (b.bit() << 7) | (v >> 1)
	}
	return v
}

type sprite struct {
	w, h int
	pix  []byte
}

// decodeBank: a sprite bank (order-1 context Huffman, tools/rsc.py) -> its sprites
func decodeBank(data []byte) (out []sprite, err error) {
	defer func() {
		if recover() != nil {
			err = errors.New("damaged sprite bank")
		}
	}()
	n := int(data[0])
	if n == 0 {
		return nil, errors.New("empty bank")
	}
	dims := make([][2]int, n)
	total := 0
	for i := range dims {
		dims[i] = [2]int{int(data[1+2*i]), int(data[2+2*i])}
		total += dims[i][0] * dims[i][1]
	}
	pos := 1 + 2*n
	nodes := int(data[pos])<<8 | int(data[pos+1])
	count := int(data[pos+2])<<24 | int(data[pos+3])<<16 | int(data[pos+4])<<8 | int(data[pos+5])
	if count != total {
		return nil, errors.New("pixel count mismatch")
	}
	b := &bits{d: data, p: pos + 6}
	left, right := map[int]int{}, map[int]int{}
	next := 0x101
	tree := func() (int, error) {
		first := true
		var rec func() (int, error)
		rec = func() (int, error) {
			if b.bit() == 1 {
				if first {
					first = false
					return 0x100, nil
				}
				return b.byte(), nil
			}
			id := next
			next++
			if id-0x101 >= nodes {
				return 0, errors.New("tree too large")
			}
			l, err := rec()
			if err != nil {
				return 0, err
			}
			r, err := rec()
			if err != nil {
				return 0, err
			}
			left[id], right[id] = l, r
			return id, nil
		}
		return rec()
	}
	escape, err := tree()
	if err != nil {
		return nil, err
	}
	var ctx [256]int
	for i := range ctx {
		if ctx[i], err = tree(); err != nil {
			return nil, err
		}
	}
	walk := func(si int) int {
		for si > 0x100 {
			if b.bit() == 1 {
				si = right[si]
			} else {
				si = left[si]
			}
		}
		return si
	}
	pix := make([]byte, 0, total)
	si := walk(escape)
	v := si
	if si == 0x100 {
		v = b.byte()
	}
	pix = append(pix, byte(v))
	for len(pix) < total {
		si = walk(ctx[v])
		if si == 0x100 {
			si = walk(escape)
			v = si
			if si == 0x100 {
				v = b.byte()
			}
		} else {
			v = si
		}
		pix = append(pix, byte(v))
	}
	p := 0
	for _, d := range dims {
		out = append(out, sprite{d[0], d[1], pix[p : p+d[0]*d[1]]})
		p += d[0] * d[1]
	}
	return out, nil
}

// palette: resource 1 -- 6-bit RGB
func palette(data []byte) [256]color.RGBA {
	var pal [256]color.RGBA
	n := int(data[0]) | int(data[1])<<8
	for i := 0; i < 256; i++ {
		pal[i] = color.RGBA{0, 0, 0, 255}
		if i < n && 2+3*i+2 < len(data) {
			c := data[2+3*i : 5+3*i]
			pal[i] = color.RGBA{byte(int(c[0]) * 255 / 63), byte(int(c[1]) * 255 / 63), byte(int(c[2]) * 255 / 63), 255}
		}
	}
	return pal
}

// pictures: the PNGs of the game's sprites and rooms, made once
type pictures struct {
	res   resources
	mu    sync.Mutex
	banks map[int][]sprite
	pngs  map[string][]byte
	pal   *[256]color.RGBA
}

func (p *pictures) bank(i int) ([]sprite, error) {
	if s, ok := p.banks[i]; ok {
		return s, nil
	}
	data, err := p.res.get(i)
	if err != nil {
		return nil, err
	}
	s, err := decodeBank(data)
	if err != nil {
		return nil, err
	}
	p.banks[i] = s
	return s, nil
}

func (p *pictures) palette() (*[256]color.RGBA, error) {
	if p.pal == nil {
		data, err := p.res.get(1)
		if err != nil {
			return nil, err
		}
		pal := palette(data)
		p.pal = &pal
	}
	return p.pal, nil
}

func encodePNG(img image.Image) ([]byte, error) {
	var b bytes.Buffer
	err := (&png.Encoder{CompressionLevel: png.BestCompression}).Encode(&b, img)
	return b.Bytes(), err
}

// spritePNG: sprite K of bank BANK; TRANSPARENT: colour 0 shows through
func (p *pictures) spritePNG(bank, k int, transparent bool) ([]byte, error) {
	p.mu.Lock()
	defer p.mu.Unlock()
	key := fmt.Sprintf("s%d/%d/%v", bank, k, transparent)
	if b, ok := p.pngs[key]; ok {
		return b, nil
	}
	sp, err := p.bank(bank)
	if err != nil {
		return nil, err
	}
	if k < 0 || k >= len(sp) {
		return nil, errors.New("no such sprite")
	}
	pal, err := p.palette()
	if err != nil {
		return nil, err
	}
	s := sp[k]
	img := image.NewNRGBA(image.Rect(0, 0, s.w, s.h))
	for y := 0; y < s.h; y++ {
		for x := 0; x < s.w; x++ {
			v := s.pix[y*s.w+x]
			c := pal[v]
			a := byte(255)
			if transparent && v == 0 {
				a = 0
			}
			img.SetNRGBA(x, y, color.NRGBA{c.R, c.G, c.B, a})
		}
	}
	b, err := encodePNG(img)
	if err == nil {
		p.pngs[key] = b
	}
	return b, err
}

// roomPNG: a room background as the game draws it (room_draw_background 0054:1EEC):
// resource n at x 0, n-50 at x 160
func (p *pictures) roomPNG(n int) ([]byte, error) {
	p.mu.Lock()
	defer p.mu.Unlock()
	key := fmt.Sprintf("r%d", n)
	if b, ok := p.pngs[key]; ok {
		return b, nil
	}
	l, err := p.bank(n)
	if err != nil {
		return nil, err
	}
	r, err := p.bank(n - 50)
	if err != nil {
		return nil, err
	}
	pal, err := p.palette()
	if err != nil {
		return nil, err
	}
	if len(l) == 0 || len(r) == 0 {
		return nil, errors.New("no such sprite")
	}
	left, right := l[0], r[0]
	h := max(left.h, right.h)
	img := image.NewNRGBA(image.Rect(0, 0, 160+right.w, h))
	for _, part := range []struct {
		s  sprite
		x0 int
	}{{left, 0}, {right, 160}} {
		for y := 0; y < part.s.h; y++ {
			for x := 0; x < part.s.w; x++ {
				c := pal[part.s.pix[y*part.s.w+x]]
				img.SetNRGBA(part.x0+x, y, color.NRGBA{c.R, c.G, c.B, 255})
			}
		}
	}
	b, err := encodePNG(img)
	if err == nil {
		p.pngs[key] = b
	}
	return b, err
}
