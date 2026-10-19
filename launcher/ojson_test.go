package main

import (
	"os/exec"
	"strings"
	"testing"
)

// the editor's files as Python writes them: json.dump(json.loads(text), indent=1,
// ensure_ascii=False) -- the keys in their order (python3 needed)
func TestOrderedLikePython(t *testing.T) {
	py, err := exec.LookPath("python3")
	if err != nil {
		t.Skip("no python3")
	}
	for _, text := range []string{
		`{"b": 1, "a": [1, 2.5, -0, 1e5, 1.0, 1E-7, 123456789012345678901234], "c": {}, "d": [], "e": null, "f": true, "g": false}`,
		`{"text": {"en": ["Ä \"x\" \\ /\u0001\t\n", "\u00e9\u20ac\ud83d\ude00"], "de": ["\u2028 <&>"]}, "id": 3}`,
		`[{"x": 1, "x": 2, "y": [[], {}, [{}]]}, "", 0.1, 1e300, -1e-300, 5e-324]`,
		`{"records": [{"id": 1, "unknown": {"0x13": "0050040a", "0x2": "00"}, "text": {"de": ["a", "b"], "en": ["c", "d"]}}]}`,
	} {
		n, err := parseOrdered([]byte(text))
		if err != nil {
			t.Fatal(err)
		}
		c := exec.Command(py, "-c", `import json, sys; sys.stdout.write(json.dumps(json.loads(sys.stdin.read()), indent=1, ensure_ascii=False))`)
		c.Stdin = strings.NewReader(text)
		want, err := c.Output()
		if err != nil {
			t.Fatal(err)
		}
		if got := string(n.pyIndent()); got != string(want) {
			t.Errorf("%s:\n got  %s\n want %s", text, got, want)
		}
		back, err := parseOrdered(n.pyIndent())
		if err != nil || !equal(back.value(), n.value()) {
			t.Errorf("%s: not the same once read again (%v)", text, err)
		}
	}
}

// the original's tables in the export's order: the keys as content.py export makes them
func TestCanonicalOrder(t *testing.T) {
	doc := obj{"records": []any{obj{"text": obj{"en": []any{"a"}, "de": []any{"b"}}, "unknown": obj{"0x13": "00", "0x2": "11"},
		"price": int64(5), "genre": int64(1), "id": int64(1), "zzz": int64(0)}}, "count": int64(1), "table": "films", "text_tail": obj{"en": "00", "de": "11"}}
	got := string(canonical("films", doc).pyIndent())
	var keys []string
	for _, l := range strings.Split(got, "\n") {
		if i := strings.Index(l, `":`); i > 0 {
			keys = append(keys, strings.TrimSpace(l[:i+1]))
		}
	}
	want := `"table" "count" "records" "id" "genre" "price" "unknown" "0x2" "0x13" "text" "de" "en" "zzz" "text_tail" "de" "en"`
	if strings.Join(keys, " ") != want {
		t.Errorf("the order:\n got  %s\n want %s", strings.Join(keys, " "), want)
	}
}

// JSON nested deeper than maxDepth: refused (not a stack overflow that ends the
// launcher -- a page's 64 MB of '[')
func TestOrderedDepth(t *testing.T) {
	nested := func(n int) []byte {
		return []byte(strings.Repeat("[", n) + strings.Repeat("]", n))
	}
	if _, err := parseOrdered(nested(maxDepth)); err != nil {
		t.Errorf("%d deep: %v", maxDepth, err)
	}
	if _, err := parseOrdered(nested(maxDepth + 1)); err == nil {
		t.Errorf("%d deep: read", maxDepth+1)
	}
	if _, err := parseOrdered([]byte(strings.Repeat(`{"a":`, maxDepth+1) + "1" + strings.Repeat("}", maxDepth+1))); err == nil {
		t.Errorf("%d objects deep: read", maxDepth+1)
	}
	if _, err := parseOrdered([]byte(strings.Repeat("[", 30<<20))); err == nil {
		t.Error("30 MB of '[': read")
	}
}
