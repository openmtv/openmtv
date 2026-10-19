package main

import (
	"os"
	"path/filepath"
	"strings"
)

// The player's settings file of the game (runtime/settings.c; the former Python settings module (gamesettings.py)
// is the launcher's full reader and writer): here only where it is and one value.

// settingsPath: $XDG_CONFIG_HOME/madtv/settings.ini or ~/.config/madtv/settings.ini
// (Windows: %APPDATA%\madtv\settings.ini) -- where runtime/settings.c looks
func settingsPath() string {
	if windows {
		base := os.Getenv("APPDATA")
		if base == "" {
			base = os.Getenv("USERPROFILE")
		}
		if base == "" {
			base = "."
		}
		return filepath.Join(base, "madtv", "settings.ini")
	}
	if x := os.Getenv("XDG_CONFIG_HOME"); x != "" {
		return filepath.Join(x, "madtv", "settings.ini")
	}
	home := os.Getenv("HOME")
	if home == "" {
		home, _ = os.UserHomeDir()
	}
	return filepath.Join(home, ".config", "madtv", "settings.ini")
}

// settingsValue: [SEC] KEY of the file at P as the game reads it (the last one
// counts); "" when it is not there -- then the game's default holds
func settingsValue(p, sec, key string) string {
	data, err := os.ReadFile(p)
	if err != nil {
		return ""
	}
	val, cur := "", ""
	for _, line := range strings.Split(strings.ReplaceAll(trimBOM(string(data)), "\r\n", "\n"), "\n") {
		s := strings.TrimSpace(line)
		if s == "" || s[0] == '#' || s[0] == ';' {
			continue
		}
		if s[0] == '[' {
			if i := strings.IndexByte(s, ']'); i >= 0 {
				cur = strings.TrimSpace(s[1:i])
			}
			continue
		}
		if k, v, ok := strings.Cut(s, "="); ok && cur == sec && strings.TrimSpace(k) == key {
			val = strings.TrimSpace(v)
		}
	}
	return val
}

// trimBOM: the text without a UTF-8 byte order mark before its first line (Windows'
// Notepad writes one), as runtime/settings.c reads it: else "\uFEFF[display]" is no
// section header
func trimBOM(s string) string { return strings.TrimPrefix(s, "\uFEFF") }
