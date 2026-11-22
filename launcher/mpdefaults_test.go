package main

import "testing"

// mpDefaults: the host's game from the network games' file, as the web page's mpDefaults
// (multiplayer.js) makes it -- the iPhone's lobby takes it from /api/mp/defaults
func TestMPDefaults(t *testing.T) {
	d := mpDefaults(map[string]map[string]string{})
	if d["lang"] != "e" || d["fixes"] != "all" || d["money"] != 200000 || d["credit"] != 700000 || d["terror"] != 50000 ||
		d["offers"] != 10 || d["filmoffers"] != 10 || d["lots"] != 10 || d["sertop"] != true || d["knock"] != 15 || d["difficulty"] != 2 || d["castfit"] != false {
		t.Fatalf("an empty file: %v", d)
	}
	d = mpDefaults(map[string]map[string]string{
		"game":    {"language": "german", "classic": "yes", "fixes": "intruder", "credit_line": "original", "terror_damage": "0", "sammy_shows": "250000", "series_topicality": "no", "fourth_speed": "yes", "ai_improve": "strong"},
		"display": {"cast_fit": "yes"},
	})
	sammy, _ := d["sammy"].([]int)
	if d["lang"] != "d" || d["fixes"] != "none" || d["credit"] != 0 || d["terror"] != 0 || len(sammy) != 4 || sammy[2] != 250000 ||
		d["sertop"] != false || d["speed4"] != true || d["aiimp"] != "strong" || d["castfit"] != true {
		t.Fatalf("a file with changes: %v", d)
	}
}
