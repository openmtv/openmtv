#!/usr/bin/env python3
"""SDL2 as a static library, for the game in the Linux and macOS packages.

    python3 tools/sdl2_static.py [DIR]      -> DIR/bin/sdl2-config, DIR/lib/libSDL2.a
                                               (default build/sdl2-static)

The players' packages (tools/package.py linux / mac) carry the game with SDL2
built in: nothing to install, no Homebrew on the Mac (whose SDL2 is the
sdl2-compat layer over SDL3 now).  This fetches SDL's source release from
GitHub (the same 2.x release as the Windows build, tools/fetch_sdl2_mingw.py),
checks its SHA-256, and builds it with its configure script, without shared
libraries.  On Linux SDL still loads X11, Wayland, PulseAudio, PipeWire and
ALSA at run time (SDL's own dynamic loading: only the ones the system has are
needed); on the Mac it takes the system's frameworks, for macOS 11 and later
($MACOSX_DEPLOYMENT_TARGET, default 11.0).  Use it with
    make build/mtv-dec SDL2_CONFIG=DIR/bin/sdl2-config SDL2_STATIC=1
Nothing happens when DIR/VERSION names this release already.  Standard
library only (and a C compiler, make)."""
import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
VERSION = '2.32.10'
NAME = 'SDL2-%s.tar.gz' % VERSION
URL = 'https://github.com/libsdl-org/SDL/releases/download/release-%s/%s' % (VERSION, NAME)
SHA256 = '5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165'
MAC = sys.platform == 'darwin'


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def fetch():
    """the source release in build/download/ (fetched once, checked every time)"""
    d = os.path.join(ROOT, 'build', 'download')
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, NAME)
    if not os.path.exists(path):
        print('sdl2_static: fetching %s' % URL)
        part = path + '.part'
        with urllib.request.urlopen(URL, timeout=120) as r, open(part, 'wb') as f:
            shutil.copyfileobj(r, f)
        os.replace(part, path)
    if sha256(path) != SHA256:
        sys.exit('sdl2_static: %s has the wrong SHA-256 (want %s) -- delete it and try again' % (path, SHA256))
    return path


def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'sdl2-static'))
    stamp = os.path.join(out, 'VERSION')
    if os.path.exists(stamp) and open(stamp).read().strip() == VERSION \
            and os.path.exists(os.path.join(out, 'lib', 'libSDL2.a')):
        return
    tgz = fetch()
    work = out + '.work'
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    with tarfile.open(tgz) as t:
        t.extractall(work, filter='data') if hasattr(tarfile, 'data_filter') else t.extractall(work)
    src = os.path.join(work, 'SDL2-%s' % VERSION)
    env = dict(os.environ)
    if MAC:
        target = env.setdefault('MACOSX_DEPLOYMENT_TARGET', '11.0')
        env['CFLAGS'] = (env.get('CFLAGS', '-O2') + ' -mmacosx-version-min=' + target).strip()
    shutil.rmtree(out, ignore_errors=True)
    args = ['./configure', '--prefix=' + out, '--disable-shared', '--enable-static', '--disable-sdltest']
    jobs = str(os.cpu_count() or 4)
    for cmd in (args, ['make', '-j' + jobs], ['make', 'install']):
        r = subprocess.run(cmd, cwd=src, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if r.returncode:
            sys.exit('sdl2_static: %s failed:\n%s' % (' '.join(cmd), r.stderr[-3000:]))
    with open(stamp, 'w') as f:
        f.write(VERSION + '\n')
    shutil.rmtree(work, ignore_errors=True)
    print('sdl2_static: SDL %s in %s' % (VERSION, os.path.relpath(out, ROOT)))


if __name__ == '__main__':
    main()
