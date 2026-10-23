package main

import (
	"encoding/binary"
	"os"
	"path/filepath"
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
