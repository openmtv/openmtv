package main

import (
	"bytes"
	"compress/zlib"
	"crypto/sha1"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"sort"
	"strconv"
	"strings"
)

// JSON for the network game, as the former Python lobby (mplobby.py) writes it: a selection's tables go
// through the server as json.dumps(docs, sort_keys=True, ensure_ascii=False,
// separators=(',', ':')) -- its SHA-1 names their build (build/mp/ID), so the text
// must be Python's to the byte, whichever launcher made it.

func pyDumps(v any) string {
	var b strings.Builder
	pyDump(&b, v)
	return b.String()
}

func pyDump(b *strings.Builder, v any) {
	switch x := v.(type) {
	case nil:
		b.WriteString("null")
	case bool:
		if x {
			b.WriteString("true")
		} else {
			b.WriteString("false")
		}
	case int64:
		b.WriteString(strconv.FormatInt(x, 10))
	case int:
		b.WriteString(strconv.Itoa(x))
	case float64:
		b.WriteString(pyFloat(x))
	case string:
		pyString(b, x)
	case []any:
		b.WriteByte('[')
		for i, e := range x {
			if i > 0 {
				b.WriteByte(',')
			}
			pyDump(b, e)
		}
		b.WriteByte(']')
	case []string:
		b.WriteByte('[')
		for i, e := range x {
			if i > 0 {
				b.WriteByte(',')
			}
			pyString(b, e)
		}
		b.WriteByte(']')
	case map[string]obj: // (the tables of a selection)
		m := make(map[string]any, len(x))
		for k, v := range x {
			m[k] = v
		}
		pyDump(b, m)
	case map[string]any:
		keys := make([]string, 0, len(x))
		for k := range x {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		b.WriteByte('{')
		for i, k := range keys {
			if i > 0 {
				b.WriteByte(',')
			}
			pyString(b, k)
			b.WriteByte(':')
			pyDump(b, x[k])
		}
		b.WriteByte('}')
	default: // (no other kinds in the tables; Go's own text as a last resort)
		j, _ := json.Marshal(x)
		b.Write(j)
	}
}

// pyFloat: Python's repr of a float -- the shortest digits, as a decimal from 1e-4 up to
// below 1e16 (1000000.0), else with an exponent (1e+16, 1.5e-05)
func pyFloat(x float64) string {
	switch {
	case math.IsInf(x, 1):
		return "Infinity"
	case math.IsInf(x, -1):
		return "-Infinity"
	case math.IsNaN(x):
		return "NaN"
	}
	e := strconv.FormatFloat(x, 'e', -1, 64)
	exp, _ := strconv.Atoi(e[strings.IndexByte(e, 'e')+1:])
	if x != 0 && (exp < -4 || exp >= 16) {
		return e
	}
	s := strconv.FormatFloat(x, 'f', -1, 64)
	if !strings.Contains(s, ".") {
		s += ".0"
	}
	return s
}

func pyString(b *strings.Builder, s string) {
	b.WriteByte('"')
	for _, r := range s {
		switch r {
		case '"':
			b.WriteString(`\"`)
		case '\\':
			b.WriteString(`\\`)
		case '\n':
			b.WriteString(`\n`)
		case '\r':
			b.WriteString(`\r`)
		case '\t':
			b.WriteString(`\t`)
		case '\b':
			b.WriteString(`\b`)
		case '\f':
			b.WriteString(`\f`)
		default:
			if r < 0x20 {
				fmt.Fprintf(b, `\u%04x`, r)
			} else {
				b.WriteRune(r)
			}
		}
	}
	b.WriteByte('"')
}

// packDocs: a selection's tables for the server -> (id, base64 of zlib of the JSON);
// the id names its build
func packDocs(docs any) (string, string) {
	raw := []byte(pyDumps(docs))
	sum := sha1.Sum(raw)
	return hex.EncodeToString(sum[:])[:16], base64.StdEncoding.EncodeToString(deflate(raw))
}

func deflate(raw []byte) []byte {
	var z bytes.Buffer
	w, _ := zlib.NewWriterLevel(&z, 9)
	w.Write(raw)
	w.Close()
	return z.Bytes()
}

// inflate: base64 of zlib -> the bytes, at most LIMIT of them (what a server sends is not trusted)
func inflate(data string, limit int) ([]byte, error) {
	raw, err := base64.StdEncoding.DecodeString(data)
	if err != nil {
		return nil, err
	}
	r, err := zlib.NewReader(bytes.NewReader(raw))
	if err != nil {
		return nil, err
	}
	out, err := io.ReadAll(io.LimitReader(r, int64(limit)+1))
	if err != nil || len(out) > limit {
		return nil, fmt.Errorf("more than %d bytes, or cut short", limit)
	}
	return out, nil
}

func unpackDocs(data string) (map[string]obj, error) {
	raw, err := inflate(data, 64<<20)
	if err != nil {
		return nil, err
	}
	v, err := parseJSON(raw)
	if err != nil {
		return nil, err
	}
	m, ok := v.(obj)
	if !ok {
		return nil, fmt.Errorf("not the tables of packs")
	}
	docs := map[string]obj{}
	for _, t := range tables {
		d, ok := m[t.name].(obj)
		if !ok {
			return nil, fmt.Errorf("'%s'", t.name)
		}
		docs[t.name] = d
	}
	return docs, nil
}

// readJSONObj: a JSON object from a file, {} when there is none or it is something else
func readJSONObj(path string) obj {
	v, err := readJSON(path)
	if m, ok := v.(obj); err == nil && ok {
		return m
	}
	return obj{}
}

// writeJSONFile: a JSON file written whole (in place of the old one), indented a little
func writeJSONFile(path string, doc any) error {
	var b bytes.Buffer
	e := json.NewEncoder(&b)
	e.SetEscapeHTML(false)
	e.SetIndent("", " ")
	if err := e.Encode(doc); err != nil {
		return err
	}
	return replaceFile(path, bytes.TrimRight(b.Bytes(), "\n"))
}
