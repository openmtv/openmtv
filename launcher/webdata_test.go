package main

import (
	"os"
	"path/filepath"
	"testing"
)

// the Mac launcher's page data under its old bundle id goes along to the new one once:
// whole, never over data the new id has already
func TestKeepWebData(t *testing.T) {
	home := t.TempDir()
	old := filepath.Join(home, "Library", "WebKit", oldBundleID, "WebsiteData", "Default", "x", "x", "LocalStorage")
	os.MkdirAll(old, 0o700)
	os.WriteFile(filepath.Join(old, "localstorage.sqlite3"), []byte("packs"), 0o644)
	if err := keepWebData(home); err != nil {
		t.Fatal(err)
	}
	moved := filepath.Join(home, "Library", "WebKit", bundleID, "WebsiteData", "Default", "x", "x", "LocalStorage", "localstorage.sqlite3")
	if b, err := os.ReadFile(moved); err != nil || string(b) != "packs" {
		t.Fatalf("not taken over: %q %v", b, err)
	}
	os.WriteFile(moved, []byte("newer"), 0o644)
	if err := keepWebData(home); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(moved); string(b) != "newer" {
		t.Errorf("the new id's data overwritten: %q", b)
	}
	if exists(filepath.Join(home, "Library", "WebKit", bundleID+".part")) {
		t.Error("a .part left")
	}
	if err := keepWebData(t.TempDir()); err != nil { // (nothing of the old id: nothing to do)
		t.Error(err)
	}
}
