package main

import (
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"regexp"
	"sort"
	"strconv"
)

// The tables of a pack packed into the game's resources, strictly as the game
// takes them: every field a whole number in its range, every text a string the
// game's character set (code page 437) has, 255 bytes at most, a table's texts
// 64 KB at most -- and when something is not, the message names the entry, the
// field or text and what is wrong ("film #12, German description: 300 bytes, at
// most 255").  The same bytes as tools/content.py builds (tools/buildtest.py).

// entryWords: what an entry of a table is called, and the table itself
var entryWords = map[string][2]word{
	"films": {{"film", "Film"}, {"films", "Filme"}}, "series": {{"series", "Serie"}, {"series", "Serien"}},
	"screenplays": {{"screenplay", "Drehbuch"}, {"screenplays", "Drehbücher"}}, "ads": {{"ad", "Werbevertrag"}, {"ads", "Werbeverträge"}},
	"news_nodes":     {{"news node", "Nachricht"}, {"news nodes", "Nachrichten"}},
	"news_topics":    {{"news topic", "Nachrichten-Thema"}, {"news topics", "Nachrichten-Themen"}},
	"news_templates": {{"news template", "Geschichte"}, {"news templates", "Geschichten"}},
}

func entryWord(t string) word { return entryWords[t][0] }
func tableWord(t string) word { return entryWords[t][1] }

// textLabel: the K-th text of an entry of table T in language LANG ("German title")
func textLabel(t string, k int, lang string) word {
	en, de := map[string]string{"de": "German", "en": "English"}[lang], map[string]string{"de": "deutsch", "en": "englisch"}[lang]
	switch {
	case (t == "films" || t == "series") && k == 0:
		return word{en + " title", de + "er Titel"}
	case (t == "films" || t == "series") && k == 1:
		return word{en + " description", de + "e Beschreibung"}
	case t == "series" && k%2 == 0:
		return word{fmt.Sprintf("%s title of episode %d", en, k/2), fmt.Sprintf("%ser Titel von Folge %d", de, k/2)}
	case t == "series":
		return word{fmt.Sprintf("%s text of episode %d", en, k/2), fmt.Sprintf("%ser Text von Folge %d", de, k/2)}
	case t == "ads" && k == 0:
		return word{en + " advertiser", de + "er Werbekunde"}
	case t == "ads":
		return word{en + " slogan", de + "er Slogan"}
	case t == "news_nodes":
		return word{en + " headline", de + "e Schlagzeile"}
	case t == "news_topics":
		return word{fmt.Sprintf("%s phrase %d", en, k+1), fmt.Sprintf("%se Phrase %d", de, k+1)}
	}
	return word{fmt.Sprintf("%s text %d", en, k+1), fmt.Sprintf("%ser Text %d", de, k+1)}
}

func langWord(lang string) word {
	return map[string]word{"de": {"German", "deutschen"}, "en": {"English", "englischen"}}[lang]
}

// textsWord: an entry's texts in one language ("German texts")
func textsWord(lang string) word {
	return map[string]word{"de": {"German texts", "deutsche Texte"}, "en": {"English texts", "englische Texte"}}[lang]
}

// entryError: what is wrong with entry ID of table T, at WHAT (a field, a text)
func entryError(t string, id int64, what any, problem msg) msg {
	return newMsg("{entry} #{id}, {what}: {problem}", "entry", entryWord(t), "id", id, "what", what, "problem", problem)
}

// jsonText: a value as the pack's file has it (in a message)
func jsonText(v any) string {
	b, _ := json.Marshal(v)
	return string(b)
}

// records: a document's records by id (a later one with the same id counts), and in order
func records(doc obj) (map[int64]obj, []obj, error) {
	list, ok := doc["records"].([]any)
	if !ok {
		return nil, nil, newMsg("no list of records")
	}
	byID := map[int64]obj{}
	var order []obj
	for _, x := range list {
		r, ok := x.(obj)
		if !ok {
			return nil, nil, newMsg("an entry that is not an object: {value}", "value", jsonText(x))
		}
		id, ok := r["id"].(int64)
		if !ok {
			return nil, nil, newMsg("an entry without its number (id): {value}", "value", jsonText(r["id"]))
		}
		byID[id] = r
		order = append(order, r)
	}
	return byID, order, nil
}

var fieldSize = map[string]int{"u8": 1, "i8": 1, "u16": 2, "i16": 2, "u32": 4, "i32": 4}

var fieldRange = map[string][2]int64{"u8": {0, 255}, "i8": {-128, 127}, "u16": {0, 65535}, "i16": {-32768, 32767},
	"u32": {0, 4294967295}, "i32": {-2147483648, 2147483647}}

func putField(b []byte, off int, typ string, v any) *msg {
	x, ok := v.(int64)
	if !ok {
		m := newMsg("{value} is not a whole number", "value", jsonText(v))
		return &m
	}
	if r := fieldRange[typ]; x < r[0] || x > r[1] {
		m := newMsg("{value} is not between {lo} and {hi}", "value", x, "lo", r[0], "hi", r[1])
		return &m
	}
	switch fieldSize[typ] {
	case 1:
		b[off] = byte(x)
	case 2:
		binary.LittleEndian.PutUint16(b[off:], uint16(x))
	default:
		binary.LittleEndian.PutUint32(b[off:], uint32(x))
	}
	return nil
}

var unknownKey = regexp.MustCompile(`^0x[0-9A-Fa-f]{1,4}$`)

// packRecord: entry ID of table T -> its bytes; "unknown" (offset -> hex) first, the
// fields over it
func packRecord(t table, id int64, r obj) ([]byte, error) {
	b := make([]byte, t.size)
	if u, ok := r["unknown"]; ok {
		unk, ok := u.(obj)
		if !ok {
			return nil, entryError(t.name, id, "unknown", newMsg("not an object of offsets"))
		}
		keys := make([]string, 0, len(unk))
		for k := range unk {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		for _, k := range keys {
			off, err := strconv.ParseInt(k[min(2, len(k)):], 16, 32)
			hx, _ := unk[k].(string)
			v, herr := hex.DecodeString(hx)
			switch {
			case !unknownKey.MatchString(k) || err != nil:
				return nil, entryError(t.name, id, "unknown "+k, newMsg("not an offset (0x..)"))
			case herr != nil || hx == "":
				return nil, entryError(t.name, id, "unknown "+k, newMsg("{value} is not hex", "value", jsonText(unk[k])))
			case int(off)+len(v) > t.size:
				return nil, entryError(t.name, id, "unknown "+k, newMsg("beyond the entry's {size} bytes", "size", t.size))
			}
			copy(b[off:], v)
		}
	}
	for _, f := range t.fields {
		val, ok := r[f.name]
		if !ok {
			continue
		}
		if f.n == 0 {
			if m := putField(b, f.off, f.typ, val); m != nil {
				return nil, entryError(t.name, id, f.name, *m)
			}
			continue
		}
		vals, ok := val.([]any)
		if !ok || len(vals) != f.n {
			got := word{"not a list", "keine Liste"}
			if ok {
				got = word{fmt.Sprintf("%d values", len(vals)), fmt.Sprintf("%d Werte", len(vals))}
			}
			return nil, entryError(t.name, id, f.name, newMsg("{got}, it takes {n} numbers", "got", got, "n", f.n))
		}
		for k, v := range vals {
			if m := putField(b, f.off+k*fieldSize[f.typ], f.typ, v); m != nil {
				return nil, entryError(t.name, id, fmt.Sprintf("%s[%d]", f.name, k), *m)
			}
		}
	}
	return b, nil
}

// encodeText: a text of entry ID in the game's character set, 255 bytes at most (LIMIT)
func encodeText(t string, id int64, k int, lang string, v any, limit int) ([]byte, error) {
	s, ok := v.(string)
	if !ok {
		return nil, entryError(t, id, textLabel(t, k, lang), newMsg("{value} is not a text", "value", jsonText(v)))
	}
	e, err := cp437Encode(s)
	if err != nil {
		u := err.(unencodable)
		return nil, entryError(t, id, textLabel(t, k, lang),
			newMsg("\"{chars}\" cannot be written in the game's character set (code page 437)", "chars", u.chars))
	}
	if limit > 0 && len(e) > limit {
		return nil, entryError(t, id, textLabel(t, k, lang), newMsg("{n} bytes, at most {max}", "n", len(e), "max", limit))
	}
	return e, nil
}

// count: a list of texts with too many or too few in it (or no list)
func count(v any, want int, what word) msg {
	if l, ok := v.([]any); ok {
		return newMsg("{n} instead of {want} ({what})", "n", len(l), "what", what, "want", want)
	}
	return newMsg("{value} is not a list of {what}", "value", jsonText(v), "what", what)
}

// tooLarge: a table's texts of one language beyond the 64 KB the game reads them in
func tooLarge(t, lang string, n int) msg {
	return newMsg("{table}: the {lang} texts are {kb} KB, at most 64 KB (the game reads them as one resource)",
		"table", tableWord(t), "lang", langWord(lang), "kb", (n+1023)/1024)
}

// buildTables: the documents -> {resource id: bytes}, as tools/content.py build
func buildTables(docs map[string]obj) (map[int][]byte, error) {
	out := map[int][]byte{}
	for _, t := range tables {
		doc, ok := docs[t.name]
		if !ok {
			return nil, newMsg("{table}: no table", "table", tableWord(t.name))
		}
		recs, _, err := records(doc)
		if err != nil {
			return nil, newMsg("{table}: {why}", "table", tableWord(t.name), "why", err)
		}
		if len(recs) == 0 {
			return nil, newMsg("{table}: no entries", "table", tableWord(t.name))
		}
		top := int64(-1)
		for id := range recs {
			if id < 0 {
				return nil, newMsg("{entry} #{id}: no such number", "entry", entryWord(t.name), "id", id)
			}
			top = max(top, id)
		}
		if top >= int64(t.top()) {
			return nil, newMsg("{entry} #{id}: the number is too high (at most {max})", "entry", entryWord(t.name), "id", top, "max", t.top()-1)
		}
		n := t.count
		if t.max > 0 {
			n = max(t.count, int(top)+1)
		}
		var data []byte
		for i := 0; i < n; i++ {
			r := recs[int64(i)]
			if r == nil {
				r = obj{}
			}
			b, err := packRecord(t, int64(i), r)
			if err != nil {
				return nil, err
			}
			data = append(data, b...)
		}
		out[t.res] = data
		if t.texts != nil {
			if err := buildTexts(t, doc, recs, n, out); err != nil {
				return nil, err
			}
		}
	}
	if err := saveTooLarge(out); err != nil {
		return nil, err
	}
	return out, nil
}

// buildTexts: table T's texts in both languages, as the game reads them
func buildTexts(t table, doc obj, recs map[int64]obj, n int, out map[int][]byte) error {
	tx := t.texts
	tails, _ := doc["text_tail"].(obj)
	extra, _ := doc["text_extra"].(obj)
	for _, lang := range langs {
		res := tx.de
		if lang == "en" {
			res = tx.en
		}
		th, _ := tails[lang].(string)
		tail, err := hex.DecodeString(th)
		if err != nil {
			return newMsg("{table}: text_tail {lang}: not hex", "table", tableWord(t.name), "lang", lang)
		}
		// text: entry I's texts in this language (a list), or none
		text := func(i int) (any, bool) {
			m, _ := recs[int64(i)]["text"].(obj)
			v, ok := m[lang]
			return v, ok
		}
		var d []byte
		switch {
		case tx.kind == "pstr": // per entry 1..n-1: its texts as Pascal strings, an index of where each begins
			var blobs [][]byte
			for i := 1; i < n; i++ {
				v, ok := text(i)
				var bl []byte
				switch x := v.(type) {
				case nil:
					if ok {
						return entryError(t.name, int64(i), textsWord(lang), newMsg("none"))
					}
				case obj: // (an entry that is not clean strings: film 200 in the original)
					raw, _ := x["raw"].(string)
					if bl, err = hex.DecodeString(raw); err != nil || raw == "" {
						return entryError(t.name, int64(i), textsWord(lang), newMsg("raw is not hex"))
					}
				case []any:
					for k, s := range x {
						e, err := encodeText(t.name, int64(i), k, lang, s, 255)
						if err != nil {
							return err
						}
						bl = append(append(bl, byte(len(e))), e...)
					}
				default:
					return entryError(t.name, int64(i), textsWord(lang), newMsg("{value} is not a list of texts", "value", jsonText(v)))
				}
				blobs = append(blobs, bl)
			}
			if d, err = indexed(blobs, tail); err != nil {
				return tooLarge(t.name, lang, len(d))
			}
		case tx.kind == "raw": // news nodes: one text each (headlines)
			var blobs [][]byte
			for i := 1; i < n; i++ {
				v, ok := text(i)
				if !ok {
					v = ""
				}
				e, err := encodeText(t.name, int64(i), 0, lang, v, 0)
				if err != nil {
					return err
				}
				blobs = append(blobs, e)
			}
			if d, err = indexed(blobs, tail); err != nil {
				return tooLarge(t.name, lang, len(d))
			}
		case tx.sections > 0: // ads: advertiser and client, one entry per record each
			secs := [2][][]byte{}
			for i := 0; i < n; i++ {
				v, ok := text(i)
				p, isList := v.([]any)
				if !ok {
					p, isList = []any{"", ""}, true
				}
				if !isList || len(p) != 2 {
					return entryError(t.name, int64(i), textsWord(lang), count(v, 2, word{"advertiser, slogan", "Werbekunde, Slogan"}))
				}
				for k := 0; k < 2; k++ {
					e, err := encodeText(t.name, int64(i), k, lang, p[k], 0)
					if err != nil {
						return err
					}
					secs[k] = append(secs[k], e)
				}
			}
			d = lens(secs[:], tail)
		default: // news phrases: 561 lengths, phrase j of topic t at t*10 + j
			flat := make([][]byte, phraseSlots)
			for i := 0; i < t.count; i++ {
				v, ok := text(i)
				if !ok {
					continue
				}
				ph, isList := v.([]any)
				if !isList || len(ph) != tx.per {
					return entryError(t.name, int64(i), textsWord(lang), count(v, tx.per, word{"phrases", "Phrasen"}))
				}
				for k, s := range ph {
					e, err := encodeText(t.name, int64(i), k, lang, s, 0)
					if err != nil {
						return err
					}
					flat[i*tx.per+1+k] = e
				}
			}
			ex, _ := extra[lang].(obj)
			for k, v := range ex { // (the slots no topic uses: the original's leftovers)
				j, err := strconv.Atoi(k)
				s, isText := v.(string)
				if err != nil || j < 0 || j >= phraseSlots || !isText {
					return newMsg("{table}: text_extra {lang} {slot}: not a phrase slot", "table", tableWord(t.name), "lang", lang, "slot", k)
				}
				e, err := cp437Encode(s)
				if err != nil {
					return newMsg("{table}: text_extra {lang} {slot}: \"{chars}\" cannot be written in the game's character set (code page 437)",
						"table", tableWord(t.name), "lang", lang, "slot", k, "chars", err.(unencodable).chars)
				}
				flat[j] = e
			}
			d = lens([][][]byte{flat}, tail)
		}
		if len(d) > 0xFFFF {
			return tooLarge(t.name, lang, len(d))
		}
		out[res] = d
	}
	return nil
}

// indexed: the texts of entries 1..n-1 -- an index of n 16-bit offsets, then the texts
// (an error when they pass 64 KB: the data so far tells how far)
func indexed(blobs [][]byte, tail []byte) ([]byte, error) {
	count := len(blobs) + 1
	d := make([]byte, 2*count)
	pos := 2 * count
	binary.LittleEndian.PutUint16(d, uint16(pos))
	for k, bl := range blobs {
		pos += len(bl)
		if pos <= 0xFFFF {
			binary.LittleEndian.PutUint16(d[2*(k+1):], uint16(pos))
		}
	}
	for _, bl := range blobs {
		d = append(d, bl...)
	}
	d = append(d, tail...)
	if pos > 0xFFFF || len(d) > 0xFFFF {
		return d, fmt.Errorf("too large")
	}
	return d, nil
}

// lens: sections of texts, each a table of their 16-bit lengths, then the texts
func lens(secs [][][]byte, tail []byte) []byte {
	var b []byte
	for _, sec := range secs {
		for _, e := range sec {
			b = binary.LittleEndian.AppendUint16(b, uint16(len(e)))
		}
		for _, e := range sec {
			b = append(b, e...)
		}
	}
	return append(b, tail...)
}
