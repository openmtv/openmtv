"""A WebSocket client for the tests of the network game's server (tools/mpsavetest.py):
the connection as a socket of the plain protocol -- messages as 32-bit length + text,
each one WebSocket message (what the launcher's bridge does, launcher/ws.go).

    ws = wsclient.ws_connect('ws://127.0.0.1:7640/lobby')

Standard library only."""
import base64
import hashlib
import os
import socket
import ssl
import struct
import threading
import urllib.parse

WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'


class WSConn:
    """a WebSocket to the server, as a socket of the plain protocol: sendall() takes messages
    as 32-bit length + text and sends each as one WebSocket message, recv() gives one message
    the same way (b'' at the end); pings answered"""

    def __init__(self, sock):
        self.sock = sock
        self.lock = threading.Lock()
        self.out = b''
        self.closed = False

    def _frame(self, op, payload):
        n = len(payload)
        if n < 126:
            head = bytes([0x80 | op, 0x80 | n])
        elif n < 65536:
            head = bytes([0x80 | op, 0x80 | 126]) + struct.pack('>H', n)
        else:
            head = bytes([0x80 | op, 0x80 | 127]) + struct.pack('>Q', n)
        mask = os.urandom(4)
        if n:
            m = (mask * (n // 4 + 1))[:n]
            payload = (int.from_bytes(payload, 'big') ^ int.from_bytes(m, 'big')).to_bytes(n, 'big')
        with self.lock:
            self.sock.sendall(head + mask + payload)

    def sendall(self, data):
        with self.lock:
            self.out += data
            msgs = []
            while len(self.out) >= 4:
                (n,) = struct.unpack('<I', self.out[:4])
                if len(self.out) < 4 + n:
                    break
                msgs.append(self.out[4:4 + n])
                self.out = self.out[4 + n:]
        for m in msgs:
            self._frame(2, m)

    def _exact(self, n):
        b = b''
        while len(b) < n:
            d = self.sock.recv(n - len(b))
            if not d:
                raise ConnectionError('closed')
            b += d
        return b

    def recv(self, _bufsize=0):
        data = b''
        try:
            while True:
                h = self._exact(2)
                fin, op, n = h[0] & 0x80, h[0] & 0x0F, h[1] & 0x7F
                if n == 126:
                    (n,) = struct.unpack('>H', self._exact(2))
                elif n == 127:
                    (n,) = struct.unpack('>Q', self._exact(8))
                payload = self._exact(n)
                if op == 8:
                    return b''
                if op == 9:
                    self._frame(10, payload)
                    continue
                if op == 10:
                    continue
                data += payload
                if fin:
                    return struct.pack('<I', len(data)) + data
        except (ConnectionError, ssl.SSLError):
            return b''

    def settimeout(self, t):
        self.sock.settimeout(t)

    def setsockopt(self, *a):
        self.sock.setsockopt(*a)

    def close(self):
        if not self.closed:
            self.closed = True
            try:
                self._frame(8, b'')
            except OSError:
                pass
        self.sock.close()


def ws_connect(url, timeout=10):
    """a WebSocket to URL (ws:// or wss://host[:port]/path)"""
    u = urllib.parse.urlsplit(url)
    secure = u.scheme == 'wss'
    port = u.port or (443 if secure else 80)
    try:
        sock = socket.create_connection((u.hostname, port), timeout=timeout)
    except OSError as e:
        raise ValueError('no connection to %s (%s)' % (u.hostname, e))
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    if secure:
        try:
            sock = ssl.create_default_context().wrap_socket(sock, server_hostname=u.hostname)
        except (ssl.SSLError, OSError) as e:
            sock.close()
            raise ValueError('no secure connection to %s (%s)' % (u.hostname, e))
    key = base64.b64encode(os.urandom(16)).decode()
    host = u.hostname + ('' if u.port in (None, 443 if secure else 80) else ':%d' % u.port)
    sock.sendall(('GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                  'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nUser-Agent: madtv-test\r\n\r\n'
                  % (u.path or '/', host, key)).encode())
    head = b''
    try:
        while b'\r\n\r\n' not in head:
            d = sock.recv(1)
            if not d:
                break
            head += d
            if len(head) > 16384:
                break
    except OSError as e:
        sock.close()
        raise ValueError('no answer from %s (%s)' % (u.hostname, e))
    lines = head.decode(errors='replace').split('\r\n')
    want = base64.b64encode(hashlib.sha1((key + WS_GUID).encode()).digest()).decode()
    if ' 101 ' not in lines[0] + ' ' or not any(l.lower().startswith('sec-websocket-accept:') and l.split(':', 1)[1].strip() == want for l in lines):
        sock.close()
        raise ValueError('%s: no game server there (%s)' % (url, lines[0] or 'no answer'))
    sock.settimeout(None)
    return WSConn(sock)
