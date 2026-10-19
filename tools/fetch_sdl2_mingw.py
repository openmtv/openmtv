#!/usr/bin/env python3
"""SDL2 for the Windows build (make win): the official MinGW development
package of libsdl-org, a pinned release checked by its SHA-256.

    python3 tools/fetch_sdl2_mingw.py [DIR]          (default build/sdl2-mingw)

downloads SDL2-devel-VERSION-mingw.tar.gz from SDL's release page on GitHub
into DIR, checks its SHA-256 (the value GitHub lists for the release asset)
and unpacks its x86-64 part into DIR/x86_64-w64-mingw32/: include/SDL2/, lib/
(the import library libSDL2.dll.a) and bin/SDL2.dll, plus SDL's LICENSE.txt
(zlib licence).  Nothing happens when DIR/VERSION names this release already.
SDL2.dll goes next to mtv-dec.exe; neither is in the repository.  Only plain
files and directories below the x86-64 folder are written, never outside DIR.
Standard library only."""
import hashlib
import os
import shutil
import sys
import tarfile
import urllib.request

VERSION = '2.32.10'                 # SDL 2.x like the Linux and macOS builds (the latest 2.x release)
NAME = 'SDL2-devel-%s-mingw.tar.gz' % VERSION
URL = 'https://github.com/libsdl-org/SDL/releases/download/release-%s/%s' % (VERSION, NAME)
SHA256 = '83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2'
ARCH = 'x86_64-w64-mingw32'

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def download(dest):
    part = dest + '.part'
    print('fetch_sdl2_mingw: %s' % URL)
    with urllib.request.urlopen(URL, timeout=120) as r, open(part, 'wb') as f:
        shutil.copyfileobj(r, f)
    os.replace(part, dest)


def unpack(archive, out):
    """the x86-64 folder (and LICENSE.txt) of the package into out/, plain files only"""
    top = 'SDL2-%s/' % VERSION
    n = 0
    with tarfile.open(archive, 'r:gz') as t:
        for m in t.getmembers():
            name = m.name
            if name == top + 'LICENSE.txt':
                rel = os.path.join(ARCH, 'LICENSE.txt')
            elif name.startswith(top + ARCH + '/'):
                rel = name[len(top):]
            else:
                continue
            parts = rel.split('/')
            if os.path.isabs(rel) or '..' in parts or not all(parts):
                sys.exit('fetch_sdl2_mingw: refusing the path %r in the package' % name)
            path = os.path.join(out, *parts)
            if m.isdir():
                os.makedirs(path, exist_ok=True)
            elif m.isfile():
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with t.extractfile(m) as src, open(path, 'wb') as dst:
                    shutil.copyfileobj(src, dst)
                n += 1
            # links and anything else: not needed, not written
    return n


def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'sdl2-mingw'))
    stamp = os.path.join(out, 'VERSION')
    if os.path.exists(stamp) and open(stamp).read().strip() == VERSION \
            and os.path.exists(os.path.join(out, ARCH, 'bin', 'SDL2.dll')):
        return 0
    os.makedirs(out, exist_ok=True)
    archive = os.path.join(out, NAME)
    if not os.path.exists(archive) or sha256(archive) != SHA256:
        download(archive)
    got = sha256(archive)
    if got != SHA256:
        os.remove(archive)
        sys.exit('fetch_sdl2_mingw: %s has SHA-256 %s, expected %s -- removed' % (NAME, got, SHA256))
    shutil.rmtree(os.path.join(out, ARCH), ignore_errors=True)
    n = unpack(archive, out)
    for need in ('include/SDL2/SDL.h', 'lib/libSDL2.dll.a', 'bin/SDL2.dll'):
        if not os.path.exists(os.path.join(out, ARCH, need)):
            sys.exit('fetch_sdl2_mingw: %s is not in the package' % need)
    with open(stamp, 'w') as f:
        f.write(VERSION + '\n')
    print('fetch_sdl2_mingw: SDL %s (SHA-256 checked), %d files in %s' % (VERSION, n, os.path.join(out, ARCH)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
