package main

import (
	"encoding/base64"
	"encoding/binary"
	"os"
	"path/filepath"
	"sync"
	"testing"
)

// two copies of the host's savegame arriving at once (a re-sent message): both written
// whole, one after the other -- no "rename ...new: no such file", no file left half written
func TestSaveStoreConcurrent(t *testing.T) {
	desc := []byte(`{"day": 3, "game": "g1", "file": "tag-003.mtvmp", "humans": [{"station": 1, "name": "A"}, {"station": 2, "name": "B"}]}`)
	data := make([]byte, 12, 12+len(desc)+200000)
	copy(data, saveMagic)
	binary.LittleEndian.PutUint32(data[8:], uint32(len(desc)))
	data = append(data, desc...)
	for i := 0; i < 200000; i++ { // (big enough that two writes overlap)
		data = append(data, byte(i*7))
	}
	h := saveHash(data)
	msg := obj{"hash": h, "data": base64.StdEncoding.EncodeToString(deflate(data)),
		"meta": obj{"game": "g1", "file": "tag-003.mtvmp", "day": int64(3)}}
	for round := 0; round < 20; round++ {
		dir := t.TempDir()
		l := &lobby{role: "guest", save: obj{"want": h}, savesDir: dir}
		var wg sync.WaitGroup
		for i := 0; i < 4; i++ {
			wg.Add(1)
			go func() { defer wg.Done(); l.saveStore(msg) }()
		}
		wg.Wait()
		got, err := os.ReadFile(filepath.Join(dir, "g1", "tag-003.mtvmp"))
		if err != nil || saveHash(got) != h {
			t.Fatalf("round %d: the savegame %v, %d bytes (want %d whole)", round, err, len(got), len(data))
		}
		l.mu.Lock()
		st, msgText := l.save["status"], l.save["msg"]
		l.mu.Unlock()
		if st != "ok" {
			t.Fatalf("round %d: status %v: %v", round, st, msgText)
		}
		left, _ := filepath.Glob(filepath.Join(dir, "g1", "*.new"))
		if len(left) > 0 {
			t.Fatalf("round %d: left behind %v", round, left)
		}
	}
}
