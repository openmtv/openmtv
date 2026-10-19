#!/usr/bin/env python3
"""The network game's client (runtime/net.c) against a broken server.

    python3 tools/netclienttest.py

A fake server here speaks the protocol (docs/multiplayer.md) to one real
game (build/mtv-dec --headless, seat 0 of two humans) and says what the
real server (launcher/mpserver.go) never says:

  * "WAIT seat 1 lost its connection (S s)" with S = 2147483647 and -7:
    the game takes S within 0..3600 (its notice says so);
  * bundles with inputs outside their frames -- one of a frame already
    complete, one far ahead: the game drops them (each with a line on
    stderr) and takes the others;

then it sends bundles past the end of the game's script: the game must
come to it ([quit]).

Standard library only."""
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import scenario  # noqa: E402
import seattest  # noqa: E402

QUIT = 300                                   # the game's script ends here


def send(c, text):
    b = text.encode()
    c.sendall(struct.pack('<I', len(b)) + b)


def recv(c):
    def exactly(n):
        b = b''
        while len(b) < n:
            k = c.recv(n - len(b))
            if not k:
                raise EOFError
            b += k
        return b
    return exactly(struct.unpack('<I', exactly(4))[0]).decode()


def server(ls, said):
    c, _ = ls.accept()
    c.settimeout(60)
    try:
        while True:
            m = recv(c)
            said.append(m)
            if m.startswith('HELLO'):
                send(c, 'WELCOME seat 0 of 2 token 00112233')
            elif m == 'GO':
                break
        send(c, 'START')
        send(c, 'WAIT seat 1 lost its connection (2147483647 s)')
        send(c, 'B 50\n10 0 pointer 100 100')
        send(c, 'WAIT seat 1 lost its connection (-7 s)')
        send(c, 'B 100\n20 0 pointer 5 5\n999999 0 pointer 6 6\n60 0 pointer 110 100\n80 1 pointer 120 100')
        for f in range(150, QUIT + 100, 50):
            send(c, 'B %d' % f)
        while True:                          # (until the game closes)
            said.append(recv(c))
    except (EOFError, OSError):
        pass
    finally:
        c.close()


def main():
    d = tempfile.mkdtemp(prefix='netclienttest_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(scenario.GAME, f), d)
        script = os.path.join(d, 'script.txt')
        with open(script, 'w') as f:
            f.write('%d quit\n' % QUIT)
        ls = socket.socket()
        ls.bind(('127.0.0.1', 0))
        ls.listen(1)
        said = []
        t = threading.Thread(target=server, args=(ls, said), daemon=True)
        t.start()
        p = subprocess.run([seattest.BIN, '--headless', '--nosound', '--game', d, '--script', script, '--args', 'e', '--logical-time',
                            '--fixes', '--new-game', '--humans', '1,2', '--seat', '0', '--mp', '127.0.0.1:%d' % ls.getsockname()[1],
                            '--mp-key', '0123abcd'], capture_output=True, text=True, timeout=120)
        t.join(5)
        err = p.stderr.split('\n')
        checks = [
            (any(m.startswith('HELLO 0 1:0123abcd humans=1,2 ') for m in said), 'HELLO with the seat\'s secret (--mp-key)'),
            (any('waits up to 3600 seconds' in l for l in err), 'WAIT 2147483647 s: up to 3600 seconds'),
            (any('waits up to 0 seconds' in l for l in err), 'WAIT -7 s: up to 0 seconds'),
            (any('input of frame 20 in the bundle of frames 51..100: dropped' in l for l in err), 'an input of a frame already complete: dropped'),
            (any('input of frame 999999 in the bundle of frames 51..100: dropped' in l for l in err), 'an input far ahead: dropped'),
            (sum('dropped' in l for l in err) == 2, 'the others taken (no other dropped)'),
            (any(l.startswith('[quit]') for l in err), 'the game came to its script\'s end (exit %d)' % p.returncode),
        ]
        for ok, what in checks:
            print('%s %s' % ('ok  ' if ok else 'FAIL', what))
        if not all(ok for ok, _ in checks):
            print('\n'.join(l for l in err if l.startswith(('[net]', '[notice]', '[quit]')))[-3000:])
            return 1
        print('all ok')
        return 0
    finally:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
