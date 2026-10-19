package main

import (
	"os"
	"os/exec"
	"strings"
)

// openBrowser: URL in the system's browser -- or in $BROWSER, as Python's webbrowser
// takes it (the first of a list, "%s" for the address, else the address after it;
// BROWSER=true: none, for the tests)
func openBrowser(url string) {
	cmd := browserCommand(url)
	if b := os.Getenv("BROWSER"); b != "" {
		first := strings.Split(b, string(os.PathListSeparator))[0]
		if strings.Contains(first, "%s") {
			cmd = strings.Fields(strings.ReplaceAll(first, "%s", url))
		} else {
			cmd = append(strings.Fields(first), url)
		}
	}
	c := exec.Command(cmd[0], cmd[1:]...)
	detach(c)
	if c.Start() == nil {
		go c.Wait()
	}
}
