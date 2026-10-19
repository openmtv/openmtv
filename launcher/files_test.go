package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
)

// Files written while the page sends two requests at once: every change is kept,
// none of them fails (each write through a new file of its own, the settings'
// read-modify-write and a pack's build one at a time)

func TestWriteSettingsConcurrent(t *testing.T) {
	p := filepath.Join(t.TempDir(), "madtv", "settings.ini")
	for round := 0; round < 10; round++ {
		os.Remove(p)
		const n = 8
		var wg sync.WaitGroup
		errs := make(chan error, n)
		for i := 0; i < n; i++ {
			wg.Add(1)
			go func(i int) {
				defer wg.Done()
				errs <- writeSettings([]settingChange{{"test", fmt.Sprintf("k%d", i), fmt.Sprint(round)}}, p)
			}(i)
		}
		wg.Wait()
		close(errs)
		for err := range errs {
			if err != nil {
				t.Fatalf("round %d: %v", round, err)
			}
		}
		d, err := readSettings(p)
		if err != nil {
			t.Fatal(err)
		}
		vals := d["values"].(map[string]map[string]string)["test"]
		for i := 0; i < n; i++ {
			if vals[fmt.Sprintf("k%d", i)] != fmt.Sprint(round) {
				t.Fatalf("round %d: k%d lost (%v)", round, i, vals)
			}
		}
	}
	if left, _ := filepath.Glob(p + "*.new"); len(left) > 0 {
		t.Errorf("left behind: %v", left)
	}
}

func TestReplaceFileConcurrent(t *testing.T) {
	p := filepath.Join(t.TempDir(), "doc.json")
	var wg sync.WaitGroup
	errs := make(chan error, 32)
	for i := 0; i < 16; i++ {
		wg.Add(2)
		go func(i int) {
			defer wg.Done()
			errs <- writeOrdered(p, &onode{kind: 'n', text: fmt.Sprint(i)})
		}(i)
		go func(i int) {
			defer wg.Done()
			errs <- writeJSONFile(p, obj{"i": i})
		}(i)
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		if err != nil {
			t.Fatal(err)
		}
	}
	if _, err := readJSON(p); err != nil {
		t.Errorf("the file written last: %v", err)
	}
	if left, _ := filepath.Glob(p + "*.new"); len(left) > 0 {
		t.Errorf("left behind: %v", left)
	}
}

// several builds of the same packs at once (the page's Build twice, the launcher's
// start): all of them done, the game files whole
func TestBuildConcurrent(t *testing.T) {
	paths := gamePaths(t)
	troot := t.TempDir()
	shipped, _ := filepath.Abs(filepath.Join("..", "tests", "packs")) // (the test's content packs)
	for _, m := range []string{"test-filme", "test-werbung"} {
		if !isDir(filepath.Join(shipped, m)) {
			t.Skipf("no mods/%s", m)
		}
		filepath.Walk(filepath.Join(shipped, m), func(p string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			to := filepath.Join(troot, "mods", strings.TrimPrefix(p, shipped))
			if info.IsDir() {
				return os.MkdirAll(to, 0o777)
			}
			return copyFile(p, to)
		})
	}
	kept := root
	defer func() { root = kept }()
	root = troot
	g, err := openGame(filepath.Dir(paths[0]))
	if err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	errs := make(chan error, 8)
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, err := buildMod(g, "test-filme+test-werbung")
			errs <- err
		}()
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		if err != nil {
			t.Fatal(err)
		}
	}
	out := filepath.Join(troot, "build", "mods", "test-filme+test-werbung")
	if _, err := readResources([]string{filepath.Join(out, "SOURCE1.RSC"), filepath.Join(out, "SOURCE2.RSC")}); err != nil {
		t.Errorf("the built files: %v", err)
	}
	if left, _ := filepath.Glob(filepath.Join(out, "*.new")); len(left) > 0 {
		t.Errorf("left behind: %v", left)
	}
}
