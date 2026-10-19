//go:build !windows

package main

import (
	"os"
	"os/exec"
	"runtime"
	"syscall"
)

// startGame: the game in place of this process, in the decomp directory
func startGame(cmd []string) int {
	if err := os.Chdir(root); err == nil {
		err = syscall.Exec(cmd[0], cmd, os.Environ())
		warn("play: %s: %v\n", cmd[0], err)
	} else {
		warn("play: %v\n", err)
	}
	return 2
}

// runWait: a program in the decomp directory, waited for -> its exit code
func runWait(cmd []string) int {
	c := exec.Command(cmd[0], cmd[1:]...)
	c.Dir, c.Stdin, c.Stdout, c.Stderr = root, os.Stdin, os.Stdout, os.Stderr
	if err := c.Run(); err != nil {
		if e, ok := err.(*exec.ExitError); ok {
			return e.ExitCode()
		}
		return 1
	}
	return 0
}

// detach: a program of its own session -- it plays on when the launcher ends
func detach(c *exec.Cmd) { c.SysProcAttr = &syscall.SysProcAttr{Setsid: true} }

// hideWindow: (Windows: no console window for a console program)
func hideWindow(c *exec.Cmd) {}

// detachConsole: (Windows: a console window of the launcher's own goes)
func detachConsole() {}

// attachConsole: (Windows: the window build takes the command line's console)
func attachConsole() {}

func browserCommand(url string) []string {
	if runtime.GOOS == "darwin" {
		return []string{"open", url}
	}
	return []string{"xdg-open", url}
}

// setBroadcast: a UDP socket may send to a broadcast address (the LAN search)
func setBroadcast(fd uintptr) error {
	return syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, syscall.SO_BROADCAST, 1)
}
