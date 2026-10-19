#!/usr/bin/env python3
"""The packages for players: everything but the original game files.

    python3 tools/package.py win     -> build/package/OpenMTV-win64-VERSION.zip
    python3 tools/package.py linux   -> build/package/OpenMTV-linux-x64-VERSION.tar.gz
    python3 tools/package.py mac     -> build/package/OpenMTV-macos-arm64-VERSION.zip  (on the Mac)
    python3 tools/package.py packs   -> build/package/inhaltspakete-VERSION.zip  (the developer's own machines)

Every package holds the game (build/mtv-dec, stripped of its debug
information: some 25 MB -> 5 MB; the full one stays beside the package,
build/package/mtv-dec-VERSION-SYSTEM, the same addresses for a player's crash),
its launcher (launcher/, in Go: the page "Play", the packs' build,
multiplayer and its server, the content editor, all pages built in, in a
window of its own) and the licences.  A player installs nothing and no
Python comes with it (docs/ROADMAP.md, "Distribution for players").  The
content packs in git (mods/: the new films, series and ads, with real films'
titles) are not in the public packages (2026-10-19, the user's wish): `packs`
puts them into a zip of their own for the developer's own machines, unpacked
over an installed OpenMTV (the same layout).  The layout is the same
everywhere -- the game files go into the top folder, one directory above decomp/:

    OpenMTV/OpenMTV.cmd               Windows: double-click (OpenMTV.cmd editor: the editor)
    OpenMTV/OpenMTV.app               macOS: double-click (the launcher's window build; it finds
                                      decomp/ beside itself)
    OpenMTV/openmtv                   Linux: the launcher (decomp/play.sh)
    OpenMTV/LIESMICH.txt, README.txt  put SOURCE1.RSC and SOURCE2.RSC here
    OpenMTV/decomp/build/             the launcher, mtv-dec, COMMIT.txt (the version the lobby compares)
    OpenMTV/decomp/mods/              only in inhaltspakete-VERSION.zip: the content packs in git
                                      (a player's own packs go here when the editor makes them)
    OpenMTV/decomp/third_party/       the licences: the sound emulator (LGPL-2.1, in mtv-dec),
                                      SDL2 (zlib), Go's library and the Go modules in the launcher

What each one needs first:
  win    make win launcher-win OPL3_SHARED=1   (MinGW; SDL2.dll and opl3.dll beside the game)
  linux  make launcher launcher-native, and the game with SDL2 built in and the sound
         chip's library beside it:
         python3 tools/sdl2_static.py && rm -f build/mtv-dec &&
         make build/mtv-dec SDL2_CONFIG=$PWD/build/sdl2-static/bin/sdl2-config SDL2_STATIC=1 OPL3_SHARED=1
         (only libc then; the window build needs WebKitGTK 4.1 -- without it
         play.sh takes build/openmtv and the browser)
  mac    the same on the Mac with MACOSX_DEPLOYMENT_TARGET=11.0 in the environment
         (the game, SDL2 and the window build for macOS 11 and later), then
         make launcher-native-app

Nuked-OPL3, the sound chip's emulator (LGPL-2.1), goes in as a library of its own
(opl3.dll, libopl3.so, libopl3.dylib -- OPL3_SHARED=1) with its source and licence in
decomp/third_party/nuked-opl3/: a player may build it anew and put theirs in its place.
The original game files are never put in.  Batch files get CRLF line ends.
Standard library only (and strip: MinGW's for Windows, $WIN_STRIP; Go for
the licences, `go env GOROOT`, `go version -m`; ditto and codesign on the Mac)."""
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
NEVER = ('SOURCE1.RSC', 'SOURCE2.RSC', 'MTV.EXE', 'MTV_U.EXE', 'INTRO.EXE')
WIN_STRIP = os.environ.get('WIN_STRIP', 'x86_64-w64-mingw32-strip')
TOP = 'OpenMTV/'
OUT = os.path.join(ROOT, 'build', 'package')

START = {   # how the launcher is started, per system: (de, en)
    'win': ('OpenMTV.cmd doppelklicken', 'Double-click OpenMTV.cmd'),
    'mac': ('OpenMTV.app doppelklicken', 'Double-click OpenMTV.app'),
    'linux': ('./openmtv starten (oder decomp/play.sh)', 'Run ./openmtv (or decomp/play.sh)'),
}
NOTES = {
    'win': ("""Es muss nichts installiert werden: das Fenster nimmt WebView2, das Windows 10
und 11 mitbringen.  Fehlt es, bietet der Launcher an, es zu installieren --
sonst öffnet er sich im Browser.
Beim ersten Eröffnen eines Mehrspieler-Spiels fragt Windows vielleicht, ob
das Spiel im Netz erreichbar sein darf: für das private Netz erlauben.
Direkt spielen ohne Launcher: OpenMTV.cmd --direct (oder mit Optionen,
z. B. OpenMTV.cmd d für Deutsch); alle Optionen: OpenMTV.cmd -h""",
            """Nothing to install: the window uses WebView2, which comes with Windows 10
and 11.  Where it is missing, the launcher offers to install it -- or opens
in the browser.
The first time a multiplayer game is opened, Windows may ask whether the
game may be reached in the network: allow it for private networks.
To play without the launcher: OpenMTV.cmd --direct (or with options,
e.g. OpenMTV.cmd e); all the options: OpenMTV.cmd -h"""),
    'mac': ("""OpenMTV ist nicht von Apple beglaubigt.  Beim ersten Öffnen meldet macOS,
dass es den Entwickler nicht prüfen kann: dann in den Systemeinstellungen
unter Datenschutz & Sicherheit "Dennoch öffnen" wählen (älteres macOS:
Rechtsklick auf OpenMTV.app, "Öffnen").  Danach startet es normal.
Oder vorher im Terminal: xattr -dr com.apple.quarantine (und OpenMTV.app
ins Terminal-Fenster ziehen), dann fragt macOS gar nicht.
Für macOS 11 und neuer, Apple-Prozessor (M1 und später).""",
            """OpenMTV is not notarized by Apple.  The first time it opens, macOS says it
cannot check the developer: choose "Open Anyway" in System Settings,
Privacy & Security (older macOS: right-click OpenMTV.app, "Open").  After
that it starts like any program.  Or first, in the Terminal: xattr -dr
com.apple.quarantine (and drag OpenMTV.app into the window): macOS asks nothing.
For macOS 11 and later, Apple silicon (M1 and later)."""),
    'linux': ("""Das Spiel braucht nichts außer der C-Bibliothek (glibc 2.34 oder neuer).
Das eigene Fenster des Launchers braucht WebKitGTK 4.1 (Debian/Ubuntu:
apt install libwebkit2gtk-4.1-0) -- ohne öffnet er sich im Browser.
Direkt spielen ohne Launcher: ./openmtv --direct; alle Optionen: ./openmtv -h""",
              """The game needs nothing but the C library (glibc 2.34 or newer).  The
launcher's own window needs WebKitGTK 4.1 (Debian/Ubuntu: apt install
libwebkit2gtk-4.1-0) -- without it, it opens in the browser.
To play without the launcher: ./openmtv --direct; all the options: ./openmtv -h"""),
}
README_DE = """OpenMTV -- Mad TV (Rainbow Arts, 1991), neu übersetzt

1. Die Originaldateien SOURCE1.RSC und SOURCE2.RSC aus deinem Mad TV in
   diesen Ordner legen.
2. {start}: der Launcher öffnet sich in einem eigenen Fenster -- Inhalte,
   Einstellungen, Spielen, Mehrspieler, und der Inhalts-Editor.  Beim
   ersten Start ohne die Originaldateien fragt er nach ihrem Ordner und
   merkt ihn sich.

{notes}

Spielstände liegen in decomp/run und decomp/build/mods: beim Ersetzen einer
alten Version diese Ordner (und eigene Pakete in decomp/mods) behalten.

Lizenzen: decomp/third_party/ -- die Soundchip-Emulation Nuked-OPL3
(LGPL-2.1: eine eigene Bibliothek neben dem Spiel, opl3.dll / libopl3, mit
ihrem Quelltext -- austauschbar gegen eine eigene Übersetzung), SDL2 (zlib), Gos Bibliothek und die Go-Module des Launchers
(BSD, MIT u. a.); die Schriften decomp/runtime/notice_font_OFL.txt,
decomp/tools/editor/fonts/OFL.txt; die Schnittstelle von ONNX Runtime (MIT,
decomp/third_party/onnxruntime).  Für das KI-Upscaling lädt der Launcher erst
auf Wunsch ein Modell (CC BY-NC-SA 4.0) und ONNX Runtime von openmtv.net,
Urheber und Lizenzen: https://openmtv.net/ki/1/README.txt.
Version: {version}
"""
README_EN = """OpenMTV -- Mad TV (Rainbow Arts, 1991), translated anew

1. Put the original game files SOURCE1.RSC and SOURCE2.RSC from your Mad TV
   into this folder.
2. {start}: the launcher opens in a window of its own -- contents,
   settings, play, multiplayer, and the content editor.  On the first start
   without the original files it asks for their folder and keeps it.

{notes}

Saved games live in decomp/run and decomp/build/mods: keep those folders
(and packs of your own in decomp/mods) when replacing an old version.

Licences: decomp/third_party/ -- the sound chip emulator Nuked-OPL3
(LGPL-2.1: a library of its own beside the game, opl3.dll / libopl3, with its
source -- you may replace it with your own build), SDL2 (zlib), Go's library and the launcher's Go modules (BSD,
MIT and others); the fonts decomp/runtime/notice_font_OFL.txt,
decomp/tools/editor/fonts/OFL.txt; ONNX Runtime's interface (MIT,
decomp/third_party/onnxruntime).  For AI upscaling the launcher loads a
model (CC BY-NC-SA 4.0) and ONNX Runtime from openmtv.net only when asked;
authors and licences: https://openmtv.net/ki/1/README.txt.
Version: {version}
"""
OPENMTV_CMD = ('@echo off\r\nrem OpenMTV: the launcher (decomp\\play.cmd; with arguments the game at once; -h: the options)\r\n'
               'setlocal\r\nset MADTV_CMD=OpenMTV.cmd\r\n"%~dp0decomp\\play.cmd" %*\r\n')   # (not "call": it would take /? itself)
OPENMTV_SH = ('#!/bin/sh\n# OpenMTV: the launcher (decomp/play.sh; with arguments the game at once; -h: the options)\n'
              'exec "$(dirname "$0")/decomp/play.sh" "$@"\n')


def in_git():
    """ROOT is a git checkout of its own -- not a copy made by git archive (tools/release.py's
    build/release/V/src, the Mac's ~/madtv-decomp/openmtv-pkg), which may lie inside another one"""
    r = subprocess.run(['git', '-C', ROOT, 'rev-parse', '--show-toplevel'], capture_output=True, text=True)
    return r.returncode == 0 and os.path.realpath(r.stdout.strip()) == os.path.realpath(ROOT)


def version():
    """git's version -- or build/COMMIT.txt in a copy made by git archive"""
    if in_git():
        return subprocess.run(['git', '-C', ROOT, 'rev-parse', '--short=12', 'HEAD'], capture_output=True,
                              text=True, check=True).stdout.strip()
    try:
        return open(os.path.join(ROOT, 'build', 'COMMIT.txt')).read().strip()
    except OSError:
        sys.exit('package: no version here (no git, no build/COMMIT.txt)')


def files_of(rel):
    """[(path below decomp/, path here)] for a file or every file below a directory"""
    src = os.path.join(ROOT, rel)
    if os.path.isfile(src):
        return [(rel.replace(os.sep, '/'), src)]
    out = []
    for dp, dns, fns in os.walk(src):
        dns[:] = sorted(x for x in dns if x != '__pycache__')
        for fn in sorted(fns):
            p = os.path.join(dp, fn)
            if fn.upper() not in NEVER:
                out.append((os.path.relpath(p, ROOT).replace(os.sep, '/'), p))
    return out


def tracked(rel):
    """[(path below decomp/, path here)] for the files below rel that are in git: no packs of
    the player's own here, no self-test's pack, no caches"""
    if not in_git():                    # (a copy by git archive: it holds only git's files)
        if not os.path.exists(os.path.join(ROOT, 'build', 'COMMIT.txt')):
            sys.exit('package: not a git checkout, and no build/COMMIT.txt (%s)' % rel)
        return files_of(rel)
    out = subprocess.run(['git', '-C', ROOT, 'ls-files', '-z', '--', rel], capture_output=True,
                         check=True).stdout.decode('utf-8')
    return [(r, os.path.join(ROOT, r)) for r in sorted(out.split('\0'))
            if r and os.path.isfile(os.path.join(ROOT, r))]


def need(*pairs):
    for rel, how in pairs:
        if not os.path.exists(os.path.join(ROOT, rel)):
            sys.exit('package: %s missing (%s)' % (rel, how))


def go_licence():
    """Go's licence (its library is in the launcher): GOROOT/LICENSE"""
    try:
        goroot = subprocess.run([os.environ.get('GO', 'go'), 'env', 'GOROOT'], capture_output=True, text=True,
                                check=True).stdout.strip()
        with open(os.path.join(goroot, 'LICENSE'), 'rb') as f:
            return f.read()
    except (OSError, subprocess.CalledProcessError) as e:
        sys.exit("package: Go's licence: %s" % e)


def go_modules(exe):
    """the licences of the Go modules built into EXE (go version -m: Wails and the ones it
    takes) -> [(path below decomp/, contents)]; a module without one stops"""
    go = os.environ.get('GO', 'go')
    try:
        info = subprocess.run([go, 'version', '-m', exe], capture_output=True, text=True, check=True).stdout
        mods = [f[1] for f in (line.split() for line in info.splitlines()) if len(f) > 2 and f[0] == 'dep']
        dirs = subprocess.run([go, 'list', '-m', '-f', '{{.Path}} {{.Dir}}'] + mods, capture_output=True, text=True,
                              check=True, cwd=os.path.join(ROOT, 'launcher')).stdout if mods else ''
    except (OSError, subprocess.CalledProcessError) as e:
        sys.exit("package: the Go modules in %s: %s" % (os.path.relpath(exe, ROOT), e))
    out = []
    for line in dirs.splitlines():
        mod, d = (line.split(' ', 1) + [''])[:2]
        if not d.strip():                     # (-mod=vendor, the Mac's way: the vendored copy)
            d = os.path.join(ROOT, 'launcher', 'vendor', mod)
        names = sorted(n for n in os.listdir(d) if n.upper().split('.')[0] in ('LICENSE', 'LICENCE', 'COPYING'))
        if not names:
            sys.exit('package: the Go module %s has no licence file (%s)' % (mod, d))
        for n in names:
            with open(os.path.join(d, n), 'rb') as f:
                out.append(('third_party/go/modules/%s/%s' % (mod, n), f.read()))
    return out


def sdl2_licence():
    """SDL2's licence (zlib), for every package: LICENSE.txt of the source release
    (tools/sdl2_static.py fetches it; the same 2.x release as the Windows DLL)"""
    sys.path.insert(0, HERE)
    import sdl2_static
    with tarfile.open(sdl2_static.fetch()) as t:
        return t.extractfile('SDL2-%s/LICENSE.txt' % sdl2_static.VERSION).read()


def stripped(src, tag, strip_cmd):
    """SRC without its debug information; the full one kept as build/package/mtv-dec-VERSION-TAG"""
    os.makedirs(OUT, exist_ok=True)
    keep = os.path.join(OUT, 'mtv-dec-%s-%s%s' % (version(), tag, os.path.splitext(src)[1]))
    shutil.copyfile(src, keep)
    tmp = keep + '.stripped.part'
    shutil.copyfile(src, tmp)
    try:
        subprocess.run(strip_cmd + [tmp], check=True)
        if sys.platform == 'darwin':          # (arm64 code must be signed; strip took the linker's ad hoc one)
            subprocess.run(['codesign', '--force', '-s', '-', tmp], check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError) as e:
        sys.exit('package: %s: %s' % (' '.join(strip_cmd), e))
    data = open(tmp, 'rb').read()
    os.remove(tmp)
    return data


def window_build(exe):
    """EXE is the launcher's window build (make launcher-win), a program of the window subsystem"""
    with open(exe, 'rb') as f:
        head = f.read(4096)
    pe = int.from_bytes(head[0x3c:0x40], 'little')
    subsystem = int.from_bytes(head[pe + 24 + 68:pe + 24 + 70], 'little')  # (the optional header's Subsystem)
    if head[pe:pe + 4] != b'PE\0\0' or subsystem != 2:
        sys.exit('package: build/openmtv.exe is not the window build (make launcher-win: -H windowsgui)')


def opl3_library(exe, lib):
    """the game takes the sound chip's emulator from LIB beside it (OPL3_SHARED=1): LGPL-2.1"""
    if not os.path.exists(os.path.join(ROOT, lib)):
        sys.exit('package: %s missing -- build the game with OPL3_SHARED=1 (see -h)' % lib)
    if os.path.basename(lib).encode() not in open(exe, 'rb').read():
        sys.exit('package: %s does not load %s -- build it with OPL3_SHARED=1 (rm it first)'
                 % (os.path.relpath(exe, ROOT), os.path.basename(lib)))


def no_sdl_library(exe):
    """the Linux/macOS game must carry SDL2 itself (SDL2_STATIC=1), not ask the system for it"""
    tool = ['otool', '-L', exe] if sys.platform == 'darwin' else ['ldd', exe]
    libs = subprocess.run(tool, capture_output=True, text=True).stdout
    if 'SDL' in libs:
        sys.exit('package: %s takes SDL2 from the system -- build it with SDL2_STATIC=1 (see -h)'
                 % os.path.relpath(exe, ROOT))


def common(launcher_exe):
    """[(path below decomp/, bytes, executable)] that every package has"""
    v = version()
    items = [('build/COMMIT.txt', (v + '\n').encode(), False),
             ('third_party/go/LICENSE', go_licence(), False)]
    items += [(rel, data, False) for rel, data in go_modules(launcher_exe)]
    for rel, src in (files_of('runtime/notice_font_OFL.txt') + files_of('tools/editor/fonts/OFL.txt')
                     + files_of('docs/CHANGES.md') + files_of('third_party/nuked-opl3')
                     + files_of('third_party/onnxruntime/LICENSE') + files_of('third_party/onnxruntime/README.md')):
        items.append((rel, open(src, 'rb').read(), False))
    return items


def readmes(system, crlf=False):
    v = version()
    de = README_DE.format(start=START[system][0], notes=NOTES[system][0], version=v)
    en = README_EN.format(start=START[system][1], notes=NOTES[system][1], version=v)
    if crlf:
        de, en = de.replace('\n', '\r\n'), en.replace('\n', '\r\n')
    return [('LIESMICH.txt', de.encode('utf-8')), ('README.txt', en.encode('utf-8'))]


def finish(part, out, n):
    os.replace(part, out)
    print('%s: %d files, %.1f MB (version %s)' % (os.path.relpath(out, ROOT), n, os.path.getsize(out) / 1e6, version()))
    return out


def win():
    need(('build/mtv-dec.exe', 'make win'), ('build/SDL2.dll', 'make win'), ('build/openmtv.exe', 'make launcher-win'))
    launcher = os.path.join(ROOT, 'build', 'openmtv.exe')
    window_build(launcher)
    opl3_library(os.path.join(ROOT, 'build', 'mtv-dec.exe'), 'build/opl3.dll')
    items = common(launcher)
    items += [('build/mtv-dec.exe', stripped(os.path.join(ROOT, 'build', 'mtv-dec.exe'), 'win64', [WIN_STRIP]), True),
              ('third_party/sdl2/LICENSE.txt', sdl2_licence(), False)]
    for rel, src in files_of('build/SDL2.dll') + files_of('build/opl3.dll') + files_of('build/openmtv.exe') + files_of('play.cmd'):
        data = open(src, 'rb').read()
        if rel.endswith('.cmd'):            # batch files: CRLF (.gitattributes does it for a checkout)
            data = data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        items.append((rel, data, True))
    out = os.path.join(OUT, 'OpenMTV-win64-%s.zip' % version())
    os.makedirs(OUT, exist_ok=True)
    with zipfile.ZipFile(out + '.part', 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(TOP + 'OpenMTV.cmd', OPENMTV_CMD)
        for name, data in readmes('win', crlf=True):
            z.writestr(TOP + name, data)
        for rel, data, _ in items:
            z.writestr(TOP + 'decomp/' + rel, data)
        n = len(z.namelist())
    return finish(out + '.part', out, n)


def linux():
    need(('build/mtv-dec', 'see -h'), ('build/openmtv', 'make launcher'), ('build/openmtv-native', 'make launcher-native'))
    game = os.path.join(ROOT, 'build', 'mtv-dec')
    no_sdl_library(game)
    opl3_library(game, 'build/libopl3.so')
    items = common(os.path.join(ROOT, 'build', 'openmtv-native'))
    items += [('build/mtv-dec', stripped(game, 'linux-x64', ['strip', '--strip-debug']), True),
              ('third_party/sdl2/LICENSE.txt', sdl2_licence(), False)]
    for rel, src in files_of('build/openmtv') + files_of('build/openmtv-native') + files_of('build/libopl3.so') + files_of('play.sh'):
        items.append((rel, open(src, 'rb').read(), True))
    out = os.path.join(OUT, 'OpenMTV-linux-x64-%s.tar.gz' % version())
    os.makedirs(OUT, exist_ok=True)
    entries = [(TOP + 'openmtv', OPENMTV_SH.encode(), True)] + [(TOP + n, d, False) for n, d in readmes('linux')]
    entries += [(TOP + 'decomp/' + rel, data, x) for rel, data, x in items]
    with tarfile.open(out + '.part', 'w:gz') as t:
        dirs = set()
        for name, data, x in entries:
            parts = name.split('/')
            for i in range(1, len(parts)):
                d = '/'.join(parts[:i])
                if d not in dirs:
                    dirs.add(d)
                    info = tarfile.TarInfo(d)
                    info.type, info.mode = tarfile.DIRTYPE, 0o755
                    t.addfile(info)
            info = tarfile.TarInfo(name)
            info.size, info.mode = len(data), 0o755 if x else 0o644
            t.addfile(info, io.BytesIO(data))
    return finish(out + '.part', out, len(entries))


def mac():
    if sys.platform != 'darwin':
        sys.exit('package: the macOS package is made on the Mac (codesign, ditto)')
    need(('build/mtv-dec', 'see -h'), ('build/OpenMTV.app', 'make launcher-native-app'))
    game = os.path.join(ROOT, 'build', 'mtv-dec')
    no_sdl_library(game)
    opl3_library(game, 'build/libopl3.dylib')
    app = os.path.join(ROOT, 'build', 'OpenMTV.app')
    items = common(os.path.join(app, 'Contents', 'MacOS', 'openmtv'))
    items += [('build/mtv-dec', stripped(game, 'macos-arm64', ['strip', '-S']), True),
              ('third_party/sdl2/LICENSE.txt', sdl2_licence(), False)]
    for rel, src in files_of('play.sh') + files_of('build/libopl3.dylib'):
        items.append((rel, open(src, 'rb').read(), True))
    stage = os.path.join(OUT, 'mac-stage')
    shutil.rmtree(stage, ignore_errors=True)
    top = os.path.join(stage, TOP.rstrip('/'))
    for name, data in readmes('mac'):
        os.makedirs(top, exist_ok=True)
        open(os.path.join(top, name), 'wb').write(data)
    for rel, data, x in items:
        p = os.path.join(top, 'decomp', rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(data)
        os.chmod(p, 0o755 if x else 0o644)
    # the application at the top, beside decomp/ (launcher/main.go findRoot): the player's double-click
    subprocess.run(['ditto', app, os.path.join(top, 'OpenMTV.app')], check=True)
    out = os.path.join(OUT, 'OpenMTV-macos-arm64-%s.zip' % version())
    if os.path.exists(out + '.part'):
        os.remove(out + '.part')
    subprocess.run(['ditto', '-c', '-k', '--keepParent', top, out + '.part'], check=True, cwd=stage)
    n = sum(len(fns) for _, _, fns in os.walk(top))
    shutil.rmtree(stage, ignore_errors=True)
    return finish(out + '.part', out, n)


def packs():
    """the content packs in git, for the developer's own machines only -- never on the page:
    unpacked over an installed OpenMTV (OpenMTV/decomp/mods/NAME/, the packages' layout)"""
    items = tracked('mods')
    if not items:
        sys.exit('package: no content packs in git (mods/)')
    out = os.path.join(OUT, 'inhaltspakete-%s.zip' % version())
    os.makedirs(OUT, exist_ok=True)
    with zipfile.ZipFile(out + '.part', 'w', zipfile.ZIP_DEFLATED) as z:
        for rel, src in items:
            z.writestr(TOP + 'decomp/' + rel, open(src, 'rb').read())
        n = len(z.namelist())
    return finish(out + '.part', out, n)


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def main():
    what = sys.argv[1:]
    if what in (['win'], ['linux'], ['mac'], ['packs']):
        out = {'win': win, 'linux': linux, 'mac': mac, 'packs': packs}[what[0]]()
        print('sha256 %s' % sha256(out))
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == '__main__':
    main()
