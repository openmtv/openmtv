package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

// The single player's saved games: the 10 tapes in the safe of the office (room 58),
// as the launcher's page "Play" shows them and starts one (--load-slot N, src/main_loop.c).
// They live in the game files the game plays from (FINDINGS "Savegames"): the names are
// resource 0xD1 of SOURCE1.RSC (10 x 40 bytes, CP437, ended by a 0), slot N is resource
// 0xD1+N (37 605 bytes, then the "MTVX" extension of a pack's game); a save appends a new
// copy at the end of the first of the game's files (as the game opens them: rscFiles) and
// points the index at it -- the newest save is the one furthest into the file (a pack's
// rebuild changes only its tables: the saves keep their places).  Every selection plays from game files of its own -- run/ the original game,
// build/mods/A+B+C the packs A, B and C in this order -- so each has its own safe.

const (
	saveNamesRes = 0xD1
	saveSlots    = 10
	saveNameLen  = 40
	saveSize     = 37605
	// where game_save_load (src/actions.c) puts what the launcher shows: the players'
	// finance records (15 bytes each, the money first), the day, the human's station
	// (g_player), the clock (g_clock_acc: 50 a second since 17:00), the managers' names
	saveMoney   = 25252
	saveDay     = 35508
	savePlayer  = 35510
	saveClock   = 35512
	saveManager = 37485
)

// saveSlot: a tape of the safe -- its name ("" for none), and when there is a save behind
// it, what it holds
type saveSlot struct {
	slot    int
	name    string
	saved   bool // a save behind the name (the index has the slot's resource)
	offset  int  // its place in the file: the newest is the furthest
	day     int
	clock   string // "HH:MM", as the game's clock shows it
	station int    // 1..3: Mad-TV, Fun-TV, Sun-TV
	manager string
	money   int64
}

// safeDir: the folder of the game files a selection plays from (spec "": the original game,
// run/) -- before its first start (not built, run/ not yet filled) the original game files,
// which that start copies
func safeDir(spec string) (string, bool) {
	dir := filepath.Join(root, "run")
	if spec != "" {
		dir = filepath.Join(root, "build", "mods", spec)
	}
	if exists(filepath.Join(dir, "SOURCE1.RSC")) {
		return dir, true
	}
	return gameDir(), false
}

// clockText: the game's clock (game_clock_update, src/system.c): seconds since 17:00 in 16 bits
func clockText(acc uint32) string {
	secs := int32(int16(uint16(acc / 50)))
	h, m := secs/3600, secs/60-secs/3600*60
	hour := h + 17
	if hour >= 24 {
		hour -= 24
	}
	return fmt.Sprintf("%02d:%02d", hour, m)
}

func cString(b []byte) string {
	if i := strings.IndexByte(string(b), 0); i >= 0 {
		b = b[:i]
	}
	return cp437Decode(b)
}

// rscFiles: the game's resource files of DIR as the game opens them (rtl_res_open_all,
// src/rtl_io.c): every *.RSC in the directory's own order -- readdir's, not sorted; a later
// file's resource overrides an earlier one's, and a save goes into the first file (the game
// appends to handle 0).  Only the headers are read here, and what is asked for later.
type rscFiles struct {
	files []*os.File
	sizes []int64
	idx   []map[int][2]int64 // resource -> (offset, size), by file
}

func openRscFiles(dir string) (*rscFiles, error) {
	d, err := os.Open(dir)
	if err != nil {
		return nil, err
	}
	names, err := d.Readdirnames(-1)
	d.Close()
	if err != nil {
		return nil, err
	}
	r := &rscFiles{}
	for _, n := range names {
		if len(n) > 12 || n[0] == '.' || !strings.EqualFold(filepath.Ext(n), ".rsc") {
			continue
		}
		f, err := os.Open(filepath.Join(dir, n))
		if err != nil {
			continue
		}
		st, err := f.Stat()
		hdr := make([]byte, 600*6)
		if err != nil || !st.Mode().IsRegular() || readAt(f, hdr, 0) != nil {
			f.Close()
			continue
		}
		idx := map[int][2]int64{}
		for i := 0; i < 600; i++ {
			if off := int64(binary.LittleEndian.Uint32(hdr[6*i:])); off != 0 {
				idx[i] = [2]int64{off, int64(binary.LittleEndian.Uint16(hdr[6*i+4:]))}
			}
		}
		r.files, r.sizes, r.idx = append(r.files, f), append(r.sizes, st.Size()), append(r.idx, idx)
	}
	if len(r.files) == 0 {
		return nil, fmt.Errorf("%s: no game files (*.RSC)", dir)
	}
	return r, nil
}

func (r *rscFiles) close() {
	for _, f := range r.files {
		f.Close()
	}
}

// find: resource ID -- its file, offset and size (the last file that has it)
func (r *rscFiles) find(id int) (int, int64, int64, bool) {
	for n := len(r.files) - 1; n >= 0; n-- {
		if e, ok := r.idx[n][id]; ok {
			return n, e[0], e[1], true
		}
	}
	return 0, 0, 0, false
}

func readAt(f *os.File, b []byte, off int64) error {
	_, err := f.ReadAt(b, off)
	return err
}

// readSafe: the 10 slots of the game files in DIR
func readSafe(dir string) ([]saveSlot, error) {
	r, err := openRscFiles(dir)
	if err != nil {
		return nil, err
	}
	defer r.close()
	slots := make([]saveSlot, saveSlots)
	names := make([]byte, saveSlots*saveNameLen)
	if n, off, size, ok := r.find(saveNamesRes); ok {
		if size < int64(len(names)) {
			names = names[:size]
		}
		if readAt(r.files[n], names, off) != nil {
			names = nil
		}
	}
	for i := range slots {
		s := &slots[i]
		s.slot = i + 1
		s.name = cString(pySlice(names, i*saveNameLen, (i+1)*saveNameLen))
		n, off, size, ok := r.find(saveNamesRes + s.slot)
		if s.name == "" || !ok || size < saveSize || off+saveSize > r.sizes[n] {
			continue // (no name: the game loads nothing; a name without its save: only the name)
		}
		f, b := r.files[n], make([]byte, 8)
		if readAt(f, b, off+saveDay) != nil {
			continue
		}
		s.saved, s.offset = true, int(off)
		s.day = int(binary.LittleEndian.Uint16(b[0:]))
		s.station = int(binary.LittleEndian.Uint16(b[2:]))
		s.clock = clockText(binary.LittleEndian.Uint32(b[4:]))
		if s.station >= 1 && s.station <= 3 {
			m, mgr := make([]byte, 4), make([]byte, saveNameLen)
			if readAt(f, m, off+saveMoney+int64(s.station-1)*15) == nil {
				s.money = int64(int32(binary.LittleEndian.Uint32(m)))
			}
			if readAt(f, mgr, off+saveManager+int64(s.station-1)*saveNameLen) == nil {
				s.manager = strings.TrimSpace(cString(mgr))
			}
		}
	}
	return slots, nil
}

// newestSave: the named slot saved last (0: none) -- the original game's own save in slot 1,
// whose name is a blank (its files have it from the start), is a tape like the others but
// never the one picked first
func newestSave(slots []saveSlot) int {
	best := 0
	for _, s := range slots {
		if s.saved && strings.TrimSpace(s.name) != "" && (best == 0 || s.offset > slots[best-1].offset) {
			best = s.slot
		}
	}
	return best
}

// safeDoc: a selection's safe for the page -- its slots, the newest, and the other
// selections that have saves (the safes of build/mods/, and run/'s)
func safeDoc(spec string) (obj, error) {
	if spec != "" {
		for _, n := range strings.Split(spec, packSep) { // (a name, no path: the folder below build/mods)
			if _, err := modDir(n); err != nil {
				return nil, err
			}
		}
	}
	dir, built := safeDir(spec)
	slots, err := readSafe(dir)
	if err != nil && built {
		return nil, err
	}
	if slots == nil {
		slots = make([]saveSlot, saveSlots)
		for i := range slots {
			slots[i].slot = i + 1
		}
	}
	out := []any{}
	for _, s := range slots {
		o := obj{"slot": s.slot, "name": s.name, "saved": s.saved}
		if s.saved {
			o["day"], o["clock"], o["station"], o["manager"], o["money"] = s.day, s.clock, s.station, s.manager, s.money
		}
		out = append(out, o)
	}
	return obj{"spec": spec, "dir": rel(dir), "built": built, "slots": out, "newest": newestSave(slots), "safes": otherSafes()}, nil
}

// otherSafes: every selection with saved games of its own (named ones; the original's blank
// one in every safe does not count): its spec, how many, the newest's name
func otherSafes() []any {
	type safe struct {
		spec   string
		count  int
		newest string
		at     int64
	}
	var all []safe
	look := func(spec, dir string) {
		st, err := os.Stat(filepath.Join(dir, "SOURCE1.RSC"))
		if err != nil {
			return
		}
		slots, err := readSafe(dir)
		if err != nil {
			return
		}
		n := 0
		for _, s := range slots {
			if s.saved && strings.TrimSpace(s.name) != "" {
				n++
			}
		}
		if n == 0 {
			return
		}
		all = append(all, safe{spec, n, slots[newestSave(slots)-1].name, st.ModTime().Unix()})
	}
	look("", filepath.Join(root, "run"))
	if es, err := os.ReadDir(filepath.Join(root, "build", "mods")); err == nil {
		for _, e := range es {
			if e.IsDir() {
				look(e.Name(), filepath.Join(root, "build", "mods", e.Name()))
			}
		}
	}
	sort.SliceStable(all, func(i, j int) bool { return all[i].at > all[j].at }) // (the last played first)
	out := []any{}
	for _, s := range all {
		out = append(out, obj{"spec": s.spec, "count": s.count, "newest": s.newest})
	}
	return out
}
