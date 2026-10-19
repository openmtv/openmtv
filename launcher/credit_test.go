package main

import "testing"

// the original's credit system: credit_line = original in the settings, credit 0 in a lobby,
// --credit-line=original for the game
func TestCreditOriginal(t *testing.T) {
	if v, err := checkSetting("game", "credit_line", "original"); err != nil || v != "original" {
		t.Errorf("credit_line = original: %q %v", v, err)
	}
	if _, err := checkSetting("game", "credit_line", "0"); err == nil {
		t.Errorf("credit_line = 0 taken")
	}
	if _, err := checkSetting("game", "terror_damage", "original"); err == nil {
		t.Errorf("terror_damage = original taken")
	}
	if x := creditLine(int64(0), creditDef); x != creditOrig {
		t.Errorf("lobby credit 0: %d", x)
	}
	if x := creditLine(int64(200000), creditDef); x != creditDef {
		t.Errorf("lobby credit 200000: %d", x)
	}
	if a := creditArg(creditOrig); a != "--credit-line=original" {
		t.Errorf("creditArg(0): %s", a)
	}
	if a := creditArg(900000); a != "--credit-line=900000" {
		t.Errorf("creditArg(900000): %s", a)
	}
}
