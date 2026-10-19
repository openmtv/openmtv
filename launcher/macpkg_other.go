//go:build !darwin || !cgo

package main

// macRelaunched: only the Mac's window build has anything to do (macpkg_darwin.go)
func macRelaunched() bool { return false }
