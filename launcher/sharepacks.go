package main

import (
	"bytes"
	"crypto/sha1"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"os"
	"path/filepath"
	"sort"
)

// Packs shared in a network game's lobby: beside the combined tables of its selection (which
// every machine builds the game from, lobby.go) the host sends each chosen pack's own files --
// its mod.json and its tables' files as they are on its disk; a guest keeps the ones it wants
// in its own mods/ ("Keep"), to play them alone, change them or host with them later.

const (
	packOfferMx   = 6 << 20 // all offered packs together: base64 of zlib (with the tables' 8 MB under the server's 16 MB a message)
	packOfferMax  = 20      // packs in one offer
	packFileMx    = 8 << 20 // one file of a pack, unpacked
	packTitleMx   = 200
	packStateNew  = "new"     // no pack of that name here
	packStateSame = "same"    // this machine has it, file for file
	packStateDiff = "differs" // a pack of that name here, with other files
)

// packFileNames: the files a pack may hold -- mod.json and one per table
func packFileNames() map[string]bool {
	out := map[string]bool{"mod.json": true}
	for _, f := range tableFiles() {
		out[f] = true
	}
	return out
}

// packOffer: the packs NAMES as this machine has them -- one entry each: {name, title, data:
// base64 of zlib of {file: text}}, for the lobby's "mod" message
func packOffer(names []string) ([]any, error) {
	var out []any
	total := 0
	for _, n := range names {
		d, err := modDir(n)
		if err != nil {
			return nil, err
		}
		files := obj{}
		for f := range packFileNames() {
			if b, err := os.ReadFile(filepath.Join(d, f)); err == nil {
				files[f] = string(b)
			}
		}
		meta, _ := modMeta(n)
		title, _ := meta["title"].(string)
		if title == "" {
			title = n
		}
		data := base64.StdEncoding.EncodeToString(deflate([]byte(pyDumps(files))))
		total += len(data)
		if total > packOfferMx {
			return nil, newMsg("the packs are too large to hand on (more than {n} MB)", "n", packOfferMx>>20)
		}
		out = append(out, obj{"name": n, "title": title, "data": data})
	}
	return out, nil
}

// offerOK: what the server takes of an offer -- a list of {name, title, data} within the limits
func offerOK(v any) ([]any, bool) {
	if v == nil {
		return nil, true
	}
	list, ok := v.([]any)
	if !ok || len(list) > packOfferMax {
		return nil, false
	}
	total := 0
	for _, x := range list {
		p, ok := x.(obj)
		if !ok {
			return nil, false
		}
		name, _ := p["name"].(string)
		title, _ := p["title"].(string)
		data, _ := p["data"].(string)
		if !nameRE.MatchString(name) || len(title) > packTitleMx || data == "" {
			return nil, false
		}
		total += len(data)
	}
	return list, total <= packOfferMx
}

// decodePack: an offered pack's files, checked as a pack's: only the names a pack has, each a
// JSON object -- a table's with its list of records
func decodePack(data string) (map[string][]byte, error) {
	raw, err := inflate(data, packOfferMx*4)
	if err != nil {
		return nil, err
	}
	v, err := parseJSON(raw)
	if err != nil {
		return nil, err
	}
	m, ok := v.(obj)
	if !ok {
		return nil, newMsg("not a pack's files")
	}
	allowed := packFileNames()
	out := map[string][]byte{}
	for f, x := range m {
		s, ok := x.(string)
		if !allowed[f] || !ok || len(s) > packFileMx {
			return nil, newMsg("not a pack's file: {file}", "file", f)
		}
		doc, err := parseJSON([]byte(s))
		if err != nil {
			return nil, newMsg("not a pack's file: {file}", "file", f)
		}
		d, ok := doc.(obj)
		if !ok {
			return nil, newMsg("not a pack's file: {file}", "file", f)
		}
		if f != "mod.json" {
			if _, _, err := records(d); err != nil {
				return nil, newMsg("not a pack's file: {file}", "file", f)
			}
		}
		out[f] = []byte(s)
	}
	if out["mod.json"] == nil {
		return nil, newMsg("not a pack's files")
	}
	return out, nil
}

// packState: an offered pack against this machine's mods/ -- also a copy kept before under
// NAME-2.. (keepPack: the same files, its title aside)
func packState(name string, files map[string][]byte) string {
	if d, err := modDir(name); err != nil || !isDir(d) {
		return packStateNew
	}
	if samePack(name, files, false) {
		return packStateSame
	}
	for k := 2; ; k++ {
		cand := keptName(name, k)
		if d, err := modDir(cand); err != nil || !isDir(d) {
			return packStateDiff
		}
		if samePack(cand, files, true) {
			return packStateSame
		}
	}
}

// keptName: NAME-K, within a pack name's 40 characters
func keptName(name string, k int) string {
	cand := fmt.Sprintf("%s-%d", name, k)
	if len(cand) > 40 {
		cand = fmt.Sprintf("%s-%d", name[:40-len(fmt.Sprint(k))-1], k)
	}
	return cand
}

// samePack: pack NAME here holds FILES, file for file -- ANYTITLE: its mod.json may have
// another title (a copy keepPack renamed)
func samePack(name string, files map[string][]byte, anyTitle bool) bool {
	d, err := modDir(name)
	if err != nil {
		return false
	}
	for f := range packFileNames() {
		b, err := os.ReadFile(filepath.Join(d, f))
		want, ok := files[f]
		if ok != (err == nil) {
			return false
		}
		if !ok || bytes.Equal(b, want) {
			continue
		}
		if f != "mod.json" || !anyTitle {
			return false
		}
		have, err1 := parseJSON(b)
		offer, err2 := parseJSON(want)
		h, ok1 := have.(obj)
		o, ok2 := offer.(obj)
		if err1 != nil || err2 != nil || !ok1 || !ok2 {
			return false
		}
		h, o = with(h, "title", nil), with(o, "title", nil)
		if !equal(h, o) {
			return false
		}
	}
	return true
}

// keepPack: an offered pack written into mods/ -- under its own name, or, when another pack
// of that name is here, the first free NAME-2, -3 .. with "(from HOST)" in its title (in LANG,
// the page's); the name used.  Written into a directory of its own first, then moved: a half pack never shows.
func keepPack(name string, files map[string][]byte, host, lang string) (string, error) {
	if samePack(name, files, false) {
		return name, nil
	}
	target := name
	if d, _ := modDir(name); isDir(d) {
		meta, err := parseJSON(files["mod.json"])
		if m, ok := meta.(obj); err == nil && ok {
			title, _ := m["title"].(string)
			if title == "" {
				title = name
			}
			if n, err := parseOrdered(files["mod.json"]); err == nil {
				n.set("title", ordered(newMsg("{title} (from {host})", "title", title, "host", host).in(lang), noOrder, nil))
				files = withFile(files, "mod.json", n.pyIndent())
			}
		}
		for k := 2; ; k++ {
			cand := keptName(name, k)
			cd, err := modDir(cand)
			if err != nil {
				return "", err
			}
			if !isDir(cd) {
				target = cand
				break
			}
			if samePack(cand, files, true) { // (kept before)
				return cand, nil
			}
		}
	}
	d, err := modDir(target)
	if err != nil {
		return "", err
	}
	sum := sha1.Sum([]byte(target))
	tmp := filepath.Join(modsDir(), ".keep-"+hex.EncodeToString(sum[:4]))
	os.RemoveAll(tmp)
	if err := os.MkdirAll(tmp, 0o777); err != nil {
		return "", err
	}
	names := make([]string, 0, len(files))
	for f := range files {
		names = append(names, f)
	}
	sort.Strings(names)
	for _, f := range names {
		if err := os.WriteFile(filepath.Join(tmp, f), files[f], 0o666); err != nil {
			os.RemoveAll(tmp)
			return "", err
		}
	}
	if err := os.Rename(tmp, d); err != nil {
		os.RemoveAll(tmp)
		return "", err
	}
	return target, nil
}

func withFile(files map[string][]byte, f string, b []byte) map[string][]byte {
	out := map[string][]byte{}
	for k, v := range files {
		out[k] = v
	}
	out[f] = b
	return out
}

// offered: a pack of the host's as a guest has it -- its files (nil: they could not be read)
type offered struct {
	name, title string
	files       map[string][]byte
}

// takeOffer: the host's packs with its "mod" message (an older host's: none)
func (l *lobby) takeOffer(v any) {
	list, _ := offerOK(v)
	var out []offered
	for _, x := range list {
		p := x.(obj)
		o := offered{name: p["name"].(string), title: p["title"].(string)}
		if files, err := decodePack(p["data"].(string)); err == nil {
			o.files = files
		}
		out = append(out, o)
	}
	l.mu.Lock()
	l.offer = out
	l.mu.Unlock()
}

// offerView: the host's packs for the page -- {name, title, state: new, same, differs, bad};
// under the lobby's lock
func (l *lobby) offerView() []any {
	out := []any{}
	for _, o := range l.offer {
		state := "bad"
		if o.files != nil {
			state = packState(o.name, o.files)
		}
		out = append(out, obj{"name": o.name, "title": o.title, "state": state})
	}
	return out
}

// keepOffer: the host's pack WHICH (a name; none: all of them) kept in this machine's mods/
// -> [{name, as}] (as: the name it got here; a renamed one's title says whose it was, in LANG)
func (l *lobby) keepOffer(which any, lang string) ([]any, error) {
	l.mu.Lock()
	offer := append([]offered(nil), l.offer...)
	host := "Host"
	if l.state != nil {
		if ps, ok := l.state["players"].([]any); ok {
			for _, p := range ps {
				if m, ok := p.(obj); ok && truthy(m["host"]) {
					if n, ok := m["name"].(string); ok && n != "" {
						host = n
					}
				}
			}
		}
	}
	l.mu.Unlock()
	want, _ := which.(string)
	kept := []any{}
	for _, o := range offer {
		if want != "" && o.name != want {
			continue
		}
		if o.files == nil {
			if want != "" {
				return nil, newMsg("this pack of the host's cannot be read here")
			}
			continue
		}
		n, err := keepPack(o.name, o.files, host, lang)
		if err != nil {
			return kept, err
		}
		kept = append(kept, obj{"name": o.name, "as": n})
	}
	if want != "" && len(kept) == 0 {
		return nil, newMsg("no such thing")
	}
	return kept, nil
}
