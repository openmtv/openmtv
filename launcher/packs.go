package main

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// Content packs (mods/NAME/), alone or played together (A+B+C), built into copies
// of the game files: what the former Python editor (editor.py) build does, with tools/modpacks.py's
// combination and the editor's checks, the same messages (in English, as the
// editor's command line has them).

const packSep = "+"   // between the names: build/mods/A+B, --mod A+B
const packLimit = 999 // the highest number of a film, series or ad (content.py)

var (
	growing  = []string{"films", "series", "ads"}            // tables a pack can add entries to
	nameRE   = regexp.MustCompile(`\A[A-Za-z0-9_-]{1,40}\z`) // (not Python's $: no final newline)
	tableIdx = map[string]int{}
)

func init() {
	for i, t := range tables {
		tableIdx[t.name] = i
	}
}

// game: the original game files and their tables with the data fixes applied (the
// editor's "original", what a pack leaves out)
type game struct {
	dir       string
	paths     []string // SOURCE1.RSC, SOURCE2.RSC
	original  map[string]obj
	datafixes []string // the data fixes applied to it (tools/datafixes.py)
}

func openGame(dir string) (*game, error) {
	g := &game{dir: absPath(dir)}
	for _, f := range []string{"SOURCE1.RSC", "SOURCE2.RSC"} {
		p := filepath.Join(g.dir, f)
		if !exists(p) {
			return nil, fmt.Errorf("%s not found (--game DIR: the directory with the original game files)", p)
		}
		g.paths = append(g.paths, p)
	}
	res, err := readResources(g.paths)
	if err != nil {
		return nil, err
	}
	docs, err := export(res)
	if err != nil {
		return nil, err
	}
	g.original, g.datafixes = applyDataFixes(docs)
	return g, nil
}

// modsDir: the packs, mods/ -- $MADTV_MODS for tests that bring packs of their own
// (tests/packs: the shipped content packs are not in the public source)
func modsDir() string {
	if d := os.Getenv("MADTV_MODS"); d != "" {
		return d
	}
	return filepath.Join(root, "mods")
}

func modDir(name string) (string, error) {
	if !nameRE.MatchString(name) {
		return "", newMsg("a mod name is 1-40 letters, digits, - or _")
	}
	return filepath.Join(modsDir(), name), nil
}

func isDir(p string) bool {
	st, err := os.Stat(p)
	return err == nil && st.IsDir()
}

func modTable(g *game, name, table string) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	p := filepath.Join(d, table+".json")
	if !exists(p) {
		return g.original[table], nil
	}
	v, err := readJSON(p)
	if err != nil {
		return nil, fmt.Errorf("%s: %v", p, err)
	}
	doc, ok := v.(obj)
	if !ok {
		return nil, fmt.Errorf("%s: not a table", p)
	}
	return withOriginal(doc, g.original[table]), nil
}

// withOriginal: a pack's table as the game plays it.  A pack keeps only its own
// entries -- the ones it adds and the original's it changes (saveTable); the others are
// the original's, from the player's own game files.  (A table that holds them all, as
// every pack's did before 2026-10-19 and a complete pack's does, stays as it is.)
func withOriginal(doc, original obj) obj {
	have, list, err := records(doc)
	if err != nil {
		return doc // (the checks name what is wrong with it)
	}
	_, olist, _ := records(original)
	merged := append([]obj(nil), list...)
	for _, o := range olist {
		if _, ok := have[o["id"].(int64)]; !ok {
			merged = append(merged, deepCopy(o).(obj))
		}
	}
	if len(merged) == len(list) {
		return doc
	}
	sort.SliceStable(merged, func(i, j int) bool { return merged[i]["id"].(int64) < merged[j]["id"].(int64) })
	out := obj{}
	for k, v := range doc {
		out[k] = v
	}
	recs := make([]any, len(merged))
	for i, r := range merged {
		recs[i] = r
	}
	out["records"] = recs
	return out
}

// ownEntries: what a pack keeps of a table on disk -- its entries without the
// original's it leaves as they are (withOriginal puts those back)
func ownEntries(n *onode, original obj) *onode {
	orig, _, _ := records(original)
	keep := &onode{kind: 'a'}
	for _, r := range n.get("records").items {
		if rec, ok := r.value().(obj); ok {
			if id, ok := rec["id"].(int64); ok {
				if o, ok := orig[id]; ok && equal(rec, o) {
					continue
				}
			}
		}
		keep.items = append(keep.items, r)
	}
	out := n.copy()
	out.set("records", keep)
	return out
}

func modDocs(g *game, name string) (map[string]obj, error) {
	docs := map[string]obj{}
	for _, t := range tables {
		doc, err := modTable(g, name, t.name)
		if err != nil {
			return nil, err
		}
		docs[t.name] = doc
	}
	return docs, nil
}

// modMeta: mod.json -- title, description, from (the packs it was made from), complete, protected
func modMeta(name string) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	p := filepath.Join(d, "mod.json")
	if !exists(p) {
		return obj{}, nil
	}
	v, err := readJSON(p)
	if err != nil {
		return nil, fmt.Errorf("%s: %v", p, err)
	}
	m, ok := v.(obj)
	if !ok {
		return nil, fmt.Errorf("%s: not an object", p)
	}
	return m, nil
}

// fromList: where a mod started from, as a list (old mods name one mod as a string)
func fromList(meta obj) []string {
	switch f := meta["from"].(type) {
	case string:
		if f != "" {
			return []string{f}
		}
	case []any:
		var out []string
		for _, x := range f {
			if s, ok := x.(string); ok {
				out = append(out, s)
			}
		}
		return out
	}
	return nil
}

// packContents: the packs a pack holds -- itself, and for a complete pack the packs it
// was made from
func packContents(name string, seen map[string]bool) (map[string]bool, error) {
	if seen[name] {
		return map[string]bool{}, nil
	}
	seen[name] = true
	out := map[string]bool{name: true}
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	meta := obj{}
	if isDir(d) {
		if meta, err = modMeta(name); err != nil {
			return nil, err
		}
	}
	if truthy(meta["complete"]) {
		for _, f := range fromList(meta) {
			if fd, err := modDir(f); err == nil && isDir(fd) {
				sub, err := packContents(f, seen)
				if err != nil {
					return nil, err
				}
				for c := range sub {
					out[c] = true
				}
			}
		}
	}
	return out, nil
}

// packNames: the mods of A+B+C, each one there and named once; no pack together with a
// complete pack that holds it already (its new entries would come twice)
func packNames(spec string) ([]string, error) {
	names := strings.Split(spec, packSep)
	for _, n := range names {
		d, err := modDir(n)
		if err != nil {
			return nil, err
		}
		if !isDir(d) {
			return nil, newMsg("no mod {name}", "name", n)
		}
	}
	seen := map[string]bool{}
	for _, n := range names {
		if seen[strings.ToLower(n)] { // (in any case: one directory on Windows and macOS)
			return nil, newMsg("a mod twice in {spec}", "spec", spec)
		}
		seen[strings.ToLower(n)] = true
	}
	held := map[string]string{}
	for _, n := range names {
		cs, err := packContents(n, map[string]bool{})
		if err != nil {
			return nil, err
		}
		sorted := make([]string, 0, len(cs))
		for c := range cs {
			sorted = append(sorted, c)
		}
		sort.Strings(sorted)
		for _, c := range sorted {
			if h, ok := held[c]; ok {
				other := h
				if h == c {
					other = n
				}
				return nil, newMsg("\"{pack}\" is in \"{other}\" already", "pack", c, "other", other)
			}
			held[c] = n
		}
	}
	return names, nil
}

type pack struct {
	name string
	docs map[string]obj
}

// packReport: what combine says of the packs played together -- per pack the numbers
// of its new entries ([first, last, n] per table) and how many of them moved; the
// entries two packs change differently (the later one wins)
type packReport struct {
	Packs      []packAdded `json:"packs"`
	Conflicts  []conflict  `json:"conflicts"`
	Incomplete []string    `json:"incomplete"`
}

type packAdded struct {
	Name  string              `json:"name"`
	Added map[string][3]int64 `json:"added"`
	Moved map[string]int      `json:"moved"`
}

type conflict struct {
	Table string   `json:"table"`
	ID    int64    `json:"id"`
	Packs []string `json:"packs"`
	Wins  string   `json:"wins"`
}

// combine: packs laid over the original in their order (tools/modpacks.py) -- an
// original entry a pack changes takes that pack's version (the later pack's when
// two do); the entries a pack adds are appended after the highest number so far
// (the first pack keeps its numbers); a changed screenplay's target follows its
// pack's new numbers
func combine(original map[string]obj, packs []pack) (map[string]obj, packReport, error) {
	docs, report, _, err := combineFrom(original, packs)
	return docs, report, err
}

// source: where an entry of a combination comes from -- pack PACK (its index), its
// entry ID there (the editor keeps that entry's key order, editor.go)
type source struct {
	pack int
	id   int64
}

// combineFrom: combine, and the source of every entry a pack gave: table -> number ->
// source (the others are the original's)
func combineFrom(original map[string]obj, packs []pack) (map[string]obj, packReport, map[string]map[int64]source, error) {
	report := packReport{Packs: []packAdded{}, Conflicts: []conflict{}}
	from := map[string]map[int64]source{}
	type version struct {
		pack string
		rec  obj
	}
	changedBy := map[string]map[int64][]version{} // table -> id -> the packs' versions
	orig := map[string]map[int64]obj{}
	result := map[string]map[int64]obj{}
	for _, t := range tables {
		byID, _, err := records(original[t.name])
		if err != nil {
			return nil, report, nil, err
		}
		changedBy[t.name] = map[int64][]version{}
		from[t.name] = map[int64]source{}
		orig[t.name] = byID
		result[t.name] = map[int64]obj{}
		for id, r := range byID {
			result[t.name][id] = deepCopy(r).(obj)
		}
	}
	highest := func(m map[int64]obj) int64 {
		top := int64(-1 << 62)
		for id := range m {
			top = max(top, id)
		}
		return top
	}
	for k, p := range packs {
		mapping := map[string]map[int64]int64{}
		added, moved := map[string][3]int64{}, map[string]int{}
		// the new entries first: their numbers in the combination
		for _, t := range growing {
			doc, ok := p.docs[t]
			if !ok {
				continue
			}
			_, list, err := records(doc)
			if err != nil {
				return nil, report, nil, err
			}
			var fresh []obj
			for _, r := range list {
				if _, ok := orig[t][r["id"].(int64)]; !ok {
					fresh = append(fresh, r)
				}
			}
			sort.SliceStable(fresh, func(i, j int) bool { return fresh[i]["id"].(int64) < fresh[j]["id"].(int64) })
			mapping[t] = map[int64]int64{}
			top := highest(result[t]) // (a later pack's new entries follow the highest number so far)
			for _, r := range fresh {
				id := r["id"].(int64)
				nid := id
				if k > 0 {
					top++
					nid = top
				}
				if _, there := result[t][nid]; there && k == 0 {
					return nil, report, nil, newMsg("pack {pack}: {entry} #{id} twice", "pack", p.name, "entry", entryWord(t), "id", id)
				}
				if nid > packLimit {
					return nil, report, nil, newMsg("more than 999 {table} with pack {pack}", "table", tableWord(t), "pack", p.name)
				}
				mapping[t][id] = nid
				rec := deepCopy(r).(obj)
				rec["id"] = nid
				result[t][nid] = rec
				from[t][nid] = source{k, id}
			}
			if len(fresh) > 0 {
				lo, hi, n := int64(1<<62), int64(-1), 0
				for _, r := range fresh {
					nid := mapping[t][r["id"].(int64)]
					lo, hi = min(lo, nid), max(hi, nid)
					if nid != r["id"].(int64) {
						n++
					}
				}
				added[t] = [3]int64{lo, hi, int64(len(fresh))}
				if n > 0 {
					moved[t] = n
				}
			}
		}
		// then the changes of original entries
		for _, t := range tables {
			doc, ok := p.docs[t.name]
			if !ok {
				continue
			}
			_, list, err := records(doc)
			if err != nil {
				return nil, report, nil, err
			}
			for _, r := range list {
				id := r["id"].(int64)
				o, ok := orig[t.name][id]
				if !ok || equal(r, o) {
					continue
				}
				rec := deepCopy(r).(obj)
				if t.name == "screenplays" && truthy(rec["target"]) {
					kind := "films"
					if intOf(rec, "flags")&1 != 0 {
						kind = "series"
					}
					if target, ok := rec["target"].(int64); ok {
						if nid, ok := mapping[kind][target]; ok {
							rec["target"] = nid
						}
					}
				}
				changedBy[t.name][id] = append(changedBy[t.name][id], version{p.name, rec})
				result[t.name][id] = rec
				from[t.name][id] = source{k, id}
			}
		}
		report.Packs = append(report.Packs, packAdded{p.name, added, moved})
	}
	for _, t := range tables {
		ids := make([]int64, 0, len(changedBy[t.name]))
		for id := range changedBy[t.name] {
			ids = append(ids, id)
		}
		sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
		for _, id := range ids {
			vs := changedBy[t.name][id]
			last := vs[len(vs)-1]
			differ := false
			for _, v := range vs[:len(vs)-1] {
				if !equal(v.rec, last.rec) {
					differ = true
				}
			}
			if len(vs) > 1 && differ {
				names := make([]string, len(vs))
				for i, v := range vs {
					names[i] = v.pack
				}
				report.Conflicts = append(report.Conflicts, conflict{t.name, id, names, last.pack})
			}
		}
	}
	docs := map[string]obj{}
	for _, t := range tables {
		doc := deepCopy(original[t.name]).(obj)
		ids := make([]int64, 0, len(result[t.name]))
		for id := range result[t.name] {
			ids = append(ids, id)
		}
		sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
		list := make([]any, len(ids))
		for i, id := range ids {
			list[i] = result[t.name][id]
		}
		doc["records"] = list
		docs[t.name] = doc
	}
	return docs, report, from, nil
}

// packsDocs: the tables of one mod as saved, or of several played together; and the
// report of combine (numbers of the new entries, conflicts)
func packsDocs(g *game, spec string) (map[string]obj, packReport, error) {
	names, err := packNames(spec)
	if err != nil {
		return nil, packReport{}, err
	}
	var packs []pack
	for _, n := range names {
		docs, err := modDocs(g, n)
		if err != nil {
			return nil, packReport{}, err
		}
		packs = append(packs, pack{n, docs})
	}
	docs, report, err := combine(g.original, packs)
	if err != nil {
		return nil, report, err
	}
	if len(names) == 1 {
		return packs[0].docs, report, nil
	}
	return docs, report, nil
}

// buildMod: the mod's tables written into copies of the game files, build/mods/NAME;
// several mods A+B+C played together as packs: build/mods/A+B+C
func buildMod(g *game, spec string) (string, error) {
	docs, _, err := packsDocs(g, spec)
	if err != nil {
		return "", err
	}
	out := filepath.Join(root, "build", "mods", spec)
	defer lockDir(out)() // (one build at a time: each reads what the last one left)
	if err := os.MkdirAll(out, 0o777); err != nil {
		return "", err
	}
	var srcs []string
	for _, p := range g.paths { // an existing copy keeps the games saved in it
		q := filepath.Join(out, filepath.Base(p))
		if exists(q) {
			srcs = append(srcs, q)
		} else {
			srcs = append(srcs, p)
		}
	}
	return out, buildDocs(g, docs, srcs, out)
}

// buildDocs: tables written into the game files SRCS, the results in OUT
func buildDocs(g *game, docs map[string]obj, srcs []string, out string) error {
	if err := checkDocs(docs); err != nil {
		return err
	}
	if inc := incomplete(docs, g.original); len(inc) > 0 {
		return newMsg("not playable yet -- {list}", "list", joined{"; ", inc})
	}
	res, err := readResources(srcs)
	if err != nil {
		return err
	}
	built, err := buildTables(docs)
	if err != nil {
		return err
	}
	save, err := res.get(startSave)
	if err != nil {
		return err
	}
	built[startSave] = patchStartSave(save, built[0xC9], built[0xCA], built[0xCB], built[0xCC])
	byFile := map[int]map[int][]byte{}
	for i, data := range built {
		n, err := res.where(i)
		if err != nil {
			return err
		}
		if byFile[n] == nil {
			byFile[n] = map[int][]byte{}
		}
		byFile[n][i] = data
	}
	for n, p := range srcs {
		orig, err := os.ReadFile(p)
		if err != nil {
			return err
		}
		data, err := writeRsc(orig, byFile[n])
		if err != nil {
			return err
		}
		if err := replaceFile(filepath.Join(out, filepath.Base(g.paths[n])), data); err != nil {
			return err
		}
	}
	return nil
}
