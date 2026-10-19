"""The page's self-tests inside the launcher's own window (launcher/window_wails.go,
docs/launcher-fenster.md) -- in the system's web view (WebKitGTK, WebView2, WKWebView)
instead of Chrome: a fresh root as tools/launchertest.py makes it with EXE as its
build/openmtv, MADTV_WINDOW_TEST=URL, the result read from the "TITLE ..." lines the
window prints.  It needs a desktop: Linux an X display (Xvfb will do), Windows the
user's session (not an ssh one: WebView2 cannot make its window there).

    python3 tools/windowtest.py [--exe build/openmtv-native] [URL ...]

(the default: build/openmtv-native; on Windows build/openmtv.exe, make launcher-win)"""
import os, shutil, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import launchertest as lt

argv = sys.argv[1:]
exe = os.path.join(lt.ROOT, 'build', 'openmtv.exe' if lt.WIN else 'openmtv-native')
if '--exe' in argv:
    k = argv.index('--exe')
    exe = os.path.abspath(argv[k + 1])
    del argv[k:k + 2]
if not os.path.exists(exe):
    sys.exit('windowtest: %s is not built (make launcher-native)' % exe)
urls = argv or ['/?selftest&player#launcher', '/?selftest&player&ui=de#launcher', '/?selftest#launcher', '/?selftest&ui=de#launcher']

base = os.path.realpath(tempfile.mkdtemp(prefix='wintest-'))
game = os.path.join(base, 'game')
os.makedirs(game)
for f in lt.GAME_FILES:
    shutil.copy(os.path.join(lt.original_dir(), f), game)
root = os.path.join(base, 'root')
lt.make_root(root, 'go', lt.test_packs(), exe)
ok_all = True
for k, url in enumerate(urls):
    for f in ('selftest-settings.ini', 'selftest-multiplayer.ini'):
        p = os.path.join(root, 'build', f)
        if os.path.exists(p):
            os.remove(p)
    env = dict(os.environ)
    for d in ('cfg', 'data', 'cache'):
        os.makedirs(os.path.join(base, '%s%d' % (d, k)))
    env.update({'XDG_CONFIG_HOME': os.path.join(base, 'cfg%d' % k), 'XDG_DATA_HOME': os.path.join(base, 'data%d' % k),
                'XDG_CACHE_HOME': os.path.join(base, 'cache%d' % k), 'APPDATA': os.path.join(base, 'cfg%d' % k),
                'BROWSER': 'cmd /c rem' if lt.WIN else 'true', 'MADTV_WINDOW_TEST': url,
                'SDL_VIDEODRIVER': 'dummy', 'SDL_AUDIODRIVER': 'dummy'})
    log = os.path.join(base, 'run%d.log' % k)
    t0 = time.time()
    with open(log, 'w') as lf:
        p = subprocess.Popen([os.path.join(root, 'build', 'openmtv' + lt.EXE), 'launcher', '--game', game], env=env, stdout=lf,
                             stderr=subprocess.STDOUT, cwd=root)
        title = None
        while time.time() - t0 < 900 and p.poll() is None:
            time.sleep(2)
            for line in open(log, encoding='utf-8', errors='replace'):
                if line.startswith('TITLE SELFTEST'):
                    title = line[6:].strip()
            if title:
                break
        p.terminate()
        try:
            p.wait(10)
        except subprocess.TimeoutExpired:
            p.kill()
    if not title:
        print('%-32s NO RESULT (%.0f s), log %s' % (url, time.time() - t0, log))
        ok_all = False
        continue
    entries = title.split('; ')
    fails = [x for x in entries if 'FAIL' in x[:20]]
    print('%-32s %s  %d checks  %.0f s' % (url, 'OK' if title.startswith('SELFTEST OK') else 'FAIL', title.count('ok '), time.time() - t0))
    for x in fails[:10]:
        print('    ' + x[:400])
    ok_all = ok_all and title.startswith('SELFTEST OK')
shutil.rmtree(base, ignore_errors=True) if ok_all else print('kept for a look:', base)
sys.exit(0 if ok_all else 1)
