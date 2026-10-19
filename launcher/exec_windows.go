//go:build windows

package main

import (
	"os"
	"os/exec"
	"os/signal"
	"syscall"
	"time"
	"unsafe"
)

// No exec on Windows: the game is started and waited for, its exit code passed on.
// Ctrl+C reaches the game as well (the same console); this program then ends
// with 130, as play.py does.

// startGame: the game in the decomp directory, waited for -> its exit code
func startGame(cmd []string) int {
	return runWait(cmd)
}

func runWait(cmd []string) int {
	c := exec.Command(cmd[0], cmd[1:]...)
	c.Dir, c.Stdin, c.Stdout, c.Stderr = root, os.Stdin, os.Stdout, os.Stderr
	intr := make(chan os.Signal, 1)
	signal.Notify(intr, os.Interrupt)
	defer signal.Stop(intr)
	if err := c.Start(); err != nil {
		warn("play: %v\n", err)
		return 2
	}
	done := make(chan error, 1)
	go func() { done <- c.Wait() }()
	select {
	case err := <-done:
		if e, ok := err.(*exec.ExitError); ok {
			return e.ExitCode()
		}
		if err != nil {
			return 1
		}
		return 0
	case <-intr:
		select { // (a moment for the game to end on its own Ctrl+C)
		case <-done:
		case <-time.After(250 * time.Millisecond):
		}
		return 130
	}
}

const (
	createNewProcessGroup = 0x00000200
	createNoWindow        = 0x08000000
)

// detach: a program on its own, without a console window -- it plays on when the
// launcher ends
func detach(c *exec.Cmd) {
	c.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNewProcessGroup | createNoWindow}
}

// hideWindow: no console window for a console program the launcher asks
// (mtv-dec.exe --fixes=list)
func hideWindow(c *exec.Cmd) { c.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNoWindow} }

// detachConsole: the launcher started on its own (play.cmd: start, or a double click)
// has a console window of its own -- it goes; one shared with a command line stays
func detachConsole() {
	k := syscall.NewLazyDLL("kernel32.dll")
	var ids [4]uint32
	n, _, _ := k.NewProc("GetConsoleProcessList").Call(uintptr(unsafe.Pointer(&ids[0])), 4)
	if n == 1 {
		k.NewProc("FreeConsole").Call()
	}
}

const attachParentProcess = ^uint32(0) // (ATTACH_PARENT_PROCESS)

// attachConsole: the launcher of the players is a program of the window subsystem
// (make launcher-win: -H windowsgui -- no console window on a double click, none
// flashing at play.cmd's start); started with arguments from a command line
// (play.cmd -h, --print, --direct) it takes that one's console: its text goes
// there and the game plays in it.  Handles passed on (a pipe, a file) stay.
func attachConsole() {
	k := syscall.NewLazyDLL("kernel32.dll")
	if w, _, _ := k.NewProc("GetConsoleWindow").Call(); w != 0 {
		return // (a console program, or one started with a console)
	}
	if r, _, _ := k.NewProc("AttachConsole").Call(uintptr(attachParentProcess)); r == 0 {
		return // (started from Explorer: no console to take)
	}
	setStd := k.NewProc("SetStdHandle")
	take := func(f **os.File, std int, name string, access uint32, path string) {
		if *f != nil {
			if t, err := syscall.GetFileType(syscall.Handle((*f).Fd())); err == nil && t != syscall.FILE_TYPE_UNKNOWN {
				return
			}
		}
		p, _ := syscall.UTF16PtrFromString(path)
		h, err := syscall.CreateFile(p, access, syscall.FILE_SHARE_READ|syscall.FILE_SHARE_WRITE, nil, syscall.OPEN_EXISTING, 0, 0)
		if err != nil {
			return
		}
		setStd.Call(uintptr(std), uintptr(h))
		*f = os.NewFile(uintptr(h), name)
	}
	rw := uint32(syscall.GENERIC_READ | syscall.GENERIC_WRITE)
	take(&os.Stdin, syscall.STD_INPUT_HANDLE, "/dev/stdin", rw, "CONIN$")
	take(&os.Stdout, syscall.STD_OUTPUT_HANDLE, "/dev/stdout", rw, "CONOUT$")
	take(&os.Stderr, syscall.STD_ERROR_HANDLE, "/dev/stderr", rw, "CONOUT$")
}

func browserCommand(url string) []string {
	return []string{"rundll32", "url.dll,FileProtocolHandler", url}
}

// setBroadcast: a UDP socket may send to a broadcast address (the LAN search)
func setBroadcast(fd uintptr) error {
	return syscall.SetsockoptInt(syscall.Handle(fd), syscall.SOL_SOCKET, syscall.SO_BROADCAST, 1)
}
