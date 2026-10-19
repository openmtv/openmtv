package main

import (
	"fmt"
	"strings"
)

// The page's language: it sends its interface language with every request
// (X-UI-Lang), the messages that reach it come back in it: msg (English text as
// the key, {name} placeholders -- the former Python editor (editor.py)'s tr()), its words in both
// languages (word); the command line has them in English.

type msg struct {
	key  string
	args map[string]any // string, int, msg, or joined
}

// joined: a list in a message, its items joined with sep (each in the language)
type joined struct {
	sep   string
	items []msg
}

func newMsg(key string, kv ...any) msg {
	m := msg{key: key, args: map[string]any{}}
	for i := 0; i+1 < len(kv); i += 2 {
		m.args[kv[i].(string)] = kv[i+1]
	}
	return m
}

func (m msg) Error() string { return m.in("en") }

func (m msg) in(lang string) string {
	s := m.key
	if lang == "de" {
		if de, ok := msgDE[m.key]; ok {
			s = de
		}
	}
	// one pass over the text, as Python's str.format: what is filled in is not read again
	// (a value with braces stays as it is)
	var b strings.Builder
	for {
		i := strings.IndexByte(s, '{')
		if i < 0 {
			break
		}
		j := strings.IndexByte(s[i:], '}')
		if j < 0 {
			break
		}
		v, ok := m.args[s[i+1:i+j]]
		if !ok {
			b.WriteString(s[:i+1])
			s = s[i+1:]
			continue
		}
		b.WriteString(s[:i])
		switch x := v.(type) {
		case msg:
			b.WriteString(x.in(lang))
		case word:
			b.WriteString(x.in(lang))
		case joined:
			parts := make([]string, len(x.items))
			for i, it := range x.items {
				parts[i] = it.in(lang)
			}
			b.WriteString(strings.Join(parts, x.sep))
		default:
			b.WriteString(fmt.Sprint(x))
		}
		s = s[i+j+1:]
	}
	b.WriteString(s)
	return b.String()
}

// render: an error in the page's language, as the former Python editor (editor.py) words it
func render(err error, lang string) string {
	switch e := err.(type) {
	case msg:
		return e.in(lang)
	case joinedError:
		return e.in(lang)
	}
	return err.Error()
}

// word: a word of a message in both languages (an entry's name, a text's: "German
// title" / "deutscher Titel")
type word struct{ en, de string }

func (w word) in(lang string) string {
	if lang == "de" {
		return w.de
	}
	return w.en
}

// joinedError: several messages as one ("; " between them)
type joinedError struct{ joined }

func (e joinedError) Error() string { return e.in("en") }

func (e joinedError) in(lang string) string {
	parts := make([]string, len(e.items))
	for i, it := range e.items {
		parts[i] = it.in(lang)
	}
	return strings.Join(parts, e.sep)
}

var msgDE = map[string]string{
	"{file}: not loaded ({why})":                       "{file}: nicht geladen ({why})",
	"{file}: not loaded (no data for a minute)":        "{file}: nicht geladen (eine Minute lang keine Daten)",
	"{file}: not the file it should be (its checksum)": "{file}: nicht die erwartete Datei (ihre Prüfsumme)",
	"AI upscaling does not run on this system ({os})":  "KI-Upscaling läuft auf diesem System nicht ({os})",
	"a picture set is being made already":              "ein Bildersatz wird gerade schon erstellt",
	"AI upscaling failed: {why}":                       "das KI-Upscaling ist fehlgeschlagen: {why}",
	"make the picture set first":                       "erst den Bildersatz erstellen",
	"no game files":                                    "keine Spieldateien",
	"a mod name is 1-40 letters, digits, - or _":       "ein Mod-Name besteht aus 1-40 Buchstaben, Ziffern, - oder _",
	"no mod {name}":                                    "kein Mod {name}",
	"no such thing":                                    "gibt es nicht",
	"not found":                                        "nicht gefunden",
	"a new game gives each station 3 films (the marked ones, then cultural programmes on the market): only {n} are free, it hangs with fewer than 9": "ein neues Spiel gibt jedem Sender 3 Filme (erst die markierten, dann Kultursendungen vom Markt): nur {n} sind frei, mit weniger als 9 bleibt es hängen",
	"a new game gives each station one series (the marked ones, then series 18-20): only {n} are free, it hangs with fewer than 3":                   "ein neues Spiel gibt jedem Sender eine Serie (erst die markierten, dann die Serien 18-20): nur {n} sind frei, mit weniger als 3 bleibt es hängen",
	"English title": "englischer Titel", "German title": "deutscher Titel",
	"English description": "englische Beschreibung", "German description": "deutsche Beschreibung",
	"English advertiser": "englischer Werbekunde", "German advertiser": "deutscher Werbekunde",
	"English slogan": "englischer Slogan", "German slogan": "deutscher Slogan",
	"genre": "Genre", "length": "Länge", "quality": "Qualität", "topicality": "Aktualität", "price": "Preis",
	"at least one produced episode": "mindestens eine gedrehte Folge", "episode length": "Folgenlänge",
	"episode price": "Folgenpreis", "spots": "Spots", "days": "Tage", "minimum audience": "Mindestpublikum",
	"film": "Film", "series": "Serie", "ad": "Werbevertrag", "screenplay": "Drehbuch",
	// a screenplay's (checks.go screenplayMissing; the editor's words, game.js spMissing)
	"the programme it becomes":                            "das Programm, das daraus wird",
	"English title of series {id}":                        "englischer Titel von Serie {id}",
	"English title of film {id}":                          "englischer Titel von Film {id}",
	"German title of series {id}":                         "deutscher Titel von Serie {id}",
	"German title of film {id}":                           "deutscher Titel von Film {id}",
	"episode length of series {id}":                       "Folgenlänge von Serie {id}",
	"length of film {id}":                                 "Länge von Film {id}",
	"episodes of series {id}":                             "Folgen von Serie {id}",
	"base quality":                                        "Grundqualität",
	"base quality (episode {list})":                       "Grundqualität (Folge {list})",
	"base quality (episodes {list})":                      "Grundqualität (Folgen {list})",
	"shopping list":                                       "Einkaufsliste",
	"shopping list (episode {list})":                      "Einkaufsliste (Folge {list})",
	"shopping list (episodes {list})":                     "Einkaufsliste (Folgen {list})",
	"actor category of role {list}":                       "Besetzungsgruppe von Rolle {list}",
	"actor category of roles {list}":                      "Besetzungsgruppe der Rollen {list}",
	"requirements of role {list}":                         "Anforderungen von Rolle {list}",
	"requirements of roles {list}":                        "Anforderungen der Rollen {list}",
	"at most 3 sets in the first list (a studio holds 3)": "höchstens 3 Kulissen in der ersten Liste (ein Studio fasst 3)",
	"{what} {id}: {missing}":                              "{what} {id}: {missing}",
	"not playable yet -- {list}":                          "noch nicht spielbar -- {list}",
	"\"{chars}\" cannot be written in the game's character set (code page 437)": "\"{chars}\" lässt sich im Zeichensatz des Spiels (Codepage 437) nicht schreiben",
	"{setting}: \"{value}\" is not a value the game takes":                      "{setting}: \"{value}\" nimmt das Spiel nicht",
	"a mod twice in {spec}":                                                   "ein Mod doppelt in {spec}",
	"\"{pack}\" is in \"{other}\" already":                                    "\"{pack}\" steckt schon in \"{other}\"",
	"no setting {setting}":                                                    "keine Einstellung {setting}",
	"the game is not built yet (make build/mtv-dec)":                          "das Spiel ist noch nicht gebaut (make build/mtv-dec)",
	"\"{file}\" cannot be read (line {line}, column {col})":                   "\"{file}\" lässt sich nicht lesen (Zeile {line}, Spalte {col})",
	"\"{file}\" has the wrong form":                                           "\"{file}\" hat nicht die richtige Form",
	"no such saved game":                                                      "kein solches gespeichertes Spiel",
	"this savegame cannot be read":                                            "dieser Spielstand lässt sich nicht lesen",
	"this saved game was played with packs whose tables are not kept here":    "dieses Spiel lief mit Paketen, deren Tabellen hier nicht aufgehoben sind",
	"the packs of this saved game cannot be read (game.json)":                 "die Pakete dieses Spiels lassen sich nicht lesen (game.json)",
	"the packs of this saved game could not be built: {why}":                  "die Pakete dieses Spiels ließen sich nicht bauen: {why}",
	"this saved game was played with other game files ({saved}, here {here})": "dieses Spiel lief mit anderen Spieldateien ({saved}, hier {here})",
	// the packs, packed (pack.go)
	"{n} instead of {want} ({what})":            "{n} statt {want} ({what})",
	"{value} is not a list of {what}":           "{value} ist keine Liste ({what})",
	"none":                                      "keine",
	"{entry} #{id}, {what}: {problem}":          "{entry} #{id}, {what}: {problem}",
	"no list of records":                        "keine Liste von Einträgen",
	"an entry that is not an object: {value}":   "ein Eintrag, der kein Objekt ist: {value}",
	"an entry without its number (id): {value}": "ein Eintrag ohne seine Nummer (id): {value}",
	"{value} is not a whole number":             "{value} ist keine ganze Zahl",
	"{value} is not between {lo} and {hi}":      "{value} liegt nicht zwischen {lo} und {hi}",
	"not an object of offsets":                  "kein Objekt aus Stellen",
	"not an offset (0x..)":                      "keine Stelle (0x..)",
	"{value} is not hex":                        "{value} ist kein Hex",
	"beyond the entry's {size} bytes":           "jenseits der {size} Bytes des Eintrags",
	"{got}, it takes {n} numbers":               "{got}, es braucht {n} Zahlen",
	"{value} is not a text":                     "{value} ist kein Text",
	"{n} bytes, at most {max}":                  "{n} Bytes, höchstens {max}",
	"{table}: the {lang} texts are {kb} KB, at most 64 KB (the game reads them as one resource)": "{table}: die {lang} Texte sind {kb} KB groß, höchstens 64 KB (das Spiel liest sie als eine Ressource)",
	"{table}: no table":             "{table}: keine Tabelle",
	"{table}: {why}":                "{table}: {why}",
	"{table}: no entries":           "{table}: keine Einträge",
	"{entry} #{id}: no such number": "{entry} #{id}: keine solche Nummer",
	"{entry} #{id}: the number is too high (at most {max})": "{entry} #{id}: die Nummer ist zu hoch (höchstens {max})",
	"{table}: text_tail {lang}: not hex":                    "{table}: text_tail {lang}: kein Hex",
	"no texts":                                              "keine Texte",
	"raw is not hex":                                        "raw ist kein Hex",
	"{value} is not a list of texts":                        "{value} ist keine Liste von Texten",
	"{value} is not two texts (advertiser, slogan)":         "{value} sind nicht zwei Texte (Werbekunde, Slogan)",
	"{value} is not {n} phrases":                            "{value} sind nicht {n} Phrasen",
	"{table}: text_extra {lang} {slot}: not a phrase slot":  "{table}: text_extra {lang} {slot}: kein Platz für eine Phrase",
	"{table}: text_extra {lang} {slot}: \"{chars}\" cannot be written in the game's character set (code page 437)": "{table}: text_extra {lang} {slot}: \"{chars}\" lässt sich im Zeichensatz des Spiels (Codepage 437) nicht schreiben",
	"pack {pack}: {entry} #{id} twice":       "Paket {pack}: {entry} #{id} doppelt",
	"more than 999 {table} with pack {pack}": "mehr als 999 {table} mit Paket {pack}",
	"resource {n} larger than 64 KB":         "Ressource {n} größer als 64 KB",
	"{films} films, {series} series and {ads} ads: a savegame would have {n} bytes, at most {max} (each film past 200 takes 18, each series past 50 41, each ad past 50 1)": "{films} Filme, {series} Serien und {ads} Werbespots: ein Spielstand hätte {n} Bytes, höchstens {max} (jeder Film über 200 braucht 18, jede Serie über 50 41, jeder Werbespot über 50 1)",
	// the Go launcher's own
	// the editor's (editor.go), as the former Python editor (editor.py) words them
	"mod \"{name}\" exists already":      "den Mod \"{name}\" gibt es schon",
	"no table {name}":                    "keine Tabelle {name}",
	"\"{name}\" names the original game": "\"{name}\" heißt das Originalspiel",
	"\"{name}\" is protected and stays":  "\"{name}\" ist geschützt und bleibt",
	"\"{name}\" is protected: unlock it in the overview to change it directly": "\"{name}\" ist geschützt: zum direkten Ändern in der Übersicht entsperren",
	"In a network game the mouse is always System.":                            "Im Netzspiel ist die Maus immer „System“.",

	// the AI upscaling's making stopped by the launcher (ki.go)
	"AI upscaling stopped: no progress in {min} min": "KI-Upscaling abgebrochen: {min} min ohne Fortschritt",
}
