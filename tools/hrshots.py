#!/usr/bin/env python3
"""Screenshots for comparing the displays: the original's 320x200 picture,
--filter mmpx and --filter hr-pixel (docs/hochaufloesend.md), at the same frame
of the same game.  Each scene is a frame of a test script (tests/scenarios,
tests/aiday.txt, tools/castfittest.py's game); one run per script with
--filter hr-pixel takes all three (shot, shothd, shothr; the display never
changes the game).  (--filter hr, its pictures smoothed, went 2026-10-18.)

  hrshots.py OUT [NAME ...]     OUT/NAME_orig.png (320x200), NAME_mmpx.png,
                                NAME_hrpix.png (1280x800, optimized),
                                OUT/shots.json
  hrshots.py --ai OUT [NAME ...]      the AI picture sets (tools/hrai.py; every
                                build/hrai/sets/V.ki, --filter hr-pixel): NAME_orig.png,
                                NAME_today.png, NAME_ai-V.png, OUT/variants.json
  hrshots.py --fonts OUT [NAME ...]   the trial fonts instead (tools/hrfont.py ttf; --filter hr-pixel):
                                NAME_orig.png, NAME_f-draft.png, NAME_f-FONT.png
                                for each font in tools/hrfont/try/, OUT/fonts.json
"""
import os, re, sys, json, shutil, subprocess, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
GAME = os.environ.get('GAME', os.path.join(ROOT, 'run'))
SCEN = os.path.join(ROOT, 'tests', 'scenarios')

# name, title (German, for the page), script, frame, language, extra options
SHOTS = [
    ('boss', 'Chef: Begrüßung', 'aiday', 1900, 'e', []),
    ('building', 'Gebäude mit Figuren', 'aiday', 6100, 'e', []),
    ('planner', 'Programmplaner', 'room_planner_schedule', 4290, 'e', []),
    ('chart', 'Statistik: Zuschauer', 'room_evening_air', 14700, 'e', []),
    ('image', 'Statistik: Image der Sender', 'room_evening_air', 15350, 'e', []),
    ('finance', 'Finanzübersicht', 'room_evening_air', 15700, 'e', []),
    ('txmap', 'Senderkarte: Abdeckung', 'room_txmap_buy', 4100, 'e', []),
    ('txmap2', 'Senderkarte: Sender kaufen', 'room_txmap_buy', 4750, 'e', []),
    ('filmcard', 'Filmhändler: Filmkarte', 'room_dealer_buy', 4050, 'e', []),
    ('seriescard', 'Filmhändler: Serienkarte', 'room_dealer_buy', 5750, 'e', ['--series-topicality=yes']),
    ('library', 'Archiv: Filmkarte', 'room_library_browse', 3300, 'e', []),
    ('episodes', 'Archiv: Folgen einer Serie', 'room_library_browse', 4900, 'e', []),
    ('adagency', 'Werbeagentur: Vertrag', 'room_adagency_sign', 4300, 'e', []),
    ('news', 'Nachrichtenstudio', 'room_news_buy', 3500, 'e', []),
    ('newsdesk', 'Nachrichten: Auswahl', 'room_news_buy', 5000, 'e', []),
    ('betty', 'Betty', 'room_betty_gift', 8200, 'e', []),
    ('betty2', 'Betty: Generalschlüssel', 'room_betty_gift', 9700, 'e', []),
    ('presents', 'Supermarkt: Geschenke', 'room_betty_gift', 5000, 'e', []),
    ('shop', 'Laden: Schauspieler', 'room_supermarket_shop', 4700, 'e', []),
    ('castfit', 'Laden: Eignung der Schauspieler', 'castfit', 6100, 'd', ['--cast-fit']),
    ('estate', 'Makler', 'room_estate_rent', 4500, 'e', []),
    ('studios', 'Makler: Studioplan', 'room_estate_rent', 6200, 'e', []),
    ('boss_de', 'Chef (deutsch)', 'german_boss_credit', 4000, 'd', []),
]


def script_lines(name, tmp):
    if name == 'castfit':
        sys.path.insert(0, HERE)
        import castfittest
        text = castfittest.SCRIPT.format(d=tmp)
    elif name == 'aiday':
        text = open(os.path.join(ROOT, 'tests', 'aiday.txt')).read()
    else:
        text = open(os.path.join(SCEN, name + '.txt')).read()
    return [l for l in text.split('\n') if l.strip() and not l.startswith('#') and not l.split()[1:2] == ['quit']]


def runs_shots():
    """the runs of each script: (filter, environment, (kind, shot command), ...)"""
    return (('hr-pixel', {}, (('orig', 'shot'), ('mmpx', 'shothd'), ('hrpix', 'shothr'))),)   # (--filter hr went 2026-10-18)


def trial_fonts():
    t = os.path.join(HERE, 'hrfont', 'try')
    return sorted(d for d in os.listdir(t) if os.path.isdir(os.path.join(t, d))) if os.path.isdir(t) else []


def runs_fonts():
    # (hr-pixel: the pictures as they are, only the text differs)
    return (('hr-pixel', {}, (('orig', 'shot'), ('f-draft', 'shothr'))),) + tuple(
        ('hr-pixel', {'HR_FONT': f}, (('f-' + f, 'shothr'),)) for f in trial_fonts())


def ai_sets():
    d = os.environ.get('HRAI_SETS', os.path.join(ROOT, 'build', 'hrai', 'sets'))
    return d, sorted(f[:-3] for f in os.listdir(d) if f.endswith('.ki')) if os.path.isdir(d) else []


def runs_ai():
    d, vs = ai_sets()
    return (('hr-pixel', {}, (('orig', 'shot'), ('today', 'shothr'))),) + tuple(
        ('hr-pixel', {'HR_KI': os.path.join(d, v + '.ki')}, (('ai-' + v, 'shothr'),)) for v in vs)


def run_script(group, out, runs):
    """all shots of one script, one run per display"""
    script, lang, extra = group[0][2], group[0][4], group[0][5]
    tmp = tempfile.mkdtemp(prefix='mtv_shots_')
    try:
        game = os.path.join(tmp, 'game')
        os.makedirs(game)
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(GAME, f), game)
        base = [l for l in script_lines(script, tmp) if not re.match(r'\d+ shot', l)]
        last = max(s[3] for s in group)
        base = [l for l in base if int(l.split()[0]) <= last]
        for filt, env, kinds in runs:
            lines = list(base)
            for s in group:
                for kind, cmd in kinds:
                    lines.append('%d %s %s/%s_%s.png' % (s[3], cmd, tmp, s[0], kind))
            lines.append('%d quit' % (last + 1))
            lines.sort(key=lambda l: int(l.split()[0]))
            path = os.path.join(tmp, 'script.txt')
            open(path, 'w').write('\n'.join(lines) + '\n')
            for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):     # (a fresh game for each run)
                shutil.copy(os.path.join(GAME, f), game)
            p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', game, '--args', lang,
                                '--script', path, '--filter', filt] + extra, capture_output=True, text=True,
                               env=dict(os.environ, **env))
        from PIL import Image
        done = []
        for s in group:
            for kind in (k for _, _, kinds in runs for k, _ in kinds):
                src = os.path.join(tmp, '%s_%s.png' % (s[0], kind))
                if not os.path.exists(src):
                    sys.exit('%s: no %s picture (exit %d)\n%s' % (s[0], kind, p.returncode, p.stderr[-1500:]))
                Image.open(src).convert('RGB').save(os.path.join(out, '%s_%s.png' % (s[0], kind)), optimize=True)
            done.append(s[0])
        return done
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    mode = sys.argv[1] if sys.argv[1] in ('--fonts', '--ai') else ''
    fonts = mode == '--fonts'
    args = sys.argv[1 + bool(mode):]
    if not args:
        sys.exit(__doc__)
    out, names = args[0], args[1:]
    runs = runs_fonts() if fonts else runs_ai() if mode == '--ai' else runs_shots()
    shots = [s for s in SHOTS if not names or s[0] in names]
    os.makedirs(out, exist_ok=True)
    groups = {}
    for s in shots:
        groups.setdefault((s[2], s[4], tuple(s[5])), []).append(s)
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        for done in ex.map(lambda g: run_script(g, out, runs), groups.values()):
            print(' '.join(done), flush=True)
    json.dump([{'name': s[0], 'title': s[1]} for s in shots], open(os.path.join(out, 'shots.json'), 'w'), ensure_ascii=False, indent=1)
    if fonts:
        json.dump(['draft'] + trial_fonts(), open(os.path.join(out, 'fonts.json'), 'w'))
    if mode == '--ai':
        json.dump(ai_sets()[1], open(os.path.join(out, 'variants.json'), 'w'))


if __name__ == '__main__':
    main()
