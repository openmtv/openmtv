//go:build wails

package main

import (
	"context"
	"encoding/json"
	"html"
	"net/http"
	"path/filepath"
	"strings"
	"sync"

	wr "github.com/wailsapp/wails/v2/pkg/runtime"
)

// The window's first start without the original game files (SOURCE1.RSC,
// SOURCE2.RSC) above the program: a page of its own asks for their directory
// through the system's folder dialog, checks it, keeps it (launcher.json:
// gameDir finds it on every later start, also ./play.sh's) and then shows the
// launcher.  In the browser it stays as before: the message and the end.

type gameSetup struct {
	s       *server
	dir     string // where they were looked for
	missing error  // why they were not found
	ctx     context.Context

	mu     sync.Mutex
	busy   bool   // the dialog is open
	errKey string // the last choice: "nofiles" or "other" (with detail)
	detail string
	chosen string
}

func (g *gameSetup) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	switch {
	case r.Method == "GET" && (r.URL.Path == "/" || r.URL.Path == "/index.html"):
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-store")
		w.Write([]byte(strings.ReplaceAll(setupPage, "{{DIR}}", html.EscapeString(g.dir))))
	case r.Method == "POST" && r.URL.Path == "/setup/choose":
		g.mu.Lock()
		start := !g.busy && g.ctx != nil
		if start {
			g.busy = true
		}
		g.mu.Unlock()
		if start {
			go g.choose(r.Header.Get("X-UI-Lang"))
		}
		g.state(w)
	case r.Method == "GET" && r.URL.Path == "/setup/state":
		g.state(w)
	default:
		http.NotFound(w, r)
	}
}

func (g *gameSetup) state(w http.ResponseWriter) {
	g.mu.Lock()
	b, _ := json.Marshal(obj{"busy": g.busy, "error": g.errKey, "detail": g.detail, "chosen": g.chosen, "ready": g.s.ready.Load()})
	g.mu.Unlock()
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.Write(b)
}

// choose: the folder dialog, the directory checked (or the one above it: the
// player chose the decomp folder) and taken
func (g *gameSetup) choose(lang string) {
	title := "Where is Mad TV? The folder with SOURCE1.RSC and SOURCE2.RSC"
	if lang == "de" {
		title = "Wo liegt Mad TV? Der Ordner mit SOURCE1.RSC und SOURCE2.RSC"
	}
	start := filepath.Dir(root)
	if !isDir(start) {
		start = ""
	}
	dir, err := wr.OpenDirectoryDialog(g.ctx, wr.OpenDialogOptions{Title: title, DefaultDirectory: start})
	errKey, detail := "", ""
	switch {
	case err != nil:
		errKey, detail = "other", err.Error()
	case dir == "": // (cancelled)
	default:
		if !hasGameFiles(dir) && hasGameFiles(filepath.Dir(dir)) {
			dir = filepath.Dir(dir)
		}
		if !hasGameFiles(dir) {
			errKey, detail = "nofiles", dir
			break
		}
		gm, err := openGame(dir)
		if err == nil {
			err = g.s.useGame(gm)
		}
		if err != nil {
			errKey, detail = "other", err.Error()
			break
		}
		changeState(func(st *launcherState) { st.Game = gm.dir })
		say("OpenMTV: game files: %s (kept in %s)\n", gm.dir, statePath())
	}
	g.mu.Lock()
	g.busy, g.errKey, g.detail, g.chosen = false, errKey, detail, dir
	g.mu.Unlock()
}

const setupPage = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OpenMTV</title>
<style>
  :root { --bg: #111214; --panel: #191a1d; --line: #2b2e34; --text: #e8e6e3; --dim: #9a9ca3; --yellow: #f5d90a; --red: #d42a20; --redtext: #ff8a80; }
  * { box-sizing: border-box; }
  html, body { margin: 0; height: 100%; background: var(--bg); color: var(--text); font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
    -webkit-user-select: none; user-select: none; }
  main { max-width: 620px; margin: 0 auto; padding: 12vh 24px 24px; }
  h1 { font-size: 28px; margin: 0 0 6px; color: var(--yellow); letter-spacing: .5px; }
  p { margin: 10px 0; color: var(--dim); }
  p.lead { color: var(--text); }
  code { font-family: ui-monospace, Menlo, Consolas, monospace; font-size: 13px; color: var(--text); background: var(--panel);
    border: 1px solid var(--line); border-radius: 6px; padding: 1px 6px; -webkit-user-select: text; user-select: text; word-break: break-all; }
  .box { margin-top: 26px; padding: 20px; background: var(--panel); border: 1px solid var(--line); border-radius: 12px; }
  button { margin-top: 6px; font: inherit; font-weight: 600; color: #fff; background: var(--red); border: 0; border-radius: 10px; padding: 11px 20px; cursor: pointer; }
  button:disabled { opacity: .6; cursor: default; }
  #msg { min-height: 1.5em; margin-top: 14px; color: var(--redtext); -webkit-user-select: text; user-select: text; }
</style>
</head>
<body>
<main>
  <h1>OpenMTV</h1>
  <p class="lead" data-t="lead"></p>
  <p><span data-t="looked"></span> <code>{{DIR}}</code></p>
  <div class="box">
    <p data-t="ask"></p>
    <button id="choose" data-t="choose"></button>
    <div id="msg" role="status"></div>
  </div>
</main>
<script>
const DE = /^de\b/i.test(navigator.language || '');
const T = DE ? {
  lead: 'Der Launcher braucht die Dateien des Originalspiels von 1991: SOURCE1.RSC und SOURCE2.RSC.',
  looked: 'Gesucht in:', ask: 'Wähle den Ordner, in dem sie liegen. Er wird gemerkt; beim nächsten Start geht es gleich los.',
  choose: 'Ordner wählen …', wait: 'Der Ordner-Dialog ist offen …',
  nofiles: 'Dort liegen SOURCE1.RSC und SOURCE2.RSC nicht: ', other: 'Die Spieldateien lassen sich nicht lesen: ',
} : {
  lead: 'The launcher needs the files of the original game from 1991: SOURCE1.RSC and SOURCE2.RSC.',
  looked: 'Looked for in:', ask: 'Choose the folder they are in. It is kept; the next start goes straight on.',
  choose: 'Choose the folder …', wait: 'The folder dialog is open …',
  nofiles: 'SOURCE1.RSC and SOURCE2.RSC are not there: ', other: 'The game files cannot be read: ',
};
document.documentElement.lang = DE ? 'de' : 'en';
document.querySelectorAll('[data-t]').forEach((e) => { e.textContent = T[e.dataset.t]; });
const btn = document.getElementById('choose'), msg = document.getElementById('msg');
const call = async (method, url) => (await fetch(url, { method, headers: { 'X-UI-Lang': DE ? 'de' : 'en' } })).json();
function show(s) {
  if (s.ready) { location.replace('/'); return; }
  btn.disabled = s.busy;
  msg.textContent = s.busy ? T.wait : s.error ? (T[s.error] || '') + (s.detail || s.error) : '';
  msg.style.color = s.busy ? 'var(--dim)' : '';
  if (s.busy) setTimeout(async () => show(await call('GET', '/setup/state')), 300);
}
btn.addEventListener('click', async () => show(await call('POST', '/setup/choose')));
</script>
</body>
</html>
`
