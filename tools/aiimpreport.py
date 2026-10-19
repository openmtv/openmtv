#!/usr/bin/env python3
"""The AI improvement's measurement (docs/ki-verbesserung.md, sections 2 and 6).

    tools/aiimpreport.py DIR [--curves FILE.json] [--csv PREFIX]

DIR holds runs of tools/ai_run.py named PLAN_LEVEL_K (e.g. early_normal_3:
--human-plan PLAN --ai-improve LEVEL, the K-th game), each with its
stats.csv and log.txt.  Printed per plan and level, over all its games:

  ratio    the stronger computer player's reach over the human's, days 5..60:
           the share of days in the level's band (normal 60-100 %, strong
           85-120 %; for "off" both bands, for comparison)
  ahead    the share of days on which a computer player's reach is above
           the human's
  waits    the waits drawn ([ai-improve] "noticed" lines) outside the level's
           range (8-20 / 4-12 played hours)
  sites    the follow-up purchases whose new viewers are 80-130 % of the gap
           (all of them: those of the site rule's second and third kind lie
           outside by design; how many there were of those: "r2+3")
  viewers  the computer players' real viewers, days 31..60: reach x
           yesterday's average rating, mean of both (millions)
  money    the computer players' cash on the last day, mean (thousands)

--curves writes the mean reach per day (human, stronger computer player,
each computer player) of every plan and level as JSON, for the report's
charts; --csv the same as PREFIX_curves.csv (plan, level, day, reach in
millions; ratio: the stronger computer player over the human, the games'
mean) and the table as PREFIX_summary.csv."""
import csv, json, os, re, sys
from collections import defaultdict

BANDS = {'normal': (0.60, 1.00), 'strong': (0.85, 1.20)}
WAITS = {'normal': (480, 1200), 'strong': (240, 720)}
RE_NOTICE = re.compile(r'\[ai-improve\] .* waits (\d+):(\d\d) h')
RE_BUY = re.compile(r'\[ai-improve\] .* bought .* rule (\d), new ([\d.]+) M for a gap of ([\d.]+) M')


def runs(d):
    out = defaultdict(list)
    for n in sorted(os.listdir(d)):
        m = re.match(r'(\w+?)_(off|normal|strong)_(\d+)$', n)
        if m and os.path.exists(os.path.join(d, n, 'stats.csv')):
            out[(m.group(1), m.group(2))].append(os.path.join(d, n))
    return out


def rows(run):
    with open(os.path.join(run, 'stats.csv')) as f:
        return [r for r in csv.DictReader(f)]


def main():
    d = sys.argv[1]
    curves_path = sys.argv[sys.argv.index('--curves') + 1] if '--curves' in sys.argv else None
    csv_prefix = sys.argv[sys.argv.index('--csv') + 1] if '--csv' in sys.argv else None
    summary = []
    groups = runs(d)
    curves = {}
    print('%-8s %-7s %5s %6s  %-13s %-13s %6s  %-6s %-12s %8s %8s %8s' % ('plan', 'level', 'games', 'days', 'in 60-100 %', 'in 85-120 %', 'ahead',
                                                                         'waits', 'sites 80-130', 'viewers', 'money', 'human'))
    for (plan, level) in sorted(groups, key=lambda k: (k[0], ['off', 'normal', 'strong'].index(k[1]))):
        n = band_n = band2_n = ahead = days = 0
        waits_bad = waits = sites_ok = sites = other_rules = 0
        real, money, hreach = [], [], []
        per_day = defaultdict(lambda: defaultdict(list))
        for run in groups[(plan, level)]:
            rs = rows(run)
            for r in rs:
                day = int(r['day'])
                h, a2, a3 = (int(r['p%d_viewers' % p]) for p in (1, 2, 3))
                per_day[day]['human'].append(h / 1e6)
                per_day[day]['ai'].append(max(a2, a3) / 1e6)
                per_day[day]['p2'].append(a2 / 1e6)
                per_day[day]['p3'].append(a3 / 1e6)
                if h:
                    per_day[day]['ratio'].append(max(a2, a3) / h)
                if 5 <= day <= 60 and h:
                    days += 1
                    q = max(a2, a3) / h
                    band_n += BANDS['normal'][0] <= q <= BANDS['normal'][1]
                    band2_n += BANDS['strong'][0] <= q <= BANDS['strong'][1]
                    ahead += max(a2, a3) > h
                if 31 <= day <= 60:
                    for p in (2, 3):
                        real.append(int(r['p%d_viewers' % p]) * int(r['p%d_avg_rating' % p]) / 100 / 1e6)
            if rs:
                money += [int(rs[-1]['p2_money']) / 1e3, int(rs[-1]['p3_money']) / 1e3]
                hreach.append(int(rs[-1]['p1_viewers']) / 1e6)
            n += 1
            with open(os.path.join(run, 'log.txt'), errors='replace') as f:
                for line in f:
                    m = RE_NOTICE.search(line)
                    if m and level in WAITS:
                        w = int(m.group(1)) * 60 + int(m.group(2))
                        waits += 1
                        waits_bad += not WAITS[level][0] <= w <= WAITS[level][1]
                    m = RE_BUY.search(line)
                    if m:
                        sites += 1
                        other_rules += m.group(1) != '1'
                        sites_ok += 0.795 <= float(m.group(2)) / max(float(m.group(3)), 0.01) <= 1.305
        pct = lambda a, b: '%5.1f %%' % (100.0 * a / b) if b else '    -'
        print('%-8s %-7s %5d %6d  %-13s %-13s %6s  %-6s %-12s %8.2f %8.0f %8.1f' % (
            plan, level, n, days, pct(band_n, days), pct(band2_n, days), pct(ahead, days),
            '%d/%d' % (waits_bad, waits) if waits else '-', ('%s (%d, r2+3 %d)' % (pct(sites_ok, sites).strip(), sites, other_rules)) if sites else '-',
            sum(real) / len(real) if real else 0, sum(money) / len(money) if money else 0, sum(hreach) / len(hreach) if hreach else 0))
        summary.append(dict(plan=plan, level=level, games=n, days=days,
                            band_60_100=round(100.0 * band_n / days, 1) if days else None,
                            band_85_120=round(100.0 * band2_n / days, 1) if days else None,
                            ahead=round(100.0 * ahead / days, 1) if days else None,
                            waits=waits, waits_outside=waits_bad, purchases=sites,
                            purchases_80_130=round(100.0 * sites_ok / sites, 1) if sites else None,
                            purchases_rule_2_3=other_rules,
                            ai_real_viewers_m=round(sum(real) / len(real), 2) if real else None,
                            ai_cash_day60_k=round(sum(money) / len(money)) if money else None,
                            human_reach_day60_m=round(sum(hreach) / len(hreach), 1) if hreach else None))
        curves['%s_%s' % (plan, level)] = {k: [round(sum(per_day[day][k]) / len(per_day[day][k]), 2) for day in sorted(per_day)]
                                           for k in ('human', 'ai', 'p2', 'p3')}
        curves['%s_%s' % (plan, level)]['ratio'] = [round(sum(per_day[day]['ratio']) / len(per_day[day]['ratio']), 3) if per_day[day]['ratio'] else None
                                                    for day in sorted(per_day)]
        curves['%s_%s' % (plan, level)]['days'] = sorted(per_day)
    print('(ratio: the stronger computer player / the human, days 5-60; waits: drawn outside the range / all;'
          ' viewers: real viewers of a computer player, days 31-60, millions; money: its cash on the last day, $1000;'
          ' human: the human\'s reach on the last day, millions)')
    if curves_path:
        with open(curves_path, 'w') as f:
            json.dump(curves, f)
    if csv_prefix:
        with open(csv_prefix + '_curves.csv', 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['plan', 'level', 'day', 'human', 'ai_stronger', 'ai_2', 'ai_3', 'ratio'])
            for key, c in curves.items():
                plan, level = key.split('_')
                for i, day in enumerate(c['days']):
                    w.writerow([plan, level, day, c['human'][i], c['ai'][i], c['p2'][i], c['p3'][i], c['ratio'][i]])
        with open(csv_prefix + '_summary.csv', 'w', newline='') as f:
            w = csv.DictWriter(f, fieldnames=list(summary[0]))
            w.writeheader()
            w.writerows(summary)


if __name__ == '__main__':
    main()
