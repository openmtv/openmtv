package main

import (
	"fmt"
	"strings"
	"unicode"
)

// The editor's checks before a pack is built (the former Python editor (editor.py) check_docs,
// incomplete) and the data fixes of the original (tools/datafixes.py).

// newGameProblems: what a new game needs (newgame_starting_programs 1DA8:1538): each
// station gets one series and three films -- the marked ones (flags2 bit 4) while one
// is free, then the original pools: the series 18-20 and the cultural programmes
// (genre 6) on the market; without enough of them it loops for ever
func newGameProblems(docs map[string]obj) []msg {
	free := func(table string, ok func(r obj, id int64) bool) int {
		_, list, _ := records(docs[table])
		ids := map[int64]bool{}
		for _, r := range list {
			id := r["id"].(int64)
			if id != 0 && !truthy(r["owner"]) && ok(r, id) {
				ids[id] = true
			}
		}
		return len(ids)
	}
	nf := free("films", func(r obj, _ int64) bool {
		return intOf(r, "flags2")&0x10 != 0 || (equal(r["genre"], int64(6)) && intOf(r, "flags")&0x20 != 0)
	})
	ns := free("series", func(r obj, id int64) bool {
		return intOf(r, "flags2")&0x10 != 0 || id == 18 || id == 19 || id == 20
	})
	var out []msg
	if nf < 9 {
		out = append(out, newMsg("a new game gives each station 3 films (the marked ones, then cultural programmes on the market): only {n} are free, it hangs with fewer than 9", "n", nf))
	}
	if ns < 3 {
		out = append(out, newMsg("a new game gives each station one series (the marked ones, then series 18-20): only {n} are free, it hangs with fewer than 3", "n", ns))
	}
	return out
}

var specialFilms = map[int64]bool{29: true, 58: true, 187: true} // the code uses them by number (live football, Betty's programme)

// pyStrip: str.strip() -- Python's whitespace (with the separators 0x1C-0x1F)
func pyStrip(s string) string {
	return strings.TrimFunc(s, func(r rune) bool { return unicode.IsSpace(r) || (r >= 0x1C && r <= 0x1F) })
}

// textAt: an entry's text K in LANG ("" where it has none)
func textAt(r obj, lang string, k int) string {
	if tx, ok := r["text"].(obj); ok {
		if t, ok := tx[lang].([]any); ok && k < len(t) {
			s, _ := t[k].(string)
			return s
		}
	}
	return ""
}

func textsMissing(r obj, names [2]string) []msg {
	var out []msg
	for _, l := range [][2]string{{"en", "English"}, {"de", "German"}} {
		if pyStrip(textAt(r, l[0], 0)) == "" {
			out = append(out, newMsg(l[1]+" "+names[0])) // 'English title' ... (msgDE)
		}
		if pyStrip(textAt(r, l[0], 1)) == "" {
			out = append(out, newMsg(l[1]+" "+names[1]))
		}
	}
	return out
}

// filmMissing: what a film in the game (on the market, in a start library or the
// starting pool) misses of the fields the editor marks as required -- the rules of
// editor/game.js REQUIRED
func filmMissing(r obj) []msg {
	i := intOf(r, "id")
	owner := intOf(r, "owner")
	if i == 0 || specialFilms[i] || !(intOf(r, "flags")&0x20 != 0 || (1 <= owner && owner <= 3) || intOf(r, "flags2")&0x10 != 0) {
		return nil
	}
	miss := textsMissing(r, [2]string{"title", "description"})
	if g := intOf(r, "genre"); !(1 <= g && g <= 10) {
		miss = append(miss, newMsg("genre"))
	}
	if intOf(r, "blocks") < 1 {
		miss = append(miss, newMsg("length"))
	}
	if intOf(r, "quality") < 1 {
		miss = append(miss, newMsg("quality"))
	}
	if tp := intOf(r, "topicality"); !(1 <= tp && tp <= intOf(r, "quality")) {
		miss = append(miss, newMsg("topicality"))
	}
	if intOf(r, "price") <= 0 {
		miss = append(miss, newMsg("price"))
	}
	return miss
}

// seriesMissing: the same for a series in the game (series 18-20: a new game may give
// them away)
func seriesMissing(r obj) []msg {
	i := intOf(r, "id")
	owner := intOf(r, "owner")
	if i == 0 || !(intOf(r, "flags")&0x20 != 0 || (1 <= owner && owner <= 3) || intOf(r, "flags2")&0x10 != 0 ||
		i == 18 || i == 19 || i == 20) {
		return nil
	}
	miss := textsMissing(r, [2]string{"title", "description"})
	q, _ := r["episodes_qtf"].([]any)
	produced := 0
	for e := 1; e < 11; e++ {
		if e*3+2 < len(q) {
			if v, ok := q[e*3+2].(int64); ok && v&1 != 0 {
				produced++
			}
		}
	}
	if g := intOf(r, "genre"); !(1 <= g && g <= 5) {
		miss = append(miss, newMsg("genre"))
	}
	if intOf(r, "episodes") < 1 || produced < 1 {
		miss = append(miss, newMsg("at least one produced episode"))
	}
	if intOf(r, "blocks") < 1 {
		miss = append(miss, newMsg("episode length"))
	}
	if intOf(r, "episode_price") <= 0 {
		miss = append(miss, newMsg("episode price"))
	}
	return miss
}

// adMissing: the same for an ad in the game (with a fee, or held at the start)
func adMissing(r obj) []msg {
	if intOf(r, "id") == 0 || !(truthy(r["fee"]) || truthy(r["holder"])) {
		return nil
	}
	miss := textsMissing(r, [2]string{"advertiser", "slogan"})
	if intOf(r, "spots") < 1 {
		miss = append(miss, newMsg("spots"))
	}
	if intOf(r, "days") < 1 {
		miss = append(miss, newMsg("days"))
	}
	if c := intOf(r, "min_audience_class"); !(1 <= c && c <= 10) {
		miss = append(miss, newMsg("minimum audience"))
	}
	return miss
}

// The production of a screenplay (editor/game.js, "screenplays: the production", read from
// studio_new_shopping_list 229E:09AF, studio_start_production 229E:0B98 and
// script_role_match_pct 1DA8:2560): episode e (1..10) at 8 + (e-1)*0x23 -- +0 its base
// quality, its shopping list at +1+3k (k 1..10: item, weight, bought; an item 0 ends it);
// items 1-15 the roles, 16 special effects, 17-41 sets; role t's requirements at
// 0x168 + 4(t-1) (humor, -, appearance, skill), its actor category at 0x1A3 + t.  A film
// is one list, a series one per episode of its series.
const spRoles, spItems, spSets = 15, 10, 3

func tableNamed(name string) table {
	for _, t := range tables {
		if t.name == name {
			return t
		}
	}
	panic(name)
}

// need: a message of what an entry misses, and what it is about (the original's own gaps
// are found by it); kind and n: a screenplay's episode or role, worded together (spCompact),
// series: the screenplay's (its episodes are named)
type need struct {
	id     string
	m      msg
	kind   string
	n      int
	series bool
}

func needs(ms []msg) []need {
	out := make([]need, len(ms))
	for i, m := range ms {
		out[i] = need{id: m.key, m: m}
	}
	return out
}

// screenplayMissing: the same for a screenplay the agency offers (flag bit 1, and 24, which
// it offers first by number, 24B8:02C1), its target looked up in DOCS
func screenplayMissing(r obj, docs map[string]obj) []need {
	id := intOf(r, "id")
	if id == 0 || !(intOf(r, "flags")&2 != 0 || id == 24) {
		return nil
	}
	b, err := packRecord(tableNamed("screenplays"), id, r)
	if err != nil {
		return nil // (the build names it)
	}
	var out []need
	add := func(id string, m msg) { out = append(out, need{id: id, m: m}) }
	if intOf(r, "price") <= 0 {
		add("price", newMsg("price"))
	}
	if pyStrip(textAt(r, "en", 0)) == "" {
		add("desc-en", newMsg("English description"))
	}
	if pyStrip(textAt(r, "de", 0)) == "" {
		add("desc-de", newMsg("German description"))
	}
	series := intOf(r, "flags")&1 != 0
	kind := "films"
	if series {
		kind = "series"
	}
	targets, _, _ := records(docs[kind])
	tgt, ok := targets[intOf(r, "target")]
	if intOf(r, "target") == 0 || !ok {
		add("target", newMsg("the programme it becomes"))
	}
	episodes := 1
	if ok && intOf(r, "target") != 0 {
		tid := intOf(tgt, "id")
		if pyStrip(textAt(tgt, "en", 0)) == "" {
			if series {
				add("target-title-en", newMsg("English title of series {id}", "id", tid))
			} else {
				add("target-title-en", newMsg("English title of film {id}", "id", tid))
			}
		}
		if pyStrip(textAt(tgt, "de", 0)) == "" {
			if series {
				add("target-title-de", newMsg("German title of series {id}", "id", tid))
			} else {
				add("target-title-de", newMsg("German title of film {id}", "id", tid))
			}
		}
		if intOf(tgt, "blocks") < 1 {
			if series {
				add("target-length", newMsg("episode length of series {id}", "id", tid))
			} else {
				add("target-length", newMsg("length of film {id}", "id", tid))
			}
		}
		if series {
			episodes = int(max(0, min(10, intOf(tgt, "episodes"))))
			if intOf(tgt, "episodes") < 1 {
				add("target-episodes", newMsg("episodes of series {id}", "id", tid))
			}
		}
	} else if series {
		episodes = 0
	}
	used := map[int]bool{}
	for e := 1; e <= episodes; e++ {
		base := 8 + (e-1)*0x23
		if b[base] < 1 {
			out = append(out, need{id: fmt.Sprintf("ep%d-quality", e), kind: "quality", n: e, series: series})
		}
		items, sets := 0, 0
		for k := 1; k <= spItems && b[base+1+3*k] != 0; k++ {
			item := int(b[base+1+3*k])
			items++
			if item >= 17 && item <= 41 {
				sets++
			}
			if item <= spRoles {
				used[item] = true
			}
		}
		if items == 0 {
			out = append(out, need{id: fmt.Sprintf("ep%d-list", e), kind: "list", n: e, series: series})
		}
		if e == 1 && sets > spSets {
			add("ep1-sets", newMsg("at most 3 sets in the first list (a studio holds 3)"))
		}
	}
	for t := 1; t <= spRoles; t++ {
		if !used[t] {
			continue
		}
		if c := b[0x1A3+t]; c < 1 || c > 7 {
			out = append(out, need{id: fmt.Sprintf("role%d-category", t), kind: "category", n: t, series: series})
		}
		q := 0x168 + 4*(t-1)
		if b[q] == 0 && b[q+2] == 0 && b[q+3] == 0 {
			out = append(out, need{id: fmt.Sprintf("role%d-req", t), kind: "req", n: t, series: series})
		}
	}
	return out
}

// numRanges: "1-3, 5"
func numRanges(ns []int) string {
	var out []string
	for i := 0; i < len(ns); i++ {
		j := i
		for j+1 < len(ns) && ns[j+1] == ns[j]+1 {
			j++
		}
		if j > i {
			out = append(out, fmt.Sprintf("%d-%d", ns[i], ns[j]))
		} else {
			out = append(out, fmt.Sprint(ns[i]))
		}
		i = j
	}
	return strings.Join(out, ", ")
}

// spCompact: a screenplay's episodes and roles of a kind in one message each
func spCompact(list []need) []msg {
	var out []msg
	at := map[string]int{}
	ns := map[string][]int{}
	series := false
	for _, x := range list {
		if x.kind == "" {
			out = append(out, x.m)
			continue
		}
		series = x.series
		if _, ok := at[x.kind]; !ok {
			at[x.kind] = len(out)
			out = append(out, msg{})
		}
		ns[x.kind] = append(ns[x.kind], x.n)
	}
	for kind, i := range at {
		l, one := numRanges(ns[kind]), len(ns[kind]) == 1
		switch {
		case kind == "quality" && !series:
			out[i] = newMsg("base quality")
		case kind == "quality" && one:
			out[i] = newMsg("base quality (episode {list})", "list", l)
		case kind == "quality":
			out[i] = newMsg("base quality (episodes {list})", "list", l)
		case kind == "list" && !series:
			out[i] = newMsg("shopping list")
		case kind == "list" && one:
			out[i] = newMsg("shopping list (episode {list})", "list", l)
		case kind == "list":
			out[i] = newMsg("shopping list (episodes {list})", "list", l)
		case kind == "category" && one:
			out[i] = newMsg("actor category of role {list}", "list", l)
		case kind == "category":
			out[i] = newMsg("actor category of roles {list}", "list", l)
		case one:
			out[i] = newMsg("requirements of role {list}", "list", l)
		default:
			out[i] = newMsg("requirements of roles {list}", "list", l)
		}
	}
	return out
}

var required = []struct {
	table, what string
	missing     func(r obj, docs map[string]obj) []need
}{
	{"films", "film", func(r obj, _ map[string]obj) []need { return needs(filmMissing(r)) }},
	{"series", "series", func(r obj, _ map[string]obj) []need { return needs(seriesMissing(r)) }},
	{"ads", "ad", func(r obj, _ map[string]obj) []need { return needs(adMissing(r)) }},
	{"screenplays", "screenplay", screenplayMissing},
}

// incomplete: the entries a mod puts into the game incomplete; what the original
// itself lacks (film 164, Live Aid: length 0) does not count.  (Not a data fix here:
// the game's fix live-aid repairs the original's record while it plays, also in a pack
// that keeps it -- the classic game keeps the original's.)
func incomplete(docs, original map[string]obj) []msg {
	var out []msg
	for _, q := range required {
		orig, _, _ := records(original[q.table])
		_, list, _ := records(docs[q.table])
		for _, r := range list {
			had := map[string]bool{}
			if o, ok := orig[r["id"].(int64)]; ok {
				for _, m := range q.missing(o, original) {
					had[m.id] = true
				}
			}
			var miss []need
			for _, m := range q.missing(r, docs) {
				if !had[m.id] {
					miss = append(miss, m)
				}
			}
			if len(miss) > 0 {
				out = append(out, newMsg("{what} {id}: {missing}", "what", newMsg(q.what), "id", r["id"].(int64), "missing", joined{", ", spCompact(miss)}))
			}
		}
	}
	return out
}

// textStrings: the strings of a text value (a string, or lists of them)
func textStrings(v any, out []string) []string {
	switch x := v.(type) {
	case string:
		out = append(out, x)
	case []any:
		for _, e := range x {
			out = textStrings(e, out)
		}
	}
	return out
}

// checkDocs: the game's own limits -- every entry and text packed as the build packs
// them (what is not the game's: a message naming it) -- and what a new game needs
func checkDocs(docs map[string]obj) error {
	if _, err := buildTables(docs); err != nil {
		return err
	}
	if bad := newGameProblems(docs); len(bad) > 0 {
		return joinedError{joined{"; ", bad}}
	}
	return nil
}

// applyDataFixes: the original's damaged data fixed (film 200, tools/datafixes.py) --
// a copy, and the names of the fixes applied; the game applies the same fixes itself
// (src/fixes.c)
func applyDataFixes(docs map[string]obj) (map[string]obj, []string) {
	out := map[string]obj{}
	for k, v := range docs {
		out[k] = deepCopy(v).(obj)
	}
	names := []string{}
	if film200(out) {
		names = append(names, "film-200")
	}
	return out, names
}

// film 200: John Carpenter's "Prince of Darkness" (1987).  The English title is the
// placeholder "xxx"; the German text resource is cut off after "Die Fuersten der" (its
// length byte, 26, is that of "Die Fuersten der Dunkelheit") and a table of numbers
// follows, the German description is lost (a new one here); never offered (flags 5).
const (
	film200EnTitle   = "Prince of Darkness"
	film200DeDamaged = "1a446965204681727374656e2064657266" // b'\x1aDie F\x81rsten derf'
)

var film200De = []string{"Die Fürsten der Dunkelheit", "Ein Horrorfilm von John Carpenter mit Teufel, Tod und Schlachthaus-Atmosphäre."}

func film200(docs map[string]obj) bool {
	_, list, _ := records(docs["films"])
	var r obj
	for _, x := range list {
		if equal(x["id"], int64(200)) {
			r = x
			break
		}
	}
	if r == nil {
		return false
	}
	done := false
	t, ok := r["text"].(obj)
	if !ok {
		t = obj{}
		r["text"] = t
	}
	if en, ok := t["en"].([]any); ok && len(en) > 0 && equal(en[0], "xxx") {
		en[0] = film200EnTitle
		done = true
	}
	if de, ok := t["de"].(obj); ok {
		if raw, _ := de["raw"].(string); strings.HasPrefix(raw, film200DeDamaged) {
			t["de"] = []any{film200De[0], film200De[1]}
			done = true
		}
	}
	if equal(r["flags"], int64(5)) && equal(r["genre"], int64(2)) && equal(r["price"], int64(90000)) {
		r["flags"] = intOf(r, "flags") | 0x20 // on the market
		done = true
	}
	return done
}
