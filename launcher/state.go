package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
)

// launcher.json beside the settings file: what the launcher's own window keeps
// (window_wails.go) -- the game's directory the player chose when it was not
// above the program, and the window's place and size.  Only the window writes it;
// gameDir reads the directory for every start.

type launcherState struct {
	Game   string       `json:"game,omitempty"`   // the original game files, chosen in the window
	Window *windowPlace `json:"window,omitempty"` // as last left
}

type windowPlace struct {
	X         int  `json:"x"`
	Y         int  `json:"y"`
	Width     int  `json:"width"`
	Height    int  `json:"height"`
	Maximised bool `json:"maximised,omitempty"`
}

func statePath() string { return filepath.Join(filepath.Dir(settingsPath()), "launcher.json") }

// loadState: the kept state (empty when there is none or it cannot be read)
func loadState() launcherState {
	var st launcherState
	if b, err := os.ReadFile(statePath()); err == nil {
		json.Unmarshal(b, &st)
	}
	return st
}

var stateMu sync.Mutex // (changeState's read-modify-write)

// changeState: the kept state changed by F, written back (through a new file)
func changeState(f func(*launcherState)) error {
	stateMu.Lock()
	defer stateMu.Unlock()
	st := loadState()
	f(&st)
	b, err := json.MarshalIndent(st, "", " ")
	if err != nil {
		return err
	}
	p := statePath()
	if err := os.MkdirAll(filepath.Dir(p), 0o777); err != nil {
		return err
	}
	return replaceFile(p, append(b, '\n'))
}

var newFiles atomic.Int64 // (the number in the next new file's name)

// replaceFile: the file P written whole, in place of the old one: through a new file
// of its own beside it (P.PID-N.new) renamed over it -- a reader sees the old file or
// the new one, two writers at once each a whole one (the last one stays)
func replaceFile(p string, data []byte) error {
	tmp := fmt.Sprintf("%s.%d-%d.new", p, os.Getpid(), newFiles.Add(1))
	f, err := os.OpenFile(tmp, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o666)
	if err != nil {
		return err
	}
	_, err = f.Write(data)
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	if err == nil {
		err = os.Rename(tmp, p)
	}
	if err != nil {
		os.Remove(tmp)
	}
	return err
}

// dirLocks: one lock per directory (lockDir)
var dirLocks sync.Map

// lockDir: the directory D held for this caller alone until the function it returns
// is called -- a build into it (the same names in any case: Windows' and macOS'
// directories do not tell them apart)
func lockDir(d string) func() {
	m, _ := dirLocks.LoadOrStore(strings.ToLower(filepath.Clean(d)), &sync.Mutex{})
	mu := m.(*sync.Mutex)
	mu.Lock()
	return mu.Unlock
}

// hasGameFiles: DIR holds the original's resource files
func hasGameFiles(dir string) bool {
	return exists(filepath.Join(dir, "SOURCE1.RSC")) && exists(filepath.Join(dir, "SOURCE2.RSC"))
}
