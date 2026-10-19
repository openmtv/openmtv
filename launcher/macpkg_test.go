package main

import (
	"path/filepath"
	"reflect"
	"testing"
)

// the quarantine comes off the application and the package's decomp directory only
// -- not the folder they lie in (the player's Downloads: every other file there)
func TestQuarantineTargets(t *testing.T) {
	app := filepath.Join("/Users", "p", "Downloads", "OpenMTV.app")
	want := []string{app, filepath.Join("/Users", "p", "Downloads", "decomp")}
	if got := quarantineTargets(app); !reflect.DeepEqual(got, want) {
		t.Errorf("quarantineTargets(%s) = %v, not %v", app, got, want)
	}
	for _, p := range quarantineTargets(app) {
		if p == filepath.Dir(app) {
			t.Errorf("the folder %s itself", p)
		}
	}
}
