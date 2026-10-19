# Computer-player baselines

`tools/ai_run.py 100 OUT` (normal difficulty; the idle human's image is held
>= 20 and its money >= 0 so it is neither fired nor visited by the bailiff):

| File | Build | Days | Use |
|------|-------|-----:|-----|
| `ai100_original.csv` | `build/mtv` (machine-level: the original program and its timing) | 101 | how the original computer players behave |
| `ai100_fixed.csv` | `build/mtv-dec --fixes` (all original-bug fixes) | 101 | the same run with the fixes: no intruder hang; computer players rent studios again; since 2026-10-10 with the five fixes from the multiplayer inventory (catch-bounty, ai-visit, ai-trendy, ai-rival, ai-arrived), since 2026-10-12 with title-cache and the five Phase 4 fixes (case-owner, ai-fill-genre, ai-lib-quality, studio-takeover, studio-reserved) |
| `ai100_decompiled.csv` | `build/mtv-dec` (classic), since 2026-10-09 with the original's stale half-day range at 2DD9:1147 reproduced (src/README.md) | 83 | regression reference for hand edits of `src/`: an edit that does not mean to change behaviour must reproduce it exactly |

The decompiled build counts virtual time per function and loop, so its game
takes a different course from the original's; both are deterministic.

`ai100_decompiled.csv` ends on day 83 because the game hangs there: an
intruder strikes the human's library, which holds only series by then, and
the original code loops forever looking for a film to destroy (1DA8:2D3B;
during the hang only random_n and
timer_read are called).  The run reproduces that original bug.  (The build
at the freeze tag hung the same way on day 38, the one with defined values
for the never-written locals on day 86; their computer players planned
differently.)  `ai100_fixed.csv` did not change when the half-day range was
reproduced: with `--fixes` the range is the half day, as before.

Observed in both: the computer players run deep into debt (original timing,
day 101: -4.70M and -3.23M) while their reach grows; only the human is ever
visited by the bailiff.

`ai100_fixed.csv` was made again on 2026-10-10 when five fixes were added;
three of them change the computer players' play (ai-trendy, ai-rival,
ai-arrived).  One run says little -- the course of a game changes with any
difference -- so the old and the new set of fixes were compared over three
games each (`--offset 0/7/13`, 60 days): with the new set the computer
players ended day 60 at -399 000 money and 34.9M viewers on average (the
two complete games of the old set: -1 018 000 and 27.6M).  The third game
of the old set hung on day 11: the "Trendy" bug made a computer player give
a gift that does not exist, which corrupted the human's news count
(names/FINDINGS.md, fix ai-trendy).

`ai100_fixed.csv` was made again on 2026-10-12 for the five Phase 4 fixes
(23 fixes; on the Ryzen's musl build -- the first 61 days of the 18 fixes
before (the 17 of 2026-10-10 and title-cache) reproduce the previous file
exactly).  Three games each
(`--offset 0/7/13`, 60 days), the two computer players' mean reach over
days 31-60 (the `viewers` columns of `tools/stats.py` are the transmitter
network's reach, not an audience) and money on day 60:

| Set | Reach d31-60 | Money d60 | Library d60 |
|-----|---------------:|----------:|------------:|
| the 18 fixes before | 25.9M | -399 000 | 11.2 |
| 23, without ai-fill-genre and ai-lib-quality | 25.9M | -399 000 | 11.2 |
| 23, without ai-fill-genre | 20.8M | -746 000 | 14.3 |
| 23, without ai-lib-quality | 23.0M | -776 000 | 12.5 |
| all 23 | 18.3M | -777 000 | 12.7 |

case-owner and the two studio fixes do not change the computer players'
game here.  Three games are too few for the two AI fixes, though: any
difference sends a game another way, and the games differ a lot.  Ten games
each (`--offset 0/7/13/21/29/37/43/51/59/67`, 60 days, 2026-10-12, an
instrumented build on the Ryzen), the computer players' means over days
31-60 (rating: yesterday's average rating % of the reach, `g_avg_rating`;
audience = reach x rating) and money on day 60, +- the standard error:

| Set | Reach | Rating | Audience | Money d60 |
|-----|------:|-------:|---------:|----------:|
| all 23 but ai-lib-quality | 21.0M +- 1.2 | 8.4 % +- 0.4 | 1.81M +- 0.15 | -836 000 +- 113 000 |
| the same without ai-fill-genre | 21.1M +- 1.4 | 8.9 % +- 0.4 | 1.96M +- 0.21 | -948 000 +- 159 000 |
| all 23 | 17.8M +- 1.1 | 9.9 % +- 0.4 | 1.91M +- 0.16 | -779 000 +- 45 000 |

ai-lib-quality changes how the computer players play, not how strong they
are: with a true library average a computer player sees itself as the
worst more often (in two 40-day games 20 and 38 of 82 AI-days instead of 1
and 13: classic, the idle human's 4 films look worst almost every day) and
puts more into films and less into transmitters -- less reach (t = -2.0),
better ratings (t = +2.5), the same audience and money (t = +0.4, +0.5).
ai-fill-genre changes nothing measurable (all |t| < 1): its genre mask
matters only while every open contract of the computer player has a genre
rule (58-100 of about 3 200 programme placements in ten games; classic
placed an excluded genre 9 times, fixed 3 times -- the first fitting
programme, kept as before).  The 3-game rows above are noise.

`ai100_fixed.csv` was made again on 2026-10-14 for the fixes ai-rating and
ai-bids (on the Ryzen, the same musl build as the earlier ones): the same
game up to day 62; from day 63 on another course (day 101: the computer
players at -1.47M / -2.09M and 24.6M / 32.3M reach, before -2.03M / -3.51M
and 30.3M / 87.8M).  Both fixes act only on large numbers -- more than 21.5
million viewers an hour, more than 11 auction bids of one computer player
-- so the first 62 days of this idle game do not reach them.
