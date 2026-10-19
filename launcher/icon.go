package main

import (
	"bytes"
	"encoding/binary"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"math"
	"os"
	"path/filepath"
)

// The launcher's icon: OpenMTV's own wooden television (iconPicture, drawn here --
// never the original's art: it goes into the players' packages) with the name's
// yellow MTV box on its screen, given rabbit ears and feet (nearly square: it fills
// the icon), on a dark tile.  The Mac's follow Apple's template (a rounded square
// with a margin around it), the others fill their square.  The window takes it at
// run time (Linux, the Mac's "About"); the builds of Windows and the Mac take it as
// files: openmtv icons DIR writes icon.png (256), icon.ico (Windows, PNG entries
// 16..256) and icon.icns (the Mac, 16..1024).

func init() { commands["icons"] = iconsCommand }

// iconsCommand: icons DIR
func iconsCommand(args []string) int {
	if len(args) != 1 || args[0] == "" || args[0][0] == '-' {
		warn("openmtv icons DIR\n")
		return 2
	}
	if err := writeIcons(nil, args[0]); err != nil {
		warn("icons: %v\n", err)
		return 1
	}
	say("%s\n", args[0])
	return 0
}

func writeIcons(p *pictures, out string) error {
	set, err := iconSet(p)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(out, 0o777); err != nil {
		return err
	}
	pngOf := func(n int, mac bool) []byte {
		var b bytes.Buffer
		png.Encode(&b, iconTile(set, n, mac))
		return b.Bytes()
	}
	if err := os.WriteFile(filepath.Join(out, "icon.png"), pngOf(256, false), 0o666); err != nil {
		return err
	}
	// .ico: the directory, then the pictures as PNG (Windows Vista on)
	sizes := []int{16, 24, 32, 48, 64, 128, 256}
	var ico, data bytes.Buffer
	binary.Write(&ico, binary.LittleEndian, [3]uint16{0, 1, uint16(len(sizes))})
	for _, n := range sizes {
		b := pngOf(n, false)
		wh := byte(n)
		if n == 256 {
			wh = 0
		}
		ico.Write([]byte{wh, wh, 0, 0})
		binary.Write(&ico, binary.LittleEndian, [2]uint16{1, 32})
		binary.Write(&ico, binary.LittleEndian, [2]uint32{uint32(len(b)), uint32(6 + 16*len(sizes) + data.Len())})
		data.Write(b)
	}
	ico.Write(data.Bytes())
	if err := os.WriteFile(filepath.Join(out, "icon.ico"), ico.Bytes(), 0o666); err != nil {
		return err
	}
	// .icns: 'icns', its length, then type + length + PNG for each size
	var icns bytes.Buffer
	for _, e := range []struct {
		kind string
		n    int
	}{{"icp4", 16}, {"icp5", 32}, {"ic07", 128}, {"ic08", 256}, {"ic09", 512}, {"ic10", 1024},
		{"ic11", 32}, {"ic12", 64}, {"ic13", 256}, {"ic14", 512}} {
		b := pngOf(e.n, true)
		icns.WriteString(e.kind)
		binary.Write(&icns, binary.BigEndian, uint32(8+len(b)))
		icns.Write(b)
	}
	head := []byte("icns\x00\x00\x00\x00")
	binary.BigEndian.PutUint32(head[4:], uint32(8+icns.Len()))
	return os.WriteFile(filepath.Join(out, "icon.icns"), append(head, icns.Bytes()...), 0o666)
}

// iconPNG: the icon as PNG of N pixels (the window's; MAC: in Apple's template)
func iconPNG(p *pictures, n int, mac bool) []byte {
	set, err := iconSet(p)
	if err != nil {
		return nil
	}
	var b bytes.Buffer
	png.Encode(&b, iconTile(set, n, mac))
	return b.Bytes()
}

// iconPicture: OpenMTV's own television (120 x 78: 40 x 26 pixels, three times) -- a
// wooden set like the game's, its screen showing the name's yellow MTV box and a red
// ON AIR light, the knobs and the speaker at its side.  Drawn here, not taken from
// the original's pictures: the icon goes into the players' packages (P is unused).
func iconPicture(p *pictures) (*image.NRGBA, error) {
	const w, h = 40, 26
	im := image.NewNRGBA(image.Rect(0, 0, w, h))
	fill := func(x0, y0, x1, y1 int, c color.NRGBA) { // (x1, y1 inclusive)
		for y := y0; y <= y1; y++ {
			for x := x0; x <= x1; x++ {
				im.SetNRGBA(x, y, c)
			}
		}
	}
	rounded := func(x0, y0, x1, y1 int, c color.NRGBA) { // a rectangle without its four corner pixels
		fill(x0+1, y0, x1-1, y1, c)
		fill(x0, y0+1, x1, y1-1, c)
	}
	outline, wood, light, grain := color.NRGBA{58, 26, 10, 255}, color.NRGBA{122, 59, 28, 255},
		color.NRGBA{165, 88, 44, 255}, color.NRGBA{104, 48, 22, 255}
	rounded(0, 0, w-1, h-1, outline)
	rounded(1, 1, w-2, h-2, wood)
	fill(2, 1, w-3, 1, light) // the top's light
	for _, y := range []int{6, 13, 20} {
		fill(30, y, 37, y, grain)
	}
	// the screen: a dark glass in a black frame, a little glow at its edge
	rounded(2, 2, 29, 23, color.NRGBA{20, 20, 22, 255})
	rounded(3, 3, 28, 22, color.NRGBA{30, 44, 54, 255})
	rounded(4, 4, 27, 21, color.NRGBA{14, 18, 24, 255})
	// the name's box: MTV in yellow, dark letters
	yellow, ink := color.NRGBA{245, 217, 10, 255}, color.NRGBA{20, 19, 6, 255}
	fill(5, 9, 26, 17, yellow)
	glyphs := map[byte][5]string{
		'M': {"10001", "11011", "10101", "10001", "10001"},
		'T': {"11111", "00100", "00100", "00100", "00100"},
		'V': {"10001", "10001", "10001", "01010", "00100"},
	}
	for i, ch := range []byte("MTV") {
		for y, row := range glyphs[ch] {
			for x := range row {
				if row[x] == '1' {
					im.SetNRGBA(7+i*6+x, 11+y, ink)
				}
			}
		}
	}
	fill(6, 5, 7, 6, color.NRGBA{222, 36, 36, 255}) // ON AIR
	// the side: two knobs and the speaker
	for _, ky := range []int{3, 8} {
		rounded(31, ky, 36, ky+3, color.NRGBA{40, 40, 44, 255})
		fill(32, ky+1, 35, ky+2, color.NRGBA{170, 170, 178, 255})
		fill(33, ky+1, 34, ky+1, color.NRGBA{220, 220, 228, 255})
	}
	for y := 14; y <= 22; y += 2 {
		fill(31, y, 36, y, color.NRGBA{58, 26, 10, 255})
	}
	return scaleUp(im, 3), nil
}

// iconSet: the television with rabbit ears above and feet below, in the game's
// pixels (120 x 114)
func iconSet(p *pictures) (*image.NRGBA, error) {
	tv, err := iconPicture(p)
	if err != nil {
		return nil, err
	}
	const ears, feet = 30, 7
	w, h := tv.Bounds().Dx(), tv.Bounds().Dy()
	set := image.NewNRGBA(image.Rect(0, 0, w, ears+h+feet))
	cx := w / 2
	dark, metal, tip := color.NRGBA{60, 60, 66, 255}, color.NRGBA{170, 170, 178, 255}, color.NRGBA{220, 220, 228, 255}
	for _, end := range []image.Point{{cx - 28, 3}, {cx + 30, 1}} {
		line(set, cx, ears-4, end.X, end.Y, 3, dark)
		line(set, cx, ears-4, end.X, end.Y, 1, metal)
		disc(set, end.X, end.Y, 2.5, dark)
		disc(set, end.X, end.Y, 1.5, tip)
	}
	for y := ears - 8; y < ears; y++ { // the ears' foot: a dome on the set
		for x := cx - 11; x <= cx+11; x++ {
			dx, dy := float64(x-cx)/11.5, float64(y-ears)/8.5
			if r := dx*dx + dy*dy; r <= 1 {
				c := color.NRGBA{40, 40, 44, 255}
				if r > 0.7 {
					c = color.NRGBA{20, 20, 22, 255}
				}
				set.SetNRGBA(x, y, c)
			}
		}
	}
	draw.Draw(set, image.Rect(0, ears, w, ears+h), tv, image.Point{}, draw.Over)
	for _, lx := range []int{14, w - 20} { // the feet
		for k := 0; k < feet; k++ {
			x0, x1 := lx+k*2/feet, lx+6-k*2/feet
			for x := x0; x <= x1; x++ {
				c := color.NRGBA{90, 30, 20, 255}
				if x == x0 || x == x1 || k == feet-1 {
					c = color.NRGBA{50, 15, 10, 255}
				}
				set.SetNRGBA(x, ears+h+k, c)
			}
		}
	}
	return set, nil
}

// line: from (X0, Y0) to (X1, Y1), WIDTH pixels thick
func line(im *image.NRGBA, x0, y0, x1, y1, width int, c color.NRGBA) {
	dx, dy := abs(x1-x0), -abs(y1-y0)
	sx, sy := 1, 1
	if x0 > x1 {
		sx = -1
	}
	if y0 > y1 {
		sy = -1
	}
	for e := dx + dy; ; {
		for j := -(width / 2); j <= width/2; j++ {
			for i := -(width / 2); i <= width/2; i++ {
				im.SetNRGBA(x0+i, y0+j, c)
			}
		}
		if x0 == x1 && y0 == y1 {
			return
		}
		if e2 := 2 * e; e2 >= dy {
			e, x0 = e+dy, x0+sx
		} else {
			e, y0 = e+dx, y0+sy
		}
	}
}

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

// disc: a filled circle of radius R around (X, Y)
func disc(im *image.NRGBA, x, y int, r float64, c color.NRGBA) {
	n := int(r) + 1
	for j := -n; j <= n; j++ {
		for i := -n; i <= n; i++ {
			if float64(i*i+j*j) <= r*r {
				im.SetNRGBA(x+i, y+j, c)
			}
		}
	}
}

// iconTile: the set on its dark tile, N pixels square.  The Mac's: Apple's
// template (the tile 824 of 1024 with rounded corners, the rest clear); the
// others': the tile fills the square, the set as large as it goes
func iconTile(set *image.NRGBA, n int, mac bool) *image.NRGBA {
	margin, fill, round := 0.0, 1.0, 0.18
	switch {
	case mac:
		margin, fill, round = 100.0/1024, 0.88, 0.225
	case n >= 256:
		margin, fill = 6.0/256, 0.92
	case n >= 128:
		margin, fill = 3.0/128, 0.94
	case n >= 64:
		margin, fill = 2.0/64, 0.95
	case n >= 48:
		margin, fill = 1.0/48, 0.96
	case n >= 32:
		fill = 0.98
	}
	out := image.NewNRGBA(image.Rect(0, 0, n, n))
	m := margin * float64(n)
	body := float64(n) - 2*m
	rad := round * body
	for y := 0; y < n; y++ {
		for x := 0; x < n; x++ {
			cover := 0 // (4 x 4 samples: the rounded edge smooth)
			for sy := 0; sy < 4; sy++ {
				for sx := 0; sx < 4; sx++ {
					px, py := float64(x)+(float64(sx)+0.5)/4, float64(y)+(float64(sy)+0.5)/4
					qx := max(m+rad-px, px-(float64(n)-m-rad), 0)
					qy := max(m+rad-py, py-(float64(n)-m-rad), 0)
					if px >= m && px <= float64(n)-m && py >= m && py <= float64(n)-m && qx*qx+qy*qy <= rad*rad {
						cover++
					}
				}
			}
			if cover == 0 {
				continue
			}
			// (a glow, brighter a little above the middle)
			dx, dy := (float64(x)-float64(n)/2)/(body/2), (float64(y)-float64(n)*0.42)/(body/2)
			d := min(1, math.Sqrt(dx*dx+dy*dy))
			out.SetNRGBA(x, y, color.NRGBA{uint8(34 - 20*d), uint8(30 - 18*d), uint8(48 - 26*d), uint8(255 * cover / 16)})
		}
	}
	sw, sh := set.Bounds().Dx(), set.Bounds().Dy()
	k := body * fill / float64(max(sw, sh))
	var pic *image.NRGBA
	if k >= 1 && k-math.Floor(k) < 0.15 { // whole pixels
		pic = scaleUp(set, int(k))
	} else { // up in whole pixels, then down by averaging: as sharp as it goes
		pic = shrink(scaleUp(set, int(math.Ceil(k))), max(1, int(math.Round(float64(sw)*k))), max(1, int(math.Round(float64(sh)*k))))
	}
	at := image.Point{(n - pic.Bounds().Dx()) / 2, (n - pic.Bounds().Dy()) / 2}
	draw.Draw(out, pic.Bounds().Add(at), pic, image.Point{}, draw.Over)
	return out
}

// scaleUp: every pixel K x K
func scaleUp(im *image.NRGBA, k int) *image.NRGBA {
	w, h := im.Bounds().Dx(), im.Bounds().Dy()
	out := image.NewNRGBA(image.Rect(0, 0, w*k, h*k))
	for y := 0; y < h*k; y++ {
		for x := 0; x < w*k; x++ {
			out.SetNRGBA(x, y, im.NRGBAAt(x/k, y/k))
		}
	}
	return out
}

// shrink: to W x H by averaging what each pixel covers (in part too; alpha weighted)
func shrink(im *image.NRGBA, w, h int) *image.NRGBA {
	sw, sh := im.Bounds().Dx(), im.Bounds().Dy()
	fx, fy := float64(sw)/float64(w), float64(sh)/float64(h)
	out := image.NewNRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		y0, y1 := float64(y)*fy, float64(y+1)*fy
		for x := 0; x < w; x++ {
			x0, x1 := float64(x)*fx, float64(x+1)*fx
			var r, g, b, a, area float64
			for sy := int(y0); sy < int(math.Ceil(y1)) && sy < sh; sy++ {
				wy := math.Min(y1, float64(sy+1)) - math.Max(y0, float64(sy))
				for sx := int(x0); sx < int(math.Ceil(x1)) && sx < sw; sx++ {
					wgt := wy * (math.Min(x1, float64(sx+1)) - math.Max(x0, float64(sx)))
					c := im.NRGBAAt(sx, sy)
					al := float64(c.A) * wgt
					r, g, b, a, area = r+float64(c.R)*al, g+float64(c.G)*al, b+float64(c.B)*al, a+al, area+wgt
				}
			}
			if a > 0 {
				out.SetNRGBA(x, y, color.NRGBA{uint8(r/a + 0.5), uint8(g/a + 0.5), uint8(b/a + 0.5), uint8(a/area + 0.5)})
			}
		}
	}
	return out
}
