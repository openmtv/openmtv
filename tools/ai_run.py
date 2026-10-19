#!/usr/bin/env python3
"""Long computer-player run: the human idles while the two computer players
play, one memory dump per game day, statistics as CSV.

    tools/ai_run.py DAYS OUTDIR [--bin build/mtv-dec] [--difficulty 1|2|3] [--no-clamp] [--stackchk]
                    [--fixes[=LIST]] [--human 1|2|3] [--offset N] [--speed 20|30|60|120]
                    [--human-plan early|steady|FILE] [--ai-improve off|normal|strong] [--ad-offers N] [--film-offers N]

Starts a new game (English, human = player 1, or the station --human names), presses Esc every 300 frames
so dialogs that stop the clock (the boss in the morning) are dismissed, and
-- unless --no-clamp -- holds the human's image at >= 20 and money at >= 0,
so the human is never fired and never visited by the bailiff (test harness:
this changes the game; the opponents still compete normally).  Uses fresh copies of the original game files from ../ (or $GAME).

Output: OUTDIR/stats.csv (tools/stats.py), OUTDIR/run.txt (script),
OUTDIR/log.txt; the per-day dumps are deleted unless --keep-dumps.
--stackchk runs the reference interpreter (slow, about a minute per game day)
and lists in log.txt every read of a never-written local ([uninit] lines).
--fixes plays with the original-bug fixes (decompiled build; build/mtv-dec
--fixes=list names them).  --offset N shifts the Esc presses by N frames
and the setup's clicks by N mod 100 (their time seeds the game's randomness):
another game (the computer players' random draws depend on the time), for
comparisons over several runs.  --speed sets the
game speed after the start (test poke of g_game_speed; 120 is the fourth
speed, with --fourth-speed), the run's length in frames scaled to it.

--human-plan gives the human transmitters on fixed days, as a good player
would build them (the AI improvement's tests, docs/ki-verbesserung.md): the
script's day lines "@DAY.HOUR txbuy PLAYER MAXFEE" (runtime/host.c) buy the
site with the most new viewers in a price class up to MAXFEE -- the same
sites in every run, they depend only on the human's own network.  Plans:
early (day 1 two transmitters of up to $100 000, then one every 2-3 days),
steady (one every 4-5 days), satellite (early, and the west satellite on
day 9, the east one on day 15 -- a play-through of 2026-10-13), or a FILE of
lines "DAY[.HOUR] MAXFEE" (HOUR 0..7 = 17:00..00:00; MAXFEE -1 / -2: the
west / east satellite; the plans buy at 18:00).  The money is clamped as
always, so the plan never runs out of it.  --ai-improve plays with the
option (build/mtv-dec --ai-improve), --ad-offers N with the ad agency's
N offers, --film-offers N with the film dealer's (10..17, build/mtv-dec).
"""
import os, sys, shutil, subprocess, argparse

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
FRAMES_PER_DAY = 69500           # measured: 17:00..01:00 at the default game speed
HUMAN_IMAGE = 0x8BDC + 1 * 0x17 + 0x15     # pl_image(1); + (n - 1) * 0x17 for player n
HUMAN_MONEY_HI = 0x8BDC + 1 * 0x17 + 0xA   # high word of pl_money(1)
DAY = 0x71E4                     # g_day
SPEED = 0x72A2                   # g_game_speed


def human_plan(name, days):
    """[(day, hour, maxfee)] of a plan (see the module's text)"""
    if name == 'satellite':
        return sorted(human_plan('early', days) + [(d, 1, -k) for d, k in ((9, 1), (15, 2)) if d <= days])
    if name == 'early':
        plan, d, step = [(1, 1, 100000), (1, 1, 100000)], 3, 0
        while d <= days:
            plan.append((d, 1, 100000))
            d += 2 + step
            step ^= 1
        return plan
    if name == 'steady':
        plan, d, step = [], 4, 0
        while d <= days:
            plan.append((d, 1, 100000))
            d += 4 + step
            step ^= 1
        return plan
    plan = []
    for line in open(name):
        f = line.split('#')[0].split()
        if not f:
            continue
        dh = f[0].split('.')
        plan.append((int(dh[0]), int(dh[1]) if len(dh) > 1 else 1, int(f[1])))
    return sorted(plan)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('days', type=int)
    ap.add_argument('outdir')
    ap.add_argument('--bin', default=os.path.join(ROOT, 'build', 'mtv-dec'))
    ap.add_argument('--difficulty', type=int, default=2)
    ap.add_argument('--no-clamp', action='store_true')
    ap.add_argument('--keep-dumps', action='store_true')
    ap.add_argument('--stackchk', action='store_true')
    ap.add_argument('--fixes', nargs='?', const='all', default=None)
    ap.add_argument('--human', type=int, default=1, choices=(1, 2, 3))
    ap.add_argument('--offset', type=int, default=0)
    ap.add_argument('--speed', type=int, default=30, choices=(20, 30, 60, 120))
    ap.add_argument('--human-plan', default=None)
    ap.add_argument('--ai-improve', default=None, choices=('off', 'normal', 'strong'))
    ap.add_argument('--ad-offers', type=int, default=None, choices=range(10, 18), metavar='10..17')
    ap.add_argument('--film-offers', type=int, default=None, choices=range(10, 18), metavar='10..17')
    a = ap.parse_args()
    game = os.environ.get('GAME', os.path.join(ROOT, '..'))
    out = os.path.abspath(a.outdir)
    if os.path.exists(out):
        shutil.rmtree(out)
    gdir = os.path.join(out, 'game')
    dumps = os.path.join(out, 'dumps')
    os.makedirs(gdir)
    os.makedirs(dumps)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(game, f), gdir)
    shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), gdir)

    end = 2000 + a.days * FRAMES_PER_DAY * 30 // a.speed
    x = a.offset % 100                  # the setup's time seeds the game's randomness (g_cursor_sprites[6])
    lines = [(400, 'key esc'), (1000 + x, 'click 64 100')]
    if a.human != 1:                    # "Computer" under the station: the human plays it
        lines.append((900 + x, 'click %d 64' % (64 + 96 * (a.human - 1))))
    if a.difficulty != 2:               # Easy / Difficult buttons on the setup screen
        lines.append((1150 + x, 'click %d 81' % (64 if a.difficulty == 1 else 256)))
    lines.append((1300 + x, 'click 172 185'))
    if a.speed != 30:
        lines.append((1500, 'poke16 39D4:%04X %d' % (SPEED, a.speed)))
    lines += [(f, 'key esc') for f in range(2000 + a.offset, end, 300)]
    lines.append((end, 'quit'))
    lines.sort(key=lambda x: x[0])
    script = os.path.join(out, 'run.txt')
    with open(script, 'w') as f:
        f.write('# tools/ai_run.py %d days, difficulty %d%s%s%s%s%s%s%s%s\n' % (a.days, a.difficulty, '' if a.no_clamp else ', human image >= 20 and money >= 0 (clamped)',
                                                           ', fixes ' + a.fixes if a.fixes else '',
                                                           ', human = station %d' % a.human if a.human != 1 else '',
                                                           ', speed %d' % a.speed if a.speed != 30 else '',
                                                           ', human plan ' + a.human_plan if a.human_plan else '',
                                                           ', ai-improve ' + a.ai_improve if a.ai_improve else '',
                                                           ', ad offers %d' % a.ad_offers if a.ad_offers else '',
                                                           ', film offers %d' % a.film_offers if a.film_offers else ''))
        f.writelines('%d %s\n' % l for l in lines)
        if a.human_plan:
            f.writelines('@%d.%d txbuy %d %d\n' % (d, h, a.human, fee) for d, h, fee in human_plan(a.human_plan, a.days))

    cmd = [a.bin, '--headless', '--fast', '--nosound', '--game', gdir, '--args', 'e',
           '--script', script, '--dump-on', '39D4:%04X:%s' % (DAY, dumps)]
    if not a.no_clamp:
        cmd += ['--clamp', '39D4:%04X:20' % (HUMAN_IMAGE + (a.human - 1) * 0x17)]   # checks happen within a frame: keep a margin above 5
        # no debt: a bailiff visiting a library that holds only series hangs
        # the original game (1DA8:2D3B, see names/FINDINGS.md)
        cmd += ['--clamp', '39D4:%04X:0' % (HUMAN_MONEY_HI + (a.human - 1) * 0x17)]
    if a.stackchk:
        cmd.append('--stackchk')
    if a.fixes:
        cmd.append('--fixes=' + a.fixes)
    if a.speed == 120:
        cmd.append('--fourth-speed')
    if a.ai_improve:
        cmd.append('--ai-improve=' + a.ai_improve)
    if a.ad_offers:
        cmd.append('--ad-offers=%d' % a.ad_offers)
    if a.film_offers:
        cmd.append('--film-offers=%d' % a.film_offers)
    with open(os.path.join(out, 'log.txt'), 'w') as log:
        subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=False)
    with open(os.path.join(out, 'stats.csv'), 'w') as f:
        subprocess.run([sys.executable, os.path.join(HERE, 'stats.py'), dumps], stdout=f, check=True)
    if not a.keep_dumps:
        for fn in os.listdir(dumps):
            if fn.endswith('.bin'):
                os.remove(os.path.join(dumps, fn))
    shutil.rmtree(gdir)
    print(open(os.path.join(out, 'stats.csv')).read().count('\n') - 1, 'days ->', os.path.join(out, 'stats.csv'))


if __name__ == '__main__':
    main()
