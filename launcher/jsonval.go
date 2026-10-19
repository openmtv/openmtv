package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strconv"
	"strings"
	"unicode/utf8"
)

// JSON for the content tables and the network game: objects (map[string]any),
// lists ([]any), whole numbers (int64), other numbers (float64), strings,
// true/false, null; compared strictly, copied deeply.

type obj = map[string]any

func readJSON(path string) (any, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	return parseJSON(data)
}

func parseJSON(data []byte) (any, error) {
	d := json.NewDecoder(bytes.NewReader(data))
	d.UseNumber()
	var v any
	if err := d.Decode(&v); err != nil {
		var se *json.SyntaxError
		pos := len(data) // (the text ends too early)
		if errors.As(err, &se) {
			pos = int(se.Offset) - 1
		}
		return nil, jsonError{data, max(pos, 0), err}
	}
	rest := int(d.InputOffset()) // (more after the value: Python's "Extra data")
	for rest < len(data) && strings.IndexByte(" \t\r\n", data[rest]) >= 0 {
		rest++
	}
	if rest < len(data) {
		return nil, jsonError{data, rest, errors.New("extra data")}
	}
	return numbers(v), nil
}

// jsonError: a JSON text that cannot be read, and where (line and column as Python's
// json module counts them: in characters, from 1)
type jsonError struct {
	data []byte
	pos  int
	err  error
}

func (e jsonError) Error() string {
	line, col := e.where()
	return fmt.Sprintf("%v (line %d, column %d)", e.err, line, col)
}

func (e jsonError) where() (int, int) {
	before := string(e.data[:min(e.pos, len(e.data))])
	nl := strings.LastIndexByte(before, '\n')
	return strings.Count(before, "\n") + 1, utf8.RuneCountInString(before[nl+1:]) + 1
}

func numbers(v any) any {
	switch x := v.(type) {
	case json.Number:
		s := string(x)
		if !strings.ContainsAny(s, ".eE") {
			if i, err := strconv.ParseInt(s, 10, 64); err == nil {
				return i
			}
		}
		f, _ := strconv.ParseFloat(s, 64)
		return f
	case []any:
		for i := range x {
			x[i] = numbers(x[i])
		}
	case map[string]any:
		for k := range x {
			x[k] = numbers(x[k])
		}
	}
	return v
}

func deepCopy(v any) any {
	switch x := v.(type) {
	case []any:
		out := make([]any, len(x))
		for i := range x {
			out[i] = deepCopy(x[i])
		}
		return out
	case map[string]any:
		out := make(map[string]any, len(x))
		for k, e := range x {
			out[k] = deepCopy(e)
		}
		return out
	}
	return v
}

// equal: two JSON values the same (a whole number is not a fraction, nor true 1)
func equal(a, b any) bool {
	switch x := a.(type) {
	case nil:
		return b == nil
	case bool:
		y, ok := b.(bool)
		return ok && x == y
	case int64:
		y, ok := b.(int64)
		return ok && x == y
	case float64:
		y, ok := b.(float64)
		return ok && x == y
	case string:
		y, ok := b.(string)
		return ok && x == y
	case []any:
		y, ok := b.([]any)
		if !ok || len(x) != len(y) {
			return false
		}
		for i := range x {
			if !equal(x[i], y[i]) {
				return false
			}
		}
		return true
	case map[string]any:
		y, ok := b.(map[string]any)
		if !ok || len(x) != len(y) {
			return false
		}
		for k, e := range x {
			f, ok := y[k]
			if !ok || !equal(e, f) {
				return false
			}
		}
		return true
	}
	return false
}

// intOf: a record's whole number KEY (0 when it has none -- or something else, which
// packing it refuses)
func intOf(r obj, key string) int64 {
	x, _ := r[key].(int64)
	return x
}

// truthy: Python's bool() of a JSON value
func truthy(v any) bool {
	switch x := v.(type) {
	case nil:
		return false
	case bool:
		return x
	case int64:
		return x != 0
	case float64:
		return x != 0
	case string:
		return x != ""
	case []any:
		return len(x) > 0
	case map[string]any:
		return len(x) > 0
	}
	return true
}
