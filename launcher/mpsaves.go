package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"time"
)

// The network games' savegames (src/mpsave.c; the former Python lobby (mplobby.py)): a directory per
// game -- game.json (its start's packs, rules and tables) and a savegame per night,
// tag-NNN.mtvmp -- and the files' hash a network game compares.

const (
	saveMagic = "MTVMPSV1"
	saveMax   = 1 << 20 // a savegame file: some 40 KB (a mod's extra films: more)
)

var saveFileRE = regexp.MustCompile(`^tag-\d{3}\.mtvmp$`)

// fnv64: FNV-1a, 64 bits
func fnv64(h uint64, data []byte) uint64 {
	for _, b := range data {
		h = (h ^ uint64(b)) * 1099511628211
	}
	return h
}

const fnvStart uint64 = 1469598103934665603

// saveHash: a savegame's name in a network game's parameters (start=load:)
func saveHash(data []byte) string { return fmt.Sprintf("%016x", fnv64(fnvStart, data)) }

// contentHash: the game's files as a network game compares them (runtime/dos.c
// dos_game_files_hash, tools/rsc.py content_hash): FNV-1a 64 over every resource of
// SOURCE1.RSC and SOURCE2.RSC -- id, size (16 bits each) and bytes, in id order --
// but the savegame slots D1h..DBh; false if a file is missing
func contentHash(dir string) (uint64, bool) {
	var paths []string
	for _, n := range []string{"SOURCE1.RSC", "SOURCE2.RSC"} {
		p := findFile(dir, n)
		if p == "" {
			return 0, false
		}
		paths = append(paths, p)
	}
	res, err := readResources(paths)
	if err != nil {
		return 0, false
	}
	ids := map[int]bool{}
	for _, f := range res {
		for i := range f.idx {
			ids[i] = true
		}
	}
	sorted := make([]int, 0, len(ids))
	for i := range ids {
		sorted = append(sorted, i)
	}
	sort.Ints(sorted)
	h := fnvStart
	for _, i := range sorted {
		if i >= 0xD1 && i < 0xDC {
			continue
		}
		data, _ := res.get(i)
		var head [4]byte
		binary.LittleEndian.PutUint16(head[:], uint16(i))
		n, _ := res.where(i)
		binary.LittleEndian.PutUint16(head[2:], uint16(res[n].idx[i][1]))
		h = fnv64(fnv64(h, head[:]), data)
	}
	return h, true
}

// findFile: NAME in directory D, whatever its case
func findFile(d, name string) string {
	entries, err := os.ReadDir(d)
	if err != nil {
		return ""
	}
	for _, e := range entries {
		if strings.EqualFold(e.Name(), name) {
			return filepath.Join(d, e.Name())
		}
	}
	return ""
}

// saveMeta: a network game's savegame -> its description (day, humans, files, rules)
func saveMeta(data []byte) (obj, error) {
	if len(data) < 12 || string(data[:8]) != saveMagic {
		return nil, errors.New("not a network game's savegame")
	}
	n := int(binary.LittleEndian.Uint32(data[8:]))
	if 12+n > len(data) {
		return nil, errors.New("a savegame without its description")
	}
	v, err := parseJSON(data[12 : 12+n])
	meta, ok := v.(obj)
	if err != nil || !ok {
		return nil, errors.New("a savegame without its description")
	}
	humans, ok := meta["humans"].([]any)
	_, dayOK := meta["day"].(int64)
	good := dayOK && ok && len(humans) >= 1 && len(humans) <= 3
	for _, x := range humans {
		h, ok := x.(obj)
		st, sok := h["station"].(int64)
		_, nameOK := h["name"].(string)
		if _, has := h["name"]; !has {
			nameOK = true
		}
		good = good && ok && sok && st >= 1 && st <= 3 && nameOK
	}
	if !good {
		return nil, errors.New("a savegame with a broken description")
	}
	return meta, nil
}

var notID = regexp.MustCompile(`[^A-Za-z0-9_-]`)

// safeID: a game's name as a directory: letters, digits, - and _ (what the server sends
// is not trusted)
func safeID(v any) string {
	s := ""
	switch x := v.(type) {
	case string:
		s = x
	case nil:
	default:
		s = fmt.Sprint(x)
	}
	s = notID.ReplaceAllString(s, "")
	if len(s) > 40 {
		s = s[:40]
	}
	return s
}

func safeFile(v any) (string, error) {
	s, _ := v.(string)
	f := filepath.Base(strings.ReplaceAll(s, "\\", "/"))
	if s == "" || !saveFileRE.MatchString(f) {
		return "", fmt.Errorf("no savegame: %s", pyRepr(s))
	}
	return f, nil
}

// gameInfo: a game's game.json, the types checked: what a host sent is not trusted
func gameInfo(path string) obj {
	info := readJSONObj(path)
	var packs []any
	if l, ok := info["packs"].([]any); ok {
		for _, p := range l {
			if s, ok := p.(string); ok && len(packs) < 16 {
				packs = append(packs, s)
			}
		}
	}
	if packs == nil {
		packs = []any{}
	}
	info["packs"] = packs
	if _, ok := info["settings"].(obj); !ok {
		info["settings"] = obj{}
	}
	if _, ok := info["mod_data"].(string); !ok {
		delete(info, "mod_data")
	}
	if _, ok := info["created"].(string); !ok {
		info["created"] = nil
	}
	if _, ok := info["title"].(string); !ok {
		delete(info, "title")
	}
	return info
}

func mtime(p string) float64 {
	st, err := os.Stat(p)
	if err != nil {
		return 0
	}
	return float64(st.ModTime().UnixNano()) / 1e9
}

// savesList: the network games saved in TOP, the latest first, each with its nights (the
// latest first) -- as the former Python lobby (mplobby.py) saves_list
func savesList(top string) []obj {
	out := []obj{}
	entries, err := os.ReadDir(top)
	if top == "" || err != nil {
		return out
	}
	for _, e := range entries {
		g := e.Name()
		d := filepath.Join(top, g)
		if safeID(g) != g || !isDir(d) {
			continue
		}
		info := gameInfo(filepath.Join(d, "game.json"))
		var saves []obj
		files, _ := os.ReadDir(d)
		for _, f := range files {
			if !saveFileRE.MatchString(f.Name()) {
				continue
			}
			p := filepath.Join(d, f.Name())
			st, err := os.Stat(p)
			if err != nil || st.Size() > saveMax {
				continue
			}
			data, err := os.ReadFile(p)
			if err != nil {
				continue
			}
			meta, err := saveMeta(data)
			if err != nil {
				continue
			}
			humans := meta["humans"]
			if humans == nil {
				humans = []any{}
			}
			var fixes any
			if s, ok := meta["fixes"].(string); ok {
				fixes = s
			}
			saves = append(saves, obj{"file": f.Name(), "day": meta["day"], "humans": humans, "difficulty": meta["difficulty"],
				"fixes": fixes, "hash": saveHash(data), "size": len(data), "time": mtime(p)})
		}
		if len(saves) == 0 {
			continue
		}
		sort.SliceStable(saves, func(i, j int) bool {
			di, dj := intOf(saves[i], "day"), intOf(saves[j], "day")
			if di != dj {
				return di > dj
			}
			return saves[i]["file"].(string) > saves[j]["file"].(string)
		})
		latest := 0.0
		for _, s := range saves {
			latest = max(latest, s["time"].(float64))
		}
		packs := info["packs"].([]any)
		loadable := len(packs) == 0 || truthy(info["mod_data"])
		var why any
		if !loadable {
			why = "tables"
		}
		classic := saves[0]["fixes"] == "classic" || saves[0]["fixes"] == "none"
		list := make([]any, len(saves))
		for i, s := range saves {
			list[i] = s
		}
		out = append(out, obj{"game": g, "title": info["title"], "created": info["created"], "packs": packs, "settings": info["settings"],
			"time": latest, "classic": classic, "loadable": loadable, "why": why, "saves": list})
	}
	sort.SliceStable(out, func(i, j int) bool { return out[i]["time"].(float64) > out[j]["time"].(float64) })
	return out
}

func savedGame(top string, game any) (string, error) {
	g := safeID(game)
	d := filepath.Join(top, g)
	if g == "" || top == "" || !isDir(d) {
		return "", newMsg("no such saved game")
	}
	return d, nil
}

// saveRename: a saved game's name (game.json "title", 40 characters at most); empty: its
// players' names again
func saveRename(top string, game, title any) error {
	d, err := savedGame(top, game)
	if err != nil {
		return err
	}
	p := filepath.Join(d, "game.json")
	info := readJSONObj(p)
	t, _ := title.(string)
	t = strings.Join(strings.Fields(t), " ")
	if r := []rune(t); len(r) > 40 {
		t = string(r[:40])
	}
	if t != "" {
		info["title"] = t
	} else {
		delete(info, "title")
	}
	return writeJSONFile(p, info)
}

// saveDelete: a night's savegame (with the other copies of it kept beside it), or (no
// file) the whole game -- on this machine only; the last night gone, the game goes too
func saveDelete(top string, game, file any) error {
	d, err := savedGame(top, game)
	if err != nil {
		return err
	}
	if !truthy(file) {
		return os.RemoveAll(d)
	}
	f, err := safeFile(file)
	if err != nil {
		return err
	}
	entries, _ := os.ReadDir(d)
	for _, e := range entries {
		if e.Name() == f || strings.HasPrefix(e.Name(), f+".") { // tag-NNN.mtvmp and tag-NNN.mtvmp.<hash>.bak
			os.Remove(filepath.Join(d, e.Name()))
		}
	}
	entries, _ = os.ReadDir(d)
	for _, e := range entries {
		if saveFileRE.MatchString(e.Name()) {
			return nil
		}
	}
	return os.RemoveAll(d)
}

// testSave: the self-test's savegame -- a description as the game writes it and empty
// bytes (the lobby reads only the description; nothing is started in a dry lobby)
func testSave(top, other string, gameDir string, b obj) ([]obj, error) {
	game := safeID(b["game"])
	if game == "" {
		game = "selftest"
	}
	if other != top { // (the second player fetches it anew)
		os.RemoveAll(other)
	}
	d := filepath.Join(top, game)
	if err := os.MkdirAll(d, 0o777); err != nil {
		return nil, err
	}
	day := intOf(b, "day")
	if day == 0 {
		day = 2
	}
	h, _ := contentHash(gameDir)
	humans := b["humans"]
	if !truthy(humans) {
		humans = []any{obj{"station": int64(1), "out": false, "name": "Anna"}, obj{"station": int64(2), "out": false, "name": "Ben"}}
	}
	meta := []byte(pyDumpsSpaced(obj{"format": int64(1), "day": day, "difficulty": int64(2), "files": fmt.Sprintf("%016x", h),
		"fixes": "all", "speed4": int64(0), "humans": humans}))
	const hl = 627 // (src/mpsave.c human_ranges)
	hum := []byte{1, 0, 2}
	for _, s := range []byte{1, 2} {
		hum = append(hum, s, 0, hl&255, hl>>8)
		hum = append(hum, make([]byte, hl)...)
	}
	data := []byte(saveMagic)
	data = binary.LittleEndian.AppendUint32(data, uint32(len(meta)))
	data = append(data, meta...)
	data = binary.LittleEndian.AppendUint32(data, 37605)
	data = append(data, make([]byte, 37605)...)
	data = binary.LittleEndian.AppendUint32(data, uint32(len(hum)))
	data = append(data, hum...)
	if err := os.WriteFile(filepath.Join(d, fmt.Sprintf("tag-%03d.mtvmp", day)), data, 0o666); err != nil {
		return nil, err
	}
	if err := writeJSONFile(filepath.Join(d, "game.json"), obj{"game": game, "created": "2026-10-11 20:00", "packs": []any{},
		"settings": obj{"fixes": "all", "speed4": false}}); err != nil {
		return nil, err
	}
	return savesList(top), nil
}

// pyDumpsSpaced: json.dumps with its default separators (", ", ": "), keys in order given
// by the test save (Python keeps insertion order; the savegame's text is only read back)
func pyDumpsSpaced(v any) string {
	s := pyDumps(v)
	var b strings.Builder
	in := false
	for i := 0; i < len(s); i++ {
		c := s[i]
		b.WriteByte(c)
		switch {
		case in && c == '\\': // (an escape: the next byte is the string's, a quote too)
			i++
			b.WriteByte(s[i])
		case c == '"':
			in = !in
		case !in && (c == ',' || c == ':'):
			b.WriteByte(' ')
		}
	}
	return b.String()
}

func nowStamp() string { return time.Now().Format("2006-01-02 15:04") }
