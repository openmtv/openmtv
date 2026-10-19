/* The network game's client (multiplayer stage 4, docs/multiplayer.md):
 * lockstep against the frame server build/openmtv serve (launcher/mpserver.go).
 *
 * Every machine computes every seat; the server only orders the inputs.  A
 * client sends its own seat's input ("IN frame command args": frame 0 = as
 * soon as possible, a script's lines with their frame) and, at each frame
 * boundary N, waits until the server has sent all of frame N's inputs -- a
 * bundle -- which go into the game as script lines of their seats
 * (runtime/host.c).  Every 70 frames it reports the game state's hash; the
 * server compares them.
 *
 * Messages: a 32-bit little-endian length, then that many bytes of text --
 * one line, or for a bundle "B last" and one line per input
 * "frame seat command args". */
#ifndef MTV_NET_H
#define MTV_NET_H
#include <stdint.h>

extern int net_active;          /* --mp HOST:PORT given */
extern int net_seat;            /* the seat this machine plays */
const char *seat_name(int seat, char *buf, size_t n);   /* a seat's human by name, for the notices */
extern int net_lead;            /* the server's lead over the fastest client in frames (LEAD n): the clients keep near it */

/* connect, say hello (seat, the game's parameters); exits with a message
 * on failure */
void net_connect(const char *hostport, int seat, const char *params);
/* everything to say before the start is said (a script's input): wait for
 * the start of the game */
void net_ready(void);
/* one line to the server (an input, a report) */
void net_send(const char *line);
/* the frame boundary N: send what is queued, then wait until frame N is
 * complete; every input of it reaches host_net_line() first */
void net_frame(uint64_t n);
/* the server has sent every input up to this frame */
uint64_t net_complete(void);
void net_close(void);

/* runtime/host.c: an input of a bundle (seat, frame, the script line's
 * command and arguments) */
void host_net_line(uint64_t frame, int seat, const char *cmd, const char *args);
/* runtime/host.c: keep the window alive while waiting for the server (SHOW: the game runs --
 * its picture and the notices shown); 1: the player pressed Enter */
int host_net_wait(const char *why, int show);
/* runtime/host.c: a notice without its line on stderr (one that counts down); the wall clock */
void host_notice_quiet(const char *utf8, double seconds);
uint64_t host_wall_ns(void);
#endif
