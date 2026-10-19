package main

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"sync"
)

// The content editor -- the launcher's page on its "Editor" (build/openmtv editor opens
// it there; what the former Python editor (editor.py) was): the films, series, ads, screenplays
// and news of the original, the packs (mods/NAME/) made, changed, renamed and deleted.
// The same pages (tools/editor/: /api/meta?editor=1, without "player"), the same
// answers; a pack's files are written as Python wrote them (ojson.go), so a change
// shows in git as itself.

var textRes = map[string]int{"de": 199, "en": 253} // the game's string tables (catalog.py TEXT_RES)

// editorData: the font and what the editor's page shows in the game's own words --
// made once, the first time the page asks
type editorData struct {
	once    sync.Once
	err     error
	font    []byte
	strings obj // de/en -> the game's strings
	meta    obj
}

func (s *server) editorData() (*editorData, error) {
	e := s.ed
	e.once.Do(func() { e.err = e.load(s) })
	return e, e.err
}

func (e *editorData) load(s *server) error {
	res := s.pics.res
	f, err := readGameFont(res)
	if err != nil {
		return err
	}
	e.font = f.ttf("Mad TV")
	e.strings = obj{}
	str := map[string][]string{}
	for lang, r := range textRes {
		data, err := res.get(r)
		if err != nil {
			return err
		}
		var list []string
		for _, p := range bytes.Split(data, []byte{0}) {
			rs := []rune(cp437Decode(p))
			for i, c := range rs {
				if c < ' ' {
					rs[i] = ' '
				}
			}
			list = append(list, string(rs))
		}
		str[lang] = list
		e.strings[lang] = list
	}
	at := func(l []string, i int) any {
		if i < len(l) {
			return l[i]
		}
		return nil
	}
	tbl, fg, sg, co, card := obj{}, obj{}, obj{}, obj{}, obj{}
	for _, t := range tables {
		m := t.max
		if m == 0 {
			m = t.count
		}
		tbl[t.name] = obj{"count": int64(t.count), "size": int64(t.size), "max": int64(m)}
	}
	for lang, l := range str {
		var a, b, c []any
		for g := 1; g <= 10; g++ {
			a = append(a, at(l, 26+g))
		}
		for g := 1; g <= 5; g++ {
			b = append(b, at(l, 292+g))
		}
		for i := 318; i < 328; i++ {
			c = append(c, at(l, i))
		}
		fg[lang], sg[lang], co[lang] = a, b, c
		cd := obj{}
		for _, k := range []int{39, 40, 77, 78, 79, 80, 376, 377, 382, 383, 385, 412, 413, 414, 415, 416, 417, 418, 419, 425} {
			cd[fmt.Sprint(k)] = at(l, k)
		}
		card[lang] = cd
	}
	metrics, glyphs := f.metrics()
	tv := []int64{}
	for i := int64(220); i < 239; i++ {
		tv = append(tv, i)
	}
	e.meta = obj{"tables": tbl, "film_genres": fg, "series_genres": sg, "countries": co, "card": card,
		"audience_classes": []any{int64(0), 0.25, 0.5, int64(1), 2.5, int64(5), int64(10), int64(25), int64(50), int64(100)},
		"metrics":          metrics, "glyphs": glyphs, "tv_pictures": tv, "datafixes": s.g.datafixes,
		"fields": obj{}, "fields_de": obj{}, "genre_aud": []int64{}}
	if data, err := s.pageFile("fields.json"); err == nil { // (the page works without the descriptions)
		if v, err := parseOrdered(data); err == nil && v.get("en") != nil && v.get("de") != nil {
			e.meta["fields"], e.meta["fields_de"] = v.get("en"), v.get("de")
		}
	}
	if d, err := res.get(0xD0); err == nil { // the audience of a genre by hour (DS:7160, 16 genre codes x 8 hours)
		o := 0
		for _, n := range [][2]int{{0x1D00, 8}, {0x6165, 0xD0}, {0x8BDC, 0x5C}, {0x5013, 0x540}, {0x6235, 0x180}, {0x268E, 0x60},
			{0x1D09, 0x738}, {0x724A, 0x58}, {0x7B42, 0xB4}, {0x7C82, 0x6AC}} { // (catalog.py INIT_LAYOUT before 0x7160)
			o += n[1]
		}
		var ga []int64
		for i := o; i < o+0x80 && i < len(d); i++ {
			ga = append(ga, int64(d[i]))
		}
		if ga != nil {
			e.meta["genre_aud"] = ga
		}
	}
	return nil
}

// pageFile: a file of the pages -- tools/editor/ beside the program, else the one built in
func (s *server) pageFile(rel string) ([]byte, error) {
	if exists(filepath.Join(s.static, "index.html")) {
		return os.ReadFile(filepath.Join(s.static, filepath.FromSlash(rel)))
	}
	return webFS.ReadFile("web/" + rel)
}

// ---------------------------------------------------------------- a pack's tables, in their order

// orderedTable: a pack's table as its file has it, the original's (in the export's
// order) where it has none, and the original's entries it leaves out put back (in the
// export's order, withOriginal) -- the former Python editor (editor.py) mod_table
func orderedTable(g *game, name, t string) (*onode, error) {
	if _, err := modTable(g, name, t); err != nil { // (the same checks and messages)
		return nil, err
	}
	d, _ := modDir(name)
	p := filepath.Join(d, t+".json")
	if !exists(p) {
		return canonical(t, g.original[t]), nil
	}
	n, err := readOrdered(p)
	if err != nil {
		return nil, fmt.Errorf("%s: %v", p, err)
	}
	doc, _ := n.value().(obj)
	have, list, err := records(doc)
	if err != nil {
		return n, nil
	}
	_, olist, _ := records(g.original[t])
	items := append([]*onode(nil), n.get("records").items...)
	for _, o := range olist {
		if _, ok := have[o["id"].(int64)]; !ok {
			items = append(items, canonical(t, obj{"records": []any{o}}).get("records").items[0])
		}
	}
	if len(items) == len(list) {
		return n, nil
	}
	id := func(r *onode) int64 { return r.get("id").valueOr(nil).(int64) }
	sort.SliceStable(items, func(i, j int) bool { return id(items[i]) < id(items[j]) })
	out := n.copy()
	out.set("records", &onode{kind: 'a', items: items})
	return out, nil
}

func orderedDocs(g *game, name string) (map[string]*onode, error) {
	out := map[string]*onode{}
	for _, t := range tables {
		n, err := orderedTable(g, name, t.name)
		if err != nil {
			return nil, err
		}
		out[t.name] = n
	}
	return out, nil
}

// orderedSelection: what the player has chosen, its tables in order -- the original
// alone, one pack as saved, or packs played together (combine: an entry of a pack in
// that pack's order, with its new number and target)
func orderedSelection(g *game, spec string) (map[string]*onode, packReport, error) {
	if spec == "" || spec == "original" {
		return canonicalDocs(g.original), packReport{Packs: []packAdded{}, Conflicts: []conflict{}}, nil
	}
	names, err := packNames(spec)
	if err != nil {
		return nil, packReport{}, err
	}
	var packs []pack
	var odocs []map[string]*onode
	for _, n := range names {
		docs, err := modDocs(g, n)
		if err != nil {
			return nil, packReport{}, err
		}
		od, err := orderedDocs(g, n)
		if err != nil {
			return nil, packReport{}, err
		}
		packs = append(packs, pack{n, docs})
		odocs = append(odocs, od)
	}
	docs, report, from, err := combineFrom(g.original, packs)
	if err != nil {
		return nil, report, err
	}
	if len(names) == 1 {
		return odocs[0], report, nil
	}
	out := map[string]*onode{}
	for _, t := range tables {
		byID := map[int]map[int64]*onode{} // pack -> its entries' trees by number (the last of a number twice, as a dict)
		entry := func(k int, id int64) *onode {
			if byID[k] == nil {
				byID[k] = map[int64]*onode{}
				if rs := odocs[k][t.name].get("records"); rs != nil {
					for _, r := range rs.items {
						if v, ok := r.get("id").valueOr(nil).(int64); ok {
							byID[k][v] = r
						}
					}
				}
			}
			return byID[k][id]
		}
		doc := canonical(t.name, docs[t.name])
		recs, _ := docs[t.name]["records"].([]any)
		list := &onode{kind: 'a', items: make([]*onode, len(recs))}
		for i, rv := range recs {
			r := rv.(obj)
			nid, _ := r["id"].(int64)
			list.items[i] = canonical(t.name, obj{"records": []any{r}}).get("records").items[0]
			if src, ok := from[t.name][nid]; ok {
				if e := entry(src.pack, src.id); e != nil {
					c := e.copy()
					c.set("id", ordered(r["id"], noOrder, nil))
					if _, ok := r["target"]; ok && t.name == "screenplays" {
						c.set("target", ordered(r["target"], noOrder, nil))
					}
					list.items[i] = c
				}
			}
		}
		doc.set("records", list)
		out[t.name] = doc
	}
	return out, report, nil
}

func noOrder([]string, obj) []string { return nil }

func valuesOf(docs map[string]*onode) map[string]obj {
	out := map[string]obj{}
	for t, n := range docs {
		out[t], _ = n.value().(obj)
	}
	return out
}

// ---------------------------------------------------------------- packs made, changed, renamed, deleted

func strOr(v any, alt string) string {
	if s, ok := v.(string); ok && s != "" {
		return s
	}
	return alt
}

// createMod: a new pack -- the original's tables, a copy of one pack, or (complete) all
// the content of a selection (the original alone, or packs played together: A+B)
func createMod(g *game, name, title string, desc any, src string, complete bool) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	if name == "original" {
		return nil, newMsg("\"{name}\" names the original game", "name", name)
	}
	if exists(d) {
		return nil, newMsg("mod \"{name}\" exists already", "name", name)
	}
	var docs map[string]*onode
	var names []string
	switch {
	case complete:
		if docs, _, err = orderedSelection(g, strOr(src, "original")); err != nil {
			return nil, err
		}
		if src != "" && src != "original" {
			if names, err = packNames(src); err != nil {
				return nil, err
			}
		}
	case src != "":
		if docs, err = orderedDocs(g, src); err != nil {
			return nil, err
		}
		names = []string{src}
	default:
		docs = canonicalDocs(g.original)
	}
	if err := os.MkdirAll(d, 0o777); err != nil {
		return nil, err
	}
	for _, t := range tables {
		if err := writeOrdered(filepath.Join(d, t.name+".json"), docs[t.name]); err != nil {
			return nil, err
		}
	}
	info := &onode{kind: 'o'}
	info.set("title", ordered(strOr(title, name), noOrder, nil))
	info.set("description", ordered(desc, noOrder, nil))
	if len(names) > 0 {
		info.set("from", ordered(names, noOrder, nil))
	}
	if complete {
		info.set("complete", &onode{kind: 't'})
	}
	if err := writeOrdered(filepath.Join(d, "mod.json"), info); err != nil {
		return nil, err
	}
	return modInfo(g, name)
}

// deleteMod: a pack of the player's own goes (its folder); a protected one stays; its
// build folder (build/mods/NAME, with the games saved in it) stays too
func deleteMod(name string) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	if !isDir(d) {
		return nil, newMsg("no mod {name}", "name", name)
	}
	meta, err := modMeta(name)
	if err != nil {
		return nil, err
	}
	if truthy(meta["protected"]) {
		return nil, newMsg("\"{name}\" is protected and stays", "name", name)
	}
	if err := os.RemoveAll(d); err != nil {
		return nil, err
	}
	return obj{"deleted": name}, nil
}

// setModInfo: a pack's title and description; what else mod.json holds (where it
// started from) stays
func setModInfo(g *game, name string, title, desc any) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	if !isDir(d) {
		return nil, newMsg("no mod {name}", "name", name)
	}
	p := filepath.Join(d, "mod.json")
	meta := &onode{kind: 'o'}
	if exists(p) {
		if meta, err = readOrdered(p); err != nil {
			return nil, fmt.Errorf("%s: %v", p, err)
		}
		if meta.kind != 'o' {
			return nil, fmt.Errorf("%s: not an object", p)
		}
	}
	if s, _ := title.(string); s == "" && !truthy(title) {
		title = name
	}
	meta.set("title", ordered(title, noOrder, nil))
	meta.set("description", ordered(desc, noOrder, nil))
	if err := writeOrdered(p, meta); err != nil {
		return nil, err
	}
	return modInfo(g, name)
}

// saveTable: a table of a pack, as the page sent its entries; a protected pack only
// when the page unlocked it on purpose.  A pack that is not complete keeps only its own
// entries (ownEntries): the original's come from the player's game files
func saveTable(g *game, name, t string, data []byte) (obj, error) {
	if _, ok := tableIdx[t]; !ok {
		return nil, newMsg("no table {name}", "name", t)
	}
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	if !isDir(d) {
		return nil, newMsg("no mod {name}", "name", name)
	}
	sent, err := parseOrdered(data)
	if err != nil || sent.kind != 'o' || sent.get("records") == nil {
		return nil, newMsg("no list of records")
	}
	meta, err := modMeta(name)
	if err != nil {
		return nil, err
	}
	if truthy(meta["protected"]) && !truthy(sent.get("unlock").valueOr(nil)) {
		return nil, newMsg("\"{name}\" is protected: unlock it in the overview to change it directly", "name", name)
	}
	docs, err := modDocs(g, name)
	if err != nil {
		return nil, err
	}
	doc, err := orderedTable(g, name, t)
	if err != nil {
		return nil, err
	}
	doc = doc.copy()
	doc.set("records", sent.get("records"))
	docs[t], _ = doc.value().(obj)
	if err := checkDocs(docs); err != nil {
		return nil, err
	}
	if !truthy(meta["complete"]) { // (a complete pack holds the original itself)
		doc = ownEntries(doc, g.original[t])
	}
	if err := writeOrdered(filepath.Join(d, t+".json"), doc); err != nil {
		return nil, err
	}
	return modInfo(g, name)
}

// valueOr: the node's value, ALT when there is none
func (n *onode) valueOr(alt any) any {
	if n == nil {
		return alt
	}
	return n.value()
}

// ---------------------------------------------------------------- the editor's requests

// editorGet: GET /api/... of the editor's own; ok false: not one of them
func (s *server) editorGet(parts []string, lang string) (reply, bool) {
	switch {
	case len(parts) == 1 && parts[0] == "strings":
		e, err := s.editorData()
		if err != nil {
			return failure(err, lang), true
		}
		return reply{200, e.strings}, true
	case len(parts) == 2 && parts[0] == "original":
		if _, ok := tableIdx[parts[1]]; ok {
			return reply{200, canonical(parts[1], s.g.original[parts[1]])}, true
		}
	case len(parts) == 3 && parts[0] == "mods":
		if _, ok := tableIdx[parts[2]]; ok {
			n, err := orderedTable(s.g, parts[1], parts[2])
			if err != nil {
				return failure(err, lang), true
			}
			return reply{200, n}, true
		}
	case len(parts) == 2 && parts[0] == "selection": // /api/selection/A+B (or original): all tables
		docs, report, err := orderedSelection(s.g, parts[1])
		if err != nil {
			return failure(err, lang), true
		}
		return reply{200, obj{"tables": docs, "report": obj{"packs": report.Packs, "conflicts": report.Conflicts}}}, true
	}
	return reply{}, false
}

// editorMeta: /api/meta of the editor -- labels in the game's own words, field
// descriptions (tools/editor/fields.json), limits, the font's measures
func (s *server) editorMeta(lang string) reply {
	e, err := s.editorData()
	if err != nil {
		return failure(err, lang)
	}
	return reply{200, e.meta}
}

func (s *server) editorPost(parts []string, b obj, lang string) (reply, bool) {
	switch {
	case len(parts) == 1 && parts[0] == "mods":
		info, err := createMod(s.g, str(b["name"]), str(b["title"]), descOf(b), str(b["from"]), truthy(b["complete"]))
		if err != nil {
			return failure(err, lang), true
		}
		return reply{200, info}, true
	case len(parts) == 3 && parts[0] == "mods" && parts[2] == "info":
		title, ok := b["title"]
		if !ok {
			title = parts[1]
		}
		info, err := setModInfo(s.g, parts[1], title, descOf(b))
		if err != nil {
			return failure(err, lang), true
		}
		return reply{200, info}, true
	}
	return reply{}, false
}

func descOf(b obj) any {
	if d, ok := b["description"]; ok {
		return d
	}
	return ""
}

func (s *server) editorPut(r *http.Request, parts []string, lang string) (reply, bool) {
	if len(parts) == 3 && parts[0] == "mods" {
		if _, ok := tableIdx[parts[2]]; ok {
			data, err := io.ReadAll(io.LimitReader(r.Body, 64<<20))
			if err != nil {
				return failure(err, lang), true
			}
			info, err := saveTable(s.g, parts[1], parts[2], data)
			if err != nil {
				return failure(err, lang), true
			}
			return reply{200, info}, true
		}
	}
	return reply{}, false
}

func (s *server) editorDelete(parts []string, lang string) reply {
	if len(parts) == 2 && parts[0] == "mods" {
		out, err := deleteMod(parts[1])
		if err != nil {
			return failure(err, lang)
		}
		return reply{200, out}
	}
	return fail(404, newMsg("no such thing").in(lang))
}

// serveFont: /font.ttf, the game's font
func (s *server) serveFont(w http.ResponseWriter, lang string) {
	e, err := s.editorData()
	if err != nil {
		s.send(w, failure(err, lang))
		return
	}
	w.Header().Set("Content-Type", "font/ttf")
	w.Header().Set("Cache-Control", "max-age=86400")
	w.Header().Set("Content-Length", fmt.Sprint(len(e.font)))
	w.Write(e.font)
}
