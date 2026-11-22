package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
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
	rules   map[string]string // the rules it was saved with (src/saverules.c), nil: none on the tape
}

// safeFolder: the folder of the game files a selection plays from (spec "": the original
// game, run/), whether there yet or not
func safeFolder(spec string) string {
	if spec == "" {
		return filepath.Join(root, "run")
	}
	return filepath.Join(root, "build", "mods", spec)
}

// safeDir: safeFolder -- before its first start (not built, run/ not yet filled) the original
// game files, which that start copies
func safeDir(spec string) (string, bool) {
	dir := safeFolder(spec)
	if exists(filepath.Join(dir, "SOURCE1.RSC")) {
		return dir, true
	}
	return gameDir(), false
}

// checkSpec: a selection names folders below build/mods -- packs' names, no path
func checkSpec(spec string) error {
	if spec == "" {
		return nil
	}
	for _, n := range strings.Split(spec, packSep) {
		if _, err := modDir(n); err != nil {
			return err
		}
	}
	return nil
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
// src/rtl_io.c): every *.RSC sorted by name as DOS lists them (runtime/dos.c find_list) -- a
// later file's resource overrides an earlier one's, and a save goes into the first file (the
// game appends to handle 0).  Only the headers are read here, and what is asked for later.
type rscFiles struct {
	names []string // as the directory has them
	files []*os.File
	sizes []int64
	idx   []map[int][2]int64 // resource -> (offset, size), by file
}

// rscNames: the names of the resource files of DIR (a DOS name: "*.rsc", at most 8.3), sorted
// by name without case, as the game sees them since 2026-11-03 (runtime/dos.c find_list; the
// directory's own order put SOURCE2.RSC first on the iPhone, and its saves under SOURCE1.RSC's)
func rscNames(dir string) ([]string, error) {
	d, err := os.Open(dir)
	if err != nil {
		return nil, err
	}
	names, err := d.Readdirnames(-1)
	d.Close()
	if err != nil {
		return nil, err
	}
	out := names[:0]
	for _, n := range names {
		if len(n) <= 12 && n[0] != '.' && strings.EqualFold(filepath.Ext(n), ".rsc") {
			out = append(out, n)
		}
	}
	sort.Slice(out, func(i, j int) bool { return strings.ToLower(out[i]) < strings.ToLower(out[j]) })
	return out, nil
}

func openRscFiles(dir string) (*rscFiles, error) {
	return openRscFilesMode(dir, os.O_RDONLY)
}

// openRscFilesMode: the same, opened with FLAG (os.O_RDWR: to write into them)
func openRscFilesMode(dir string, flag int) (*rscFiles, error) {
	names, err := rscNames(dir)
	if err != nil {
		return nil, err
	}
	r := &rscFiles{}
	for _, n := range names {
		f, err := os.OpenFile(filepath.Join(dir, n), flag, 0)
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
		r.names, r.files, r.sizes, r.idx = append(r.names, n), append(r.files, f), append(r.sizes, st.Size()), append(r.idx, idx)
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
		if ext := make([]byte, size-saveSize); size > saveSize && off+size <= r.sizes[n] && readAt(f, ext, off+saveSize) == nil {
			s.rules = saveRules(ext)
		}
	}
	return slots, nil
}

// saveRules: the line of the game's rules in a savegame's extension (after "MTVX" and its
// counts the data of each, the eighth count the line's length: src/saverules.c, the game's
// savegame_extras) as its pairs -- nil for none (a tape from before, or a game started
// without rules)
func saveRules(x []byte) map[string]string {
	if len(x) < 6 || string(x[:4]) != "MTVX" {
		return nil
	}
	n := int(binary.LittleEndian.Uint16(x[4:]))
	if n < 8 || len(x) < 6+2*n {
		return nil
	}
	c := make([]int, n)
	for k := range c {
		c[k] = int(binary.LittleEndian.Uint16(x[6+2*k:]))
	}
	at := 6 + 2*n + max(c[0]-200, 0)*0x12 + max(c[1]-50, 0)*0x29 + max(c[2]-50, 0) + c[3] +
		max(c[4]-10, 0)*7 + max(c[5]-10, 0)*8 + max(c[6]-10, 0)*9
	if c[7] == 0 || at+c[7] > len(x) {
		return nil
	}
	out := map[string]string{}
	for _, kv := range strings.FieldsFunc(string(x[at:at+c[7]]), func(r rune) bool { return r == ' ' }) { // (at spaces only, as the game splits it)
		if k, v, ok := strings.Cut(kv, "="); ok {
			out[k] = v
		}
	}
	return out
}

// tapeRules: a savegame's rules as the settings file names them ([game] classic, fixes,
// fourth_speed, ...: what the page compares with the settings) -- a rule the line lacks, or
// one out of its range, the original's, as the game takes it (src/saverules.c)
func tapeRules(r map[string]string) obj {
	num := func(k string, def, lo, hi int64) int64 {
		if v, err := strconv.ParseInt(r[k], 10, 64); err == nil && v >= lo && v <= hi {
			return v
		}
		return def
	}
	yes := func(k string) string {
		if num(k, 0, 0, 1) == 1 {
			return "yes"
		}
		return "no"
	}
	o := obj{"classic": "yes"}
	if f, ok := r["fixes"]; ok && f != "classic" && f != "" {
		o["classic"], o["fixes"] = "no", f
	}
	o["fourth_speed"] = yes("speed4")
	o["ai_improve"] = aiImprove[num("aiimp", 0, 0, 2)]
	if c := num("credit", -1, -1, 100000000); c == 0 { // the original's credit system chosen (fix credit-repay off)
		o["credit_line"] = "original"
	} else if c < 300000 || c%100000 != 0 { // (-1: none chosen -- the original's line of $300 000, the fixes as they are)
		o["credit_line"] = "300000"
	} else {
		o["credit_line"] = strconv.FormatInt(c, 10)
	}
	if t := num("terror", -1, -1, 100000000); t < 0 {
		o["terror_damage"] = "50000"
	} else {
		o["terror_damage"] = strconv.FormatInt(t, 10)
	}
	sammy := strings.Split(r["sammy"], ",")
	for i, k := range []string{"sammy_news", "sammy_culture", "sammy_shows", "sammy_series"} {
		o[k] = "0"
		if i < len(sammy) {
			if v, err := strconv.ParseInt(sammy[i], 10, 64); err == nil && v > 0 && v <= 100000000 {
				o[k] = strconv.FormatInt(v, 10)
			}
		}
	}
	o["ad_offers"] = strconv.FormatInt(num("offers", 10, 10, 17), 10)
	o["film_offers"] = strconv.FormatInt(num("filmoffers", 10, 10, 17), 10)
	o["auction_lots"] = strconv.FormatInt(num("lots", 10, 10, 20), 10)
	o["series_topicality"] = yes("sertop")
	o["original_screenplays"] = yes("scripts")
	return o
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
	if err := checkSpec(spec); err != nil {
		return nil, err
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
			if s.rules != nil {
				o["rules"] = tapeRules(s.rules)
			}
		}
		out = append(out, o)
	}
	return obj{"spec": spec, "dir": rel(dir), "built": built, "slots": out, "newest": newestSave(slots), "safes": otherSafes()}, nil
}

// otherSafes: every selection with saved games of its own (named ones; the original's blank
// one in every safe does not count): its spec, how many, their names, the newest's
func otherSafes() []any {
	type safe struct {
		spec   string
		names  []any
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
		names := []any{}
		for _, s := range slots {
			if s.saved && strings.TrimSpace(s.name) != "" {
				names = append(names, s.name)
			}
		}
		if len(names) == 0 {
			return
		}
		all = append(all, safe{spec, names, slots[newestSave(slots)-1].name, st.ModTime().Unix()})
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
		out = append(out, obj{"spec": s.spec, "count": len(s.names), "names": s.names, "newest": s.newest})
	}
	return out
}

// The safes emptied (the page "Play": a tape of the safe shown, a selection's whole safe in
// the panel "Packs") -- never while a game started here plays from it (server.useSafe).

// deleteTape: tape SLOT of the safe in DIR as the game came -- its name and its index entry
// the original game files' (the shipped empty slot; slot 1: the original's own blank-named
// save, while its bytes are still where the original has them), written in place as the game
// writes its names (res_write: the last file with resource 0xD1) and a save's index entry (in
// every file that has one); the name first, so that no moment holds a loadable half.  The file
// does not shrink: the saved game's bytes stay behind, as those of every save written over.
func deleteTape(dir string, slot int) error {
	if slot < 1 || slot > saveSlots {
		return newMsg("no such thing")
	}
	id := saveNamesRes + slot
	var ship struct { // the original's: the slot's entry, its file's name, its bytes, the slot's name
		file      string
		off, size int64
		data      []byte
		name      []byte
	}
	ship.name = make([]byte, saveNameLen)
	if o, err := openRscFiles(gameDir()); err == nil {
		if n, off, size, ok := o.find(id); ok {
			if b := make([]byte, size); readAt(o.files[n], b, off) == nil {
				ship.file, ship.off, ship.size, ship.data = o.names[n], off, size, b
			}
		}
		if n, off, size, ok := o.find(saveNamesRes); !ok || size < int64(slot*saveNameLen) || readAt(o.files[n], ship.name, off+int64((slot-1)*saveNameLen)) != nil {
			ship.name = make([]byte, saveNameLen)
		}
		o.close()
	}
	r, err := openRscFilesMode(dir, os.O_RDWR)
	if err != nil {
		return err
	}
	defer r.close()
	entries := make([][2]int64, len(r.files)) // what each file's entry becomes
	restored := false
	for n := range r.files {
		if _, ok := r.idx[n][id]; !ok {
			continue
		}
		if ship.data != nil && strings.EqualFold(r.names[n], ship.file) && ship.off+ship.size <= r.sizes[n] {
			if b := make([]byte, ship.size); readAt(r.files[n], b, ship.off) == nil && string(b) == string(ship.data) {
				entries[n], restored = [2]int64{ship.off, ship.size}, true
			}
		}
	}
	name := make([]byte, saveNameLen)
	if restored {
		name = ship.name
	}
	if n, off, size, ok := r.find(saveNamesRes); ok && size >= int64(slot*saveNameLen) {
		if _, err := r.files[n].WriteAt(name, off+int64((slot-1)*saveNameLen)); err != nil {
			return err
		}
	}
	for n := range r.files {
		if _, ok := r.idx[n][id]; !ok {
			continue
		}
		e := make([]byte, 6)
		binary.LittleEndian.PutUint32(e, uint32(entries[n][0]))
		binary.LittleEndian.PutUint16(e[4:], uint16(entries[n][1]))
		if _, err := r.files[n].WriteAt(e, int64(6*id)); err != nil {
			return err
		}
	}
	return nil
}

// deleteSafe: a selection's whole safe -- its folder build/mods/SPEC, the game files with the
// games saved in them; its packs in mods/ stay, its next start builds the folder anew (as the
// first time).  The original game's run/ is no build: its tapes go one by one (deleteTape).
func deleteSafe(spec string) error {
	if spec == "" {
		return newMsg("the original game's safe stays: its tapes go one by one")
	}
	if err := checkSpec(spec); err != nil {
		return err
	}
	dir := safeFolder(spec)
	if !isDir(dir) {
		return newMsg("no safe of {spec}", "spec", spec)
	}
	defer lockDir(dir)() // (not while it is being built)
	return os.RemoveAll(dir)
}

// safeTestSave: a saved game for the page's self-test, as the game writes one -- the original's
// own save in slot 1 (day DAY) appended to the first of the game files of DIR, the index
// entry of slot SLOT pointing to it, NAME its name; RULES (not ""): the line of its rules
// (src/saverules.c) in an extension of its own
func safeTestSave(dir string, slot int, name string, day int, rules string) error {
	if len(rules) > saveRulesMax {
		return newMsg("no such thing")
	}
	if slot < 1 || slot > saveSlots || name == "" || len(name) > 10 { // (the game's field: 10 letters)
		return newMsg("no such thing")
	}
	o, err := openRscFiles(gameDir())
	if err != nil {
		return err
	}
	save := make([]byte, saveSize)
	n, off, size, ok := o.find(saveNamesRes + 1)
	if ok && size >= saveSize {
		err = readAt(o.files[n], save, off)
	}
	o.close()
	if !ok || size < saveSize || err != nil {
		return newMsg("no such saved game")
	}
	binary.LittleEndian.PutUint16(save[saveDay:], uint16(day))
	if rules != "" {
		save = append(save, "MTVX\x08\x00"...)
		for _, c := range []int{200, 50, 50, 0, 10, 10, 10, len(rules)} {
			save = binary.LittleEndian.AppendUint16(save, uint16(c))
		}
		save = append(save, rules...)
	}
	r, err := openRscFilesMode(dir, os.O_RDWR)
	if err != nil {
		return err
	}
	defer r.close()
	n, off, size, ok = r.find(saveNamesRes)
	if !ok || size < saveSlots*saveNameLen {
		return newMsg("no such saved game")
	}
	enc, err := cp437Encode(name)
	if err != nil {
		return err
	}
	nb := make([]byte, saveNameLen)
	copy(nb, enc)
	if _, err := r.files[n].WriteAt(nb, off+int64((slot-1)*saveNameLen)); err != nil {
		return err
	}
	if _, err := r.files[0].WriteAt(save, r.sizes[0]); err != nil {
		return err
	}
	e := make([]byte, 6)
	binary.LittleEndian.PutUint32(e, uint32(r.sizes[0]))
	binary.LittleEndian.PutUint16(e[4:], uint16(len(save)))
	_, err = r.files[0].WriteAt(e, int64(6*(saveNamesRes+slot)))
	return err
}
