package main

import (
	"bytes"
	"encoding/json"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

// go test (in launcher/; the original game files from $GAME, default ../..):
// the tables of the original survive export and build byte for byte, and the
// export with the data fixes is tools/content.py's and tools/datafixes.py's
// (python3 needed for that one).

func gamePaths(t *testing.T) []string {
	dir := os.Getenv("GAME")
	if dir == "" {
		dir = filepath.Join("..", "..")
	}
	var paths []string
	for _, f := range []string{"SOURCE1.RSC", "SOURCE2.RSC"} {
		p := filepath.Join(dir, f)
		if !exists(p) {
			t.Skipf("no %s (set GAME)", p)
		}
		paths = append(paths, p)
	}
	return paths
}

func TestRoundTrip(t *testing.T) {
	res, err := readResources(gamePaths(t))
	if err != nil {
		t.Fatal(err)
	}
	docs, err := export(res)
	if err != nil {
		t.Fatal(err)
	}
	// (through JSON, as a pack is read)
	data, _ := json.Marshal(docs)
	v, err := parseJSON(data)
	if err != nil {
		t.Fatal(err)
	}
	again := map[string]obj{}
	for k, d := range v.(obj) {
		again[k] = d.(obj)
	}
	built, err := buildTables(again)
	if err != nil {
		t.Fatal(err)
	}
	save, _ := res.get(startSave)
	built[startSave] = patchStartSave(save, built[0xC9], built[0xCA], built[0xCB], built[0xCC])
	for i, b := range built {
		o, _ := res.get(i)
		if !bytes.Equal(b, o) {
			t.Errorf("resource 0x%X differs", i)
		}
	}
}

func TestExportLikePython(t *testing.T) {
	paths := gamePaths(t)
	py, err := exec.LookPath("python3")
	if err != nil {
		t.Skip("no python3")
	}
	script := `import json, sys; sys.path.insert(0, sys.argv[1]); import content, datafixes
print(json.dumps(datafixes.apply(content.export(content.Resources(sys.argv[2:])))[0], ensure_ascii=False))`
	out, err := exec.Command(py, append([]string{"-c", script, filepath.Join("..", "tools")}, paths...)...).Output()
	if err != nil {
		t.Fatal(err)
	}
	want, err := parseJSON(out)
	if err != nil {
		t.Fatal(err)
	}
	res, _ := readResources(paths)
	docs, err := export(res)
	if err != nil {
		t.Fatal(err)
	}
	got := map[string]any{}
	fixed, _ := applyDataFixes(docs)
	for k, d := range fixed {
		got[k] = d
	}
	for _, tb := range tables {
		if !equal(got[tb.name], want.(obj)[tb.name]) {
			g, _ := json.Marshal(got[tb.name])
			w, _ := json.Marshal(want.(obj)[tb.name])
			t.Errorf("%s differs (%d / %d bytes of JSON)", tb.name, len(g), len(w))
		}
	}
}

// TestPyDumpsLikePython: the text a selection's id is made of is Python's
// json.dumps(sort_keys=True, ensure_ascii=False, separators=(',', ':')), to the byte
func TestPyDumpsLikePython(t *testing.T) {
	paths := gamePaths(t)
	py, err := exec.LookPath("python3")
	if err != nil {
		t.Skip("no python3")
	}
	res, _ := readResources(paths)
	docs, err := export(res)
	if err != nil {
		t.Fatal(err)
	}
	m := map[string]any{"tables": docs, "odd": []any{"\"quote\" \\ back\n\t\x01 ümlaut   <&>", int64(-5), 1.5, 100.0, true, nil, obj{},
		1e6, 123456789012345.6, 1e16, 1.5e-5, 0.0001, -0.0, 1e22, "ends in \\", obj{"a\\": "b\\\"", "c": "d, e: f"}}}
	text := pyDumps(m)
	c := exec.Command(py, "-c", `import json, sys; d = json.loads(sys.stdin.read()); sys.stdout.write(json.dumps(d, sort_keys=True, ensure_ascii=False, separators=(',', ':')))`)
	c.Stdin = bytes.NewReader([]byte(text))
	want, err := c.Output()
	if err != nil {
		t.Fatal(err)
	}
	if string(want) != text {
		for i := range text {
			if i >= len(want) || want[i] != text[i] {
				t.Fatalf("differs at %d: go %q, python %q", i, text[max(0, i-40):min(len(text), i+40)], string(want[max(0, i-40):min(len(want), i+40)]))
			}
		}
		t.Fatalf("python's is longer")
	}
	// (the test save's text: the default separators)
	c = exec.Command(py, "-c", `import json, sys; d = json.loads(sys.stdin.read()); sys.stdout.write(json.dumps(d, sort_keys=True, ensure_ascii=False))`)
	c.Stdin = bytes.NewReader([]byte(text))
	if want, err = c.Output(); err != nil {
		t.Fatal(err)
	}
	if got := pyDumpsSpaced(m); got != string(want) {
		t.Errorf("pyDumpsSpaced differs from json.dumps: %d / %d bytes", len(got), len(want))
	}
}
