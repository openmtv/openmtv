package main

import (
	"fmt"
	"strings"
	"unicode/utf8"
)

// Code page 437, the game's character set, as Python's codec has it: bytes 0-127
// are ASCII (the control characters too), 128-255 these.
const cp437High = "ÇüéâäàåçêëèïîìÄÅ" +
	"ÉæÆôöòûùÿÖÜ¢£¥₧ƒ" +
	"áíóúñÑªº¿⌐¬½¼¡«»" +
	"░▒▓│┤╡╢╖╕╣║╗╝╜╛┐" +
	"└┴┬├─┼╞╟╚╔╩╦╠═╬╧" +
	"╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀" +
	"αßΓπΣσµτΦΘΩδ∞φε∩" +
	"≡±≥≤⌠⌡÷≈°∙·√ⁿ²■\u00a0"

var (
	cp437Dec [256]rune
	cp437Enc = map[rune]byte{}
)

func init() {
	for i := 0; i < 128; i++ {
		cp437Dec[i] = rune(i)
	}
	i := 128
	for _, r := range cp437High {
		cp437Dec[i] = r
		i++
	}
	for b, r := range cp437Dec {
		cp437Enc[r] = byte(b)
	}
}

func cp437Decode(b []byte) string {
	var s strings.Builder
	for _, c := range b {
		s.WriteRune(cp437Dec[c])
	}
	return s.String()
}

// unencodable: what Python's encoder names in its error -- the first run of
// characters code page 437 does not have
type unencodable struct{ chars string }

func (e unencodable) Error() string {
	return fmt.Sprintf("'charmap' codec can't encode characters: %s", e.chars)
}

func cp437Encode(s string) ([]byte, error) {
	out := make([]byte, 0, len(s))
	for i := 0; i < len(s); {
		r, n := utf8.DecodeRuneInString(s[i:])
		b, ok := cp437Enc[r]
		if !ok || (r == utf8.RuneError && n == 1) {
			j := i
			for j < len(s) {
				r2, n2 := utf8.DecodeRuneInString(s[j:])
				if _, ok2 := cp437Enc[r2]; ok2 && !(r2 == utf8.RuneError && n2 == 1) {
					break
				}
				j += n2
			}
			return nil, unencodable{s[i:j]}
		}
		out = append(out, b)
		i += n
	}
	return out, nil
}
