package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

// the game's clock as it shows it: 50 a second since 17:00, past midnight from 0 again
func TestClockText(t *testing.T) {
	for acc, want := range map[uint32]string{10200: "17:03", 68880: "17:22", 1435000: "00:58", 0: "17:00", 1080000: "23:00"} {
		if got := clockText(acc); got != want {
			t.Errorf("clockText(%d) = %s, want %s", acc, got, want)
		}
	}
}

// game files with the safe in FILE: names in resource 0xD1, saves in 0xD1+N where the index
// says; the other game file without them -> the folder
func fakeSafe(t *testing.T, names map[int]string, saves map[int][3]int, order []int, file ...string) string {
	idx := make([]byte, 600*6)
	data := []byte{}
	put := func(id int, b []byte) {
		binary.LittleEndian.PutUint32(idx[6*id:], uint32(len(idx)+len(data)))
		binary.LittleEndian.PutUint16(idx[6*id+4:], uint16(len(b)))
		data = append(data, b...)
	}
	nb := make([]byte, saveSlots*saveNameLen)
	for k, n := range names {
		copy(nb[(k-1)*saveNameLen:], n)
	}
	put(saveNamesRes, nb)
	for _, k := range order { // (in this order into the file: the last is the newest)
		v := saves[k] // day, station, money
		b := make([]byte, saveSize)
		binary.LittleEndian.PutUint16(b[saveDay:], uint16(v[0]))
		binary.LittleEndian.PutUint16(b[savePlayer:], uint16(v[1]))
		binary.LittleEndian.PutUint32(b[saveClock:], 68880)
		binary.LittleEndian.PutUint32(b[saveMoney+(v[1]-1)*15:], uint32(int32(v[2])))
		copy(b[saveManager+(v[1]-1)*saveNameLen:], "Manager")
		put(saveNamesRes+k, b)
	}
	dir, in, other := t.TempDir(), "SOURCE1.RSC", "SOURCE2.RSC"
	if len(file) > 0 {
		in, other = file[0], "SOURCE1.RSC"
	}
	if err := os.WriteFile(filepath.Join(dir, in), append(idx, data...), 0o666); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, other), make([]byte, 600*6), 0o666); err != nil {
		t.Fatal(err)
	}
	return dir
}

// the slots: names, what a save holds, the newest by its place in the file -- the
// original's blank-named save only when there is no other
func TestReadSafe(t *testing.T) {
	p := fakeSafe(t, map[int]string{1: " ", 2: "Montag", 3: "kein", 7: "Tag 31"},
		map[int][3]int{1: {1, 1, 200000}, 2: {11, 2, -48000}, 7: {31, 3, 5120000}}, []int{1, 2, 7})
	slots, err := readSafe(p)
	if err != nil {
		t.Fatal(err)
	}
	s2, s3, s7 := slots[1], slots[2], slots[6]
	if !s2.saved || s2.day != 11 || s2.station != 2 || s2.money != -48000 || s2.clock != "17:22" || s2.manager != "Manager" || s2.name != "Montag" {
		t.Errorf("slot 2: %+v", s2)
	}
	if s3.saved || s3.name != "kein" {
		t.Errorf("slot 3 (a name without its save): %+v", s3)
	}
	if !s7.saved || s7.station != 3 || s7.money != 5120000 {
		t.Errorf("slot 7: %+v", s7)
	}
	if slots[4].name != "" || slots[4].saved {
		t.Errorf("slot 5 (empty): %+v", slots[4])
	}
	if n := newestSave(slots); n != 7 {
		t.Errorf("newest: %d, want 7", n)
	}
	// saved again into slot 2: now the furthest into the file
	p = fakeSafe(t, map[int]string{2: "Montag", 7: "Tag 31"}, map[int][3]int{2: {12, 1, 1}, 7: {31, 3, 5}}, []int{7, 2})
	if slots, _ = readSafe(p); newestSave(slots) != 2 {
		t.Errorf("newest after saving slot 2 again: %d", newestSave(slots))
	}
	// only the original's blank one: a tape, never picked first
	p = fakeSafe(t, map[int]string{1: " "}, map[int][3]int{1: {1, 1, 200000}}, []int{1})
	if slots, _ = readSafe(p); newestSave(slots) != 0 || !slots[0].saved {
		t.Errorf("only the blank save: newest %d, %+v", newestSave(slots), slots[0])
	}
	// the saves in SOURCE2.RSC (where the game puts them when the directory lists it first)
	p = fakeSafe(t, map[int]string{4: "Zwei"}, map[int][3]int{4: {7, 2, 1234}}, []int{4}, "SOURCE2.RSC")
	if slots, _ = readSafe(p); !slots[3].saved || slots[3].day != 7 || slots[3].money != 1234 || newestSave(slots) != 4 {
		t.Errorf("a save in SOURCE2.RSC: %+v", slots[3])
	}
}

// the original game files: their own save in slot 1 (a blank name, day 1, 17:03, $ 200 000) --
// never the one picked first
func TestReadSafeOriginal(t *testing.T) {
	if !exists(filepath.Join(gameDir(), "SOURCE1.RSC")) {
		t.Skip("no original game files")
	}
	slots, err := readSafe(gameDir())
	if err != nil {
		t.Fatal(err)
	}
	s := slots[0]
	if !s.saved || s.day != 1 || s.clock != "17:03" || s.money != 200000 || s.station != 1 || newestSave(slots) != 0 {
		t.Errorf("the original's slot 1: %+v, newest %d", s, newestSave(slots))
	}
}

// a selection names folders below build/mods: no path in it
func TestSafeDocSpec(t *testing.T) {
	for _, spec := range []string{"../x", "a/b", "a+../b", "+"} {
		if _, err := safeDoc(spec); err == nil {
			t.Errorf("safeDoc(%q): taken", spec)
		}
	}
}

// fakeOriginal: original game files of the test's own, as GAME -- the blank-named save in slot 1
// (day 1, station 1, $ 200 000) -> their folder
func fakeOriginal(t *testing.T) string {
	d := fakeSafe(t, map[int]string{1: " "}, map[int][3]int{1: {1, 1, 200000}}, []int{1})
	t.Setenv("GAME", d)
	return d
}

// copyDir: the game files of SRC into a new folder
func copyDir(t *testing.T, src string) string {
	d := t.TempDir()
	for _, f := range []string{"SOURCE1.RSC", "SOURCE2.RSC"} {
		if err := copyFile(filepath.Join(src, f), filepath.Join(d, f)); err != nil {
			t.Fatal(err)
		}
	}
	return d
}

// a tape deleted: its name and its index entry as the game came -- the others stay; slot 1
// back to the original's own blank save; all of them gone: the files' first bytes the
// original's again (the saves' bytes behind them, dead)
func TestDeleteTape(t *testing.T) {
	orig := fakeOriginal(t)
	safe := copyDir(t, orig)
	for _, s := range []struct {
		slot int
		name string
		day  int
	}{{2, "Montag", 11}, {7, "Tag 31", 31}, {1, "Meins", 5}} {
		if err := safeTestSave(safe, s.slot, s.name, s.day, ""); err != nil {
			t.Fatal(err)
		}
	}
	slots, _ := readSafe(safe)
	if slots[0].name != "Meins" || slots[0].day != 5 || slots[1].name != "Montag" || slots[6].day != 31 || newestSave(slots) != 1 {
		t.Fatalf("saved: %+v", slots)
	}
	if err := deleteTape(safe, 2); err != nil {
		t.Fatal(err)
	}
	slots, _ = readSafe(safe)
	if slots[1].name != "" || slots[1].saved || !slots[6].saved || slots[6].name != "Tag 31" || slots[0].name != "Meins" {
		t.Errorf("tape 2 deleted: %+v", slots)
	}
	if err := deleteTape(safe, 1); err != nil {
		t.Fatal(err)
	}
	slots, _ = readSafe(safe)
	if s := slots[0]; s.name != " " || !s.saved || s.day != 1 || s.money != 200000 || newestSave(slots) != 7 {
		t.Errorf("tape 1 deleted: the original's blank save again? %+v, newest %d", s, newestSave(slots))
	}
	if err := deleteTape(safe, 7); err != nil {
		t.Fatal(err)
	}
	for _, f := range []string{"SOURCE1.RSC", "SOURCE2.RSC"} {
		a, _ := os.ReadFile(filepath.Join(orig, f))
		b, _ := os.ReadFile(filepath.Join(safe, f))
		if len(b) < len(a) || string(b[:len(a)]) != string(a) {
			t.Errorf("%s: not the original's bytes again (%d, %d bytes)", f, len(a), len(b))
		}
	}
	if err := deleteTape(safe, 11); err == nil {
		t.Error("tape 11: taken")
	}
	// a name without its save: the name goes; saves in SOURCE2.RSC: their entry there
	p := fakeSafe(t, map[int]string{3: "kein", 4: "Zwei"}, map[int][3]int{4: {7, 2, 1234}}, []int{4}, "SOURCE2.RSC")
	if err := deleteTape(p, 3); err != nil {
		t.Fatal(err)
	}
	if err := deleteTape(p, 4); err != nil {
		t.Fatal(err)
	}
	if slots, _ = readSafe(p); slots[2].name != "" || slots[3].name != "" || slots[3].saved {
		t.Errorf("a name alone, a save in SOURCE2.RSC: %+v", slots[2:4])
	}
	if r, _ := openRscFiles(p); r != nil {
		if _, _, _, ok := r.find(saveNamesRes + 4); ok {
			t.Error("slot 4's index entry kept")
		}
		r.close()
	}
}

// a selection's whole safe: its folder below build/mods -- not the original's, no path, only one that is there
func TestDeleteSafe(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	dir := filepath.Join(root, "build", "mods", "a+b")
	os.MkdirAll(dir, 0o777)
	os.WriteFile(filepath.Join(dir, "SOURCE1.RSC"), make([]byte, 600*6), 0o666)
	for _, spec := range []string{"", "../x", "a/b", "+", "zz"} {
		if err := deleteSafe(spec); err == nil {
			t.Errorf("deleteSafe(%q): taken", spec)
		}
	}
	if err := deleteSafe("a+b"); err != nil || exists(dir) {
		t.Errorf("deleteSafe(a+b): %v, the folder there: %v", err, exists(dir))
	}
	if !isDir(filepath.Join(root, "build", "mods")) {
		t.Error("build/mods went with it")
	}
}

// the page's way: a safe a game started here plays from (or starts to) is not emptied -- a
// tape neither; the safe then as it is now
func TestSavesPostInUse(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	orig := fakeOriginal(t)
	dir := filepath.Join(root, "build", "mods", "a")
	os.MkdirAll(filepath.Join(root, "build", "mods"), 0o777)
	if err := os.Rename(copyDir(t, orig), dir); err != nil {
		t.Fatal(err)
	}
	if err := safeTestSave(dir, 3, "Drei", 4, ""); err != nil {
		t.Fatal(err)
	}
	s := testServer(t)
	release := s.useSafe(safeFolder("a"))
	for _, b := range []obj{{"mod": "a"}, {"mod": "a", "slot": int64(3)}} {
		if r := s.savesPost("delete", b, "de"); r.code != 400 || r.body.(obj)["error"] != "aus diesem Tresor spielt gerade ein Spiel -- erst beenden" {
			t.Errorf("in use, %v: %d %v", b, r.code, r.body)
		}
	}
	release()
	release() // (once only)
	if r := s.savesPost("delete", obj{"mod": "a", "slot": int64(3)}, "en"); r.code != 200 || r.body.(obj)["newest"] != 0 {
		t.Errorf("tape 3: %d %v", r.code, r.body)
	}
	if r := s.savesPost("delete", obj{"mod": "a"}, "en"); r.code != 200 || r.body.(obj)["built"] != false || exists(dir) {
		t.Errorf("the safe: %d %v", r.code, r.body)
	}
	for _, b := range []obj{{"mod": "a", "slot": int64(11)}, {"mod": "a", "slot": "1"}, {"mod": "../a"}, {"mod": "a", "slot": int64(2)}} {
		if r := s.savesPost("delete", b, "en"); r.code != 400 {
			t.Errorf("%v: taken (%d %v)", b, r.code, r.body)
		}
	}
	if r := s.savesPost("testsave", obj{"mod": "a", "slot": int64(1), "name": "x"}, "en"); r.code != 400 {
		t.Errorf("testsave outside the self-test's safe: %d", r.code)
	}
}

// the game's own saves deleted -- slot 7 a new one, slot 1 over the original's blank one:
// the files' first bytes the original's again, build/mtv-dec finds slot 7 empty and slot 1
// the original's (day 1); as tools/loadslottest.py saves and loads
func TestDeleteTapeGame(t *testing.T) {
	paths := gamePaths(t)
	bin := filepath.Join("..", "build", "mtv-dec")
	if !exists(bin) {
		t.Skip("no build/mtv-dec")
	}
	orig, _ := filepath.Abs(filepath.Dir(paths[0]))
	t.Setenv("GAME", orig)
	d := copyDir(t, orig)
	game := func(script []string, extra ...string) string {
		os.WriteFile(filepath.Join(d, "s.txt"), []byte(strings.Join(script, "\n")+"\n"), 0o666)
		b, _ := exec.Command(bin, append([]string{"--headless", "--nosound", "--logical-time", "--game", d, "--args", "e", "--script", filepath.Join(d, "s.txt")}, extra...)...).CombinedOutput()
		return string(b)
	}
	// the setup's "Load game", slot 1; the safe in the office, "Save", the slot, the name, Enter
	save := func(x, y int, name string) {
		s := []string{"400 key esc", "1000 click 160 100", "1200 click 100 28", "1600 click 76 35", "1800 click 134 75", fmt.Sprintf("2000 click %d %d", x, y)}
		for i, c := range name {
			s = append(s, fmt.Sprintf("%d key %c", 2100+10*i, c))
		}
		game(append(s, "2250 key enter", "2400 quit"))
	}
	save(220, 45, "Seven")
	save(100, 28, "One")
	slots, _ := readSafe(d)
	if slots[6].name != "Seven" || !slots[6].saved || !strings.EqualFold(strings.TrimSpace(slots[0].name), "one") || newestSave(slots) != 1 { // (slot 1: the game's field keeps the blank, " one")
		t.Fatalf("the game's saves: %+v", slots)
	}
	for _, n := range []int{7, 1} {
		if err := deleteTape(d, n); err != nil {
			t.Fatal(err)
		}
	}
	a, _ := os.ReadFile(paths[0])
	b, _ := os.ReadFile(filepath.Join(d, "SOURCE1.RSC"))
	if len(b) <= len(a) || string(b[:len(a)]) != string(a) {
		t.Errorf("SOURCE1.RSC: not the original's bytes again (%d, %d bytes)", len(a), len(b))
	}
	if out := game([]string{"900 quit"}, "--load-slot", "7"); !strings.Contains(out, "[load-slot] slot 7 not loaded") {
		t.Errorf("--load-slot 7 after the delete: %s", out)
	}
	if out := game([]string{"900 quit"}, "--load-slot", "1"); !strings.Contains(out, "[load-slot] slot 1 loaded: day 1") {
		t.Errorf("--load-slot 1 after the delete: %s", out)
	}
}

// The game files in DOS order whatever the directory's own (the iPhone's lists SOURCE2.RSC
// first): rscNames sorted by name without case, as runtime/dos.c find_list gives them the game.
func TestRscNamesSorted(t *testing.T) {
	dir := t.TempDir()
	for _, n := range []string{"source2.rsc", "SOURCE1.RSC", "notes.txt", "B.RSC"} {
		if err := os.WriteFile(filepath.Join(dir, n), nil, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	got, err := rscNames(dir)
	if err != nil {
		t.Fatal(err)
	}
	want := []string{"B.RSC", "SOURCE1.RSC", "source2.rsc"}
	if strings.Join(got, ",") != strings.Join(want, ",") {
		t.Fatalf("rscNames = %v, want %v", got, want)
	}
}

// a savegame's extension: "MTVX", COUNTS, then each one's data -- the rules' line (the eighth
// count) after the lots; the pairs of the line, and as the settings name them
func mtvx(counts []int, line string) []byte {
	x := []byte("MTVX\x00\x00")
	binary.LittleEndian.PutUint16(x[4:], uint16(len(counts)))
	for _, c := range counts {
		x = binary.LittleEndian.AppendUint16(x, uint16(c))
	}
	sizes := []int{max(counts[0]-200, 0) * 0x12, max(counts[1]-50, 0) * 0x29, max(counts[2]-50, 0), counts[3],
		max(counts[4]-10, 0) * 7, max(counts[5]-10, 0) * 8, max(counts[6]-10, 0) * 9}
	for _, n := range sizes {
		x = append(x, make([]byte, n)...)
	}
	return append(x, line...)
}

func TestSaveRules(t *testing.T) {
	line := "fixes=all speed4=1 aiimp=2 credit=900000 terror=0 sammy=1000,0,0,4000 offers=13 filmoffers=17 lots=20 sertop=1 scripts=0"
	r := saveRules(mtvx([]int{250, 60, 55, 86, 13, 17, 20, len(line)}, line))
	got := tapeRules(r)
	want := obj{"classic": "no", "fixes": "all", "fourth_speed": "yes", "ai_improve": "strong", "credit_line": "900000",
		"terror_damage": "0", "sammy_news": "1000", "sammy_culture": "0", "sammy_shows": "0", "sammy_series": "4000",
		"ad_offers": "13", "film_offers": "17", "auction_lots": "20", "series_topicality": "yes", "original_screenplays": "no"}
	if fmt.Sprint(got) != fmt.Sprint(want) {
		t.Errorf("tapeRules = %v\nwant %v", got, want)
	}
	// the original's for what a line lacks or has out of range; a later build's key passed over
	got = tapeRules(saveRules(mtvx([]int{200, 50, 50, 0, 10, 10, 10, 30}, "fixes=classic lots=99 newrule=1")))
	if got["classic"] != "yes" || got["fixes"] != nil || got["auction_lots"] != "10" || got["credit_line"] != "300000" || got["terror_damage"] != "50000" {
		t.Errorf("a short line: %v", got)
	}
	// none: no extension, seven counts (before the rules), a line of length 0, a line cut short
	for i, x := range [][]byte{nil, []byte("XXXX\x08\x00"), mtvx([]int{200, 50, 50, 0, 10, 10, 20}, ""), mtvx([]int{200, 50, 50, 0, 10, 10, 10, 0}, ""),
		mtvx([]int{200, 50, 50, 0, 10, 10, 10, 40}, "fixes=all")} {
		if r := saveRules(x); r != nil {
			t.Errorf("case %d: rules %v, want none", i, r)
		}
	}
}
