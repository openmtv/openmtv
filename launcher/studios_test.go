package main

import (
	"bytes"
	"image/png"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"testing"
)

// the offsets of studios.go are the game's: the savegame's order of blocks
// (src/actions.c save_blocks_a/b, game_save_load) and the start tables' order
// (src/main_loop.c game_load_init_data, initdata_read)
func TestStudioOffsets(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "src", "actions.c"))
	if err != nil {
		t.Skip("no src/actions.c")
	}
	block := regexp.MustCompile(`\{0x([0-9A-F]+), 0x([0-9A-F]+)\}`)
	list := func(name string) [][2]int {
		i := bytes.Index(src, []byte(name+"[] = {"))
		j := bytes.Index(src[i:], []byte("};"))
		var out [][2]int
		for _, m := range block.FindAllSubmatch(src[i:i+j], -1) {
			a, _ := strconv.ParseInt(string(m[1]), 16, 32)
			b, _ := strconv.ParseInt(string(m[2]), 16, 32)
			out = append(out, [2]int{int(a), int(b)})
		}
		return out
	}
	at := map[int]int{}
	o := 200*0x12 + 50*0x29 + 50*0x161 + 50 // films, series, screenplays, ads
	for _, b := range list("save_blocks_a") {
		at[b[0]] = o
		o += b[1]
	}
	o += 3 * 0xF
	for _, b := range list("save_blocks_b") {
		at[b[0]] = o
		o += b[1]
	}
	if at[0x7C82] != saveFloors || at[0x6165] != saveStudios {
		t.Errorf("savegame: floors at %d, studios at %d", at[0x7C82], at[0x6165])
	}
	ml, err := os.ReadFile(filepath.Join("..", "src", "main_loop.c"))
	if err != nil {
		t.Skip("no src/main_loop.c")
	}
	o, init := 0, map[int]int{}
	for _, m := range regexp.MustCompile(`initdata_read\(0x([0-9A-F]+), DSEG, (0x[0-9A-F]+|[0-9]+)\)`).FindAllSubmatch(ml, -1) {
		a, _ := strconv.ParseInt(string(m[1]), 16, 32)
		b, _ := strconv.ParseInt(string(m[2]), 0, 32)
		init[int(a)] = o
		o += int(b)
	}
	if init[0x7C82] != initFloors || init[0x6165] != initStudios {
		t.Errorf("start tables: floors at %d, studios at %d", init[0x7C82], init[0x6165])
	}
}

// the original's studios: the stations' 6, 8, 10 (size 1, $2 000), nine public rooms
func TestStudioDoors(t *testing.T) {
	g, err := openGame(filepath.Dir(gamePaths(t)[0]))
	if err != nil {
		t.Fatal(err)
	}
	if len(g.doors) != 12 {
		t.Fatalf("%d studio doors", len(g.doors))
	}
	recs, _, _ := records(g.original["studios"])
	for _, d := range g.doors {
		r := recs[int64(d.Studio)]
		switch d.Studio {
		case 6, 8, 10:
			if d.Room != 5 || intOf(r, "renter") != int64(d.Studio/2-2) || intOf(r, "size") != 1 || intOf(r, "rent") != 2000 {
				t.Errorf("studio %d: %+v %v", d.Studio, d, r)
			}
		default:
			if !publicRoom(d.Room) || intOf(r, "renter") != 0 {
				t.Errorf("studio %d: %+v %v", d.Studio, d, r)
			}
		}
	}
	for _, d := range g.doors {
		if d.Studio == 11 && (d.Floor != 10 || d.PlanX != 0x81 || d.PlanY != 69) {
			t.Errorf("studio 11: %+v", d)
		}
	}
}

// a pack's studios: built into the start savegame and the start tables alike (a free
// public room's door as a bomb leaves it), back to the original's when the next build
// has them public again; what the game cannot take is refused
func TestStudiosBuild(t *testing.T) {
	paths := gamePaths(t)
	g, err := openGame(filepath.Dir(paths[0]))
	if err != nil {
		t.Fatal(err)
	}
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	write := func(recs string) {
		d := filepath.Join(root, "mods", "st")
		os.MkdirAll(d, 0o777)
		os.WriteFile(filepath.Join(d, "studios.json"), []byte(`{"table": "studios", "records": [`+recs+`]}`), 0o666)
	}
	free := `{"id": 5, "size": 3, "renter": 255, "rent": 8000}, {"id": 11, "size": 3, "renter": 255, "rent": 14000}`
	write(free + `, {"id": 12, "size": 3, "renter": 0, "rent": 10000}`)
	out, err := buildMod(g, "st")
	if err != nil {
		t.Fatal(err)
	}
	read := func() (save, init []byte) {
		res, err := readResources([]string{filepath.Join(out, "SOURCE1.RSC"), filepath.Join(out, "SOURCE2.RSC")})
		if err != nil {
			t.Fatal(err)
		}
		save, _ = res.get(startSave)
		init, _ = res.get(initRes)
		return
	}
	save, init := read()
	door := func(fl []byte, floor, d int) [4]byte {
		b := floor*floorSize + d
		return [4]byte{fl[b+0x1E], fl[b+0x28], fl[b+0x32], fl[b+0x46]}
	}
	for _, c := range []struct {
		data     []byte
		sAt, fAt int
		what     string
	}{{save, saveStudios, saveFloors, "savegame"}, {init, initStudios, initFloors, "start tables"}} {
		st, fl := c.data[c.sAt:c.sAt+studiosLen], c.data[c.fAt:c.fAt+floorsLen]
		if st[5*16+3] != 0xFF || st[11*16+3] != 0xFF || st[12*16+3] != 0 || st[12*16+2] != 3 || st[6*16+3] != 1 {
			t.Errorf("%s: studios %x", c.what, st)
		}
		if door(fl, 4, 1) != [4]byte{roomFree, signFree, signFreeOf, 0} || door(fl, 10, 2) != [4]byte{roomFree, signFree, signFreeOf, 0} {
			t.Errorf("%s: doors %v %v", c.what, door(fl, 4, 1), door(fl, 10, 2))
		}
		if door(fl, 12, 1)[0] != 0x10 || door(fl, 7, 1)[0] != consulateOne {
			t.Errorf("%s: public doors changed", c.what)
		}
	}
	// the next build (from that copy, which keeps the saved games) has studio 5 public again
	write(`{"id": 11, "size": 3, "renter": 255, "rent": 14000}`)
	if out, err = buildMod(g, "st"); err != nil {
		t.Fatal(err)
	}
	save, _ = read()
	if fl := save[saveFloors : saveFloors+floorsLen]; door(fl, 4, 1)[0] != 0x15 || door(fl, 10, 2)[0] != roomFree || save[saveStudios+5*16+3] != 0 {
		t.Errorf("rebuilt: door 4/1 %v, door 10/2 %v", door(fl, 4, 1), door(fl, 10, 2))
	}
	for recs, want := range map[string]string{
		`{"id": 7, "size": 3, "renter": 255, "rent": 8000}`:                            "consulate",
		`{"id": 8, "size": 1, "renter": 0, "rent": 2000}`:                              "start studio stays",
		`{"id": 8, "size": 2, "renter": 2, "rent": 2000}`:                              "three start studios differ",
		`{"id": 4, "size": 4, "renter": 255, "rent": 5000}`:                            "not between 1 and 3",
		`{"id": 4, "size": 2, "renter": 3, "rent": 5000}`:                              "public (0) or a free studio",
		`{"id": 4, "size": 2, "renter": 255, "rent": -1}`:                              "not between 0 and 1000000",
		`{"id": 4, "size": 2, "renter": 255, "rent": 1e300}`:                           "",
		`{"id": 0, "size": 3, "renter": 1, "rent": 5}`:                                 "no such studio",
		`{"id": 5, "size": 3, "renter": 255, "rent": 8000, "unknown": {"0x0F": "02"}}`: "only size, renter and rent",
		`{"id": 5, "size": 3, "renter": 255}`:                                          "rent: missing",
	} {
		write(recs)
		_, err := buildMod(g, "st")
		if err == nil || (want != "" && !strings.Contains(err.Error(), want)) {
			t.Errorf("%s: %v", recs, err)
		}
	}
	write(`{"id": 6, "size": 2, "renter": 1, "rent": 3000}, {"id": 8, "size": 2, "renter": 2, "rent": 3000}, {"id": 10, "size": 2, "renter": 3, "rent": 3000}`)
	if _, err := buildMod(g, "st"); err != nil {
		t.Errorf("three start studios alike: %v", err)
	}
	// the network game's tables of a host from before the studios: the original's
	docs, _, err := packsDocs(g, "st")
	if err != nil {
		t.Fatal(err)
	}
	delete(docs, "studios")
	if err := buildDocs(g, docs, g.paths, t.TempDir()); err != nil {
		t.Errorf("without studios: %v", err)
	}
}

// the plan is the game's: the estate agent's plan (room 62) as build/mtv-dec draws it
// at the start of a new game (the scenario room_estate_rent's way there), pixel for
// pixel except under the mouse pointer
func TestPlanLikeGame(t *testing.T) {
	paths := gamePaths(t)
	bin := filepath.Join("..", "build", "mtv-dec")
	if !exists(bin) {
		t.Skip("no build/mtv-dec")
	}
	g, err := openGame(filepath.Dir(paths[0]))
	if err != nil {
		t.Fatal(err)
	}
	keptRoot := root
	defer func() { root = keptRoot }()
	root = ".." // (the font: build/MTV_U.EXE or the game's data)
	tmp := t.TempDir()
	for _, p := range paths {
		data, _ := os.ReadFile(p)
		os.WriteFile(filepath.Join(tmp, filepath.Base(p)), data, 0o666)
	}
	scen, err := os.ReadFile(filepath.Join("..", "tests", "scenarios", "room_estate_rent.txt"))
	if err != nil {
		t.Skip("no tests/scenarios/room_estate_rent.txt")
	}
	var script []string
	for _, l := range strings.Split(string(scen), "\n") {
		f := strings.Fields(l)
		if len(f) > 1 && !strings.HasPrefix(l, "#") {
			if n, _ := strconv.Atoi(f[0]); n < 5300 && f[1] != "hash" && f[1] != "quit" {
				script = append(script, l)
			}
		}
	}
	shot := filepath.Join(tmp, "plan.png")
	script = append(script, "5300 shot "+shot, "5301 quit")
	os.WriteFile(filepath.Join(tmp, "s.txt"), []byte(strings.Join(script, "\n")+"\n"), 0o666)
	if b, err := exec.Command(bin, "--headless", "--nosound", "--logical-time", "--game", tmp, "--args", "e", "--script", filepath.Join(tmp, "s.txt")).CombinedOutput(); err != nil {
		t.Fatalf("%v %s", err, b)
	}
	res, _ := readResources(paths)
	pics := &pictures{res: res, banks: map[int][]sprite{}, pngs: map[string][]byte{}}
	save, _ := res.get(startSave)
	data, err := pics.planPNG(g.floors, save[saveStudios:saveStudios+studiosLen], func() (*gameFont, error) { return readGameFont(res) })
	if err != nil {
		t.Fatal(err)
	}
	ours, _ := png.Decode(bytes.NewReader(data))
	f, err := os.Open(shot)
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	game, err := png.Decode(f)
	if err != nil {
		t.Fatal(err)
	}
	px, py := 50, 60 // the mouse pointer (room_estate_rent's last click is beside the plan)
	diff := 0
	for y := 0; y < 120; y++ {
		for x := 0; x < 320; x++ {
			if x >= px-2 && x < px+12 && y >= py-2 && y < py+12 {
				continue
			}
			r1, g1, b1, _ := ours.At(x, y).RGBA()
			r2, g2, b2, _ := game.At(x, y).RGBA()
			if r1>>8 != r2>>8 || g1>>8 != g2>>8 || b1>>8 != b2>>8 {
				diff++
			}
		}
	}
	if diff > 0 {
		t.Errorf("%d pixels differ from the game's plan", diff)
	}
}

// a pack's free studio in the game: free when a new game starts, its door a free
// studio's, offered by the estate agent at 1.5 x its rent, rented: the station's, the
// price paid (the scenario room_estate_rent's way to the plan; German)
func TestStudiosInGame(t *testing.T) {
	paths := gamePaths(t)
	bin := filepath.Join("..", "build", "mtv-dec")
	if !exists(bin) {
		t.Skip("no build/mtv-dec")
	}
	scen, err := os.ReadFile(filepath.Join("..", "tests", "scenarios", "room_estate_rent.txt"))
	if err != nil {
		t.Skip("no tests/scenarios/room_estate_rent.txt")
	}
	g, err := openGame(filepath.Dir(paths[0]))
	if err != nil {
		t.Fatal(err)
	}
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	d := filepath.Join(root, "mods", "st")
	os.MkdirAll(d, 0o777)
	os.WriteFile(filepath.Join(d, "studios.json"), []byte(`{"table": "studios", "records": [{"id": 11, "size": 3, "renter": 255, "rent": 14000}]}`), 0o666)
	out, err := buildMod(g, "st")
	if err != nil {
		t.Fatal(err)
	}
	var script []string
	for _, l := range strings.Split(string(scen), "\n") {
		f := strings.Fields(l)
		if len(f) > 1 && !strings.HasPrefix(l, "#") {
			if n, _ := strconv.Atoi(f[0]); n < 5300 && f[1] != "hash" && f[1] != "quit" {
				script = append(script, l)
			}
		}
	}
	door := 0x7C82 + 10*floorSize + 2 // floor 10, door 2: the gun agency's
	script = append(script, "2000 peek8 39D4:6218", "2000 peek8 39D4:"+strconv.FormatInt(int64(door+0x1E), 16),
		"5600 click 134 73", // studio 11 on the plan: "... for $ 21000 a day (size 3)"
		"6200 click 124 87", // thumbs up
		"6600 peek8 39D4:6218", "6600 peek32 39D4:8BFB", "6600 peek32 39D4:621B", "6601 quit")
	frame := func(l string) int { n, _ := strconv.Atoi(strings.Fields(l)[0]); return n }
	sort.SliceStable(script, func(i, j int) bool { return frame(script[i]) < frame(script[j]) })
	os.WriteFile(filepath.Join(out, "s.txt"), []byte(strings.Join(script, "\n")+"\n"), 0o666)
	b, err := exec.Command(bin, "--headless", "--nosound", "--logical-time", "--game", out, "--args", "d", "--script", filepath.Join(out, "s.txt")).CombinedOutput()
	if err != nil {
		t.Fatalf("%v %s", err, b)
	}
	got := regexp.MustCompile(`PEEK frame \d+ \S+ (\d+)`).FindAllStringSubmatch(string(b), -1)
	var vals []string
	for _, m := range got {
		vals = append(vals, m[1])
	}
	// free, a free studio's door; then Mad-TV's, $200 000 - 3 x $21 000, the rent raised by half
	if want := "255 54 1 137000 21000"; strings.Join(vals, " ") != want {
		t.Errorf("got %v, want %s", vals, want)
	}
}

// the computer players' sabotage with every public room but the consulates a free studio
// from the start: its victim among the public rooms is mostly gone (32BA:1800 tries 20
// times, else swaps nothing); the scenario event_ai_sabotage's terrorist with ten sabotages
// before he sets off -- he bombs, the event ends, the game goes on (no hang, no halt)
func TestStudiosSabotage(t *testing.T) {
	paths := gamePaths(t)
	bin := filepath.Join("..", "build", "mtv-dec")
	if !exists(bin) {
		t.Skip("no build/mtv-dec")
	}
	scen, err := os.ReadFile(filepath.Join("..", "tests", "scenarios", "event_ai_sabotage.txt"))
	if err != nil {
		t.Skip("no tests/scenarios/event_ai_sabotage.txt")
	}
	g, err := openGame(filepath.Dir(paths[0]))
	if err != nil {
		t.Fatal(err)
	}
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	d := filepath.Join(root, "mods", "st")
	os.MkdirAll(d, 0o777)
	var recs []string
	for _, r := range [][3]int{{1, 1, 2000}, {2, 1, 2000}, {3, 2, 8000}, {4, 2, 5000}, {5, 3, 8000}, {11, 3, 14000}, {12, 2, 9000}} {
		recs = append(recs, `{"id": `+strconv.Itoa(r[0])+`, "size": `+strconv.Itoa(r[1])+`, "renter": 255, "rent": `+strconv.Itoa(r[2])+`}`)
	}
	os.WriteFile(filepath.Join(d, "studios.json"), []byte(`{"table": "studios", "records": [`+strings.Join(recs, ", ")+`]}`), 0o666)
	out, err := buildMod(g, "st")
	if err != nil {
		t.Fatal(err)
	}
	var script []string
	for _, l := range strings.Split(string(scen), "\n") {
		f := strings.Fields(l)
		if len(f) > 1 && !strings.HasPrefix(l, "#") && f[1] != "hash" && f[1] != "quit" && !strings.HasPrefix(f[1], "peek") && l != "4800 call 32BA:1800" {
			script = append(script, l)
		}
	}
	for i := 0; i < 10; i++ {
		script = append(script, strconv.Itoa(4800+11*i)+" call 32BA:1800")
	}
	script = append(script, "12000 peek16 39D4:7A56", "12000 peek16 39D4:71E4", "12001 quit")
	frame := func(l string) int { n, _ := strconv.Atoi(strings.Fields(l)[0]); return n }
	sort.SliceStable(script, func(i, j int) bool { return frame(script[i]) < frame(script[j]) })
	os.WriteFile(filepath.Join(out, "s.txt"), []byte(strings.Join(script, "\n")+"\n"), 0o666)
	b, err := exec.Command(bin, "--headless", "--nosound", "--logical-time", "--fixes", "--game", out, "--args", "e", "--script", filepath.Join(out, "s.txt")).CombinedOutput()
	if err != nil {
		t.Fatalf("%v %s", err, b)
	}
	var vals []string
	for _, m := range regexp.MustCompile(`PEEK frame \d+ \S+ (\d+)`).FindAllStringSubmatch(string(b), -1) {
		vals = append(vals, m[1])
	}
	if want := "1 2"; strings.Join(vals, " ") != want { // the terrorist's event over, day 2 still running
		t.Errorf("got %v, want %s\n%s", vals, want, b)
	}
}
