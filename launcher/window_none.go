//go:build !wails

package main

// Without -tags wails: no window of its own (window_wails.go), the page is the browser's.

const windowBuilt = false

func (s *server) window(page, dir string, missing error) (int, bool) { return 0, false }
