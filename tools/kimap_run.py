#!/usr/bin/env python3
"""The recordings for tools/hrai.py parts (runtime/ki_parts.txt, docs/ki-bilder.md):
every scenario and the AI day with the recorder HR_KIMAP (src/hr.c), each run with
its own copy of the game files ($GAME, else run/).

  kimap_run.py OUT [JOBS]      OUT/NAME.map each, JOBS at once (3); then
  tools/hrai.py parts runtime/ki_parts.txt OUT/*.map
"""
import os, sys, glob, shutil, subprocess, tempfile, re
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, 'run'))
if len(sys.argv) not in (2, 3):
    sys.exit(__doc__)
OUT, JOBS = os.path.abspath(sys.argv[1]), int(sys.argv[2]) if len(sys.argv) > 2 else 3
os.makedirs(OUT, exist_ok=True)
scripts = sorted(glob.glob(ROOT + '/tests/scenarios/*.txt')) + [ROOT + '/tests/aiday.txt']
scripts = [s for s in scripts if not s.endswith('README.txt')]
def lang(path):
    for l in open(path):
        m = re.search(r'--args "?([de])', l)
        if m: return m.group(1)
    return 'd' if os.path.basename(path).startswith('german') else 'e'
def run(path):
    name = os.path.basename(path)[:-4]
    tmp = tempfile.mkdtemp(prefix='kimap_')
    game = os.path.join(tmp, 'game'); os.makedirs(game)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'): shutil.copy(os.path.join(GAME, f), game)
    out = os.path.join(OUT, name + '.map')
    p = subprocess.run([ROOT + '/build/mtv-dec', '--headless', '--nosound', '--logical-time', '--game', game, '--args', lang(path),
                        '--script', path, '--filter', 'hr-pixel'], capture_output=True, text=True, env=dict(os.environ, HR_KIMAP=out), timeout=1800)
    shutil.rmtree(tmp, ignore_errors=True)
    return name, p.returncode, os.path.exists(out)
with ThreadPoolExecutor(JOBS) as ex:
    res = list(ex.map(run, scripts))
print(sum(1 for r in res if r[2]), 'of', len(res), 'maps;', [r for r in res if not r[2]])
