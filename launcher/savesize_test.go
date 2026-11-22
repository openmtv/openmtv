package main

import (
	"strings"
	"testing"
)

// TestSavegameSize: a pack whose savegame would not fit in the game's 16 bits (src/actions.c
// SAVEGAME_MAX) is refused -- 999 films and 354 series (with the rules' line: 373 before) --,
// one just below is not (tools/content.py savegame_size alike)
func TestSavegameSize(t *testing.T) {
	if n := savegameSize(999, 353, 50); n > savegameMax {
		t.Errorf("999 films, 353 series: %d bytes, over %d", n, savegameMax)
	}
	if n := savegameSize(999, 354, 50); n <= savegameMax {
		t.Errorf("999 films, 354 series: %d bytes, not over %d", n, savegameMax)
	}
	orig := original(t)
	grow := func(nseries int64) map[string]obj {
		d := copyDocs(orig)
		short := func(n int) obj {
			l := make([]any, n)
			for i := range l {
				l[i] = ""
			}
			l[0] = "X"
			return obj{"de": l, "en": append([]any{}, l...)}
		}
		for id := int64(201); id <= 999; id++ {
			r := deepCopy(record(d, "films", 1+(id-1)%200)).(obj)
			r["id"], r["owner"], r["text"] = id, int64(0), short(2)
			d["films"]["records"] = append(d["films"]["records"].([]any), r)
		}
		for id := int64(51); id <= nseries; id++ {
			r := deepCopy(record(d, "series", 1+(id-1)%50)).(obj)
			r["id"], r["owner"], r["text"] = id, int64(0), short(22)
			d["series"]["records"] = append(d["series"]["records"].([]any), r)
		}
		return d
	}
	if err := checkDocs(grow(353)); err != nil {
		t.Errorf("999 films, 353 series refused: %v", err)
	}
	err := checkDocs(grow(354))
	if err == nil || !strings.Contains(render(err, "en"), "a savegame would have 65522 bytes, at most 65520") {
		t.Errorf("999 films, 354 series: %v", err)
	}
}
