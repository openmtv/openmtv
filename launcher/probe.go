package main

import (
	"net/http"
	"time"
)

// openmtv probe URL: exit 0 when URL answers 200 within 5 s, else 1 -- the container's health
// check of the public server (the image is FROM scratch: no shell, no curl -- the compose
// file's healthcheck runs it)
func init() { commands["probe"] = probeCommand }

func probeCommand(args []string) int {
	if len(args) != 1 {
		warn("openmtv probe URL\n")
		return 2
	}
	c := http.Client{Timeout: 5 * time.Second}
	r, err := c.Get(args[0])
	if err != nil {
		warn("probe: %v\n", err)
		return 1
	}
	r.Body.Close()
	if r.StatusCode != http.StatusOK {
		warn("probe: %s\n", r.Status)
		return 1
	}
	return 0
}
