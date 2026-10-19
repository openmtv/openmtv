/* The network game's client (net.h): TCP, length-prefixed text messages.
 * BSD sockets; Winsock on Windows. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
#define BAD_SOCK INVALID_SOCKET
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
typedef int sock_t;
#define CLOSESOCK close
#define BAD_SOCK (-1)
#endif
#include "net.h"
#include "runtime.h"
#include "seat.h"

int net_active;
int net_seat;
const char *net_key;               /* --mp-key: the seat's secret from the lobby (none: no lobby, or an older one) */
int net_lead = 12;                 /* the server's lead (LEAD n; an older server: its --lead default) */

static sock_t sock = BAD_SOCK;
static int sock_dead;                    /* the server closed it: no BYE */
static unsigned char *outb; static size_t outn, outcap;   /* messages to send */
static unsigned char *inb; static size_t inn, incap;      /* bytes received, not yet a whole message */
static uint64_t complete;                /* the server has sent every input up to this frame */
static int started, waiting;
static uint64_t heard_ns;                /* the server's last message: when */
static int pinged;                       /* the server PINGs (pace realtime): its silence means something */
static uint64_t wait_end_ns[SEATS_MAX];  /* WAIT: a seat waited for, until when (wall clock; 0: not) */
static int wait_dropped[SEATS_MAX];      /* DROP said for it: not again */

#define SERVER_QUIET_S 10                /* the server silent that long: a notice */
#define SERVER_GONE_S 90                 /*                   that long: the end (it drops a silent seat after 60) */

/* Back after a break (a newer server: WELCOME ... token T): the connection lost -- closed,
 * a send that fails, the server silent RECONNECT_S -- a new one takes the seat over
 * ("RESUME seat T complete"); the server sends what this machine missed, this machine its
 * last hashes again (they may have gone with the old connection).  Tried for RESUME_S. */
#define RECONNECT_S 8
#define RESUME_S 75
static char token[64];
static char srv_host[256], srv_port[16];
static int broken;                       /* the connection is gone: net_frame tries a new one */
static int resuming, resumed, refused;   /* RESUME said: its answer */
static char hashes[64][48];              /* this machine's last HASH lines */
static int hash_n;
static char unsent[256][96];             /* this seat's live inputs ("IN 0 ...") the server has not
                                          * brought back yet: lost with a connection, said again */
static int unsent_first, unsent_n;
static uint64_t resumed_to;              /* RESUMED n: the server had sent up to n */

/* a seat's human by name, for the notices (--names; else "Player N") */
const char *seat_name(int seat, char *buf, size_t n) {
    int st = (seat >= 0 && seat < SEATS_MAX) ? seat_station[seat] : 0;
    if (st >= 1 && st <= 3 && new_game_names[st - 1][0]) snprintf(buf, n, "%s", new_game_names[st - 1]);
    else snprintf(buf, n, notice_german ? "Spieler %d" : "Player %d", seat + 1);
    return buf;
}

/* the end of the network game for this machine: the notice a moment in the window, then out */
static void end_with(const char *de, const char *en, int code) {
    char t[200];
    snprintf(t, sizeof t, "%s", notice_german ? de : en);
    host_notice(t, 30);
    char one[200];                       /* "[end] why" on one line: the launcher shows it (launcher/server.go) */
    snprintf(one, sizeof one, "%s", t);
    for (char *c = one; *c; c++) if (*c == '\n') *c = ' ';
    fprintf(stderr, "[end] %s\n", one);
    fflush(stdout);
    host_notice_hold(8);
    exit(code);
}

static void die(const char *what) {
    fprintf(stderr, "[net] %s\n", what);
    end_with("Die Verbindung zum Server ist weg.\nDas Spiel endet.", "The connection to the server is gone.\nThe game ends.", 3);
}

void net_send(const char *line) {
    size_t n = strlen(line);
    if (!strncmp(line, "HASH ", 5) && n < sizeof hashes[0]) memcpy(hashes[hash_n++ % 64], line, n + 1);
    if (!strncmp(line, "IN 0 ", 5) && n - 5 < sizeof unsent[0]) {   /* (live: in order, frame 0) */
        if (unsent_n == 256) { unsent_first = (unsent_first + 1) % 256; unsent_n--; }
        memcpy(unsent[(unsent_first + unsent_n++) % 256], line + 5, n - 5 + 1);
    }
    if (outn + 4 + n > outcap) {
        outcap = (outn + 4 + n) * 2 + 4096;
        if (!(outb = realloc(outb, outcap))) die("out of memory");
    }
    outb[outn] = (unsigned char)n; outb[outn + 1] = (unsigned char)(n >> 8);
    outb[outn + 2] = (unsigned char)(n >> 16); outb[outn + 3] = (unsigned char)(n >> 24);
    memcpy(outb + outn + 4, line, n);
    outn += 4 + n;
}

static int receive(int ms);

static void flush_out(void) {
    size_t done = 0;
    if (broken || sock == BAD_SOCK) { outn = 0; return; }   /* (gone: net_frame tries a new one) */
    while (done < outn) {
        long k = send(sock, (const char *)outb + done, (int)(outn - done), 0);
        if (k <= 0) {
            outn = 0;
            if (token[0] && started) { /* a new connection: net_frame.  No reading here: this may be
                                        * inside receive (a PING's PONG) */
                fprintf(stderr, "[net] the connection is gone (send)\n");
                broken = 1;
                return;
            }
            while (receive(500) > 0) ; /* the server's last words first (DESYNC, END), then the end */
            die("the server is gone (send)");
        }
        done += (size_t)k;
    }
    outn = 0;
}

/* one message from the server */
static void handle(char *msg) {
    if (!strncmp(msg, "B ", 2)) {                  /* a bundle: "B last" and its inputs */
        char *nl = strchr(msg, '\n');
        uint64_t last = strtoull(msg + 2, NULL, 10), had = complete;
        for (char *l = nl ? nl + 1 : NULL; l && *l; ) {
            char *e = strchr(l, '\n');
            if (e) *e = 0;
            unsigned long long f; int seat, k = 0;
            char cmd[32] = "";
            int got = sscanf(l, "%llu %d %31s %n", &f, &seat, cmd, &k);
            if (got >= 3 && (f <= had || f > last)) {
                /* (the server sends each frame's inputs once, in the bundle up to it: had < f <= last,
                 * a RESUME's too.  Not so -- a broken server: one already past would come late here
                 * (another game), one far ahead would hold up every later input) */
                fprintf(stderr, "[net] an input of frame %llu in the bundle of frames %llu..%llu: dropped\n", f,
                        (unsigned long long)had + 1, (unsigned long long)last);
            } else if (got >= 3) {
                const char *args = l + k;
                if (k <= 0 || (size_t)k > strlen(l)) args = "";
                host_net_line(f, seat, cmd, args);
                if (seat == net_seat && unsent_n) {     /* (one of this seat's live inputs is back) */
                    char back[96];
                    snprintf(back, sizeof back, "%s %s", cmd, args);
                    if (!strcmp(back, unsent[unsent_first])) { unsent_first = (unsent_first + 1) % 256; unsent_n--; }
                }
            }
            l = e ? e + 1 : NULL;
        }
        if (last > complete) complete = last;
        memset(wait_end_ns, 0, sizeof wait_end_ns);   /* (the server goes on: it waits for nobody) */
        if (waiting) {
            waiting = 0;
            fprintf(stderr, "[net] going on\n");
            host_notice(notice_german ? "Es geht weiter." : "The game goes on.", 3);
        }
    } else if (!strncmp(msg, "PING ", 5)) {        /* the server measures the round trip: answered at once */
        char pong[64];
        pinged = 1;
        snprintf(pong, sizeof pong, "PONG %s", msg + 5);
        net_send(pong);
        flush_out();
    } else if (!strncmp(msg, "LEAD ", 5)) {        /* the server's lead over the fastest client (host.c keeps near it) */
        int n = atoi(msg + 5);
        if (n > 0 && n < 200) net_lead = n;
    } else if (!strncmp(msg, "START", 5)) {
        started = 1;
    } else if (!strncmp(msg, "WELCOME", 7)) {
        const char *t = strstr(msg, " token ");
        if (t) snprintf(token, sizeof token, "%s", t + 7);
        fprintf(stderr, "[net] %.*s\n", t ? (int)(t - msg) : (int)strlen(msg), msg);
    } else if (!strncmp(msg, "RESUMED", 7)) {
        resumed = 1;
        resumed_to = strtoull(msg + 7, NULL, 10);
    } else if (!strncmp(msg, "REFUSE", 6)) {
        if (resuming) {
            fprintf(stderr, "[net] %s\n", msg);
            refused = 1;
        } else {
            die(msg);
        }
    } else if (!strncmp(msg, "WAIT", 4)) {
        fprintf(stderr, "[net] %s\n", msg);
        int seat, secs;
        char who[24], t[200];
        if (sscanf(msg, "WAIT seat %d lost its connection (%d s)", &seat, &secs) == 2 && seat == net_seat) {
            /* (this machine was silent -- a stall, a broken network: the others waited for it) */
        } else if ((waiting = 1) && sscanf(msg, "WAIT seat %d lost its connection (%d s)", &seat, &secs) == 2) {
            if (secs < 0) secs = 0;               /* (the server's word: within reason -- no overflow below) */
            if (secs > 3600) secs = 3600;
            seat_name(seat, who, sizeof who);
            if (notice_german) snprintf(t, sizeof t, "Die Verbindung zu %s ist unterbrochen.\nDas Spiel wartet bis zu %d Sekunden.", who, secs);
            else snprintf(t, sizeof t, "The connection to %s is broken.\nThe game waits up to %d seconds.", who, secs);
            host_notice(t, secs + 2);
            if (seat >= 0 && seat < SEATS_MAX) {
                wait_end_ns[seat] = host_wall_ns() + (uint64_t)secs * 1000000000ull;
                wait_dropped[seat] = 0;
            }
        }
    } else if (!strncmp(msg, "LEFT", 4)) {         /* a human left: "LEFT seat bye" (window closed, the game ended there), "... gone" (no connection) */
        int seat;
        char how[16] = "", who[24], t[200];
        fprintf(stderr, "[net] %s\n", msg);
        if (sscanf(msg, "LEFT %d %15s", &seat, how) >= 1 && seat >= 0 && seat < SEATS_MAX && wait_end_ns[seat]) {
            /* (its notice says the game goes on: no "goes on" over it -- unless the server
             * still waits for another) */
            wait_end_ns[seat] = 0;
            waiting = 0;
            for (int i = 0; i < SEATS_MAX; i++) waiting = waiting || wait_end_ns[i];
        }
        if (sscanf(msg, "LEFT %d %15s", &seat, how) >= 1 && seat != net_seat) {
            seat_name(seat, who, sizeof who);
            if (!strcmp(how, "gone")) {
                if (notice_german) snprintf(t, sizeof t, "Die Verbindung zu %s ist abgerissen.\nDas Spiel geht ohne %s weiter.", who, who);
                else snprintf(t, sizeof t, "The connection to %s is lost.\nThe game goes on without %s.", who, who);
            } else {
                if (notice_german) snprintf(t, sizeof t, "%s hat das Spiel verlassen.\nDas Spiel geht weiter.", who);
                else snprintf(t, sizeof t, "%s left the game.\nThe game goes on.", who);
            }
            host_notice(t, 8);
        }
    } else if (!strncmp(msg, "DESYNC", 6)) {
        fprintf(stderr, "[net] %s: the machines no longer compute the same game\n", msg);
        end_with("Die Rechner rechnen nicht mehr dasselbe Spiel (Desync).\nDas Spiel endet.",
                 "The machines no longer compute the same game (desync).\nThe game ends.", 4);
    } else if (!strncmp(msg, "END", 3)) {
        fprintf(stderr, "[net] %s\n", msg);
        const char *why = msg[3] ? msg + 4 : "";
        if (strstr(why, "away too long")) end_with("Du warst zu lange nicht erreichbar.\nDas Spiel ging ohne dich weiter.", "You could not be reached for too long.\nThe game went on without you.", 0);
        if (strstr(why, "not every player")) end_with("Nicht bei jedem Spieler ist das Spiel gestartet.\nDas Spiel endet.", "Not every player's game started.\nThe game ends.", 0);
        if (strstr(why, "over")) {
            fflush(stdout);
            exit(0);                                /* (the game's own end: it showed it) */
        }
        end_with("Der Server hat das Spiel beendet.", "The server ended the game.", 0);
    }
}

/* receive what is there (wait up to ms); 0 on a timeout, -1: the connection is gone */
static int receive(int ms) {
    if (broken || sock == BAD_SOCK) return -1;
    fd_set r;
    FD_ZERO(&r);
    FD_SET(sock, &r);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    if (select((int)sock + 1, &r, NULL, NULL, &tv) <= 0) return 0;
    if (inn + 65536 > incap) {
        incap = (inn + 65536) * 2;
        if (!(inb = realloc(inb, incap))) die("out of memory");
    }
    long k = recv(sock, (char *)inb + inn, 65536, 0);
    if (k <= 0) {                        /* (the server's own ends come with a word first: END, DESYNC) */
        fprintf(stderr, "[net] the connection to the server is gone\n");
        broken = 1;
        return -1;
    }
    heard_ns = host_wall_ns();
    inn += (size_t)k;
    size_t at = 0;
    while (inn - at >= 4) {
        size_t n = inb[at] | (size_t)inb[at + 1] << 8 | (size_t)inb[at + 2] << 16 | (size_t)inb[at + 3] << 24;
        if (inn - at - 4 < n) break;
        char *msg = malloc(n + 1);
        if (!msg) die("out of memory");
        memcpy(msg, inb + at + 4, n);
        msg[n] = 0;
        handle(msg);
        free(msg);
        at += 4 + n;
    }
    memmove(inb, inb + at, inn - at);
    inn -= at;
    return 1;
}

/* at the exit: "BYE" -- this machine leaves on purpose, the others go on
 * without waiting for it (its seat has no more input); errors ignored.
 * Then wait (a moment at most) for the server to close: a socket closed
 * with unread bundles in it is reset, and a reset can lose the BYE. */
static void bye(void) {
    if (sock == BAD_SOCK || sock_dead) return;
    outn = 0;
    net_send("BYE");
    send(sock, (const char *)outb, (int)outn, 0);
    outn = 0;
#ifdef _WIN32
    shutdown(sock, SD_SEND);
#else
    shutdown(sock, SHUT_WR);
#endif
    char buf[4096];
    for (int i = 0; i < 20; i++) {          /* up to 1 s */
        fd_set r;
        FD_ZERO(&r);
        FD_SET(sock, &r);
        struct timeval tv = { 0, 50000 };
        if (select((int)sock + 1, &r, NULL, NULL, &tv) > 0 && recv(sock, buf, sizeof buf, 0) <= 0) break;
    }
    CLOSESOCK(sock);
    sock = BAD_SOCK;
}

static void broke_off(void) {
    sock_dead = 1;
    end_with("Die Verbindung zum Server ist abgerissen.\nDas Spiel endet.", "The connection to the server broke off.\nThe game ends.", 3);
}

/* a connection to the server (srv_host, srv_port) within MS -- the window kept alive meanwhile;
 * BAD_SOCK if none */
static sock_t connect_to(int ms) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(srv_host[0] ? srv_host : "127.0.0.1", srv_port, &hints, &res) || !res) return BAD_SOCK;
    sock_t s = BAD_SOCK;
    for (struct addrinfo *a = res; a && s == BAD_SOCK; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == BAD_SOCK) continue;
#ifdef _WIN32
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
#else
        int fl = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
        int ok = connect(s, a->ai_addr, (int)a->ai_addrlen) == 0;
        /* under way: until writable -- or failed (Windows says that in the except set) --, or the
         * time is up */
        for (int t = 0; !ok && t < ms; t += 100) {
            fd_set w, x;
            FD_ZERO(&w); FD_ZERO(&x);
            FD_SET(s, &w); FD_SET(s, &x);
            struct timeval tv = { 0, 100000 };
            int r = select((int)s + 1, NULL, &w, &x, &tv);
            if (r > 0) {
                int err = 0;
                socklen_t len = sizeof err;
                ok = !FD_ISSET(s, &x) && getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len) == 0 && err == 0;
                break;
            }
            if (r < 0) break;
            host_net_wait("connecting", started);
        }
#ifdef _WIN32
        nb = 0;
        ioctlsocket(s, FIONBIO, &nb);
#else
        fcntl(s, F_SETFL, fl);
#endif
        if (!ok) { CLOSESOCK(s); s = BAD_SOCK; }
    }
    freeaddrinfo(res);
    if (s != BAD_SOCK) {
        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    }
    return s;
}

/* the connection is gone: a new one takes the seat over (RESUME), tried for RESUME_S -- or the end */
static void reconnect(void) {
    if (!token[0] || !started) broke_off();   /* (an older server, or before the start: the end) */
    if (sock != BAD_SOCK) CLOSESOCK(sock);
    sock = BAD_SOCK;
    inn = outn = 0;
    uint64_t lost = host_wall_ns(), last_try = 0, shown = 0;
    char t[200];
    fprintf(stderr, "[net] the connection to the server is gone: a new one ...\n");
    host_notice(notice_german ? "Die Verbindung zum Server ist unterbrochen.\nNeuer Versuch ..." :
                                "The connection to the server is broken.\nTrying again ...", 3);
    for (int tries = 0;; ) {
        uint64_t now = host_wall_ns();
        int secs = (int)((now - lost) / 1000000000ull);
        if (secs >= RESUME_S) {
            fprintf(stderr, "[net] no new connection within %d s\n", RESUME_S);
            broke_off();
        }
        if (now / 1000000000ull != shown) {
            shown = now / 1000000000ull;
            if (notice_german) snprintf(t, sizeof t, "Die Verbindung zum Server ist unterbrochen.\nNeuer Versuch ... (seit %d s)", secs);
            else snprintf(t, sizeof t, "The connection to the server is broken.\nTrying again ... (for %d s)", secs);
            host_notice_quiet(t, 2);
        }
        host_net_wait("connecting again", 1);
        if (now - last_try < 2000000000ull) {
#ifdef _WIN32
            Sleep(100);
#else
            struct timeval tv = { 0, 100000 };
            select(0, NULL, NULL, NULL, &tv);
#endif
            continue;
        }
        last_try = now;
        tries++;
        sock = connect_to(3000);
        if (sock == BAD_SOCK) continue;
        broken = 0;
        resuming = 1; resumed = refused = 0;
        char m[160];
        snprintf(m, sizeof m, "RESUME %d %s %llu", net_seat, token, (unsigned long long)complete);
        net_send(m);
        flush_out();
        uint64_t asked = host_wall_ns();
        while (!resumed && !refused && !broken && host_wall_ns() - asked < 5000000000ull)
            if (!receive(200)) host_net_wait("connecting again", 1);
        resuming = 0;
        while (resumed && !broken && complete < resumed_to && host_wall_ns() - asked < 10000000000ull)
            if (!receive(200)) host_net_wait("connecting again", 1);   /* (what it missed: inputs back too) */
        if (resumed && !broken && complete >= resumed_to) {
            fprintf(stderr, "[net] back on a new connection (try %d, after %d s)\n", tries, secs);
            char again_in[256][96];
            int k = unsent_n, first = unsent_first;
            memcpy(again_in, unsent, sizeof again_in);
            unsent_n = 0;
            if (k) fprintf(stderr, "[net] %d inputs again (lost with the old connection)\n", k);
            for (int i = 0; i < k; i++) {
                char m2[112];
                snprintf(m2, sizeof m2, "IN 0 %s", again_in[(first + i) % 256]);
                net_send(m2);
            }
            int n = hash_n;                   /* (net_send keeps them again: the count before) */
            char again[64][48];
            memcpy(again, hashes, sizeof again);
            hash_n = 0;
            for (int i = n > 64 ? n - 64 : 0; i < n; i++) net_send(again[i % 64]);
            flush_out();
            heard_ns = host_wall_ns();
            host_notice(notice_german ? "Wieder verbunden." : "Connected again.", 3);
            return;
        }
        if (refused) broke_off();
        resumed = 0;
        if (sock != BAD_SOCK) CLOSESOCK(sock);
        sock = BAD_SOCK;
        broken = 0;
        inn = outn = 0;
    }
}

void net_connect(const char *hostport, int seat, const char *params) {
#ifdef _WIN32
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd)) die("Winsock does not start");
#else
    signal(SIGPIPE, SIG_IGN);              /* a server gone: send() fails, no signal */
#endif
    char host[256];
    snprintf(host, sizeof host, "%s", hostport);
    char *colon = strrchr(host, ':');
    const char *port = "7640";
    if (colon) { *colon = 0; port = colon + 1; }
    snprintf(srv_host, sizeof srv_host, "%s", host);
    snprintf(srv_port, sizeof srv_port, "%s", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host[0] ? host : "127.0.0.1", port, &hints, &res) || !res) die("unknown server address");
    freeaddrinfo(res);
    for (int tries = 0; sock == BAD_SOCK && tries < 120; tries++) {   /* the server may not be up yet: 30 s */
        if (tries) {
            if (tries % 12 == 1) fprintf(stderr, "[net] no server at %s:%s yet, trying again ...\n", host[0] ? host : "127.0.0.1", port);
#ifdef _WIN32
            Sleep(250);                     /* (Winsock's select wants a socket) */
#else
            struct timeval tv = { 0, 250000 };
            select(0, NULL, NULL, NULL, &tv);
#endif
            host_net_wait("waiting for the server", 0);
        }
        sock = connect_to(1000);
    }
    if (sock == BAD_SOCK) die("cannot connect to the server");
    net_active = 1;
    net_seat = seat;
    char hello[2304];                        /* params: up to 2047 (runtime/main.c) */
    if (net_key) snprintf(hello, sizeof hello, "HELLO %d 1:%s %s", seat, net_key, params);   /* (an older server ignores the ":KEY") */
    else snprintf(hello, sizeof hello, "HELLO %d 1 %s", seat, params);
    net_send(hello);
    flush_out();
    atexit(bye);
    fprintf(stderr, "[net] seat %d connected to %s:%s\n", seat, host[0] ? host : "127.0.0.1", port);
}

void net_ready(void) {
    net_send("GO");
    flush_out();
    while (!started) {
        int r = receive(200);
        if (r < 0) broke_off();
        if (!r) host_net_wait("waiting for the other players", 0);
    }
    fprintf(stderr, "[net] the game starts\n");
}

/* NET_DROP_WAIT=S (the tests): Enter pressed S seconds into a wait for a player */
static int drop_wait_s(void) {
    static int s = -1;
    if (s < 0) { const char *e = getenv("NET_DROP_WAIT"); s = e ? atoi(e) : 0; }
    return s;
}

/* one wait for the server: the window alive; a player's broken connection counted down (Enter:
 * go on without them); the server itself silent -- a notice, and at last the end */
static void wait_step(void) {
    int enter = host_net_wait(waiting ? "waiting for a player" : "waiting for the server", started);
    uint64_t now = host_wall_ns();
    static uint64_t shown;                /* the countdown's last second shown */
    char t[200], who[24];
    int wait_seat = -1;                   /* (the first seat waited for that no one gave up on yet) */
    for (int i = 0; i < SEATS_MAX && wait_seat < 0; i++)
        if (wait_end_ns[i] && !wait_dropped[i]) wait_seat = i;
    if (waiting && wait_seat >= 0) {
        uint64_t left = wait_end_ns[wait_seat] > now ? (wait_end_ns[wait_seat] - now) / 1000000000ull : 0;
        seat_name(wait_seat, who, sizeof who);
        if (now / 1000000000ull != shown) {
            shown = now / 1000000000ull;
            if (notice_german) snprintf(t, sizeof t, "Die Verbindung zu %s ist unterbrochen.\nNoch %d s -- Enter: ohne %s weiter", who, (int)left, who);
            else snprintf(t, sizeof t, "The connection to %s is broken.\n%d s more -- Enter: go on without %s", who, (int)left, who);
            host_notice_quiet(t, 2);
        }
        static uint64_t since;            /* (NET_DROP_WAIT: when this wait began) */
        static int since_seat = -1;
        if (since_seat != wait_seat || !since) { since_seat = wait_seat; since = now; }
        if (enter || (drop_wait_s() > 0 && now - since >= (uint64_t)drop_wait_s() * 1000000000ull)) {
            char m[32];
            snprintf(m, sizeof m, "DROP %d", wait_seat);
            net_send(m);
            flush_out();
            wait_dropped[wait_seat] = 1;
            since = 0;
            fprintf(stderr, "[net] not waiting for seat %d any longer\n", wait_seat);
            if (notice_german) snprintf(t, sizeof t, "Das Spiel geht ohne %s weiter.", who);
            else snprintf(t, sizeof t, "The game goes on without %s.", who);
            host_notice(t, 4);
        }
    }
    if (started && pinged && token[0] && heard_ns && now - heard_ns > RECONNECT_S * 1000000000ull) {
        fprintf(stderr, "[net] the server silent for %d s\n", RECONNECT_S);
        broken = 1;                       /* (a connection broken quietly: a new one) */
        return;
    }
    if (started && pinged && heard_ns && now - heard_ns > SERVER_QUIET_S * 1000000000ull) {
        int quiet = (int)((now - heard_ns) / 1000000000ull);
        if (quiet >= SERVER_GONE_S) {
            fprintf(stderr, "[net] the server silent for %d s\n", quiet);
            sock_dead = 1;
            end_with("Der Server antwortet nicht mehr.\nDas Spiel endet.", "The server does not answer any more.\nThe game ends.", 3);
        }
        if (now / 1000000000ull != shown) {
            shown = now / 1000000000ull;
            if (quiet == SERVER_QUIET_S + 1) fprintf(stderr, "[net] the server silent for %d s\n", quiet);
            if (notice_german) snprintf(t, sizeof t, "Der Server antwortet nicht (seit %d s).\nDas Spiel wartet.", quiet);
            else snprintf(t, sizeof t, "The server does not answer (for %d s).\nThe game waits.", quiet);
            host_notice_quiet(t, 2);
        }
    }
}

/* Every frame what has come is read, needed or not: a PING waited otherwise until this machine
 * needed the next bundle -- as long as it ran behind the front -- and the server took that for
 * the round trip: its lead grew to the most (12 frames), the clients fell further behind,
 * and a click came 0.3-0.7 s late. */
void net_frame(uint64_t n) {
    char at[48];
    snprintf(at, sizeof at, "AT %llu", (unsigned long long)n);
    net_send(at);
    flush_out();
    while (receive(0) > 0) ;
    while (complete < n || broken) {
        if (broken) {                     /* (a new connection; then this frame's AT again) */
            reconnect();
            net_send(at);
            flush_out();
            continue;
        }
        int r = receive(200);
        if (!r) wait_step();
        if (outn) flush_out();
    }
}

uint64_t net_complete(void) { return complete; }

void net_close(void) {
    if (sock == BAD_SOCK) return;
    if (outn) flush_out();
    CLOSESOCK(sock);
    sock = BAD_SOCK;
}
