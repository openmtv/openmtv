package main

import (
	"image"
	"image/color"
)

// The studios: the table "studios" (content.go) is the 12 studio records the start
// savegame holds (DS:6165, 16 bytes each: +2 size, +3 renter, +6 rent).  Only three are
// studios when a game starts, the stations' (renter 1-3); the other nine numbers belong
// to the doors of public rooms (renter 0: the laundry, the consulates, ...) -- a bomb
// there makes the door a free studio (npc_event_strike 1DA8:2D3B), and only a free one
// (renter 0xFF) is offered by the estate agent (room_estateagent_click 05FF:3AB7).  A pack
// that marks one free from the start gets that door as the bomb leaves it: room 0x36,
// signs "Free studio" / "Free", no owner -- in the start savegame and in the start
// tables (resource 0xD0, game_load_init_data 0054:0A0B), which hold the same bytes.

const (
	saveFloors  = 23544 // the floor table (DS:7C82, 14 x 0x7A) in a savegame (game_save_load 229E:139A)
	saveStudios = 34642 // the studios (DS:6165, 13 x 16) in a savegame
	initFloors  = 4248  // the same in the start tables, resource 0xD0
	initStudios = 8
	initRes     = 0xD0
	floorSize   = 0x7A
	floorsLen   = 14 * floorSize
	studiosLen  = 13 * 0x10

	studiosBlock = -1 // buildTables' key of the studios' bytes (no resource: buildDocs puts them in)

	renterFree   = 0xFF
	roomFree     = 0x36 // a free studio's door (after a bomb)
	signFree     = 0x89 // "Free studio" (text 137)
	signFreeOf   = 0x8A // "Free" (text 138)
	maxRent      = 1000000
	consulateOne = 0x12 // the consulates: the terrorists come from them
	consulateTwo = 0x13
)

// studioDoor: where a studio is in the building -- floor 1-13, door 0-9, the room behind
// it in the original (16-24 a public room, 5 a station's studio), its sign (a text of the
// language's 430), and its marker on the estate agent's plan (planX, planY)
type studioDoor struct {
	Studio int `json:"studio"`
	Floor  int `json:"floor"`
	Door   int `json:"door"`
	Room   int `json:"room"`
	Sign   int `json:"sign"`
	Owner  int `json:"owner"`
	PlanX  int `json:"x"`
	PlanY  int `json:"y"`
}

func publicRoom(room int) bool { return room >= 16 && room <= 24 }
func consulate(room int) bool  { return room == consulateOne || room == consulateTwo }

// studioDoors: the doors of the 12 studios in a floor table (the original's)
func studioDoors(floors []byte) []studioDoor {
	var out []studioDoor
	for f := 1; f < 14 && (f+1)*floorSize <= len(floors); f++ {
		b := f * floorSize
		for d := 0; d < 10; d++ {
			if n := int(floors[b+0x6E+d]); n >= 1 && n <= 12 {
				x, y := planPos(f, int(floors[b+d]))
				out = append(out, studioDoor{n, f, d, int(floors[b+0x1E+d]), int(floors[b+0x28+d]), int(floors[b+0x46+d]), x, y})
			}
		}
	}
	return out
}

// planPos: a door's marker on the estate agent's plan (door_board_pos 1DA8:1CC2): floors
// 13-8 in the left column, 7-1 in the right one; four door places
func planPos(floor, doorX int) (int, int) {
	x, y := map[int]int{0x50: 0x26, 0x74: 0x3D, 0xB4: 0x6A, 0xD8: 0x81}[doorX], (14-floor)*15+9
	if floor <= 7 {
		x, y = x+0xA0, y-0x69
	}
	return x, y
}

// startStudio: a station's studio when a game starts (renter 1-3 in the original)
func startStudio(orig obj) bool { r := intOf(orig, "renter"); return r >= 1 && r <= 3 }

// studioProblems: what the game cannot take of a pack's studios -- size 1-3 (the sets a
// studio holds), a rent of 0..maxRent, a public room either public (0) or free (0xFF) but a
// consulate public, a station's studio its station's; and the three start studios alike
// (no station starts better off)
func studioProblems(docs, original map[string]obj, doors []studioDoor) []msg {
	recs, _, err := records(docs["studios"])
	if err != nil {
		return nil // (buildTables names it)
	}
	orig, _, _ := records(original["studios"])
	room := map[int64]int{}
	for _, d := range doors {
		room[int64(d.Studio)] = d.Room
	}
	var out []msg
	bad := func(id int64, what string, m msg) {
		out = append(out, entryError("studios", id, word{what, studioFieldDE[what]}, m))
	}
	if r, ok := recs[0]; ok && (intOf(r, "size") != 0 || intOf(r, "renter") != 0 || intOf(r, "rent") != 0 || r["unknown"] != nil) {
		out = append(out, newMsg("{entry} #{id}: no such studio (the building has 1-12)", "entry", entryWord("studios"), "id", int64(0)))
	}
	var start []obj
	for id := int64(1); id <= 12; id++ {
		r, o := recs[id], orig[id]
		if r == nil || o == nil {
			continue
		}
		if r["unknown"] != nil && !equal(r["unknown"], o["unknown"]) { // (the running production, a renter waiting: the game's own state)
			bad(id, "unknown", newMsg("a studio takes only size, renter and rent"))
		}
		missing := false
		for _, f := range []string{"size", "renter", "rent"} {
			if _, ok := r[f]; !ok {
				bad(id, f, newMsg("missing"))
				missing = true
			}
		}
		if missing {
			continue
		}
		if s := intOf(r, "size"); s < 1 || s > 3 {
			bad(id, "size", newMsg("{value} is not between {lo} and {hi}", "value", s, "lo", 1, "hi", 3))
		}
		if v := intOf(r, "rent"); v < 0 || v > maxRent {
			bad(id, "rent", newMsg("{value} is not between {lo} and {hi}", "value", v, "lo", 0, "hi", maxRent))
		}
		was, now := intOf(o, "renter"), intOf(r, "renter")
		switch {
		case startStudio(o):
			start = append(start, r)
			if now != was {
				bad(id, "renter", newMsg("a station's start studio stays its station's"))
			}
		case consulate(room[id]):
			if now != 0 {
				bad(id, "renter", newMsg("a consulate stays a public room: the terrorists come from it"))
			}
		case now != 0 && now != renterFree:
			bad(id, "renter", newMsg("{value}: a public room is public (0) or a free studio (255)", "value", now))
		}
	}
	for _, r := range start[min(1, len(start)):] {
		if intOf(r, "size") != intOf(start[0], "size") || intOf(r, "rent") != intOf(start[0], "rent") {
			out = append(out, newMsg("the three start studios differ: they have one size and one rent (no station starts better off)"))
			break
		}
	}
	return out
}

var studioFieldDE = map[string]string{"size": "Größe", "renter": "Start", "rent": "Grundmiete", "unknown": "weitere Bytes"}

// gameProblems: what the build refuses besides the tables' own limits (checkDocs) -- the
// studios, which need the original's building
func gameProblems(g *game, docs map[string]obj) error {
	if err := checkDocs(docs); err != nil {
		return err
	}
	if _, ok := docs["studios"]; !ok {
		return nil
	}
	if bad := studioProblems(docs, g.original, g.doors); len(bad) > 0 {
		return joinedError{joined{"; ", bad}}
	}
	return nil
}

// putStudios: the studios' bytes (buildTables) into a start savegame or the start tables:
// the records at SAT, the floor table at FAT -- the original's (ORIG), with the door of
// every public room that is free from the start turned into a free studio's
func putStudios(data, block, orig []byte, sAt, fAt int, doors []studioDoor) []byte {
	out := append([]byte{}, data...)
	if sAt+studiosLen > len(out) || fAt+floorsLen > len(out) || len(block) != studiosLen || len(orig) != floorsLen {
		return out
	}
	copy(out[sAt:], block)
	fl := out[fAt : fAt+floorsLen]
	copy(fl, orig)
	for _, d := range doors {
		if block[d.Studio*0x10+3] == renterFree && publicRoom(d.Room) {
			b := d.Floor * floorSize
			fl[b+0x1E+d.Door], fl[b+0x28+d.Door], fl[b+0x32+d.Door], fl[b+0x46+d.Door] = roomFree, signFree, signFreeOf, 0
		}
	}
	return out
}

// studioView: a studio as the launcher's overview shows it -- its door, its sign in both
// languages, size, renter and rent as a game with these tables starts, and whether the
// packs change it
type studioView struct {
	studioDoor
	Name    map[string]string `json:"name"`
	Size    int64             `json:"size"`
	Renter  int64             `json:"renter"`
	Rent    int64             `json:"rent"`
	Changed bool              `json:"changed"`
}

// studioViews: the 12 studios of a game with these tables -- nil while they are the
// original's (the overview shows only what packs change)
func studioViews(g *game, docs map[string]obj) []studioView {
	recs, _, err := records(docs["studios"])
	if err != nil {
		return nil
	}
	orig, _, _ := records(g.original["studios"])
	var out []studioView
	changed := false
	for _, d := range g.doors {
		r, o := recs[int64(d.Studio)], orig[int64(d.Studio)]
		if r == nil {
			r = o
		}
		if r == nil {
			continue
		}
		v := studioView{studioDoor: d, Name: map[string]string{}, Size: intOf(r, "size"), Renter: intOf(r, "renter"), Rent: intOf(r, "rent"), Changed: !equal(r, o)}
		for lang, l := range g.texts {
			if d.Sign < len(l) {
				v.Name[lang] = l[d.Sign]
			}
		}
		changed = changed || v.Changed
		out = append(out, v)
	}
	if !changed {
		return nil
	}
	return out
}

// planPNG: the plan (pictures.planPNG) of this game
func (s *server) planPNG() ([]byte, error) {
	save, err := s.pics.res.get(startSave)
	if err != nil || len(save) < saveStudios+studiosLen {
		return nil, newMsg("no such thing")
	}
	return s.pics.planPNG(s.g.floors, save[saveStudios:saveStudios+studiosLen], func() (*gameFont, error) { return readGameFont(s.pics.res) })
}

// ---------------------------------------------------------------- the estate agent's plan

// planPNG: the estate agent's plan of the building (room 62, ui_draw_building_plan
// 26A7:32CA) as a game starts: the view's frame (resource 4), the floors and elevator
// shafts, the floor numbers and a marker per door (resource 0xA0) -- 320 x 120, the
// same pixels as the game's (tools/plantest.py).  A free studio has the marker of a
// public room's (both renter < 1 as a signed byte), so it is one picture for every pack.
func (p *pictures) planPNG(floors, studios []byte, readFont func() (*gameFont, error)) ([]byte, error) {
	p.mu.Lock()
	b, ok := p.pngs["plan"]
	p.mu.Unlock()
	if ok {
		return b, nil
	}
	font, err := readFont() // (outside the lock: it may run mtv-dec --write-data)
	if err != nil {
		return nil, err
	}
	p.mu.Lock()
	defer p.mu.Unlock()
	frame, err := p.bank(4)
	if err != nil {
		return nil, err
	}
	gfx, err := p.bank(0xA0)
	if err != nil {
		return nil, err
	}
	pal, err := p.palette()
	if err != nil {
		return nil, err
	}
	const W, H = 320, 120
	pix := make([]byte, W*H)
	put := func(s sprite, x, y int, opaque bool) {
		for j := 0; j < s.h; j++ {
			for i := 0; i < s.w; i++ {
				c := s.pix[j*s.w+i]
				if (opaque || c != 0) && x+i >= 0 && x+i < W && y+j >= 0 && y+j < H {
					pix[(y+j)*W+x+i] = c
				}
			}
		}
	}
	at := func(bank []sprite, off int) sprite { // a sprite by the game's table offset (8k - 6)
		if k := (off - 2) / 8; k >= 0 && k < len(bank) {
			return bank[k]
		}
		return sprite{}
	}
	tile := func(off, x, y int) { put(at(frame, off), x, y, true) }
	spr := func(off, x, y int) { put(at(gfx, off), x, y, false) }
	text := func(s string, x, y int) { // text_draw_full: a glyph's top 8 rows below Y
		for _, c := range []byte(s) {
			if g := int(font.fmap[c]) - 1; g >= 0 && g < len(font.glyphs) {
				put(font.glyphs[g], x, y, false)
			}
			x += max(int(font.widths[c]), 1) + 1
		}
	}
	number := func(n, x, y int) { // "#_2d": a one-digit number after a digit's width
		if n < 10 {
			x += max(int(font.widths['0']), 1) + 1
		}
		text(itoa(n), x, y+8)
	}
	// room_draw_border 105E:0855: the view's frame
	for x := 8; x <= 304; x += 8 {
		tile(0x52, x, 0)
		tile(0xA, x, 0x72)
	}
	for y := 8; y < 110; y += 6 {
		tile(0x1A, 0, y)
		tile(0x32, 0x138, y)
	}
	for _, t := range [][3]int{{0x4A, 0, 0}, {0x5A, 0x138, 0}, {2, 0, 0x72}, {0x12, 0x138, 0x72},
		{0x22, 0, 4}, {0x2A, 0, 0x6E}, {0x3A, 0x138, 4}, {0x42, 0x138, 0x6E}} {
		tile(t[0], t[1], t[2])
	}
	spr(0x42, 0x19, 7) // the house sign
	for di := 0; di <= 6; di++ {
		y := di*15 + 7
		if di != 0 {
			spr(0x4A, 8, y)
			number(14-di, 0xB, y-5)
			for x := 0x19; x < 152; x += 8 {
				spr(0x52, x, y)
			}
			spr(0x52, 0x90, y)
			spr(0x3A, 0x54, y+2)
		}
		spr(0x4A, 0xA8, y)
		number(7-di, 0xAB, y-5)
		for x := 0xB9; x < 312; x += 8 {
			spr(0x52, x, y)
		}
		spr(0x52, 0x130, y)
		spr(0x3A, 0xF4, y+2)
	}
	for f := 1; f <= 13; f++ {
		b := f * floorSize
		for d := 0; d < 10; d++ {
			if k := floors[b+0x14+d]; k < 1 || k > 3 {
				break
			}
			col, n, owner := 1, int(floors[b+0x6E+d]), int(int8(floors[b+0x46+d]))
			if n == 0 && owner >= 1 {
				col = owner + 4
			} else if n*0x10+0x10 <= len(studios) {
				reserved, renter := int(int8(studios[n*0x10+0xF])), int(int8(studios[n*0x10+3]))
				switch {
				case reserved > 0:
					col = reserved + 0xC
				case renter >= 1:
					col = renter + 1
				case n > 0:
					col = 0xC
				}
			}
			x, y := planPos(f, int(floors[b+d]))
			spr(col*8-6, x, y)
		}
	}
	img := image.NewNRGBA(image.Rect(0, 0, W, H))
	for i, c := range pix {
		rgb := pal[c]
		img.SetNRGBA(i%W, i/W, color.NRGBA{rgb.R, rgb.G, rgb.B, 255})
	}
	b, err = encodePNG(img)
	if err == nil {
		p.pngs["plan"] = b
	}
	return b, err
}

func itoa(n int) string {
	if n < 10 {
		return string(rune('0' + n))
	}
	return itoa(n/10) + string(rune('0'+n%10))
}
