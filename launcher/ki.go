package main

import (
	"bufio"
	"compress/gzip"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

// AI pictures (docs/ki-bilder.md, stage 5): the launcher makes a picture set on the
// player's machine from their own game files -- the pictures derive from the
// original's art and are never shipped.  It loads the model, its extra colours and
// ONNX Runtime from openmtv.net (each checked against the checksum here), runs
// build/mtv-dec --make-ki (src/ki_make.c) and keeps the set beside the settings
// file (ki/MODEL.ki, with ki/MODEL.json: what it was made from) -- the files it was
// made with go again when the making ends (kiTidy): most players need only the set,
// and making one again loads them again; "use" writes
// [display] ai_pictures into the settings (single player and network games) and
// turns the HR display on, where the pictures show (Ctrl+K switches them).

const (
	kiBase = "https://openmtv.net/ki/1/" // KI_BASE: another place (tests)
	kiTool = 3                           // the set's making (2: its format with the pictures' sums, 3: groups and rings); a set made otherwise is old
)

type kiFile struct {
	path string // under kiBase
	size int64
	sha  string // of the file as served
}

type kiModel struct {
	ID, Name, Author, Licence, Link string
	model, pal                      kiFile
}

var kiModels = []kiModel{
	{"rebout", "Rebout-Blend", "LyonHrt", "CC BY-NC-SA 4.0", "https://openmodeldb.info/models/4x-Rebout-Blend",
		kiFile{"models/4x-Rebout-Blend.onnx", 67132607, "9e3eb7c559b9e0ff2a757ab8c3190b95a4d200338c4a7e997125d8ce41920317"},
		kiFile{"models/4x-Rebout-Blend.pal", 768, "65d30c13bee224a56fd8ef24b4a8fb974b549fb0609523d985ffc3e670bbd08c"}},
	{"msu", "MS-Unpainter", "foolhardy", "CC BY-NC-SA 4.0", "https://openmodeldb.info/models/8x-MS-Unpainter-De-Dither",
		kiFile{"models/8x-MS-Unpainter-De-Dither.onnx", 67281091, "8a871cbb8f20d1b2a0d3fe836134952dec670b788e69569cd55586a12823199a"},
		kiFile{"models/8x-MS-Unpainter-De-Dither.pal", 768, "be0d0e12351edf64bcf40313beeb2c04dd298121212f29b63503964051665df3"}},
}

// ONNX Runtime 1.31.0 (Microsoft, MIT), the library alone, gzipped; unpacked into a directory
// named as the one it comes from (ort-1.31.0/: Windows' onnxruntime.dll carries no version)
// and checked there too, each time
var kiRuntimes = map[string]struct {
	file kiFile
	name string // its name unpacked
	lib  kiFile // unpacked (no path)
}{
	"windows/amd64": {kiFile{"ort-1.31.0/windows-amd64.gz", 6310381, "fd14a2576641c198a259ebefd53d054bd0829fb432b5941c98296e64fe5aaee8"}, "onnxruntime.dll",
		kiFile{"", 17253176, "0c42dc4e278f0968fdf9ebf1b1e54a74c657a9942e70803e453c91993451cf4c"}},
	"darwin/arm64": {kiFile{"ort-1.31.0/darwin-arm64.gz", 12536414, "8c4ede20fd84add9de919bfe2633feabb4f05216d6b9682ab1fbeeaac57c7f2d"}, "libonnxruntime.1.31.0.dylib",
		kiFile{"", 44830288, "2a50a63e7f41f8d4d551d3ee47248df3864f6540109fce8ad520ca3b51af4b70"}},
	"linux/amd64": {kiFile{"ort-1.31.0/linux-amd64.gz", 11115163, "29bc6cdfd8692273db7b2703fc823e9653606e23dac13becb1abf2b2790a2f18"}, "libonnxruntime.so.1.31.0",
		kiFile{"", 29345384, "469849977cb60744ea160927e1a3a0643fd4c55c0aff743251c6d293402e334e"}},
}

// kiIdle: a download that sends nothing for so long has stopped; kiMakeIdle: a making
// that tells no progress for so long hangs (a picture takes seconds, CoreML compiling
// the model a minute or two) -- vars for the tests
var kiIdle, kiMakeIdle = time.Minute, 10 * time.Minute

// kiDir: beside the settings file (ki/; ki/download/ the model files and the runtime, while
// a set is made)
func kiDir() string { return filepath.Join(filepath.Dir(settingsPath()), "ki") }

// kiLib: where the runtime of RT is unpacked
func kiLib(file kiFile, name string) string {
	return filepath.Join(kiDir(), "download", filepath.Dir(filepath.FromSlash(file.path)), name)
}

// kiChecked: the file at P is F (its size and checksum)
func kiChecked(f kiFile, p string) bool {
	b, err := os.ReadFile(p)
	if err != nil || int64(len(b)) != f.size {
		return false
	}
	h := sha256.Sum256(b)
	return hex.EncodeToString(h[:]) == f.sha
}

// kiErrText: an error of the making for the page -- one of the system's marked as one
func kiErrText(err error, lang string) string {
	switch err.(type) {
	case msg, joinedError:
		return render(err, lang)
	}
	if lang == "de" {
		return "Systemfehler: " + err.Error()
	}
	return err.Error()
}

func kiModelByID(id string) *kiModel {
	for i := range kiModels {
		if kiModels[i].ID == id {
			return &kiModels[i]
		}
	}
	return nil
}

// what a set was made from (ki/MODEL.json)
type kiMade struct {
	Model string `json:"model"`
	SHA   string `json:"sha256"`
	Game  string `json:"game"` // contentHash of the game files
	Tool  int    `json:"tool"`
	When  string `json:"made"`
	Size  int64  `json:"size"`
}

// the one making there is at a time
type kiJob struct {
	mu     sync.Mutex
	model  string
	phase  string // "download", "make", "done", "failed", "cancelled"
	done   int64  // bytes loaded, or pictures made
	total  int64
	work   [2]int64  // making: the model's pixels done, of all (the time left)
	since  time.Time // making: since when
	err    string
	cancel context.CancelFunc
}

var kiNow kiJob

func (j *kiJob) set(f func(*kiJob)) {
	j.mu.Lock()
	defer j.mu.Unlock()
	f(j)
}

// running: whether model ID is being made ("": any)
func (j *kiJob) running(id string) bool {
	j.mu.Lock()
	defer j.mu.Unlock()
	return (j.phase == "download" || j.phase == "make") && (id == "" || j.model == id)
}

// kiStatus: the models, each with its set (none, ready, stale or old -- an earlier --make-ki; in use), and the making
func kiStatus(lang string) obj {
	active := settingsValue(settingsPath(), "display", "ai_pictures")
	gh, _ := contentHash(gameDir())
	var models []obj
	for _, m := range kiModels {
		set := filepath.Join(kiDir(), m.ID+".ki")
		o := obj{"id": m.ID, "name": m.Name, "author": m.Author, "licence": m.Licence, "link": m.Link,
			"download": m.model.size + m.pal.size, "state": "none"}
		if st, err := os.Stat(set); err == nil {
			o["state"], o["size"] = "ready", st.Size()
			var made kiMade
			if b, err := os.ReadFile(filepath.Join(kiDir(), m.ID+".json")); err == nil && json.Unmarshal(b, &made) == nil {
				o["made"] = made.When
				if made.SHA != m.model.sha || made.Game != fmt.Sprintf("%016x", gh) {
					o["state"] = "stale"
				} else if made.Tool != kiTool { // (it still shows: made again it is better)
					o["state"] = "old"
				}
			}
			o["used"] = active != "" && sameFile(active, set)
		}
		models = append(models, o)
	}
	rt, ok := kiRuntimes[runtime.GOOS+"/"+runtime.GOARCH]
	kiNow.mu.Lock()
	job := obj{"model": kiNow.model, "phase": kiNow.phase, "done": kiNow.done, "total": kiNow.total, "error": kiNow.err}
	if t := time.Since(kiNow.since); kiNow.phase == "make" && kiNow.work[0] > 0 && t > 15*time.Second {
		job["left"] = int64(t.Seconds() * float64(kiNow.work[1]-kiNow.work[0]) / float64(kiNow.work[0])) // seconds
	}
	kiNow.mu.Unlock()
	out := obj{"models": models, "job": job, "supported": ok, "made_here": exists(program("mtv-dec"))}
	if ok {
		out["runtime"] = rt.file.size
		out["runtime_here"] = exists(kiLib(rt.file, rt.name))
	}
	return out
}

func sameFile(a, b string) bool {
	x, err1 := os.Stat(a)
	y, err2 := os.Stat(b)
	return err1 == nil && err2 == nil && os.SameFile(x, y)
}

// kiFetch: FILE from kiBase into DST, checked against its checksum (a file there
// already that checks is kept); progress adds to the job's bytes
func kiFetch(ctx context.Context, f kiFile, dst string) error {
	if kiChecked(f, dst) {
		kiNow.set(func(j *kiJob) { j.done += f.size })
		return nil
	}
	base := kiBase
	if b := os.Getenv("KI_BASE"); b != "" {
		base = b
	}
	// (no data for kiIdle: the download ends -- else a connection that stays up and sends
	// nothing would keep it going for good)
	ctx, stop := context.WithCancel(ctx)
	defer stop()
	var stalled atomic.Bool
	idle := time.AfterFunc(kiIdle, func() { stalled.Store(true); stop() })
	defer idle.Stop()
	req, err := http.NewRequestWithContext(ctx, "GET", base+f.path, nil)
	if err != nil {
		return err
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		if stalled.Load() {
			return newMsg("{file}: not loaded (no data for a minute)", "file", f.path)
		}
		return newMsg("{file}: not loaded ({why})", "file", f.path, "why", err.Error())
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return newMsg("{file}: not loaded ({why})", "file", f.path, "why", resp.Status)
	}
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	tmp := dst + ".part"
	out, err := os.Create(tmp)
	if err != nil {
		return err
	}
	h := sha256.New()
	buf := make([]byte, 1<<16)
	var n int64
	for {
		k, rerr := resp.Body.Read(buf)
		idle.Reset(kiIdle)
		if k > 0 {
			n += int64(k)
			if n > f.size {
				break
			}
			h.Write(buf[:k])
			if _, err := out.Write(buf[:k]); err != nil {
				out.Close()
				os.Remove(tmp)
				return err
			}
			kiNow.set(func(j *kiJob) { j.done += int64(k) })
		}
		if rerr == io.EOF {
			break
		}
		if rerr != nil {
			out.Close()
			os.Remove(tmp)
			if stalled.Load() {
				return newMsg("{file}: not loaded (no data for a minute)", "file", f.path)
			}
			return newMsg("{file}: not loaded ({why})", "file", f.path, "why", rerr.Error())
		}
	}
	if err := out.Close(); err != nil {
		os.Remove(tmp)
		return err
	}
	if n != f.size || hex.EncodeToString(h.Sum(nil)) != f.sha {
		os.Remove(tmp)
		return newMsg("{file}: not the file it should be (its checksum)", "file", f.path)
	}
	return os.Rename(tmp, dst)
}

// kiUnpack: the gzipped runtime SRC as DST (MAX bytes at most: more is not the file)
func kiUnpack(src, dst string, max int64) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	z, err := gzip.NewReader(in)
	if err != nil {
		return err
	}
	tmp := dst + ".part"
	out, err := os.Create(tmp)
	if err != nil {
		return err
	}
	n, err := io.Copy(out, io.LimitReader(z, max+1))
	if err == nil && n > max {
		err = newMsg("{file}: not the file it should be (its checksum)", "file", filepath.Base(dst))
	}
	if err == nil {
		err = out.Sync()
	}
	if cerr := out.Close(); err == nil {
		err = cerr
	}
	if err == nil {
		err = os.Rename(tmp, dst)
	}
	if err != nil {
		os.Remove(tmp)
	}
	return err
}

// kiMake: the set of model ID, in the background (one at a time)
func kiMake(id, lang string) error {
	m := kiModelByID(id)
	if m == nil {
		return newMsg("no such thing")
	}
	rt, ok := kiRuntimes[runtime.GOOS+"/"+runtime.GOARCH]
	if !ok {
		return newMsg("AI upscaling does not run on this system ({os})", "os", runtime.GOOS+"/"+runtime.GOARCH)
	}
	if !exists(program("mtv-dec")) {
		return newMsg("the game is not built yet (make build/mtv-dec)")
	}
	if _, ok := contentHash(gameDir()); !ok {
		return newMsg("no game files")
	}
	kiNow.mu.Lock()
	if kiNow.phase == "download" || kiNow.phase == "make" {
		kiNow.mu.Unlock()
		return newMsg("a picture set is being made already")
	}
	ctx, cancel := context.WithCancel(context.Background())
	kiNow.model, kiNow.phase, kiNow.err, kiNow.cancel = id, "download", "", cancel
	kiNow.done, kiNow.total = 0, rt.file.size+m.model.size+m.pal.size
	kiNow.mu.Unlock()
	go func() {
		err := kiRun(ctx, m, rt.file, rt.name, rt.lib)
		kiTidy() // (before the phase: kiStop waits for it)
		kiNow.set(func(j *kiJob) {
			switch {
			case ctx.Err() != nil:
				j.phase = "cancelled"
			case err != nil:
				j.phase, j.err = "failed", kiErrText(err, lang)
			default:
				j.phase = "done"
			}
			j.cancel = nil
		})
		cancel()
	}()
	return nil
}

func kiRun(ctx context.Context, m *kiModel, rtFile kiFile, rtName string, rtLib kiFile) error {
	dl := filepath.Join(kiDir(), "download")
	lib := kiLib(rtFile, rtName)
	model := filepath.Join(dl, filepath.Base(m.model.path))
	pal := filepath.Join(dl, filepath.Base(m.pal.path))
	if !kiChecked(rtLib, lib) { // (none yet, or one cut short or changed: loaded anew)
		gz := filepath.Join(dl, filepath.Base(rtFile.path))
		if err := kiFetch(ctx, rtFile, gz); err != nil {
			return err
		}
		if err := os.MkdirAll(filepath.Dir(lib), 0o755); err != nil {
			return err
		}
		if err := kiUnpack(gz, lib, rtLib.size); err != nil {
			return err
		}
		os.Remove(gz)
		if !kiChecked(rtLib, lib) {
			os.Remove(lib)
			return newMsg("{file}: not the file it should be (its checksum)", "file", filepath.Base(lib))
		}
	} else {
		kiNow.set(func(j *kiJob) { j.done += rtFile.size })
	}
	if err := kiFetch(ctx, m.model, model); err != nil {
		return err
	}
	if err := kiFetch(ctx, m.pal, pal); err != nil {
		return err
	}
	kiNow.set(func(j *kiJob) { j.phase, j.done, j.total, j.work, j.since = "make", 0, 1, [2]int64{}, time.Now() })
	set := filepath.Join(kiDir(), m.ID+".ki")
	tmp := set + ".part"
	// (--ki-parent: a launcher killed outright cannot stop it -- it ends then by itself)
	args := []string{"--make-ki", model, "--game", gameDir(), "--ki-out", tmp, "--ki-palette", pal,
		"--ki-model", strings.TrimSuffix(filepath.Base(m.model.path), ".onnx"), "--ki-ort", lib, "--ki-parent", strconv.Itoa(os.Getpid())}
	if runtime.GOOS == "darwin" { // (CoreML: seven times the CPU's speed on an M3; the compiled model kept)
		cache := filepath.Join(dl, "coreml")
		if err := os.MkdirAll(cache, 0o755); err != nil {
			return err
		}
		args = append(args, "--ki-gpu", "--ki-cache", cache)
	}
	mctx, stop := context.WithCancel(ctx)
	defer stop()
	var stalled atomic.Bool
	idle := time.AfterFunc(kiMakeIdle, func() { stalled.Store(true); stop() })
	defer idle.Stop()
	c := exec.CommandContext(mctx, program("mtv-dec"), args...)
	hideWindow(c)
	out, err := c.StdoutPipe()
	if err != nil {
		return err
	}
	errTail := &tailWriter{n: 8000}
	c.Stderr = errTail
	if err := c.Start(); err != nil {
		return err
	}
	sc := bufio.NewScanner(out)
	for sc.Scan() { // "ki N of M (P of Q)"
		idle.Reset(kiMakeIdle)
		var n, t, p, q int64
		if k, _ := fmt.Sscanf(sc.Text(), "ki %d of %d (%d of %d)", &n, &t, &p, &q); k >= 2 {
			kiNow.set(func(j *kiJob) { j.done, j.total, j.work = n, t, [2]int64{p, q} })
		}
	}
	io.Copy(io.Discard, out) // (a line too long for the scanner: the rest read all the same -- else mtv-dec waits on a full pipe)
	if err := c.Wait(); err != nil {
		os.Remove(tmp)
		if ctx.Err() != nil {
			return ctx.Err()
		}
		if stalled.Load() {
			return newMsg("AI upscaling stopped: no progress in {min} min", "min", strconv.Itoa(int(kiMakeIdle.Minutes())))
		}
		// the reason: its last "--make-ki: " line (src/ki_make.c die, ort_check); none -- it
		// crashed or was killed -- the way it ended
		why := ""
		for _, l := range strings.Split(string(errTail.b), "\n") {
			if strings.HasPrefix(l, "--make-ki: ") {
				why = strings.TrimSpace(l[len("--make-ki: "):])
			}
		}
		if why == "" {
			why = err.Error()
		}
		return newMsg("AI upscaling failed: {why}", "why", why)
	}
	if err := os.Rename(tmp, set); err != nil {
		os.Remove(tmp)
		return err
	}
	gh, _ := contentHash(gameDir())
	st, _ := os.Stat(set)
	made := kiMade{Model: m.Name, SHA: m.model.sha, Game: fmt.Sprintf("%016x", gh), Tool: kiTool,
		When: time.Now().Format("2006-01-02 15:04"), Size: st.Size()}
	b, _ := json.MarshalIndent(made, "", " ")
	return os.WriteFile(filepath.Join(kiDir(), m.ID+".json"), b, 0o644)
}

// tailWriter keeps the last N bytes written (the reason comes last)
type tailWriter struct {
	b []byte
	n int
}

func (t *tailWriter) Write(p []byte) (int, error) {
	t.b = append(t.b, p...)
	if len(t.b) > t.n {
		t.b = append([]byte(nil), t.b[len(t.b)-t.n:]...)
	}
	return len(p), nil
}

// kiLeftovers: at the launcher's start, what a launcher ended outright while making left
// in ki/download/ (its mtv-dec has ended by itself: --ki-parent) -- there is one launcher
// at a time (its port), so no making of another one is under way
func kiLeftovers() {
	if d := filepath.Join(kiDir(), "download"); !kiNow.running("") && exists(d) {
		kiTidy()
	}
}

// kiTidy: the files a set is made with -- the model, its colours, ONNX Runtime, CoreML's
// compiled models: all of ki/download/ -- removed when a making ends, however it ended
// (the user: in the end the players need only the set); a making loads them again
func kiTidy() {
	if err := os.RemoveAll(filepath.Join(kiDir(), "download")); err != nil {
		warn("AI pictures: %v\n", err) // (left for the next making's end or a Delete)
	}
}

// kiUnuse: SET no more in the settings files that have it (each looked at alone: the
// network games' may hold another set than the single player's)
func kiUnuse(set string) error {
	for _, p := range []string{settingsPath(), filepath.Join(filepath.Dir(settingsPath()), "multiplayer.ini")} {
		v := settingsValue(p, "display", "ai_pictures")
		if v == "" || !(sameFile(v, set) || filepath.Clean(v) == filepath.Clean(set)) {
			continue
		}
		if err := writeSettings([]settingChange{{"display", "ai_pictures", ""}}, p); err != nil {
			return err
		}
	}
	return nil
}

// kiUse: the set of model ID shown in the game (single player and network games),
// with the HR display where it is not on yet; "" : none
func kiUse(id string) error {
	val := ""
	if id != "" {
		if kiModelByID(id) == nil {
			return newMsg("no such thing")
		}
		val = filepath.Join(kiDir(), id+".ki")
		if !exists(val) {
			return newMsg("make the picture set first")
		}
	}
	for _, mp := range []bool{false, true} {
		p, err := settingsFile(false, mp)
		if err != nil {
			return err
		}
		ch := []settingChange{{"display", "ai_pictures", val}}
		if f := settingsValue(p, "display", "filter"); val != "" && f != "hr-pixel" {
			ch = append(ch, settingChange{"display", "filter", "hr-pixel"})
		}
		if err := writeSettings(ch, p); err != nil {
			return err
		}
	}
	return nil
}

// kiDelete: the set of model ID; one in use is used no more.  What a making left behind
// (ki/download/: a launcher ended hard while making) goes too -- not while the other
// model is made, it is its; and ki/ itself once nothing is left in it
func kiDelete(id string) error {
	if kiModelByID(id) == nil {
		return newMsg("no such thing")
	}
	kiNow.mu.Lock() // (held: no making starts meanwhile)
	defer kiNow.mu.Unlock()
	making := kiNow.phase == "download" || kiNow.phase == "make"
	if making && kiNow.model == id {
		return newMsg("a picture set is being made already")
	}
	set := filepath.Join(kiDir(), id+".ki")
	if err := kiUnuse(set); err != nil {
		return err
	}
	for _, p := range []string{set, filepath.Join(kiDir(), id+".json"), set + ".part"} {
		if err := os.Remove(p); err != nil && !os.IsNotExist(err) {
			return err
		}
	}
	if !making {
		if err := os.RemoveAll(filepath.Join(kiDir(), "download")); err != nil {
			return err
		}
		os.Remove(kiDir()) // (not empty: the other set there)
	}
	return nil
}

func kiCancel() {
	kiNow.mu.Lock()
	defer kiNow.mu.Unlock()
	if kiNow.cancel != nil {
		kiNow.cancel()
	}
}

// kiStop: a making ends with the launcher (mtv-dec would go on alone)
func kiStop() {
	kiCancel()
	for i := 0; i < 50 && kiNow.running(""); i++ {
		time.Sleep(100 * time.Millisecond)
	}
}

// the API: GET /api/ki, POST /api/ki/make|use|delete|cancel|link {model}
func (s *server) kiGet(parts []string, lang string) (reply, bool) {
	if len(parts) != 1 || parts[0] != "ki" {
		return reply{}, false
	}
	return reply{200, kiStatus(lang)}, true
}

func (s *server) kiPost(parts []string, b obj, lang string) (reply, bool) {
	if len(parts) != 2 || parts[0] != "ki" {
		return reply{}, false
	}
	var err error
	switch parts[1] {
	case "make":
		err = kiMake(str(b["model"]), lang)
	case "use":
		err = kiUse(str(b["model"]))
	case "delete":
		err = kiDelete(str(b["model"]))
	case "cancel":
		kiCancel()
	case "link": // the model's page, in the system's browser (the window opens no new windows)
		if m := kiModelByID(str(b["model"])); m != nil {
			openBrowser(m.Link)
		}
	default:
		return fail(404, newMsg("no such thing").in(lang)), true
	}
	if err != nil {
		return failure(err, lang), true
	}
	return reply{200, kiStatus(lang)}, true
}
