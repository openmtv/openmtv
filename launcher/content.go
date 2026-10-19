package main

import (
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"sort"
	"strconv"
)

// The game's content tables, read from and written into the .rsc files (the
// tables: tools/content.py's docstring), as JSON documents -- the editor's and
// the packs' format; a pack built here is the pack the former Python editor (editor.py) builds, byte
// for byte (tools/buildtest.py).  Packing them: pack.go.

type field struct {
	name string
	off  int
	typ  string // u8 i8 u16 i16 u32 i32
	n    int    // a list of n values (0: one value)
}

type textSpec struct {
	kind     string // pstr, raw (indexed) or lens
	de, en   int
	sections int // lens: ads -- advertiser and client, one entry per record each
	per      int // lens: news phrases per topic
}

type table struct {
	name                  string
	res, size, count, max int // max 0: the count is fixed
	texts                 *textSpec
	fields                []field
}

var tables = []table{
	{"films", 0xC9, 0x17, 201, 1000, &textSpec{kind: "pstr", de: 0xCF, en: 0xFC}, []field{
		{"genre", 0x0, "u8", 0}, {"quality", 0x1, "u8", 0}, {"wear_rate", 0x2, "u8", 0}, {"topicality", 0x3, "u8", 0},
		{"blocks", 0x4, "u8", 0}, {"owner", 0x5, "u8", 0}, {"flags", 0x6, "u8", 0}, {"flags2", 0x7, "u8", 0},
		{"price", 0x8, "i32", 0}, {"jingle", 0xC, "u16", 0}, {"live_day", 0xF, "u16", 0}, {"live_slot", 0x11, "u16", 0}}},
	{"series", 0xCA, 0x39, 51, 1000, &textSpec{kind: "pstr", de: 0x2F, en: 0xFB}, []field{
		{"episodes_qtf", 0x0, "u8", 33}, // episode e at 3e: quality, topicality, flags (bit0 exists)
		{"genre", 0x21, "u8", 0}, {"episodes", 0x22, "u8", 0}, {"blocks", 0x23, "u8", 0}, {"owner", 0x24, "u8", 0},
		{"flags", 0x25, "u8", 0}, {"flags2", 0x26, "u8", 0}, {"price", 0x27, "i32", 0}, {"episode_price", 0x2B, "i32", 0},
		{"jingle", 0x2F, "u16", 0}, {"next_episode", 0x31, "u16", 0}, {"wear_rate", 0x33, "u8", 0},
		{"critics", 0x35, "u8", 0}, {"press", 0x36, "u8", 0}}},
	{"screenplays", 0xCB, 0x1B3, 51, 0, &textSpec{kind: "pstr", de: 0x2D, en: 0xFA}, []field{
		{"price", 0x0, "i32", 0}, {"state", 0x4, "u8", 0}, {"flags", 0x5, "u8", 0}, {"target", 0x6, "u16", 0},
		{"duration_hours", 0x166, "u16", 0}}},
	{"ads", 0xCC, 0x18, 51, 1000, &textSpec{kind: "lens", de: 0x2E, en: 0xF9, sections: 2}, []field{
		{"days", 0x8, "u8", 0}, {"spots", 0x9, "u8", 0}, {"min_audience_class", 0xA, "u8", 0}, {"target_group", 0xB, "u8", 0},
		{"genre_rule", 0xC, "i8", 3}, {"holder", 0xF, "u8", 0}, {"fee", 0x10, "i32", 0}, {"penalty", 0x14, "i32", 0}}},
	{"news_nodes", 0xCD, 0xA, 401, 0, &textSpec{kind: "raw", de: 0x31, en: 0xF8}, []field{
		{"successors", 0x0, "u16", 4}, {"value", 0x8, "u8", 0}, {"delay_hours", 0x9, "u8", 0}}},
	{"news_topics", 0xCE, 0x38, 51, 0, &textSpec{kind: "lens", de: 0x2C, en: 0xF7, per: 10}, []field{
		{"phrase_value", 0x2D, "u8", 10}}},
	{"news_templates", 0x30, 0xE, 51, 0, nil, []field{
		{"parts", 0x0, "u16", 5}, {"start_node", 0xA, "u16", 0}, {"category", 0xD, "u8", 0}}},
}

var langs = []string{"de", "en"}

const (
	phraseSlots = 561  // length table of the phrase resources (0x462 bytes)
	startSave   = 0x2B // main() loads this savegame at startup (game_load(0x2B))
)

// A savegame (game_save_load 229E:139A, src/actions.c) is the original's 37 605 bytes and,
// with more than 200 films, 50 series or 50 ads or a game option that needs it, an
// extension (savegame_extra_size): "MTVX" and up to 6 u16 counts, films 201.. (0x12 bytes
// each), series 51.. (0x29), ads 51.. (1), the computer players' state (--ai-improve: 86),
// the ad agency's offers 11..17 (7 each) and the film dealer's 11..17 (8 each).  Its length
// is 16 bits, and the game addresses it through one far pointer: at most savegameMax bytes
// (tools/content.py SAVE_MAX alike)
const (
	savegameBase    = 37605
	savegameMax     = 0xFFF0
	savegameOptions = 6 + 2*6 + (2 + 3*28) + 7*7 + 7*8 // the head, every option at its largest
)

// savegameSize: the largest savegame of a game with these counts (every option on)
func savegameSize(films, series, ads int) int {
	return savegameBase + savegameOptions + max(films-200, 0)*0x12 + max(series-50, 0)*0x29 + max(ads-50, 0)
}

// saveTooLarge: the built tables' films, series and ads, if a savegame of them would not
// fit (the game would not save it), else nil
func saveTooLarge(out map[int][]byte) error {
	n := map[string]int{}
	for _, t := range tables {
		if t.max > 0 {
			n[t.name] = len(out[t.res])/t.size - 1
		}
	}
	if size := savegameSize(n["films"], n["series"], n["ads"]); size > savegameMax {
		return newMsg("{films} films, {series} series and {ads} ads: a savegame would have {n} bytes, at most {max} (each film past 200 takes 18, each series past 50 41, each ad past 50 1)",
			"films", n["films"], "series", n["series"], "ads", n["ads"], "n", size, "max", savegameMax)
	}
	return nil
}

func (t table) top() int {
	if t.max > 0 {
		return t.max
	}
	return t.count
}

func phraseExtraSlots(t table) []int {
	used := map[int]bool{}
	for i := 0; i < t.count; i++ {
		for k := i*t.texts.per + 1; k < i*t.texts.per+1+t.texts.per; k++ {
			used[k] = true
		}
	}
	var out []int
	for k := 0; k < phraseSlots; k++ {
		if !used[k] {
			out = append(out, k)
		}
	}
	return out
}

// ---------------------------------------------------------------- RSC files

type rscFile struct {
	data []byte
	idx  map[int][2]int // resource -> (offset, size)
}

func readRsc(path string) (rscFile, error) {
	d, err := os.ReadFile(path)
	if err != nil {
		return rscFile{}, err
	}
	if len(d) < 600*6 {
		return rscFile{}, fmt.Errorf("%s: not a resource file", path)
	}
	f := rscFile{d, map[int][2]int{}}
	for i := 0; i < 600; i++ {
		off, size := int(binary.LittleEndian.Uint32(d[6*i:])), int(binary.LittleEndian.Uint16(d[6*i+4:]))
		if off != 0 {
			f.idx[i] = [2]int{off, size}
		}
	}
	return f, nil
}

type resources []rscFile

func readResources(paths []string) (resources, error) {
	var res resources
	for _, p := range paths {
		f, err := readRsc(p)
		if err != nil {
			return nil, err
		}
		res = append(res, f)
	}
	return res, nil
}

// get: resource I (later files override)
func (res resources) get(i int) ([]byte, error) {
	if n, err := res.where(i); err == nil {
		e := res[n].idx[i]
		return pySlice(res[n].data, e[0], e[0]+e[1]), nil
	}
	return nil, fmt.Errorf("resource %d", i)
}

func (res resources) where(i int) (int, error) {
	for n := len(res) - 1; n >= 0; n-- {
		if _, ok := res[n].idx[i]; ok {
			return n, nil
		}
	}
	return 0, fmt.Errorf("%d", i)
}

// pySlice: b[lo:hi] as Python cuts it (never past the end)
func pySlice(b []byte, lo, hi int) []byte {
	if hi > len(b) {
		hi = len(b)
	}
	if lo > hi {
		lo = hi
	}
	return b[lo:hi]
}

// ---------------------------------------------------------------- records

func getField(b []byte, off int, typ string) int64 {
	switch typ {
	case "u8":
		return int64(b[off])
	case "i8":
		return int64(int8(b[off]))
	case "u16":
		return int64(binary.LittleEndian.Uint16(b[off:]))
	case "i16":
		return int64(int16(binary.LittleEndian.Uint16(b[off:])))
	case "u32":
		return int64(binary.LittleEndian.Uint32(b[off:]))
	}
	return int64(int32(binary.LittleEndian.Uint32(b[off:])))
}

func covered(t table) []bool {
	cov := make([]bool, t.size)
	for _, f := range t.fields {
		n := fieldSize[f.typ] * max(f.n, 1)
		for k := f.off; k < f.off+n; k++ {
			cov[k] = true
		}
	}
	return cov
}

func unpackRecord(t table, b []byte) obj {
	r := obj{}
	for _, f := range t.fields {
		if f.n > 0 {
			vals := make([]any, f.n)
			for k := range vals {
				vals[k] = getField(b, f.off+k*fieldSize[f.typ], f.typ)
			}
			r[f.name] = vals
		} else {
			r[f.name] = getField(b, f.off, f.typ)
		}
	}
	cov := covered(t)
	unk := obj{}
	for k := 0; k < t.size; {
		if cov[k] {
			k++
			continue
		}
		s := k
		for k < t.size && !cov[k] {
			k++
		}
		for _, c := range b[s:k] {
			if c != 0 {
				unk[fmt.Sprintf("0x%02X", s)] = hex.EncodeToString(b[s:k])
				break
			}
		}
	}
	if len(unk) > 0 {
		r["unknown"] = unk
	}
	return r
}

// ---------------------------------------------------------------- texts

func parsePstrs(blk []byte) []string {
	var out []string
	for i := 0; i < len(blk); {
		n := int(blk[i])
		out = append(out, cp437Decode(pySlice(blk, i+1, i+1+n)))
		i += 1 + n
	}
	return out
}

// pstrBytes: texts read from the game, as Pascal strings again
func pstrBytes(strs []string) []byte {
	var b []byte
	for _, s := range strs {
		e, _ := cp437Encode(s)
		b = append(append(b, byte(len(e))), e...)
	}
	return b
}

func parseIndexed(d []byte, count int, kind string) ([]any, []byte, error) {
	if len(d) < 2*count {
		return nil, nil, errors.New("unpack_from requires a buffer of at least " + strconv.Itoa(2*count) + " bytes")
	}
	ix := make([]int, count)
	for k := range ix {
		ix[k] = int(binary.LittleEndian.Uint16(d[2*k:]))
	}
	var out []any
	for e := 1; e < count; e++ {
		blk := pySlice(d, ix[e-1], ix[e])
		if kind == "pstr" {
			strs := parsePstrs(blk)
			// entries that are not clean strings (film 200 in the original) stay raw
			if string(pstrBytes(strs)) == string(blk) {
				l := make([]any, len(strs))
				for k, s := range strs {
					l[k] = s
				}
				out = append(out, l)
			} else {
				out = append(out, obj{"raw": hex.EncodeToString(blk)})
			}
		} else {
			out = append(out, cp437Decode(blk))
		}
	}
	return out, pySlice(d, ix[count-1], len(d)), nil
}

func parseLens(d []byte, nlen, sections int) ([][]string, []byte, error) {
	var out [][]string
	pos := 0
	for s := 0; s < sections; s++ {
		if pos+2*nlen > len(d) {
			return nil, nil, errors.New("unpack_from: the length table is cut off")
		}
		lens := make([]int, nlen)
		for k := range lens {
			lens[k] = int(binary.LittleEndian.Uint16(d[pos+2*k:]))
		}
		pos += 2 * nlen
		var sec []string
		for _, l := range lens {
			sec = append(sec, cp437Decode(pySlice(d, pos, pos+l)))
			pos += l
		}
		out = append(out, sec)
	}
	return out, pySlice(d, pos, len(d)), nil
}

// ---------------------------------------------------------------- export / import

func hasText(r obj) bool {
	tx, _ := r["text"].(obj)
	for _, v := range tx {
		switch x := v.(type) {
		case string, obj:
			if truthy(x) {
				return true
			}
		case []any:
			for _, s := range x {
				if truthy(s) {
					return true
				}
			}
		}
	}
	return false
}

func setText(r obj, lang string, v any) {
	tx, ok := r["text"].(obj)
	if !ok {
		tx = obj{}
		r["text"] = tx
	}
	tx[lang] = v
}

// export: the tables of the game files -> {table: document}, as content.export
func export(res resources) (map[string]obj, error) {
	out := map[string]obj{}
	for _, t := range tables {
		d, err := res.get(t.res)
		if err != nil {
			return nil, err
		}
		n := t.count
		if t.max > 0 { // films: as many as the table holds
			n = len(d) / t.size
		}
		if len(d) != t.size*n || !(t.count <= n && n <= t.top()) {
			return nil, fmt.Errorf("%s: resource size %d, expected %d", t.name, len(d), t.size*t.count)
		}
		recs := make([]obj, n)
		for i := range recs {
			recs[i] = unpackRecord(t, d[i*t.size:(i+1)*t.size])
		}
		tails, extras := obj{}, obj{}
		if tx := t.texts; tx != nil {
			for _, lang := range langs {
				textRes := tx.de
				if lang == "en" {
					textRes = tx.en
				}
				td, err := res.get(textRes)
				if err != nil {
					return nil, err
				}
				var tail []byte
				switch {
				case tx.kind == "pstr" || tx.kind == "raw":
					var ents []any
					if ents, tail, err = parseIndexed(td, n, tx.kind); err != nil {
						return nil, err
					}
					for i, e := range ents {
						setText(recs[i+1], lang, e)
					}
				case tx.sections > 0: // ads: advertiser and client, one entry per record each
					var secs [][]string
					if secs, tail, err = parseLens(td, n, tx.sections); err != nil {
						return nil, err
					}
					for i := 0; i < n; i++ {
						setText(recs[i], lang, []any{secs[0][i], secs[1][i]})
					}
				default: // news phrases: 561 lengths, phrase j of topic t at t*10 + j
					var secs [][]string
					if secs, tail, err = parseLens(td, phraseSlots, 1); err != nil {
						return nil, err
					}
					flat := secs[0]
					for i := 0; i < t.count; i++ {
						ph := make([]any, tx.per)
						for k := range ph {
							ph[k] = flat[i*tx.per+1+k]
						}
						setText(recs[i], lang, ph)
					}
					extra := obj{}
					for _, k := range phraseExtraSlots(t) {
						if flat[k] != "" {
							extra[strconv.Itoa(k)] = flat[k]
						}
					}
					if len(extra) > 0 {
						extras[lang] = extra
					}
				}
				if len(tail) > 0 {
					tails[lang] = hex.EncodeToString(tail)
				}
			}
		}
		var items []any
		for i, r := range recs {
			if i == 0 && allZero(d[:t.size]) && !hasText(r) {
				continue
			}
			r["id"] = int64(i)
			items = append(items, r)
		}
		doc := obj{"table": t.name, "resource": int64(t.res), "record_size": int64(t.size), "count": int64(n), "records": items}
		if len(tails) > 0 {
			doc["text_tail"] = tails
		}
		if len(extras) > 0 {
			doc["text_extra"] = extras
		}
		out[t.name] = doc
	}
	return out, nil
}

func allZero(b []byte) bool {
	for _, c := range b {
		if c != 0 {
			return false
		}
	}
	return true
}

// patchStartSave: the per-item fields that game_save_load (229E:139A) stores, written
// into the start savegame from the packed tables; bytes the save routine does not
// set (stack leftovers of the original tool) are kept
func patchStartSave(save, films, series, scripts, ads []byte) []byte {
	s := append([]byte{}, save...)
	bits := func(flags, flags2 byte) byte {
		return ((flags >> 5) & 1) | (((flags >> 7) & 1) << 1) | ((flags2 & 1) << 2) | (((flags2 >> 1) & 1) << 3)
	}
	o := 0
	for i := 1; i < 201; i++ {
		t, b := films[i*0x17:(i+1)*0x17], o+(i-1)*0x12
		s[b+0], s[b+1], s[b+3] = t[1], t[3], t[5]
		s[b+4] = (s[b+4] & 0xF0) | bits(t[6], t[7])
		copy(s[b+5:b+9], t[8:12])
		s[b+0xD] = t[0xE]
		copy(s[b+0xE:b+0x12], t[0xF:0x13])
	}
	o += 200 * 0x12
	for i := 1; i < 51; i++ {
		t, b := series[i*0x39:(i+1)*0x39], o+(i-1)*0x29
		copy(s[b:b+0x21], t[0:0x21])
		s[b+0x21] = t[0x24]
		s[b+0x22] = (s[b+0x22] & 0xF0) | bits(t[0x25], t[0x26])
		copy(s[b+0x23:b+0x27], t[0x27:0x2B])
		copy(s[b+0x27:b+0x29], t[0x31:0x33])
	}
	o += 50 * 0x29
	for i := 1; i < 51; i++ {
		t, b := append([]byte{}, scripts[i*0x1B3:(i+1)*0x1B3]...), o+(i-1)*0x161
		if t[5]&1 == 0 && t[5]&4 != 0 { // script_init_episodes: block 0 -> 1..9
			for e := 1; e < 10; e++ {
				copy(t[8+e*0x23:8+(e+1)*0x23], t[8:8+0x23])
			}
		}
		s[b] = t[4]
		copy(s[b+1:b+1+10*0x23], t[8:8+10*0x23])
	}
	o += 50 * 0x161
	for i := 1; i < 51; i++ {
		s[o+i-1] = ads[i*0x18+0xF]
	}
	return s
}

// writeRsc: the original file's bytes and {id: new bytes} -> the new file's bytes
// (a changed resource in place when it fits, else appended)
func writeRsc(orig []byte, changed map[int][]byte) ([]byte, error) {
	d := append([]byte{}, orig...)
	ids := make([]int, 0, len(changed))
	for i := range changed {
		ids = append(ids, i)
	}
	sort.Ints(ids)
	for _, i := range ids {
		data := changed[i]
		off, size := int(binary.LittleEndian.Uint32(d[6*i:])), int(binary.LittleEndian.Uint16(d[6*i+4:]))
		if off != 0 && string(pySlice(d, off, off+size)) == string(data) {
			continue
		}
		if len(data) > 0xFFFF {
			return nil, newMsg("resource {n} larger than 64 KB", "n", i)
		}
		if off != 0 && len(data) <= size {
			copy(d[off:], data) // fits in place
			binary.LittleEndian.PutUint16(d[6*i+4:], uint16(len(data)))
		} else {
			binary.LittleEndian.PutUint32(d[6*i:], uint32(len(d)))
			binary.LittleEndian.PutUint16(d[6*i+4:], uint16(len(data)))
			d = append(d, data...)
		}
	}
	return d, nil
}
