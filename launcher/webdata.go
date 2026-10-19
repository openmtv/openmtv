package main

import (
	"io/fs"
	"os"
	"path/filepath"
)

// The Mac application's name for macOS, its bundle id (launcher/Info.plist): until
// 2026-10-19 net.tcjew.madtv (a private domain), now net.openmtv.launcher.  macOS keeps
// the launcher window's page data under it -- ~/Library/WebKit/ID: the packs chosen, the
// languages, the network game's name, server and packs (the page's localStorage) -- so
// the first start under the new id takes over the old one's (keepWebData); the settings,
// saved games and AI pictures lie in ~/.config/madtv either way.
const (
	bundleID    = "net.openmtv.launcher"
	oldBundleID = "net.tcjew.madtv"
)

// keepWebData: WebKit's data of the old bundle id copied for the new one, below HOME's
// Library/WebKit, when the new one has none yet -- whole or not at all (a copy that
// broke off is not taken for the new id's data next time)
func keepWebData(home string) error {
	base := filepath.Join(home, "Library", "WebKit")
	from, to := filepath.Join(base, oldBundleID), filepath.Join(base, bundleID)
	if !isDir(from) || exists(to) {
		return nil
	}
	part := to + ".part"
	os.RemoveAll(part)
	err := filepath.WalkDir(from, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, _ := filepath.Rel(from, p)
		dst := filepath.Join(part, rel)
		switch {
		case d.IsDir():
			return os.MkdirAll(dst, 0o700)
		case d.Type().IsRegular():
			return copyFile(p, dst)
		}
		return nil // (no links or others in WebKit's data)
	})
	if err == nil {
		err = os.Rename(part, to)
	}
	if err != nil {
		os.RemoveAll(part)
	}
	return err
}
