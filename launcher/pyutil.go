package main

import (
	"fmt"
	"path/filepath"
	"strings"
	"unicode"
	"unicode/utf8"
)

// repr() of a string -- the lobby's messages, which the Python launcher and the
// Go one word alike (tools/mpedgetest.py) -- and Windows' quoting of a command
// line (the MS C runtime's rules: what --print shows can be pasted).

// absPath: P made absolute (as it is if that fails)
func absPath(p string) string {
	if a, err := filepath.Abs(p); err == nil {
		return a
	}
	return filepath.Clean(p)
}

// pyRepr: repr() of a string
func pyRepr(s string) string {
	q := byte('\'')
	if strings.IndexByte(s, '\'') >= 0 && strings.IndexByte(s, '"') < 0 {
		q = '"'
	}
	var b strings.Builder
	b.WriteByte(q)
	for i := 0; i < len(s); {
		r, n := utf8.DecodeRuneInString(s[i:])
		switch {
		case r == utf8.RuneError && n == 1: // (Python: a surrogate escape)
			fmt.Fprintf(&b, `\udc%02x`, s[i])
		case r == '\\' || r == rune(q):
			b.WriteByte('\\')
			b.WriteRune(r)
		case r == '\t':
			b.WriteString(`\t`)
		case r == '\n':
			b.WriteString(`\n`)
		case r == '\r':
			b.WriteString(`\r`)
		case r == ' ' || unicode.IsPrint(r):
			b.WriteRune(r)
		case r < 0x100:
			fmt.Fprintf(&b, `\x%02x`, r)
		case r < 0x10000:
			fmt.Fprintf(&b, `\u%04x`, r)
		default:
			fmt.Fprintf(&b, `\U%08x`, r)
		}
		i += n
	}
	b.WriteByte(q)
	return b.String()
}

// winQuote: a command line as Windows programs read it (the MS C runtime's rules)
func winQuote(seq []string) string {
	var b strings.Builder
	for i, arg := range seq {
		if i > 0 {
			b.WriteByte(' ')
		}
		quote := arg == "" || strings.ContainsAny(arg, " \t")
		if quote {
			b.WriteByte('"')
		}
		bs := 0 // backslashes not written yet: doubled before a quote
		for j := 0; j < len(arg); j++ {
			switch c := arg[j]; c {
			case '\\':
				bs++
			case '"':
				b.WriteString(strings.Repeat(`\`, bs*2) + `\"`)
				bs = 0
			default:
				b.WriteString(strings.Repeat(`\`, bs))
				bs = 0
				b.WriteByte(c)
			}
		}
		b.WriteString(strings.Repeat(`\`, bs))
		if quote {
			b.WriteString(strings.Repeat(`\`, bs) + `"`)
		}
	}
	return b.String()
}

// pyTypeName: what Python calls a JSON value's type (the lobby's messages)
func pyTypeName(v any) string {
	switch v.(type) {
	case nil:
		return "NoneType"
	case bool:
		return "bool"
	case int64:
		return "int"
	case float64:
		return "float"
	case string:
		return "str"
	case []any:
		return "list"
	}
	return "dict"
}
