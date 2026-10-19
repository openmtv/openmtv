package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"sort"
	"strconv"
	"strings"
)

// JSON with its objects' key order -- the order Python's json keeps (a dict remembers
// the order its keys came in): the editor's packs are written as
// the former Python editor (editor.py) wrote them (json.dump, indent 1, the characters as they
// are), so a pack saved here differs from its last version only where it changed.
// The tables of the original (export, map[string]any) get the export's order
// (canonical); a pack's file and a page's records keep theirs.  For the checks and the
// build the same text is read once more as plain values (parseJSON).

type onode struct {
	kind  byte // 'o' object, 'a' array, 's' string, 'n' number, 't' true, 'f' false, 'z' null
	keys  []string
	vals  []*onode // an object's values, in the order of keys
	items []*onode // an array's
	text  string   // a string's value, a number's text
}

// maxDepth: the deepest a text may nest its objects and arrays (a pack's tables: 4) --
// deeper ones are refused, not read into a stack overflow that ends the launcher
const maxDepth = 1000

func parseOrdered(data []byte) (*onode, error) {
	d := json.NewDecoder(bytes.NewReader(data))
	d.UseNumber()
	n, err := orderedValue(d, 0)
	if err != nil {
		return nil, err
	}
	if _, err := d.Token(); err == nil {
		return nil, errors.New("extra data")
	}
	return n, nil
}

func orderedValue(d *json.Decoder, depth int) (*onode, error) {
	tok, err := d.Token()
	if err != nil {
		return nil, err
	}
	switch t := tok.(type) {
	case json.Delim:
		if depth++; depth > maxDepth {
			return nil, fmt.Errorf("nested deeper than %d", maxDepth)
		}
		switch t {
		case '{':
			n := &onode{kind: 'o'}
			for d.More() {
				kt, err := d.Token()
				if err != nil {
					return nil, err
				}
				k, _ := kt.(string)
				v, err := orderedValue(d, depth)
				if err != nil {
					return nil, err
				}
				n.set(k, v) // (a key twice: Python keeps the first place, the last value)
			}
			_, err := d.Token()
			return n, err
		case '[':
			n := &onode{kind: 'a', items: []*onode{}}
			for d.More() {
				v, err := orderedValue(d, depth)
				if err != nil {
					return nil, err
				}
				n.items = append(n.items, v)
			}
			_, err := d.Token()
			return n, err
		}
	case string:
		return &onode{kind: 's', text: t}, nil
	case json.Number:
		return &onode{kind: 'n', text: string(t)}, nil
	case bool:
		if t {
			return &onode{kind: 't'}, nil
		}
		return &onode{kind: 'f'}, nil
	case nil:
		return &onode{kind: 'z'}, nil
	}
	return nil, errors.New("not JSON")
}

func readOrdered(path string) (*onode, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	return parseOrdered(data)
}

// get: an object's value of KEY (nil: none, or not an object)
func (n *onode) get(key string) *onode {
	if n == nil || n.kind != 'o' {
		return nil
	}
	for i, k := range n.keys {
		if k == key {
			return n.vals[i]
		}
	}
	return nil
}

// set: KEY's value -- in its place where it is there, else at the end (as a dict)
func (n *onode) set(key string, v *onode) {
	for i, k := range n.keys {
		if k == key {
			n.vals[i] = v
			return
		}
	}
	n.keys = append(n.keys, key)
	n.vals = append(n.vals, v)
}

func (n *onode) copy() *onode {
	c := *n
	if n.kind == 'o' {
		c.keys = append([]string(nil), n.keys...)
		c.vals = make([]*onode, len(n.vals))
		for i, v := range n.vals {
			c.vals[i] = v.copy()
		}
	}
	if n.kind == 'a' {
		c.items = make([]*onode, len(n.items))
		for i, v := range n.items {
			c.items[i] = v.copy()
		}
	}
	return &c
}

// value: the plain JSON value (jsonval.go: objects, lists, int64/float64, ...)
func (n *onode) value() any {
	switch n.kind {
	case 'o':
		m := obj{}
		for i, k := range n.keys {
			m[k] = n.vals[i].value()
		}
		return m
	case 'a':
		l := make([]any, len(n.items))
		for i, v := range n.items {
			l[i] = v.value()
		}
		return l
	case 's':
		return n.text
	case 'n':
		return numbers(json.Number(n.text))
	case 't':
		return true
	case 'f':
		return false
	}
	return nil
}

// ordered: a plain value as a tree, objects in ORDER's order of their keys (the keys
// it does not name after them, sorted; numbers among them by value)
func ordered(v any, order func(path []string, m obj) []string, path []string) *onode {
	switch x := v.(type) {
	case obj:
		n := &onode{kind: 'o'}
		for _, k := range keyOrder(x, order(path, x)) {
			n.keys = append(n.keys, k)
			n.vals = append(n.vals, ordered(x[k], order, append(path[:len(path):len(path)], k)))
		}
		return n
	case []any:
		n := &onode{kind: 'a', items: make([]*onode, len(x))}
		for i, e := range x {
			n.items[i] = ordered(e, order, append(path[:len(path):len(path)], "[]"))
		}
		return n
	case []string:
		n := &onode{kind: 'a', items: make([]*onode, len(x))}
		for i, e := range x {
			n.items[i] = &onode{kind: 's', text: e}
		}
		return n
	case string:
		return &onode{kind: 's', text: x}
	case int64:
		return &onode{kind: 'n', text: strconv.FormatInt(x, 10)}
	case int:
		return &onode{kind: 'n', text: strconv.Itoa(x)}
	case float64:
		return &onode{kind: 'n', text: pyFloat(x)}
	case bool:
		if x {
			return &onode{kind: 't'}
		}
		return &onode{kind: 'f'}
	}
	return &onode{kind: 'z'}
}

func keyOrder(m obj, first []string) []string {
	out := make([]string, 0, len(m))
	seen := map[string]bool{}
	for _, k := range first {
		if _, ok := m[k]; ok && !seen[k] {
			out = append(out, k)
			seen[k] = true
		}
	}
	var rest []string
	for k := range m {
		if !seen[k] {
			rest = append(rest, k)
		}
	}
	sort.Slice(rest, func(i, j int) bool {
		a, aok := keyNumber(rest[i])
		b, bok := keyNumber(rest[j])
		if aok && bok && a != b {
			return a < b
		}
		if aok != bok {
			return aok
		}
		return rest[i] < rest[j]
	})
	return append(out, rest...)
}

// keyNumber: a key that is a number ("12", "0x13") -> its value
func keyNumber(k string) (int64, bool) {
	if strings.HasPrefix(k, "0x") {
		v, err := strconv.ParseInt(k[2:], 16, 64)
		return v, err == nil
	}
	v, err := strconv.ParseInt(k, 10, 64)
	return v, err == nil
}

// canonical: a content table (or a value in one) in the export's order -- the document's
// keys, a record's id, its fields, unknown and text, the languages de and en
func canonical(table string, v any) *onode {
	t := tables[tableIdx[table]]
	order := func(path []string, m obj) []string {
		switch {
		case len(path) == 0:
			return []string{"table", "resource", "record_size", "count", "records", "text_tail", "text_extra"}
		case len(path) == 2 && path[0] == "records":
			keys := []string{"id"}
			for _, f := range t.fields {
				keys = append(keys, f.name)
			}
			return append(keys, "unknown", "text")
		case len(path) == 3 && path[0] == "records" && path[2] == "text", len(path) == 1:
			return langs
		}
		return nil
	}
	return ordered(v, order, nil)
}

// canonicalDocs: the tables of a selection in the export's order
func canonicalDocs(docs map[string]obj) map[string]*onode {
	out := map[string]*onode{}
	for t, d := range docs {
		out[t] = canonical(t, d)
	}
	return out
}

// pyIndent: the text json.dump(doc, f, ensure_ascii=False, indent=1) writes
func (n *onode) pyIndent() []byte {
	var b strings.Builder
	n.write(&b, 0, true)
	return []byte(b.String())
}

func (n *onode) write(b *strings.Builder, depth int, indent bool) {
	nl := func(d int) {
		if indent {
			b.WriteByte('\n')
			b.WriteString(strings.Repeat(" ", d))
		}
	}
	switch n.kind {
	case 'o':
		if len(n.keys) == 0 {
			b.WriteString("{}")
			return
		}
		b.WriteByte('{')
		for i, k := range n.keys {
			if i > 0 {
				b.WriteByte(',')
			}
			nl(depth + 1)
			pyString(b, k)
			b.WriteByte(':')
			if indent {
				b.WriteByte(' ')
			}
			n.vals[i].write(b, depth+1, indent)
		}
		nl(depth)
		b.WriteByte('}')
	case 'a':
		if len(n.items) == 0 {
			b.WriteString("[]")
			return
		}
		b.WriteByte('[')
		for i, v := range n.items {
			if i > 0 {
				b.WriteByte(',')
			}
			nl(depth + 1)
			v.write(b, depth+1, indent)
		}
		nl(depth)
		b.WriteByte(']')
	case 's':
		pyString(b, n.text)
	case 'n':
		b.WriteString(pyNumber(n.text))
	case 't':
		b.WriteString("true")
	case 'f':
		b.WriteString("false")
	default:
		b.WriteString("null")
	}
}

// pyNumber: a number as Python writes it again after reading it: a whole number as it
// is (-0 is 0), any other as its float's repr
func pyNumber(text string) string {
	if !strings.ContainsAny(text, ".eE") {
		if text == "-0" {
			return "0"
		}
		return text
	}
	f, err := strconv.ParseFloat(text, 64)
	if err != nil { // (out of range: Python's float gives inf)
		if strings.HasPrefix(text, "-") {
			return "-Infinity"
		}
		return "Infinity"
	}
	return pyFloat(f)
}

// MarshalJSON: the tree in a reply, its order kept
func (n *onode) MarshalJSON() ([]byte, error) {
	var b strings.Builder
	n.write(&b, 0, false)
	return []byte(b.String()), nil
}

// writeOrdered: a JSON file written whole (in place of the old one), as the Python
// editor wrote it
func writeOrdered(path string, n *onode) error {
	return replaceFile(path, n.pyIndent())
}
