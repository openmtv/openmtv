package main

import (
	"os"
	"strings"
	"unicode/utf8"
)

// The help, for players (the former Python launcher (play.py) has the developers' options too).

var helpText = map[string]string{
	"en": `OpenMTV -- how to start it

{top}
    --classic                exactly the original (none of its bugs fixed)
    --fixes=a,b              only the named bug fixes
    --mod NAME | A+B+C       with the content pack mods/NAME (several: in this order)
    --fullscreen             fullscreen (F11 or Alt+Enter switch while playing)
    --scale N                the window N times 320x240
    --aspect 4:3|square      4:3 like a VGA monitor, or square pixels
    --filter sharp|nearest|linear|mmpx|hr-pixel
                             sharp: even pixels at any size; mmpx: smoothed edges;
                             hr-pixel: drawn at 4 times the resolution (a finer font,
                             exact frames and bars), the pictures' pixels as they are
    --integer                whole-number scale factors only
    --mouse captured|free|system
                             free: the system pointer moves the game's cursor;
                             system: the system's arrow is the game's
    --start-money=N          the cash of a new game, in dollars (the original: 200000)
    --credit-line=N          the most the boss lends (image below 50), in dollars:
                             +100000 a day from day 2 (the original: 300000);
                             original: the original's credit, fix credit-repay off
    --terror-damage=N        what a terrorist's bomb costs the owner of the room, in
                             dollars (the original: 50000; 0: nothing, no message)
    --sammy-prize=N[,N,N,N]  the Sammy winner's prize money, in dollars: one amount, or
                             news, culture, shows, series (the original: none)
    --fourth-speed[=yes|no]  a fourth game speed, twice the fastest
    --fast-rooms[=yes|no]    room changes at once, without the wipe and loading wait
    --cast-fit[=yes|no]      the shop shows each actor's fit for the role, in per cent
    --series-topicality[=yes|no]
                             the series card shows how topical the series still is
    --ai-improve=off|normal|strong
                             computer players build up their transmitters when you
                             build up yours, a while later (the original: off)
    --ad-offers=N            the contracts the ad agency offers at a time, 10 to 17
                             (the original: 10)
    --film-offers=N          the films and series the film dealer offers at a time,
                             10 to 17 (the original: 10)
    --settings=FILE          another settings file
    --print                  the game's command line, printed instead of started
    -h, --help               this help

The settings: {settings}
The original game files (SOURCE1.RSC, SOURCE2.RSC): {files}
`,
	"de": `OpenMTV -- so startet man es

{top}
    --classic                genau das Original (keiner seiner Fehler behoben)
    --fixes=a,b              nur die genannten Fehlerbehebungen
    --mod NAME | A+B+C       mit dem Inhaltspaket mods/NAME (mehrere: in dieser Folge)
    --fullscreen             Vollbild (F11 oder Alt+Enter schalten beim Spielen um)
    --scale N                das Fenster N-mal 320x240
    --aspect 4:3|square      4:3 wie ein VGA-Monitor, oder quadratische Pixel
    --filter sharp|nearest|linear|mmpx|hr-pixel
                             sharp: gleichmäßige Pixel in jeder Größe; mmpx: geglättete Kanten;
                             hr-pixel: in vierfacher Auflösung gezeichnet (feinere Schrift,
                             genaue Rahmen und Balken), die Pixel der Bilder wie sie sind
    --integer                nur ganzzahlige Vergrößerung
    --mouse captured|free|system
                             free: der Mauszeiger des Systems führt den des Spiels;
                             system: der Pfeil des Systems ist der des Spiels
    --start-money=N          das Startgeld eines neuen Spiels in Dollar (Original: 200000)
    --credit-line=N          so viel leiht der Chef höchstens (Image unter 50), in Dollar:
                             ab Tag 2 jeden Tag 100000 mehr (Original: 300000);
                             original: der Kredit des Originals, Fix credit-repay aus
    --terror-damage=N        so viel kostet die Bombe eines Terroristen den Besitzer des Raums,
                             in Dollar (Original: 50000; 0: nichts, keine Meldung)
    --sammy-prize=N[,N,N,N]  das Preisgeld des Sammy-Gewinners in Dollar: ein Betrag, oder
                             News, Kultur, Shows, Serien (Original: keins)
    --fourth-speed[=yes|no]  eine vierte Spielgeschwindigkeit, doppelt so schnell wie die schnellste
    --fast-rooms[=yes|no]    Raumwechsel sofort, ohne Blende und Ladezeit
    --cast-fit[=yes|no]      der Laden zeigt, wie gut jeder Schauspieler zur Rolle passt, in Prozent
    --series-topicality[=yes|no]
                             die Serienkarte zeigt, wie aktuell die Serie noch ist
    --ai-improve=off|normal|strong
                             Computerspieler bauen ihr Sendenetz nach, wenn du deins
                             ausbaust, mit etwas Verzögerung (das Original: off)
    --ad-offers=N            so viele Verträge bietet die Werbeagentur auf einmal an,
                             10 bis 17 (Original: 10)
    --film-offers=N          so viele Filme und Serien bietet der Filmhändler auf einmal an,
                             10 bis 17 (Original: 10)
    --settings=FILE          eine andere Einstellungsdatei
    --print                  die Befehlszeile des Spiels nur ausgeben, nicht starten
    -h, --help               diese Hilfe

Die Einstellungen: {settings}
Die Original-Spieldateien (SOURCE1.RSC, SOURCE2.RSC): {files}
`,
}

type helpRow struct {
	arg, text string
	cont      bool
} // cont: a second line of the one above

var helpTop = map[string][]helpRow{
	"en": {
		{"", "the launcher in the browser: the contents, the settings,", false},
		{"", "play, multiplayer (LAN or internet), the content editor", true},
		{"editor", "the content editor in the browser: films, series, ads, news", false},
		{"--direct", "the game at once, as the settings have it", false},
		{"d | e", "the game at once, in German | English this time", false},
		{"OPTION ...", "the game at once, with these options this time:", false},
	},
	"de": {
		{"", "der Launcher im Browser: Inhalte, Einstellungen, Spielen,", false},
		{"", "Mehrspieler (LAN oder Internet), der Inhalts-Editor", true},
		{"editor", "der Inhalts-Editor im Browser: Filme, Serien, Werbung, Nachrichten", false},
		{"--direct", "sofort das Spiel, so wie die Einstellungen es sagen", false},
		{"d | e", "sofort das Spiel, diesmal auf Deutsch | Englisch", false},
		{"OPTION ...", "sofort das Spiel, diesmal mit diesen Optionen:", false},
	},
}

// showHelp: -h, --help -- the options, in the settings' language (German or English)
func showHelp() int {
	lang := "en"
	settings := settingsPath()
	if settingsValue(settings, "game", "language") == "german" {
		lang = "de"
	}
	files := gameDir()
	if os.Getenv("GAME") != "" {
		files += " (GAME=...)"
	}
	cmd := os.Getenv("MADTV_CMD") // (OpenMTV.cmd says it is the one)
	if cmd == "" {
		cmd = play
	}
	w := utf8.RuneCountInString(cmd) + 15
	if w < 27 {
		w = 27
	}
	var top strings.Builder
	for _, r := range helpTop[lang] {
		a := ""
		if !r.cont {
			a = strings.TrimSpace(cmd + " " + r.arg)
		}
		top.WriteString("  " + a + strings.Repeat(" ", max(0, w-utf8.RuneCountInString(a))) + r.text + "\n")
	}
	say("%s", strings.NewReplacer("{top}", top.String(), "{settings}", settings, "{files}", files).Replace(helpText[lang]))
	return 0
}
