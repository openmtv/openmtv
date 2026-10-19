// Command stub: the game's stand-in for tools/playtest.py on Windows, where its
// shell scripts cannot run -- prints its name, directory and arguments as they
// do, and ends with 3.  Only for the test (make playtest-win), never in a package.
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func main() {
	name := strings.TrimSuffix(filepath.Base(os.Args[0]), ".exe")
	wd, _ := os.Getwd()
	fmt.Printf("%s in %s: %s\n", name, wd, strings.Join(os.Args[1:], " "))
	os.Exit(3)
}
