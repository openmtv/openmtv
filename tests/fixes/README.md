# Tests of the original-bug fixes

Every fix of an original bug (`src/fixes.h`, `build/mtv-dec --fixes=list`)
has a test here: an input script whose header names the fix and states what
must hold in classic mode (the bug shows) and with the fix (it is gone).
`tools/fixtest.py` runs the decompiled build twice on fresh game files — once
classic, once with only that fix — and checks the expectations; `verify.sh`
runs it as step 5.  A `# args:` line gives more options to every run of a
test (e.g. `--logical-time`).

| Test | Fix | What it shows |
|------|-----|---------------|
| `intruder_hang` | `intruder` | human $500k in debt, library holds only the starting series: the bailiff hangs the game on day 2; fixed, the days go on |
| `intruder_love_films` | `intruder` | library holds only love films: classic never seizes one, fixed does |
| `random_zero` | `random0` | `random_n(0)` called directly: classic stops (divide error), fixed returns 1 |
| `ai_studio` | `ai-studio` | computer player 2 without a studio: classic "rents" studio 13 (the props price table), fixed rents studio 1 |
| `ai_fill_hour` | `ai-fill-hour` | computer player 2 fills 22:00: classic estimates films for the hour = their category and rejects three good ones, fixed estimates for 22:00 |
| `ai_series_rule` | `ai-series-rule` | an ad that excludes series of a genre: classic checks film id−24, fixed the series |
| `series_avg` | `series-avg` | series 12 rates 60 classic, 67 fixed; series 29 is bought by a computer player only fixed |
| `estate_text` | `estate-text` | giving up a studio during a production: classic "Warning: Psychiatrist", fixed the real message |
| `tx_price` | `tx-price` | $150 000 cash, a transmitter priced $250 000: classic sells it (the check asks $120 000), fixed says "Not enough money!" |
| `library_shelf_owner` | `library-owner` | film 66 off the shelf and back (directly, and via the case), market drained: classic leaves it ownerless and the dealer's refill offers it, fixed it stays the player's |
| `news_headline_cache` | `news-cache` | 20 inbox items, three whale headlines in the broadcast slots: classic shows "Whale swimms back to sea." in all three, fixed the right ones |
| `ai_odd_range` | `ai-odd-range` | idle game, day 2: computer player 2 fills 22:00 with the 3-hour film 88 classic (the odd-length rule never runs), the 2-hour film 129 fixed |
| `boss_music` | `boss-music` | "I quit" (the ending_resigned input): classic the boss room's sample plays on into "THE END" and room 1 is recorded as the last room, fixed the sample is stopped |
| `catch_bounty` | `catch-bounty` | human on station 2, computer player 1 standing in the human's office: classic the human gets nothing and the bounty stays $10 000, fixed the human gets $10 000 and it doubles |
| `ai_visit` | `ai-visit` | human on station 2, computer player 1 about to sneak into "the human's" news room: classic it goes to its own (floor 5), fixed to the human's (floor 8) |
| `ai_trendy` | `ai-trendy` | "Trendy" aired by computer player 3, a series genre among the trends: classic the trends land on DS:78BA.., player 2 holds "gift 11" and giving it adds 256 to the human's news count (the start of a hang), fixed AI index 1 learns them |
| `ai_rival` | `ai-rival` | images 20/60/40, player 2 planned with g_ai_cur = 1: classic its rival is itself, image rank 2 although it leads; fixed rank 1 |
| `ai_arrived` | `ai-arrived` | computer player 3 (AI index 1) gets its current door as its next goal: classic it must wait again, fixed it may act at once |
| `film200` | `film-200` | data fix: film 200's flags byte (4DA5:1206) classic 5 (never offered), fixed 37 (on the market) |
| `live_aid` | `live-aid` | data fix: film 164's length (4DA5:0EC8) classic 0, fixed 2; given to the human and placed at 18:00 on an empty evening: classic 19:00 stays empty and at 19:00 the tape is still in the archive (it goes at night as "missed"), fixed 19:00 is its second hour and the tape is used up after it |
| `video_connection` | `video-connection` | data fix: how well actor 1 fits the host's and the guests' role of screenplay 20 (`script_role_match_pct` 1DA8:2560): classic 0 % (its roles ask nothing), fixed 59 % and 62 % |
| `title_cache` | `title-cache` | 40 archive films and 10 in the case: classic the Sundry films 5 and 42 have no title (their slots 49.. lie on the series cache, whose slot 0 then says film 5: "Gunsmoke" shows "Once upon a time in the West" and garbage episodes); fixed the titles are there and the series cache untouched, and leaving frees them |
| `case_owner` | `case-owner` | film 66 off the library shelf into the case, into the film agency and out again (market drained): classic the tape is ownerless and the refill offers it in dealer slot 1, fixed it keeps owner 1 and nothing is offered |
| `ai_fill_genre_mask` | `ai-fill-genre` | computer player 2's contracts exclude love films, it fills 22:00: classic love film 88 and no ads, fixed action film 129 and ad 13 |
| `ai_fill_genre_mask2` | `ai-fill-genre` | the contracts exclude comedies, it fills 21:00: classic comedy 198 slips through, fixed 129 and ad 13 |
| `ai_fill_series_row` | `ai-fill-genre` | two series for 23:00: classic the whodunit (21) is scored on the family row and loses to family series 12, fixed 21 stays |
| `ai_lib_quality` | `ai-lib-quality` | player 2 holds the best films: classic its average is 9 (below 10 and 11) and its film priority 3 / pot 44 %, fixed 29 and 2 / 35 % |
| `studio_takeover` | `studio-takeover` | the human queued for a bombed studio 6, "Trendy" airs: classic studio 2 (the Psychiatrist's door) becomes his, fixed studio 6 |
| `studio_reserved_ff` | `studio-reserved` | a deferred give-up, then the human rents studio 6 again: classic it is released after his next episode, fixed it stays his |
| `credit_repay` | `credit-repay` | cash -$230 000, "repay credit: all": classic the boss "is repaid" -$200 000 (cash -30 000, credit 400 000), fixed nothing ("not enough money") |
| `credit_repay_over` | `credit-repay` | a credit of $50 000, "repay $100 000": classic the credit is -50 000 and the day's interest pays +$2 500, fixed $50 000 repaid, credit 0 |
| `credit_repay_mp` | `credit-repay` (network game) | `credit_repay` with two human seats and no `--fixes`: nothing lent (`FIX_MP_ALWAYS`) |
| `case_corner` | `case-corner` | in the film agency, the human's programme score of the hour poked to 4: classic the lower right corner of the case shows an untitled film 4, it is taken (the case count drops, a tape bought before is no longer counted) and sold to Mr. Jabble for $160 000; fixed no info, nothing taken, nothing sold -- and the bought tape still goes in and out of the case |
| `case_corner_mp` | `case-corner` (network game) | the same run with two human seats and no `--fixes`: nothing taken, nothing sold (a network game always has the fix, `FIX_MP_ALWAYS`) |
| `studio_reserved_queue` | `studio-reserved` | a queued hand-over: classic the studio is blocked, taken over and charged again after the next episode, and its show hands out studio 1; fixed none of it |
| `ai_rating` | `ai-rating` | day 2, player 2 with a reach of 283 M and 30 M viewers in each of yesterday's hours: classic its rating is -4 (the product overflows 32 bits), fixed 10 % |
| `ai_bids` | `ai-bids` | player 3 with 11 bid entries (lots 1..10 and 1 again) bids on lot 3: classic a 12th entry over DS:7672 (count 12), fixed lot 3's entry reserves more (count 11) |
| `ai_warning` | `ai-warning` | 20:00, player 2 places a programme at 18:00: classic the warning screen and a second box on the human's screen, fixed refused without a word; the human's own placement still warns |
| `trendy_studios` | `trendy-studios` | the human gives up studio 6 while Love boat shoots there until 20:00, player 2 airs "Trendy" at 18:00 (its own Trendy waits in studio 8): classic studio 6 is freed at 18:00, the production lost, and at 20:00 "screenplay 0" finishes over player 2's news budget (1470 -> 0); fixed only studio 8 is let go, Love boat reaches the archive at 20:00, then studio 6 is freed |
| `start_money` | option `--start-money` | not a fix, a game option: a new game gives the human $1 234 000 instead of $200 000, the credit stays $200 000 |
| `credit_line` | option `--credit-line` | not a fix, a game option: day 4, four times "a minimum of $100 000?": classic the second is refused (the original's $300 000), with `--credit-line=700000` 300 000, 400 000, 500 000, then refused (day 4's line) |
| `credit_original_line` | option `--credit-line=original` | `credit_line` with the original's credit system: the original's line, the second "minimum" refused as in classic |
| `credit_original` | option `--credit-line=original` | `credit_repay` with `--fixes`: the original's credit system turns `credit-repay` off -- the boss lends the debt as in classic |
| `credit_original_mp` | option `--credit-line=original` (network game) | the same with two human seats: off there too, although a network game otherwise always has the fix |
| `credit_line_cap` | option `--credit-line` | day 10, a credit of $600 000: classic refused at once, with the option 700 000 and then refused (the line) |
| `terror_damage` | option `--terror-damage` | not a fix, a game option (logical clock): a computer player's sabotage sends the bomb into the human's studio: classic the human pays $50 000 (cash 117 000, the ledger 50 000, messages 0x134 and 0x135), with `--terror-damage=250000` $250 000 |
| `terror_damage_zero` | option `--terror-damage` | the same with 0: the studio is lost, no costs, no message 0x135 |
| `terror_damage_ai` | option `--terror-damage` | the human's sign swap sends the bomb into Fun-TV's library: classic it pays $50 000, with `--terror-damage=200000` $200 000; a film is destroyed either way |
| `sammy_prize` | option `--sammy-prize` | the human wins the news Sammy: classic no money, with `--sammy-prize=300000,1,2,3` cash +300 000, yesterday's sundry income 300 000, the message 0x159 carries it |
| `sammy_prize_mp` | option `--sammy-prize` (network game) | two humans (`# args: --humans 1,2`), station 2 wins: both runs tell seat 0 "the Sammy goes to ..." and seat 1 "you won" (`expect ... seat 1 ...`: another seat's memory); with `--sammy-prize=300000` station 2's cash, its message and its ledger have the prize |
| `fourth_speed` | option `--fourth-speed` | not a fix, a game option: the planner's Control-Center has four speed buttons; a click beside the original's three (x 200) changes nothing classic and picks 120 with the option, the other three still give 20 / 30 / 60 |
| `fast_rooms` | option `--fast-rooms` | not a fix, a game option (both runs on the logical clock, `# args: --logical-time`): classic the way into the office takes 57 frames and out 39 (the clock paused, the cursor hidden meanwhile), with the option the office stands 3 frames after the door and the way out takes 3 |
| `ai_improve` | option `--ai-improve` | not a fix, a game option: the human buys two transmitters (the harness's `txbuy`); classic the computer players' reach stays the original's (player 2 7.0 M), with `--ai-improve=strong` both follow from their cash within about 1.6 days; the behaviour in detail: `tools/aiimptest.py` |
| `ad_offers` | option `--ad-offers` | not a fix, a game option: in the ad agency with 17 offers, offers 12 (front row) and 15 (back row) poked: the tip (also of a back folder through an empty front place), taking a folder, putting it back on the desk beside the original's desk (left of the front, right of the back), the briefcase and back, signing -- one contract, offer 15 refilled; classic no tip of those ads and no contract of ad 14 |
| `film_offers` | option `--film-offers` | not a fix, a game option (logical clock): the film agency with 17 offers, slot 15 (left of the original's row) poked to film 8: its tip (on the right), buying it, giving it back at the dealer's counter left of the original's hotspot, buying it again, into the case, leaving: refunded and paid as the original's slots, the film in the library, slot 15 refilled; classic nothing answers there |
| `film_offers_save` | option `--film-offers` | slots 11 and 17 saved (`game_save`), emptied and loaded again: with the option they come back (the `MTVX` extension's sixth count) |
| `ad_offers_save` | option `--ad-offers` | offers 11 and 17 saved (`game_save`, the harness's `call`), emptied and loaded again: with the option they come back (the `MTVX` extension's fifth count) |

Header syntax (see `tools/fixtest.py`):

```
# fix: NAME
# expect classic|fixed FRAME byte|word SEG:OFF  OP VALUE | halted
# expect classic|fixed FRAME call SEG:OFF,ARG,...  OP VALUE | halted
# expect classic|fixed FRAME seat S byte|word SEG:OFF  OP VALUE
```

`seat S`: another seat's memory, in a test that plays a network game
(`--humans` in its `# args:` or `# option:` line; the script's `N seat S
peek16` lines).

A game option (the player's choice, not a fix) is tested the same way with
`# option: --start-money=N` (the options of that run) and `expect option`
lines instead of `# fix:` and `expect fixed`.

The scripts use the test-harness commands `poke16`, `peek8`/`peek16` and
`call` (runtime/host.c) to set up a situation that would otherwise take
weeks of play.
