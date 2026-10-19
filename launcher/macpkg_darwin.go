//go:build darwin && cgo

package main

/*
#cgo LDFLAGS: -framework CoreFoundation -framework Security
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>
#include <stdlib.h>

// Security.framework's (macOS 10.12+), declared here: the SDK has no header for them
extern bool SecTranslocateIsTranslocatedURL(CFURLRef path, bool *isTranslocated, CFErrorRef *error);
extern CFURLRef SecTranslocateCreateOriginalPathForURL(CFURLRef translocatedPath, CFErrorRef *error);
#include <string.h>

// the place a translocated application really lies (macOS 10.12+), "" when it is not translocated
static char *original_path(const char *path) {
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), true);
	if (!url) return strdup("");
	bool moved = false;
	char *out = NULL;
	if (SecTranslocateIsTranslocatedURL(url, &moved, NULL) && moved) {
		CFURLRef orig = SecTranslocateCreateOriginalPathForURL(url, NULL);
		if (orig) {
			char buf[4096];
			if (CFURLGetFileSystemRepresentation(orig, true, (UInt8 *)buf, sizeof buf)) out = strdup(buf);
			CFRelease(orig);
		}
	}
	CFRelease(url);
	return out ? out : strdup("");
}
*/
import "C"

import (
	"os"
	"os/exec"
	"path/filepath"
	"unsafe"
)

// The players' Mac package (tools/package.py mac) is not notarized: a zip from the
// internet comes with the quarantine attribute on every file.  Once the player
// allowed OpenMTV.app (System Settings, "Open Anyway"), macOS still starts it
// from a copy of the application alone, in a read-only place ("App
// Translocation") -- without the decomp directory beside it -- and the game
// program in decomp/build, being quarantined too, would be refused.  So the
// application takes the attribute off itself and the package's decomp directory
// (not the folder they lie in: that may be the player's Downloads) and, when
// translocated, starts itself again from its real place.
func macRelaunched() bool {
	p, err := os.Executable()
	if err != nil {
		return false
	}
	app := macApp(p)
	if app == "" {
		return false
	}
	if home, err := os.UserHomeDir(); err == nil { // (the page data of the bundle id before 2026-10-19: webdata.go)
		if err := keepWebData(home); err != nil {
			warn("OpenMTV: the launcher's page data of %s not taken over: %v\n", oldBundleID, err)
		}
	}
	cs := C.CString(app)
	defer C.free(unsafe.Pointer(cs))
	co := C.original_path(cs)
	orig := C.GoString(co)
	C.free(unsafe.Pointer(co))
	place := app
	if orig != "" {
		place = orig
	}
	pkg := filepath.Dir(place)
	if !exists(filepath.Join(pkg, "decomp", "play.sh")) {
		return false // (a developer's build/OpenMTV.app: nothing to do)
	}
	if exec.Command("xattr", "-p", "com.apple.quarantine", filepath.Join(pkg, "decomp", "build", "mtv-dec")).Run() == nil ||
		orig != "" {
		for _, q := range quarantineTargets(place) {
			if err := exec.Command("xattr", "-dr", "com.apple.quarantine", q).Run(); err != nil {
				warn("OpenMTV: could not take the quarantine off %s: %v\n", q, err)
			}
		}
	}
	if orig == "" {
		return false
	}
	args := append([]string{"-n", orig, "--args"}, os.Args[1:]...)
	if err := exec.Command("open", args...).Run(); err != nil {
		warn("OpenMTV: could not start %s again: %v\n", orig, err)
		return false
	}
	return true
}
