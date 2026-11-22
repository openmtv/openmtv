#!/usr/bin/env python3
"""SDL2 for iOS: a static library (arm64), for the iPhone/iPad build (make ios, docs/ios.md).

    python3 tools/sdl2_ios.py [DIR]      -> DIR/lib/libSDL2.a, DIR/include/SDL2/
                                            (default build/ios/sdl2)

The same source release as the Linux and macOS packages (tools/sdl2_static.py
fetches it and checks its SHA-256), built with CMake for iOS: on the Mac, with
Xcode and CMake (brew install cmake).  ios/sdl2-uiscene.patch goes on first:
apps built with the iOS 27 SDK must use the UIScene life cycle, which SDL2 never
got (SDL3 did, b46e26e and later); without it the app stops at launch.  The app's entry is ios/ios_main.c (SDL's
SDL_uikit_main.c), so SDL2main is not needed.  Nothing happens when DIR/VERSION
names this release already.  Standard library only."""
import hashlib
import os
import shutil
import subprocess
import sys
import tarfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sdl2_static as base  # noqa: E402

IOS_MIN = os.environ.get('IOS_MIN', '17.0')
PATCH = os.path.join(base.ROOT, 'ios', 'sdl2-uiscene.patch')


def main():
    if sys.platform != 'darwin':
        sys.exit('sdl2_ios: only on the Mac (Xcode)')
    if not shutil.which('cmake'):
        sys.exit('sdl2_ios: CMake is missing -- brew install cmake')
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(base.ROOT, 'build', 'ios', 'sdl2'))
    stamp = os.path.join(out, 'VERSION')
    with open(PATCH, 'rb') as f:
        want = '%s ios %s patch %s' % (base.VERSION, IOS_MIN, hashlib.sha256(f.read()).hexdigest()[:12])
    if os.path.exists(stamp) and open(stamp).read().strip() == want \
            and os.path.exists(os.path.join(out, 'lib', 'libSDL2.a')):
        return
    tgz = base.fetch()
    work = out + '.work'
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    with tarfile.open(tgz) as t:
        t.extractall(work, filter='data') if hasattr(tarfile, 'data_filter') else t.extractall(work)
    src = os.path.join(work, 'SDL2-%s' % base.VERSION)
    r = subprocess.run(['patch', '-p1', '-s', '-d', src, '-i', PATCH], stderr=subprocess.PIPE, text=True)
    if r.returncode:
        sys.exit('sdl2_ios: %s does not apply:\n%s' % (os.path.relpath(PATCH, base.ROOT), r.stderr))
    bld = os.path.join(work, 'build')
    shutil.rmtree(out, ignore_errors=True)
    jobs = str(os.cpu_count() or 4)
    steps = [
        ['cmake', '-S', src, '-B', bld,
         '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_ARCHITECTURES=arm64',
         '-DCMAKE_OSX_DEPLOYMENT_TARGET=' + IOS_MIN, '-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_INSTALL_PREFIX=' + out,
         '-DSDL_SHARED=OFF', '-DSDL_STATIC=ON', '-DSDL_TEST=OFF', '-DSDL_TESTS=OFF',
         # CMake 4 refuses projects that allow versions below 3.5 (SDL 2: 3.0...3.10)
         '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'],
        ['cmake', '--build', bld, '-j', jobs],
        ['cmake', '--install', bld],
    ]
    for cmd in steps:
        r = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if r.returncode:
            sys.exit('sdl2_ios: %s failed:\n%s' % (' '.join(cmd[:3]), r.stderr[-3000:]))
    if not os.path.exists(os.path.join(out, 'lib', 'libSDL2.a')):
        sys.exit('sdl2_ios: no lib/libSDL2.a in %s' % out)
    with open(stamp, 'w') as f:
        f.write(want + '\n')
    shutil.rmtree(work, ignore_errors=True)
    print('sdl2_ios: SDL %s for iOS %s+ in %s' % (base.VERSION, IOS_MIN, os.path.relpath(out, base.ROOT)))


if __name__ == '__main__':
    main()
