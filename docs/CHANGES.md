# What differs from the original Mad TV

Everything the recompiled game does differently from the original
(Rainbow Arts, 1991), in one list.  Keep it up to date with every fix and
improvement.

The original game itself stays available: `./play.sh --classic` (or
`classic = yes` -- or `fixes = none` -- in the settings file) plays exactly the original's game --
the same rules, the same bugs, the same timing --, and `build/mtv` is the
original machine code, recompiled bit-exact.  The bug fixes below are
switches; `./play.sh` plays with all of them by default.

## 1. Fixes of the original's bugs (switchable)

`--fixes` (all), `--fixes=NAME,NAME` (some), `--classic` (none); in the
settings file `[game] fixes = all | none | NAME,NAME`, and `[game] classic =
yes` for none whatever `fixes` says (the launcher's switch "Classic": the
choice of fixes stays for when it is off again).  Each has a test in
`tests/fixes/` that shows the bug with the fix off and its absence with it
on (`verify.sh` step 5); `build/mtv-dec --fixes=list` prints the table.

| Name | The original | With the fix | Where | Test |
|------|--------------|--------------|-------|------|
| `intruder` | A bailiff or terrorist striking a library that holds only series hangs the game for good (it counts genres 2..12 but picks from 1..10); love films are never seized | counts and picks the same genres | `1DA8:2D3B` | `intruder_hang`, `intruder_love_films` |
| `random0` | `random_n(0)` divides by zero: "Divide error", the game ends (reachable from the title sequence) | returns 1 | `0D5E:0229` | `random_zero` |
| `ai-studio` | a computer player rents studio 13 instead of the one it chose | rents the chosen studio | `348B:098D` | `ai_studio` |
| `ai-fill-hour` | a computer player estimates a programme's audience for "hour = its category" | for the hour | `2DD9:0DD9` | `ai_fill_hour` |
| `ai-series-rule` | a computer player checks an ad's genre rule on film "id − 24" instead of the series | on the series | `2DD9:162A` | `ai_series_rule` |
| `series-avg` | a series' average topicality sums episodes 2..n+1: computer players undervalue series | episodes 1..n | `2DD9:3085` | `series_avg` |
| `estate-text` | the estate agent shows room names ("Warning: Psychiatrist") as its messages | its real messages | `05FF:3AB7` | `estate_text` |
| `tx-price` | a transmitter purchase is checked against new viewers × 61.6, not the price it costs: you can buy what you cannot pay | checked against its price | `05FF:2C02` | `tx_price` |
| `library-owner` | a film taken off the library shelf and put back stays "on the market": the dealer sells it while it is in your library | gets its owner back | `0AA4:24C3` | `library_shelf_owner` |
| `news-cache` | with a full news inbox, broadcast slots 2 and 3 show slot 1's headline (the lookup searches 20 of 24 entries) | searches all 24 | `26A7:292F` | `news_headline_cache` |
| `ai-odd-range` | a computer player's "odd length" rule checks films against a half-day range read from stale stack bytes (1..0: it never rejects one) | the half day of the hour | `2DD9:1147` | `ai_odd_range` |
| `boss-music` | the boss room stops its music without arguments before the end sequence: the room music plays on | stops it properly | `0AA4:000E` | `boss_music` |
| `catch-bounty` | catching a rival in your own room pays the bounty only to figure 0: playing station 2 or 3 you never get it (the message shows a stale amount) | the human's figure gets it | `1DA8:3643` | `catch_bounty` |
| `ai-visit` | computer players with the master key sneak into "player 1's" rooms: with the human on station 2 or 3, computer player 1 visits its own rooms and the other one visits it, nobody the human | they visit the human's rooms | `2DD9:3C0D` | `ai_visit` |
| `ai-trendy` | a computer player airing film 138 "Trendy" should learn the trends but writes them by player number past its table: over the odd-length counters and the pending Betty gifts, and no computer player learns them.  A series genre (11..15) becomes a gift that does not exist; giving it writes the gift's decay into the human's news count (20 -> 270) and the hourly news ageing runs for good: **the game hangs** (seen in a computer-player run on day 11, also with all the other fixes) | written into its own entry | `24B8:0C4A` | `ai_trendy` |
| `ai-rival` | a computer player's daily budget plan takes "the computer player that is not g_ai_cur" as its rival, but both are planned with the same g_ai_cur: for one of them the rival is itself -- never best in image, its reach counted twice | the other computer player | `2DD9:26EA` | `ai_rival` |
| `ai-arrived` | when a computer player's next goal is the door it already stands at, only AI index 0 may act at once; the second one waits the 300 ticks at the door again | both act at once | `2DD9:4126` | `ai_arrived` |
| `title-cache` | the film text cache has 49 slots, but the own library loads the case's films and then the whole archive into it without a bound (3 + 10 + 40).  A film bought for a full genre stays in the case, so 47 films in archive and case are enough: the last archive films (Sundry first) show no title and no description, the series cache's first slot is overwritten -- a series then shows a film's title and garbage episode names in every room (e.g. "Gunsmoke" as "Once upon a time in the West": "$", "", "", "Paris, Texas" ...) until that slot is loaded again -- and about 8 strings leak per visit | the slots past 48 are kept outside the data segment (64 more, `film_txt_more`, carried per seat in a network game), searched and freed with the others | `048C:025B` | `title_cache` |
| `case-owner` | a tape taken off the library shelf into the case is withdrawn like a sale (owner 0, "on the market") and stays so while carried: every dealer refill -- leaving the film agency (the refill runs before the case is unloaded), the day change, a computer player's visit to the dealer -- can put it on the dealer's shelf.  Bought back it is paid twice (a second library entry), bought by a computer player it sits in two libraries; on the agency's shelf it also counts as "just bought" (a sale becomes a refund) | the tape keeps its owner until it is really sold (a sale to the dealer and the auction set owner 0 themselves) | `05FF:090A` | `case_owner` |
| `ai-fill-genre` | a computer player filling an hour tests every candidate against the genre bit of the best programme so far, not its own genre: the best so far decides -- while its genre is allowed, excluded genres pass; once an excluded one is best (the first fitting programme is the best before any test), nothing replaces it.  A love film is often that first one and "no love films" the commonest exclusion: the computer player airs love films exactly where its contracts forbid them and books no ads there.  And a series best is scored on the family-series audience row, whatever its genre | the candidate's genre; the best's own audience row (rarely matters: only while every open contract has a genre rule; no measurable change over ten games, `tests/baseline/README.md`) | `2DD9:12C1`, `2DD9:1316` | `ai_fill_genre_mask`, `ai_fill_genre_mask2`, `ai_fill_series_row` |
| `ai-lib-quality` | a computer player's library average counts slots 0..count and genre 0 too: 12 phantom entries (sum / (n + 12)), the next genre's first programme after a full genre, the count bytes after a full series shelf, programmes withdrawn long ago.  Its daily budget plan's "worst quality" flag (how much goes into films) then follows library size, not quality | slots 0..count-1 (the computer players put more into films and less into transmitters: less reach, better ratings, the same audience and money over ten games, `tests/baseline/README.md`) | `348B:0895` | `ai_lib_quality` |
| `studio-takeover` | when the show of a studio with a queued renter airs, the hand-over passes the player loop index as the studio: the renter gets studio 1, 2 or 3 -- the Laundry room's, the Psychiatrist's or the Bio-control door becomes his studio, at its price -- and the studio he queued for stays free and blocked | the studio | `24B8:170C` | `studio_takeover` |
| `studio-reserved` | a studio's pending byte (a queued renter, or "give it up when the production ends") is never cleared: a leftover give-up releases the studio from its next renter after each of his productions; a leftover renter blocks it for good (no give-up, nobody can rent it once it is free) and takes it over again after every episode -- 3 x the rent, +50 % each time, the screenplay wiped | cleared when the studio changes hands, with its end day (else a studio freed mid-production and handed on would "finish" screenplay 0 for its new renter) | `229E:1080`, `229E:1264` | `studio_reserved_ff`, `studio_reserved_queue` |
| `credit-repay` | "repay credit" at the boss (menu 5): the amount is the smaller of the choice ($100 000, $200 000, all) and the cash, rounded toward zero to $50 000 -- with a negative cash a negative repayment: the cash goes up, the credit too, past every credit line and without the image check (cash -230 000 -> -30 000, credit 200 000 -> 400 000; -1 000 000 -> 0 and 1 200 000; "There! And now back to work").  So the bailiff (cash below -100 000 at the day change) never comes either.  And nothing caps it at the credit: $100 000 on a credit of $50 000 makes it -50 000, and the daily interest (money -= credit / 20) then pays 5 % a day, without a limit | at most the credit and the cash, never below 0 -- a 0 gives the original's "You haven't got enough money!".  **Always on in a network game** (`FIX_MP_ALWAYS`) -- except with the original's credit system (credit line `original`): then off, in every game | `0AA4:000E` | `credit_repay`, `credit_repay_over`, `credit_repay_mp`, `credit_original`, `credit_original_mp` |
| `case-corner` | the film agency's case takes a click anywhere in its frame: slot = column + 5 x row, 7 columns and 3 rows (0..16) where the tapes are drawn 5 by 2, and only asks whether the word at that slot is non-zero -- the library asks slot < count.  Behind the 10 used slots lie 5 never written, then the programme scores of the hour: slot 16, the lower right corner of the frame (x 278..279, y 88..103), is the human's (station 1's) score, taken for a film id.  Hovered, it shows that film's info without a title (titles are loaded only for the case, shelf and auction); taken and dropped on Mr. Jabble, it is sold (or put into the auction) for its full price, again and again, a new film every hour.  Each grab counts the case down past zero: the next tapes bought go to the words before it (computer player 3's transmitter search) and are lost | only the drawn tapes: column 0..4 and slot < count, for the click and the info.  **Always on in a network game**, whatever the host chose, Classic included (`FIX_MP_ALWAYS`, `seat_humans`); there the corner held station 1's score for every seat | `05FF:0359`, `14E1:000E` | `case_corner`, `case_corner_mp` |
| `ai-rating` | a computer player's average rating of yesterday is viewers x 100 / (reach x 2464), the product in 32 signed bits: above 21.5 million viewers an hour -- reached only with a satellite -- it overflows to a rating of 0 or below; it then takes only ads with no minimum audience and ranks itself last (measured with the AI improvement's satellites: ratings about 1 %, cash -75 M on day 60) | the product in 64 bits | `2DD9:2B8D` | `ai_rating` |
| `ai-bids` | a computer player's auction bids go into a list of 11 places (DS:756A), one new entry per bid -- the same lot again after the other computer player outbid it -- and only the human outbidding it removes one: an active bidder writes past the list over the contract counts and the hour's news viewers; the ad agency then marks thousands of garbage "contracts" as held and writes into the sound driver's code -- the game halts (18 of 70 measured games with the AI improvement) | a lot already in the list keeps its entry, the reserve added (10 lots, 11 places) | `2DD9:1DDA` | `ai_bids` |
| `ai-warning` | a placement refused for a computer player (its live film's hour is over) shows the human's "Warning" box and switches his screen; the computer player tries again at every office visit, each try opens another box, none closed, until a box's background save gets a null bitmap and writes the screen over the interrupt vectors -- the game halts (8 of 70 measured games with the AI improvement's satellites) | no box for a computer player; the placement is refused as before | `229E:0225`, `2DD9:4719` | `ai_warning` |
| `film-200` (data) | film 200, John Carpenter's "Prince of Darkness" (1987), has the placeholder English title "xxx", and its German text is cut off in the resource after "Die Fürsten der" (a table of numbers follows; the length byte, 26, is that of "Die Fürsten der Dunkelheit"; the description is lost); it is never offered | titles "Prince of Darkness" / "Die Fürsten der Dunkelheit", a new German description, offered by the film dealer -- only while the data is the original's damaged data | `src/fixes.c` (texts as read, the market flag after the start savegame); the editor's starting point: `tools/datafixes.py` | `film200` |
| `live-aid` (data) | film 164, "Live Aid" (a live broadcast in the auction pool: quality 50, $700 000), is 0 hours long -- the only film without a length.  The computer players value a lot at hours × topicality × 20 / thousands of dollars: 0, so they never bid on it (`2DD9:20BA`, `2DD9:2386`).  Its date is drawn as evening hour 1..8 − length (`1DA8:0CB3`): one time in eight 1 a.m., an hour that can never be planned (it is lost as missed).  Placed in the planner (`229E:0225`) it clears no hour: a programme it is put on keeps its continuation hours, which then air as Live Aid, and the planner draws its box 0 pixels high (length × 16).  Aired it never reaches its last hour (`24B8:0C4A` counts from 1), so the tape is not used up: it stays in the archive and is taken away at night with "You missed ... live" (`1DA8:135D`).  Its record also lacks the value 4 in `flags2` (bit 2) that the other live broadcasts and the shows have -- "used up when aired", what a live broadcast is anyway by its genre (`24B8:0C4A` asks for either), otherwise read only for studio productions -- and anything in the low five bits of `flags`, which the code never reads: both left as they are | 2 hours: the other ten live broadcasts cost about $7 000 per hour and quality point (59: 1 hour, quality 50, $100 000; 163: 2 hours, quality 60, $750 000; 187: 4 hours, quality 80, $3 000 000 -- the best fit $6 965), which makes $700 000 at quality 50 2.01 hours, and puts the computer players' value of it (2.9) among the others' (2.0 .. 3.7) -- only while the record is the original's (a mod's film 164 stays its own) | `src/fixes.c` (the record after the start savegame; also a pack's film 164 while it is the original's record).  The editor keeps the original's length 0 (a pack made there carries it, the game repairs it) | `live_aid` |
| `video-connection` (data) | screenplay 20, "Video-Connection" (film 134, a show: "a low budget music video show -- a few star guests and a sparkling host make it a success"), asks nothing of its two roles, the host (category 5, slot 1, weight 10) and the star guests (category 7, slots 2 and 3, weight 4 each): their requirements (humor, the unused second byte, looks, craft at +0x168) are all 0 -- the only roles of the 50 screenplays without any.  How well an actor fits a role (`script_role_match_pct` `1DA8:2560`: the sum of min(requirement, attribute) over the sum of the requirements) is then 0 for everyone, so the shooting (`229E:0B98`: quality = base x sum(score) / sum(weight) x 125 %) counts only the set (weight 4 of 22): quality 20 x 4 / 22 x 125 % = 3 at best (2 / 22: 1 with the cheapest set), which draws no viewers; whatever the actors cost is lost.  The shop's role fit (`--cast-fit`) shows "-" for every actor | both roles get the requirements of "Talk for Two" (screenplay 16, also a host and a guest): humor 20, looks 18, craft 16 -- only while the record is the original's (a mod's screenplay 20 with requirements stays its own).  Played again from a game where it was produced with a fair cast: quality 12 instead of 3, 1.15 million viewers at midnight instead of 0 | `src/fixes.c` (the requirements after the start savegame; a savegame holds only the episode blocks, so a loaded game has them too).  The editor keeps the original's record | `video_connection` |

### Game options (the player's choice, not fixes)

In the settings file and on the command line; the default is the original's
value (the credit line's is 700 000 in the settings file and the launcher; without
it the binary keeps the original's 300 000, so tests and recordings do).  They hold in classic mode too, never under `--difftest`, and only in
the decompiled build (`build/mtv`, and so `./play.sh --record`, keeps the
original's).

| Option | The original | With the option | Where | Test |
|--------|--------------|-----------------|-------|------|
| Start money: `[game] start_money = N`, `--start-money=N` | a new game gives the human $200 000 cash (and $200 000 credit) | N dollars cash, 0 to 100 000 000 (the status panel shows 6 digits of thousands); the credit stays $200 000; a saved game keeps its own money; a recording's header names it | `0AA4:0AC5` (`start_money_human`, `src/fixes.c`) | `start_money` (`# option:`) |
| Credit line: `[game] credit_line = N`, `--credit-line=N` (300 000 to 100 000 000 in steps of 100 000; settings default 700 000), or `original` | image 10..49: $200 000 credit at the start, another $100 000 from day 2 while the credit is <= $200 000 -- a line of $300 000 (image >= 50: $400 000, +$200 000 up to $600 000; the day's Sammy winner more) | the haggling allows another $100 000 while the credit is at most min(N, $100 000 x (day + 1)) - $100 000: day 2 $300 000, day 3 $400 000, ... up to N (a day skipped can be caught up; N = 300 000 is the original).  Image >= 50, image < 10, the day-1 refusal and the Sammy winner's $200 000 stay the original's; the boss's texts fit (he still haggles in steps of $100 000).  In a network game a setting of the host's (`credit=` in the game's parameters, the network savegame's description; a savegame from before the option: 300 000); a recording's header names it.  `original` (the launcher's "Original 1991" beside the amount; a lobby's `credit` 0, the game's parameter `credit=0`): the original's credit system -- its line, no staircase -- and the fix `credit-repay` off, also in a network game, where it is otherwise always on (`fix_on`); the launcher shows the fix locked and keeps the choice for it | `0AA4:000E` (`credit_haggle_max`, `fix_on`, `src/fixes.c`) | `credit_line`, `credit_line_cap`, `credit_original_line`, `credit_original`, `credit_original_mp` (`# option:`) |
| Terror damage: `[game] terror_damage = N`, `--terror-damage=N` (0 to 100 000 000; default 50 000, the original's) | a terrorist's bomb in a room with an owner -- one whose sign was swapped at the information board, by the human or by a computer player's sabotage; the consulates are public and cost nobody anything -- costs the owner $50 000 (the ledger's "credit repaid", shown under sundry costs) and message 0x135 says so ("... are $50.000!") | N dollars, for humans and computer players alike; the message names the amount the way the original writes it ("$250.000": "$50.000" replaced in the text as it is shown, both languages; a pack's own text without it stays); 0: no costs and no message -- the room's other losses (a film, the production, the studio) stay.  In a network game a setting of the host's (`terror=` in the game's parameters, the network savegame's description; one from before the option: 50 000); a recording's header names it | `1DA8:2D3B` npc_event_strike, `2B74:1875` msg_show_next (`terror_damage`, `terror_text`, `src/fixes.c`) | `terror_damage`, `terror_damage_zero`, `terror_damage_ai` (`# option:`) |
| Sammy prize money: `[game] sammy_news`, `sammy_culture`, `sammy_shows`, `sammy_series` = N; `--sammy-prize=N` (every category) or `--sammy-prize=NEWS,CULTURE,SHOWS,SERIES` (0 to 100 000 000 each; default 0, the original's) | the Sammy -- every three days a category: the best news, cultural engagement, self-produced shows, self-produced series (the last two count only what is shot in one's own studio; before day 5 only the first two) -- brings its winner image (+8/4/6/6, taken from the others) and a message (0x159), no money | the winner, human or computer player, also gets the category's amount at the day change: cash, and for a human the ledger's sundry income (in the report of the day that ends, like the credit's interest); the winner's message adds "It comes with prize money of $300.000!" / "Dazu erhalten Sie ein Preisgeld von $300.000!" (the amount travels in the message, a savegame keeps it).  In a network game a setting of the host's (`sammy=` in the game's parameters, the network savegame's description; one from before the option: none); a recording's header names it | `1DA8:4481` sammy_daily_update, `2B74:1875` msg_show_next (`sammy_prize`, `sammy_pay`, `sammy_prize_text`, `src/fixes.c`) | `sammy_prize`, `sammy_prize_mp` (`# option:`) |
| Fourth speed: `[game] fourth_speed = yes`, `--fourth-speed` | the planner's Control-Center has three speed buttons: 20, 30 and 60 game seconds per 50 timer ticks (`g_game_speed`) | a fourth button, 120 -- twice the third (a game hour in about 2 100 frames instead of 4 250 at 60).  Four buttons of the original's size (28 pixels, the car 26) do not fit the 96-pixel row, so the panel and its boxes grow by 16 pixels on each side and the buttons move closer together (gaps 5-6 instead of 6); the fourth shows the third's car with a flame out of its back (3 rows, palette 19/26/25).  The original's hotspot table has three speed entries; with the option a check before it gives the four new ones (0x3F..0x42).  At 120 the clock counts whole seconds -- (accumulated + ticks x speed) / 50 instead of whole steps of 50 ticks x speed -- or it would move 2 minutes at a time and skip the planner's refreshes at :00, :05 and :55.  A game saved at 120 and loaded without the option goes on at 60.  A recording's header names it | `2B74:0E63` options_draw, `05FF:0D0E` (case 0x42), `0AA4:2197` hotspots, `0D5E:02E4` clock, `229E:1386` load (`speed4_*`, `src/fixes.c`) | `fourth_speed` (`# option:`) |
| Fast room changes: `[game] fast_rooms = yes`, `--fast-rooms` | entering and leaving a room takes 30 to 82 frames on the logical clock (the office 57 in and 39 out, the studio 82 in): a wipe with a timer tick's delay per step (20 steps), the sprite decoder's time, some 7000 AdLib accesses of the room's music start.  The game's clock stands still meanwhile (`game_clock_pause`), so it costs no game time -- only the player's wait | 0 to 3 frames: the wipe without its delay per step (the room is there at once), the sprite decoder (`huff_*`) free on the logical clock, the AdLib accesses during a room change or disk access at a tenth of their price -- what a network game always does (there the world runs in the host's frames and stood still for everyone).  The rules are unchanged; random draws seeded from the timer fall otherwise, as with any other pace.  Without `--logical-time` (the tests' timing) only part of the time goes.  A recording's header names it | `0054:0D03` room_return, `0054:0E5F` room_enter (`fast_wipe_delay`, `src/main_loop.c`), `37B4:*` (`FUNC_TICK_FAST_ROOMS`, `src/huffman.c`), `port_clock` (`runtime/hw.c`); `fast_rooms()` in `runtime/runtime.h` | `fast_rooms` (`# option:`) |
| AI improvement: `[game] ai_improve = normal` or `strong`, `--ai-improve=normal` or `strong` (off by default) | the computer players buy transmitters only from their transmitter pot (35 % x a priority of every income; over $50 000, from 18:00, the site with the most new viewers whose price fits the pot); a good player who builds early passes them by day 10 or so, for good | each computer player watches the strongest human's reach: once it is 10 % above its reference, it notices it, remembers it (S), waits (normal 8-20, strong 4-12 played hours; a day is 17:00-01:00) and then buys towards S x F (normal 0.50-0.80, strong 0.80-1.02: the plan's 0.75-1.00 and 0.90-1.15 measured too strong) up to one (strong two) transmitters, if the gap is a million viewers or more: of all 1 500 map points (30 a step) the one whose new viewers are 80-130 % of the gap with the most new viewers per dollar (otherwise its usual rules), paid from the cash, which may go negative; their daily fees from the cash and half from the transmitter pot (as the original's for every transmitter) -- the pots for films, news and productions stay untouched.  The search measures against a copy of its own coverage and leaves the game's shared map as it was (in a network game another human may be on the transmitter map); it notices only what a human has paid for (not masts set on the map and not yet paid).  If more than 15 % of the target is still missing after them, it follows up: a new wait, half one of its level's, then the same target again, until the gap is smaller or nothing is left to buy (a satellite of the human's leaves a gap two transmitters do not close).  The satellites (the whole west / east half, $2.5M / $4.5M), which the original's computer players never buy: it may own as many as the strongest human has paid for (strong) or one fewer (normal); an allowed one is one more site of the rule -- its new viewers the half's it does not reach yet -- if they are at most 130 % of the gap or keep it at most at the human's reach; bought from the cash, its fees like a follow-up transmitter's.  Then its reference is S and it looks again: one step behind the human.  Further expansions during a wait are not counted; one ahead of the human plays as the original; its own purchases from the pot go on.  A hidden personality per computer player (cautious or ambitious, drawn with its state) takes the waits from the upper and the factors from the lower half of the ranges, or the other way round.  The draws: `random_n`, mixed with the played time.  The state goes into the savegame (the fourth part of the `MTVX` extension, also without a mod; a savegame without it starts it anew, with the human's reach then as the reference); in a network game a rule of the host's, in the game's parameters (`aiimp=`) and the network savegame.  Diagnostics: `[ai-improve]` lines on stderr (`build/game.log`).  A recording's header names it | `2DD9:000C` ai_tick, `2DD9:4B84` ai_want_transmitter, `2DD9:4862` ai_scan_transmitter_site, `1DA8:0FB9` pay_daily_costs, `229E:139A` the savegame's extension (`src/ai_improve.c`, `src/actions.c`) | `ai_improve` (`# option:`), `tools/aiimptest.py`, `tests/mp/ai_improve*`, `tests/mpsave/ai_*` |
| More ad offers: `[game] ad_offers = N`, `--ad-offers=N` (10 to 17; default 10, the original's) | the ad agency offers 10 contracts at a time (`g_ad_offers` DS:19A0, 10 x 7 bytes; the film dealer's slots follow at 19ED), drawn every night and refilled when a slot is free (`adagency_refill`); its desk holds them in three stacks of 3/4/3 folders | N offers, the same draw for every slot (which classes come up does not change: early demands stay high, the best-paid ads stay rare late).  Offers 11..N live in a host array behind the far segment EE00 (`g_ad_offers_extra`, `ad_offers_at`; any other index keeps the original's address), every loop over the offers runs to N (refill, the night's release, the computer players' choice, signing, giving one back, the briefcase's search).  10 keeps the original's desk; from 11 the folders stand in a front row at y 66 (x 68 + 8 per offer, up to 13: it covers the agent's hands) and a back row of up to 4 at y 43 (x 148.., it covers the clock on the desk -- decoration, no hotspot), drawn as the original draws (the back first, each row from the right) inside the background it saves (64..207 x 43..112 with the rows) -- the mock-ups chosen with the user (`https://claude.ai/artifact/SFKBcSBUPRcNyXSN6EBjQz`) 15 pixels further left: a folder's tip opens at x 184 and covered the rows' right end (the user, after playing it).  A click takes the folder at X for the cursor at X+5..X+12 (a row's last to X+9), the front row first unless its folder there is gone (then the back row's shows through); the tip the same with the mouse one pixel left, as the original's; a carried folder goes back on the desk around both rows (hotspot 0x26: (66,62)-(176,106) and (146,43)-(184,74), holding the original's two).  More slots need more ads: every ad has one holder, the pool is every free ad below 40 % of the largest reach -- with the original's 50 some places stay empty early (not drawn: gaps, as in the original); the neue-werbung pack (75) helps.  The savegame keeps offers 11.. (a fifth count of the `MTVX` extension, after the computer players' state; a savegame without them has none, offers past the game's N are left out and their ads free again at the next refill); in a network game a rule of the host's (`offers=` in the game's parameters, the network savegame's description when above 10; one without it: 10).  A recording's header names it | `24B8:000C` adagency_refill, `1DA8:1227` market_day_cleanup, `2DD9:311D` ai_take_contracts, `05FF:33F6` room_adagency_click, `0AA4:23B5` ad_agency_sign_contracts, `0AA4:267A` ad_return_offer, `26A7:14FB` ui_draw_ad_offers, `0AA4:2197` hotspots, the status panel's tip, `229E:139A` the savegame's extension (`ad_offers_count`, `adoffer_*`, `src/fixes.c`; `src/state.c`, `src/game_types.h`) | `ad_offers`, `ad_offers_save` (`# option:`), `tests/mp/ad_offers` |
| More film offers: `[game] film_offers = N`, `--film-offers=N` (10 to 17; default 10, the original's) | the film dealer offers 10 films and series at a time (`g_dealer` DS:19ED, 10 x 8 bytes; the players' screenplays follow at 1A45), drawn every night and refilled when a slot is empty (`dealer_refill`); his counter holds them as a row of 10 tapes, smaller towards the back right, each slot with a sprite (resource 155, 17 x 36 down to 14 x 26) and a rectangle (`g_filmshelf_rects` DS:032E) of its own | N offers, the same draw for every slot (the pool is about 80 films).  Slots 11..N live in a host array behind the far segment EE10 (`g_dealer_extra`, `dealer_at`), their rectangles behind EE20 (`filmshelf_rect_at`); every loop over the slots runs to N (refill, the night's return to the market, the computer players' shopping plan, giving a fresh film back, the case's search, the title caches).  10 keeps the original's counter; from 11 the row grows to the left, towards the viewer: new tape j (1..7) has tape 1's shape scaled to 36 + j rows (Pillow's nearest scaling) and the spine width, label lines and colour mark of another original tape, a different one each; it stands where its right edge ends just before its neighbour's label face, its bottom on the row's line extended (x 71, 62, 54, 43, 32, 20, 11) -- built once from the decoded bank and drawn by `src/film_offers.c` itself, not through the game's sprite bank, pixel for pixel the mock-ups chosen with the user (`https://claude.ai/artifact/1bmf7kkbbJ3q1znnRBhBEM`, all 8 steps compared).  The new tapes stand in front: drawn after the original's ten, tested first by the click and the tip; the tip of tapes 11.. opens on the right (the original's back tapes 9 and 10 have it on the left); Mr. Jabble's animation (x 16..87, y 18..70, every frame) draws them again over its picture; the dealer's hotspot 0x18 (a fresh film given back) reaches left along the row; the background the shelf saves grows to 8..183 x 59..112.  Slots the game has and a savegame not stay empty until the next refill (a new game's first, `new_day`, fills them before the first morning; refilling at the load took films off the market the new game's setup still draws its starting programmes from -- with several humans that draw never ended); offers of a savegame past the game's N go back to the market (else they would stay off it: only the night's cleanup returns an offer, and only from the slots it knows).  The series title cache (15 places) stops at its end in the agency (the original does not check).  The savegame keeps slots 11.. (a sixth count of the `MTVX` extension, after the ad offers); in a network game a rule of the host's (`filmoffers=` in the game's parameters, the network savegame's description when above 10).  A recording's header names it | `24B8:04B9` dealer_refill, `1DA8:1227` market_day_cleanup, `2DD9:1A3F` ai_plan_film_purchases, `05FF:0359` room_filmagency_click, `0AA4:2586` filmagency_return_film, `26A7:0FE5` ui_draw_dealer_shelf, `105E:17D1` the room animation, `14E1:000E` the tip, `048C:025B`/`048C:0B69` the title caches, `0AA4:2197` hotspots, `229E:139A` the savegame's extension (`src/film_offers.c`, `src/state.c`, `src/game_types.h`) | `film_offers`, `film_offers_save` (`# option:`), `tests/mp/film_offers` |
| Role fit in the shop: `[display] cast_fit = yes`, `--cast-fit` (off by default) | the production shop's actor panel shows, for the five actors on view, the fee and three bars -- humor, appearance, craft (bytes +6, +8, +9 of an actor; +7 counts nowhere and shows nowhere); the role picked in the shopping list has a requirement in the same three (screenplay +0x168, 4 bytes a role), never shown: the game rates an actor when shooting starts as the sum of min(requirement, attribute) over the sum of the requirements (`script_role_match_pct`), so what lies above the requirement is wasted and the fee counts nowhere; the panel knows only the slot's category, not the slot | the bars give way to one row "Eignung" ("Fit"): each actor's rating for the role picked, in per cent, the game's own sum (the best of the five in green, below 60 in red; "-" for a role without requirements, where every actor scores 0); the panel ends after it, 14 rows shorter, the room's picture under it again (kept as the shop is entered, before the panel covers it).  The role: the slot last clicked (the slot clicks tell it, per seat), else the first slot of the panel's category.  Display only: the game draws its panel with the same calls, then `src/cast_fit.c` paints over the screen buffer pixel by pixel -- no time taken, no game state touched: a recording replays the same.  The mock-up chosen with the user: `https://claude.ai/artifact/Kt9rUAFxEjVKWLMV59HzU5` (B; then without the bars, the panel shorter); in a network game the host chooses it in the lobby, the same for everyone (the game starts with `--cast-fit=yes|no`, over each player's own setting) | `26A7:3B04` ui_draw_actor_card, `0AA4:10CC` room_shop_click (the slot clicks), `0054:0F79` room_load_assets (the shop entered, room 63), `1DA8:2560` script_role_match_pct (`src/cast_fit.c`, `src/screens.c`, `src/rooms_people.c`, `src/main_loop.c`) | `tools/castfittest.py` |
| Series topicality: `[game] series_topicality = yes` (the default), `--series-topicality`; in a network game the host's (lobby key `sertop`, every machine the same: it draws more) | the film card shows the film's topicality (text 417, a bar of topicality x 40 / quality pixels, row 11); the series card (`tooltip_show` 14E1:16BB, id 1000..) shows none, although every episode has its own topicality (byte e*3+1 beside its quality at e*3) and the series' price is reckoned from them (`series_price` 1DA8:4035); its row 11 is empty, its bands go light (genre), dark (critics, press), light (episodes, price), dark (next episode), light (the last row, "(Program)") | the bar under the next episode on a light band of its own, the last row dark, so the bands go on alternating: all produced episodes' topicality against their quality (as the price weighs them); the card keeps its 12 rows -- when the next episode's title takes two lines (some 15 to 20 of the 389 German titles, about 30 English) it grows by one row; in the programme planner, whose cards share one window bitmap allocated on entry (the original's 0x3A88 bytes fit 18 x 13 cells exactly), that bitmap is 8 + 64 x 18 x 15 bytes with the option (up to 3dcd737 the taller card wrote past it, over the next heap block: wrong pictures, then a hang or an exit some game days later; `window_create` now halts on a window larger than the buffer). Also with Classic; the machine-level build has the original's card | `14E1:16BB` (`series_top_*`, `src/fixes.c`) | `tools/sertoptest.py` (verify step 15) |
| Multiplayer (in progress): `--humans A,B[,C]`; over the network `--mp HOST:PORT --seat K` against the server (`build/madtv serve`) | one human, on the station chosen at the setup; two computer players | two or three humans, each on their own station, computer players on the rest (or none); one world with the rules for several humans (daily costs, messages -- the Sammy's result too, which the original tells only `g_player` --, catching, the bailiff, the end, a shared pause); every human has their own screen, input and sound (a copy of the sound driver plays that seat's songs on the machine that plays it); `--new-game` starts without the intro and the setup screen.  Over the network every machine computes the same game from the same inputs (lockstep, the state's hash compared every 70 frames) | `runtime/seat.c`, `src/seatctx.c`, `runtime/net.c`, `tools/mpserver.py` | `tests/mp` (`tools/seattest.py`, `tools/mptest.py`) |
| A network game saved each night: `--mp-save DIR`; on from it: `--mp-load FILE` (with `--humans` its stations) | the safe in the office saves one human's game into a slot of `SOURCE1.RSC`; it stays shut with several humans | at every day's end -- every human gone home, the new day made -- every machine writes the same file `DIR/tag-NNN.mtvmp`: the original's savegame (the same bytes as `game_save`, written without changing the game) and each human's own data (credit, bag, ledgers, office, the boss's complaints, Madris); `--mp-load` starts from it without the intro and the setup: that day's morning, every human with their own data, a fired one watching.  The launcher keeps a directory per network game and lets the host go on from any night; the guests fetch the file through the server | `src/mpsave.c`, `229E:139A` (`mp_savegame_write`, `src/actions.c`), `tools/mplobby.py` | `tests/mpsave` (`tools/mpsavetest.py`), `tools/lobbytest.py --load` |
| Knocking (network games): `--knock=N` (game minutes, 0 to 120; 0 or none: off), in the lobby the host's "Knocking" (Off, 5, 10, 15, 30; default 15) | the film dealer, the ad agency and the supermarket take one figure at a time (`door_check_entry`: "someone inside", 0x12B); a human inside stays as long as it likes, whoever comes waits or goes away | a human who finds another human inside one of the three knocks: the one inside has N game minutes (at the normal speed one is two seconds; a pause stops them), then it is out of the room at the top of its main loop, as when caught.  The room then waits as long for the knocker: nobody else gets in, neither the one thrown out nor a computer player; the knocker walks in by itself (waiting at the door, in the building, not in the lift: once, as a click on the door).  Without a knock nobody is hurried; the knock holds only while the knocker waits -- standing where it knocked or walking to that door again; a knocker who goes anywhere else (a click on the floor, another door, the lift) takes the knock back (the one inside is told), and a room already waiting for it is free again; a second knocker meanwhile finds the room occupied, as before.  Notices over the picture, one line each: the one inside (who knocks, how long; the time is up), the knocker only at its turn (at the knock its status line says it, a notice would cover it); the knocker's status line, instead of 0x12B, tells the game minutes the one inside has left (at every click on the door anew).  Everything follows from the door checks and the world's clock (`g_clock_seconds`), the same on every machine, outside the game's memory; the day it was made in ends it.  A rule of the host's, in the game's parameters (`knock=`); not in the savegame (the host chooses it anew) | `1DA8:3643` door_check_entry (`mp_knock`, `mp_knock_held`, `mp_knock_tick`, `src/seatctx.c`) | `tests/mp/knock`, `tests/mp/knock_away`, `tools/lobbytest.py` |

## 2. Different in every mode (no switch)

Places where the original reads never-written memory ("stack garbage")
whose value the recompiled C cannot know; it uses a defined value instead.
Classic mode reproduces the original's garbage where it could be traced
(`2DD9:1147`, `0AA4:000E`, see the fixes above); these three it cannot:

| Where | The original | The recompiled game |
|-------|--------------|---------------------|
| `32BA:07EC` | a computer player's audience estimate for tomorrow adds a fifth of a stack word nothing wrote for it | adds nothing (the one known way the decompiled game can play a different game than the original; ROADMAP Phase 3) |
| `193D:2424` | a building switch case returns stale stack bytes for room type 0 | "no label" |
| `32BA:1800` | a terrorist from a consulate whose door is gone makes the sign swap write to a random address (seen: the film dealer's door destroyed, the elevator hung) | no swap |

## 3. Engine and platform (the game unchanged)

- Runs as native C: no emulated CPU except for the AdLib sound driver
  (which is code inside the data files); the game's state lives in host
  memory.
- Runs without `MTV.EXE`: the game directory needs only `SOURCE1.RSC` and
  `SOURCE2.RSC`.  What the game took from the EXE -- its initial data
  segment -- is built into `build/mtv-dec` from the EXE at build time
  (`tools/exeimage.py`); the 202 bytes of code segment it still read (the
  sound driver's signatures and stubs) are written by `src/rtl_snd.c`.
  The same game with and without the EXE (`tools/exefree.py`).  The EXE is
  still needed to build, for the machine-level build and for `--interp`,
  `--difftest`, `--stackchk`; the editor takes its font data from the game
  binary when there is no EXE (`--write-data`).
- Paced by a logical clock (function entries and I/O), so the game's speed
  does not depend on how fast the C code runs; sound in real time, paced by
  the audio device's clock (no drift, no vsync throttling).
- Sound: the AdLib chip hears each register write at least 28 us after the
  one before, as on a real card (runtime/adlib.c).  The driver times its
  waits between writes with PIT channel 2 at start-up, which the emulation
  does not count as the hardware did, so its writes came ~5 us apart; the
  chip reads a voice's key bit once per sample (20 us) and missed most of
  the "key off, key on" pairs with which the driver strikes a note again --
  about 400 of 700 such notes in the title music, many of them silent.  The
  game's own timing is unchanged (tests and recordings stay valid).
- Display: fullscreen (`--fullscreen`; F11, Alt+Enter, Cmd+F), 4:3 like a
  VGA monitor (the default; square pixels as an option), sharp scaling
  (even pixels at any size), full resolution on Retina displays.
- Display filter MMPX (`--filter mmpx`): the picture 4 times its size with
  smoothed edges and diagonals, made only of the picture's own colours
  (runtime/mmpx.c).
- Display filter HR Pixel (`--filter hr-pixel`, `[display] filter =
  hr-pixel`; decompiled build only, branch `hr`): the game drawn at 4 times
  its resolution.  Every byte of the game's memory carries a cell of 4 x 4
  sub-pixels, kept by the drawing helpers, made when a sprite bank is decoded
  and copied by far_memcpy and setmem (src/hr.c); a pixel written past them
  shows as itself, 4 x 4.  Pictures and figures keep their pixels (4 x 4
  each), frames, bars and lines stay exact rectangles, the
  text comes from a finer font drawn after the original's strokes
  (tools/hrfont/; pixel staircases read as slants, near-straight runs made
  straight; 4 6 9 ? and its turned twin drawn by hand), the statistics' bars in quarter pixels, the Betty meter in
  184 steps instead of 46, the transmitter map's coverage in true circles,
  the cards' bars and one-pixel lines half as thick (runtime/hr_rules.txt).
  Game state, time and recordings are those of the other filters
  (tools/hrtest.py, verify step 17).  The first
  version also had `--filter hr`, its pictures smoothed each on its own
  (MMPX); taken out 2026-10-18 at the user's wish -- `hr` in a settings file
  or on the command line plays as hr-pixel.
- AI upscaling (`[display] ai_pictures = FILE`, `--ai-pictures FILE`; with
  `hr-pixel`, decompiled build only): every picture sprite and room
  drawn as a pixel-art model (4x-Rebout-Blend or 8x-MS-Unpainter-De-Dither,
  CC BY-NC-SA 4.0) redrew it at 4 times its size, with its own outline from
  the model's 4x mask and 254 colours (the set's own at palette entries
  32..254, for its cells only: a few of the original's pictures use those
  indices, black in its palette).  Never shipped -- the pictures derive from the
  original's art: the launcher's card "AI pictures" makes the set on the
  player's machine from their own game files (`mtv-dec --make-ki`, ONNX
  Runtime; model, colours and runtime loaded from
  https://openmtv.net/ki/1/ for the making, each checked against its
  checksum, and removed again when it ends) and keeps only the set beside the
  settings file.  Animation frames and parts are made on what
  the game draws around and beneath them, so they fit the room
  (runtime/ki_parts.txt); pictures the game always shows together, piece
  beside piece (the bottom panel, the right monitor's frame, the title), are
  one picture for the model, so there is no seam between them, and around
  the sharp frames that lie on them (the TV's buttons) a one-pixel ring keeps
  the original's colour where it is flat.  A set made by an earlier version
  still works; the launcher marks it "made by an older version".  Each picture of the set carries the sum of the
  pixels it was made from: a sprite that differs (the tower sign, other game
  files) keeps its plain cells.  Ctrl+K (on the Mac also Cmd+K) switches them off
  and on at once, also for what is on the screen (every cell carries both).
  Display only: game state, time and network games as without them
  (tools/kitest.py).
- Mouse: captured (as the original) or free -- the system pointer moves the
  game's cursor (`[input] mouse = free`) -- or system: as free, and the
  system's own arrow is the game's, without the frame or two the game's
  arrow follows behind (`[input] mouse = system`, decompiled build only).
  The game still draws its arrow with the same calls and puts back what it
  covered, so it plays and takes its time exactly as with the free mouse;
  the busy and carrying cursors and carried items stay the game's, and its
  arrow comes back where the cursor is not under the pointer (moved with
  the arrow keys).  In a network game it stands for the arrow of the seat
  this machine shows, where the server's bundle has taken that seat's
  cursor (a few frames behind the pointer itself); the other seats' arrows
  are drawn as they are.  Test: tools/arrowtest.py (verify.sh step 9, with
  a network game).  On macOS the system pointer is hidden by AppKit as well
  (NSCursor hide, 2026-10-17): SDL 2 hides it there with an invisible
  cursor area of the window, which macOS gave up after some seconds -- the
  system's arrow came back over the game's own cursor (the game's start, a
  film carried in the planner, a satellite on the map).
- Settings file `~/.config/madtv/settings.ini` (Windows:
  `%APPDATA%\madtv\settings.ini`; display, language, classic, fixes, start
  money, fourth speed, fast room changes, AI improvement, sound, mouse); command-line options override it.
- Runs on Windows (x86-64, cross-built with MinGW-w64: `make win`,
  `play.cmd`): the same hashes as on Linux and macOS for every test script
  (`tools/hostrun.py`).  The start of `./play.sh` is `build/madtv` (the
  launcher in Go, below; `the former Python launcher (play.py)` where it is not built),
  the same on every system.
- The launcher without Python (2026-10-13): `build/madtv` (`launcher/`, Go,
  standard library only: `make launcher`, `launcher-win`, `launcher-mac`)
  starts the game, builds the content packs (`build/madtv build A+B`,
  `--mod`), serves the launcher page (port 8641: "Play" and "Multiplayer",
  the pages built in), is the content editor (the launcher's "Editor", in
  the same page; `./play.sh editor` opens it there) and the network
  game's server (`build/madtv serve`, the public one
  too); `./play.sh`, `play.cmd` and `MadTV.cmd` use it.  It starts and
  builds 2-11 times faster than the Python launcher, its server needs less
  memory and CPU.  The Windows package needs no Python any more
  (`tools/package.py win`).  The Python programs it replaced --
  `tools/play.py`, `tools/editor.py`, `tools/mplobby.py`,
  `tools/mpserver.py` -- went to `tools/deprecated/` and are gone since
  2026-10-19: the developers' start options `--lifted`, `--record`, `intro`
  are the Go launcher's, the comparison tests compare with what it gave when
  recorded (`tests/golden/`, checked against the Python programs one last time).  The editor writes a pack's files as the Python editor did
  (the keys in their order), so a saved pack differs only where it
  changed.  The protocol is unchanged: launchers of older packages meet the
  new ones in one lobby.
- The launcher in its own window (2026-10-14, branch `wails`): the same page
  and server in the system's web view (Wails v2: WebView2, WKWebView,
  WebKitGTK), no port, no browser.  Windows: `make launcher-win` builds
  `build/madtv.exe` so, the players' package has it -- without a console
  window (with arguments it writes to the command line's console); the
  package carries the Go modules' licences.  Linux and the Mac: `make
  launcher-native` / `launcher-native-app` (`build/MadTV.app`), `./play.sh`
  takes them where they are built; Linux's needs WebKitGTK 4.1
  (`libwebkit2gtk-4.1-0`) -- without it, or without a display, the launcher
  opens in the browser as before.  An icon from the game's pictures (made at build time),
  no right-click menu, dark title bars; without the game files above the
  program it asks for their folder (kept in `launcher.json` beside
  `settings.ini`, which `./play.sh` then finds too); it keeps its place and
  size; its close asks before unsaved editor changes go.
- No language prompt when the settings name a language.

- **OpenMTV** (2026-10-14): the name for players -- the launcher
  (`openmtv`, `OpenMTV.app`, `OpenMTV.cmd`), its window and page (a wordmark
  instead of the game's logo), its own icon (a television drawn by
  `launcher/icon.go`, no longer the original's art) and the packages.
  Packages for Windows, macOS 11+ (Apple silicon) and Linux x86-64; on macOS
  and Linux the game carries SDL2 itself.  A Mac package started from where
  macOS moved it ("App Translocation") takes the quarantine off itself and
  its decomp folder and starts again from its place (`launcher/macpkg_darwin.go`).
  In the packages the sound chip emulator Nuked-OPL3 (LGPL-2.1) is a
  library of its own beside the game, with its source (`OPL3_SHARED=1`).  The
  release page at https://openmtv.net/ (`site/`, `tools/release.py`).
- Every game's traces (2026-10-17): the launcher keeps the last ten games in
  `build/logs/<start time>/` -- the game's messages (`game.log`;
  `build/game.log` is the same file, so the next start no longer wipes them),
  how it ended (`end.txt`: its exit status, e.g. 3 for a halt) and, for a game
  alone, its input (`input.txt`, the game's `--record`) with its settings file
  and the game files it started from.  `python3 tools/replay.py
  build/logs/<start time>` plays it again without a window, frame by frame
  (the recording holds the game state every 1000 frames: where a replay goes
  its own way), `--shots` takes pictures on the way.  A recording keeps the
  free and the system mouse as where the pointer went (`N pointer X Y`, so a
  replay moves the game's cursor and the system arrow the same way), is
  written out every frame and names its command line.  The game's log has a
  line at every focus change of the window, with the buttons held.

## 4. Content and tools

- Content editor (`python3 tools/editor.py`): films, series, ads,
  screenplays, news stories and topics, in the game's look, as mods; entries
  can be taken out of the game and copied into free slots (it asks which slot
  it replaces), a mod or one section can be set back to the original (mod
  overview; saved only with Save), Reset discards the unsaved changes.  It starts
  from the original with the damaged data repaired (`film-200`).  `./play.sh
  --mod NAME` plays a mod, `--mod A+B+C` several together as packs (their
  new films, series and ads appended one after the other, conflicts listed
  in the launcher).  The original files are never changed.
- Editor as a work bench (redesigned 2026-10-09): a calm, dark page in a
  readable typeface (IBM Plex), the game's pixel look kept for what the game
  shows -- the cards, the TV, the headlines; a hint of the original in the
  game's red for a chosen entry and for play, its yellow for focus and change,
  the red-and-blue bars, genre colours as tape spines.  Films, series and ads
  open on their card over the room they are offered in: texts typed on the
  card, a row clicked shows its values below the card (a text: width, lines,
  the other language), beside it what is not on the card, the TV, checks and
  changes; "All fields" is the form (both languages side by side, a changed
  field shows the original value and resets alone, the market one choice).
  Undo; a search over all content.  Modes: a table for many entries (sorting,
  filters like `quality>40`, values changed in place or in bulk; films and
  series with their audience 18-24 as seven bars -- the share alone on air at
  each hour, `broadcast_phase_eval` 24B8:0C4A as `tools/catalog.py` ports it,
  from the entry as it is now; X rated before 21:00 hatched -- beside the
  card and the form as a chart with the best hour), the TV family (who
  of the five watches a film or series -- now editable for series too --
  drawn as the game draws the office's sofa: `tv_family_update` 0054:39CF
  and the first frame of `tvfam_render` 105E:32ED, faces by topicality and
  genre, at most three on the sofa, the child in bed from 23:00; pixel for
  pixel the game's picture of an evening), the news
  stories as a tree of steps (loops and ends marked), the overview (changes
  per section and entry, the new-game checks, every pack).  The TV picture is
  chosen in the game's own monitor (the bottom panel's TV, resource 9, the
  picture at +8,+2 as `tv_show_picture` puts it), with the hour's fixed
  pictures (news 236, ads 237/238) shown.  A card's year takes 1900-2099 (from
  2000 the card says 20nn, as the game prints it).  The sections are schemas
  (`tools/editor/schema.js`), the modes plug in; below 1360 px the navigation
  folds to icons, below 1180 px the side column moves under the work.
- Packs as the selection (2026-10-09): the editor shows what the launcher
  plays -- the original, or packs on it -- and never changes a combination
  of packs, the original or a protected pack (the content packs): their first
  change makes a new pack with all of the content (`mod.json` "complete",
  "from"); own packs are changed in place, protected ones only after "Change
  it directly"; own packs can be deleted (a delete button on each own pack in the
  launcher's and the overview's lists).  The original is a pack in the
  launcher's list, left out when a complete pack (which holds it) is chosen;
  the server refuses a pack played together with a complete pack that holds
  it.  Starting the game is the launcher's alone (no "On air" in the top bar).
  A pack that is not complete keeps only its own entries on disk (2026-10-19):
  the ones it adds and the original's it changes; the launcher puts the
  original's others back from the player's own game files as it reads the
  pack (`withOriginal`, `launcher/packs.go`), and the editor saves it back to
  its own entries.  The content packs hold their new films, series and ads
  alone (before: the original's 200 films, 50 series, 50 ads too, copies);
  they play as before, the same tables byte for byte.  They are no longer in
  the players' packages (the release page): `tools/package.py packs` puts them
  into a zip of their own for the developer's machines.
- The editor in English and German: its interface (labels, checks, field
  help, the server's messages) switches in its settings ("Interface"),
  apart from the language of the game texts being edited ("Game texts" in
  the top bar), and the German uses the game's own words (Film-Agentur,
  Versteigerung, Kinokasse, ...).
- More than 200 films, 50 series and 50 ads (decompiled build): a mod can
  add films, series and ads up to #999 each (the editor's "New ...", or
  Copy → new entry); the game takes the counts from the sizes of their
  tables and sizes the tables, the texts' indexes, the dealer's, the
  auction's and the ad agency's draws and the starting draws by them.  Ads
  1..50 stay in the original's data segment, the others live beside it.  A
  savegame keeps the original layout for the original counts and appends
  the others at its end ("MTVX" and the counts first); older savegames
  still load, one made with more films, series or ads than the game has (a
  pack rebuilt smaller) is refused with a warning (2026-10-15: before, its
  libraries, schedules and contracts pointed past the tables).  The original
  data plays exactly as before (the counts are read from the resource index,
  no file access).  Limits: each language's texts of a table at most 64 KB;
  a savegame at most 65 520 bytes with every option on (e.g. 999 films and
  at most 374 series): `tools/content.py` and the editor refuse a larger set,
  the game refuses to save instead of overrunning its buffer (2026-10-15);
  a table resource longer than its table is read only as far as the table
  holds; the machine-level build (`--lifted`) keeps the original counts.
- Ads without a fee are out of the game: the ad agency counts only ads with
  a fee, but with five candidates or fewer it took the first free one
  without looking at the fee (24B8:000C); now it skips them there too.  The
  original data has no ad without a fee.
- Starting pools (decompiled build): a mod can mark any films and series
  for a new game to draw from first -- each station gets one series and
  three films (any genre) from the marked ones while one is free, then
  from the original pools, the series 18-20 and the cultural programmes on
  the market (`newgame_starting_programs` 1DA8:1538; marks in an unused flag
  bit, so the original data behaves as before).  The human's first evening
  is filled as far as their programmes fit; the original draws for ever
  when none fits the last hours (only possible with changed data).  The
  editor checks that a new game finds enough free programmes -- the
  original loops for ever without them.
- Film years from 2000 on (decompiled build): the card's year byte counts
  years since 1900 and the game's card templates print "19" and two digits;
  a year from 2000 on (byte 100..199, up to 2099) prints with "20"
  (`src/status_panel.c`, the film card 14E1:4172).  The original data
  (1939-1991) draws as before.
- Launcher (editor, "Play" in its navigation): the selection (the original
  and the packs, in their order), the settings file
  (`~/.config/madtv/settings.ini`: display -- with the MMPX filter --, sound,
  mouse, language, the fixes -- all, none or chosen ones --, the start money
  with its default beside it and a reset to it, the fourth speed, the fast room changes, the
  AI improvement) edited in place, the
  start, "Build only" (until 2026-10-12 a language or "classic" for one start only); the
  selection is kept for the next visit (in the browser) and is what the
  editor shows.
- Multiplayer page (2026-10-11): network games have settings of their own
  (`multiplayer.ini` beside `settings.ini`: display and sound, made from the
  single player's the first time; the mouse is always System there, each
  network game starts with `--settings=` that file and `--mouse=system`);
  the host picks the content packs on the page (a selection of its own) and
  the rules as for a single player (language, difficulty, fourth speed, AI
  improvement, start money, the fixes -- all, none or chosen ones), kept for the next game; the
  list of open games (on the internet first) asks again by itself every few
  seconds; the public server can be changed (ours stays the default).
  "Play" and "Multiplayer" are the first group of the editor's navigation
  ("Game"); the top bar's selection button is gone.
- Launcher and multiplayer page without the side column (2026-10-12): a band
  on top holds the switch "Classic" (exactly the original: `[game] classic`,
  kept in the settings file, the choice of fixes kept beside it) and the start
  -- a big button with a second line saying how it stands, "Build only" or
  "Leave" under it --; the messages show under the band once there are any;
  "For this start only" (language, classic) is gone.  The bug fixes are a
  block of their own, folded to one line (how many are on, which are off),
  unfolded in groups (crashes, computer players, rules, texts and data) with
  what the original does; locked with a reason (blue: Classic; grey: the
  savegame's, or as the host sets it).  In the lobby: the invitation, who is
  ready, Classic and start/ready in the band (the host's button orange while
  someone is not ready, green when all are; a guest's "I am ready" orange,
  then green), the stations first, in the game's colours and names (Mad-TV
  red, Fun-TV green, Sun-TV blue), then new or saved.  Before a game the saved
  network games: a card each (its players in their stations' colours, since
  when, difficulty, packs, the latest night), its nights unfolded with each
  player's money; go on from the latest night or load any (a game opened as
  its host), rename, show the folder, delete a night or the game -- on this
  machine only.  "Quit the launcher" sits at the foot of the navigation.
- Starting (2026-10-11): `./play.sh`, `play.cmd` and the Windows package's
  `MadTV.cmd` without arguments open the launcher; any argument (or
  `--direct`) starts the game at once; `-h` / `--help` lists the options (in
  German when the settings say german).  The launcher has a button to end
  it.  The Windows package (`tools/package.py win`) carries its own Python
  (until 2026-10-13: now the launcher in Go, without Python, above).
- Multiplayer after the first evening's play (2026-10-11): the game speed
  changed by a human no longer makes the world's clock jump; the world no
  longer stands while the host is with the boss in the morning; the other
  seats go on while the host changes rooms (`tests/mp/speed`,
  `tests/mp/host_rooms`); the safe (saving and loading) stays shut with
  several humans: every machine has saved games of its own (`tests/mp/no_save`);
  a guest signing an ad no longer hangs every machine (`random_n` in another
  seat's turn); every human's station gets its first evening; a fired host
  no longer stops the day's end; a guest needs the master key for rivals'
  rooms; one pause at a time (`tests/mp/ad_agency`, `first_evening`,
  `host_fired`, `guest_locked`, `pause_both`).  With several humans the
  host's room change no longer stops the world for everyone: its wipe
  without the delay per step, the sprite decoder and the AdLib waits of the
  music start without (most of) their time on the logical clock, and
  `random_n` counts its calls into the seed (the original's draw-until-free
  loops waited for the timer's next tick).  A network game tells its players
  what happens in the window (a player left, the connection broken or lost,
  the end, a desync -- a font of its own, runtime/notice_font_OFL.txt); a
  player held up for up to a second (a window on another desktop) holds up
  nobody else, and a window nobody sees is not drawn into.  In a network game
  with the system mouse the window draws what is carried (an ad, a film)
  where the pointer is, and the game leaves its own out: that trailed the
  mouse by the server's round -- on the guest's machine too (a present comes
  in seat 0's turn: the shown seat is the one in memory, `seat_in_memory`).
  A client reads the server's messages every frame: a ping waited while it
  ran behind, the server took that for the round trip and its lead stood at
  the most (12 frames; a click came up to 0.7 s late).  The lead is the
  longest round trip and 2 frames (was 3).  A network game writes a line a
  minute on its pace; the launcher keeps the game's messages in
  `build/game.log` on every system.
- Sound (2026-10-11): it runs 100 ms ahead of the device (was 70), after a
  gap (the game stood: a window being moved, minimized, on another desktop)
  half again as much up to 250 ms, 10 ms less every minute without one.  On
  macOS a window nobody sees (another desktop, covered) is not drawn into
  -- a present there waited up to a second, and the sound broke off -- and
  the game is a latency-critical activity (no App Nap).
  `tools/mpfuzz.py`: random play of two or three humans, locally and over
  the network.
- Multiplayer page of the launcher (2026-10-11): in the LAN (this machine
  runs the server, the others find it or type its address) or on the
  internet (the public server `wss://openmtv.net`: a game is a room
  with a six-letter code, listed or not; the game's connection goes
  through the launcher).  The host sets the game (settings, content packs:
  their tables go through the server, each machine builds them from its
  own original files), everyone picks a station and says ready, the host
  starts.
- Saving network games (2026-10-12): every network game saves itself at
  each day's end on every player's machine (beside the settings file:
  `mp-saves/GAME/tag-NNN.mtvmp`, with `game.json`: the game's packs as
  their tables, its rules).  In the lobby the host chooses "a new game" or
  one of them and a night; the others fetch it through the server if they
  have no copy, and sit on its humans' stations; the rules and the packs
  are the saved game's, the language is free.  A saved game is not the
  saved game's own continuation (the computer players draw from the timer,
  as after a single-player load), it is the same day.  A network game
  started with `--new-game` (every lobby game) now has the host's player
  record from the start: without a left click on the first day the host's
  boss read the image from a null record the next morning and fired them.
- Broken connections in network games (2026-10-14): a connection that breaks
  quietly (WLAN gone, a lid shut, a NAT forgetting it) no longer stands every
  game without a word: the server takes a player silent for 8 s as lost and
  waits (it goes on when they speak again); the waiting window keeps its
  picture and counts down ("55 s more -- Enter: go on without Ben").  A game
  whose connection broke connects again by itself (up to 75 s; through the
  launcher's bridge on the internet) and takes its seat back: the server
  sends what it missed, it sends its last hashes again -- the game the same
  as without the break.  Too long away: the others go on, and the one back
  is told so.  Every connection gone at once (the server's network) no
  longer ends the game.  The launcher's page says why this machine's game
  ended; quitting the launcher keeps an internet game's bridge until the
  game ends.
- Review fixes (2026-10-15), network games: a player's seat can no longer be
  taken by someone who only knows the room's code -- the lobby gives each
  player's game a secret for its seat (`--mp-key`, sent with HELLO; older
  launchers and servers work as before, their seats unprotected); the host's
  language and fixes reach the others' games only as what they are; the
  lobby starts a game once.  A connection that broke and came back (RESUME)
  no longer gets a click or key sent just before the break applied twice on
  every machine.  The game ignores impossible values from a broken server
  (wait times, inputs outside their frames).  The server bounds what a
  client can make it keep or do (inputs 256 bytes, 16 384 per seat, no
  far-future frames once the game runs; hashes only of frames played; AT
  at most a window ahead; lobby settings 4 KB per value); a connection
  that never joins ends after 30 s, a dead WebSocket after 600 s, a closed
  room closes all its connections, a public lobby nobody spoke in for 2 h
  closes.
- Review fixes (2026-10-15), launcher: changes sent at the same moment
  (settings, a pack saved twice, two builds) no longer lose one of them or
  fail with "rename ... no such file"; a deeply nested JSON text is refused
  instead of crashing the launcher; its pages cannot be framed by another
  website (clickjacking); a setting cannot smuggle extra lines into
  settings.ini; pack names may not end in a line break, the same pack twice
  in another case is refused; a second installation no longer opens the
  first one's running launcher; on macOS the app takes the quarantine off
  itself and its decomp folder only, no longer off its whole folder (e.g.
  Downloads); only plain files are served.  settings.ini saved with a UTF-8
  byte order mark (Windows Notepad) reads right in the game and the
  launcher (before, its first section was lost for good).
- Genre filter in the editor's table (2026-10-14, user's request): films and series have
  a box "all genres" beside the filter, as in the list; the inspector's genre table
  (click a genre) sets it too.
- Required fields (editor): a film in the game (on the market, in a start
  library or the starting pool) needs both titles and descriptions, a
  genre, a length, quality, topicality (up to the quality) and a price; the
  editor marks what is missing, saves drafts, but does not put a pack on
  air (or build it) while a film is incomplete.  What the original itself
  lacks does not count (film 164, "Live Aid": length 0 -- the game's data
  fix `live-aid`, §1, gives it 2 hours while it plays).
- Found in the original data, never used: screenplays 25-33 (game shows)
  and 37-50 (series) are never offered -- nothing sets their "available"
  flag.  They are unfinished: no shopping lists, no role requirements;
  26-33 and 37-50 also no price, shooting time, base quality or
  description (25, "Guess!", has these, but no list); the series 37-50 have
  no episode length, episode price or episode values, 44 ("Mary & John")
  no episodes.  Not enabled in the original data.  Produced as they are,
  the shooting would not fail: an empty list gives quality 0
  (`studio_start_production` 229E:0B98 tests the sum of the weights before
  it divides, the studio's forecast as well).  But the computer players rate
  a screenplay at the agency by its base quality averaged over its series'
  episodes (`script_avg_quality` 2DD9:434F, from 348B:0AAF): 44 offered
  stops the game with "Divide error".  The editor now refuses both (below).
  (Corrected 2026-10-17: this said the quality formula divides by zero.)
- Screenplays in the editor (2026-10-17): their production can be edited
  -- the roles (name in both languages, actor category, the three
  requirements the fit reads: humor, appearance, skill, 0-20) and the
  shopping lists (a film's one, one for each episode of a series' series:
  its base quality, up to 80 -- x1.25 at best is 100 -- and up to 10
  entries, a role, special effects or a set, each with its weight 1-10; a
  list copied to the episodes without one); a show's "up to 10
  productions" (flag bit 2).  A screenplay the agency offers (and 24, which
  it always offers first) must be complete before a pack goes on air or is
  built, in the editor and in the launcher (`launcher/checks.go`): price,
  description in both languages, its film or series with both titles and
  a length (a series: with episodes), base quality and a list for every
  episode, at most 3 sets in the first list (no studio holds more, the game
  never takes it into one), an actor category and requirements for every
  role in a list.  What the original itself lacks does not count
  (screenplay 20: two roles without requirements).  Sets 39-41 are not
  offered for new entries: the shop's price and quality tables end at 38
  (39, "Scene No. 23", is in later lists of 13, 15 and 34 in the original
  -- kept as it is).  Bytes past a list's end stay as they were (11 and
  14 have weights there; an item 0 counts nowhere).
- Settings page (2026-10-18, user's request): what is the player's and not
  the game's -- the display (fullscreen, window size, aspect, filter,
  whole-number scaling), the AI upscaling, the sound, the mouse -- is a page
  of its own, "Settings", under "Play" and "Multiplayer": one choice for the
  single player and network games.  The launcher writes the display and the
  sound to `settings.ini` and `multiplayer.ini` at once (`sharedSettings`,
  `launcher/server.go`), copies them into `multiplayer.ini` before a network
  game and back after it (a switch to fullscreen while playing).  The mouse
  stays the single player's (network games: always System).  "Play" keeps
  the game's settings -- money, the card "Game" (now with the role fit in the
  shop, a rule of the game's start; in a network game the host's, in the
  lobby), the fixes; the multiplayer page's "Your settings" are gone.  "Build
  only" is gone from "Play" (confusing; the API stays for the editor's
  checks).
- The launcher's version (2026-10-18, user's report: the release page says it
  is at the launcher's foot, it was not to be seen): always at the foot of the
  board, as the release page writes it (the commit's first 7, the whole one in
  its tooltip) -- before, it was hidden in windows under 1360 px (the launcher's
  own window opens 1280 wide) and on a folded board, and the editor's board
  (where the launcher starts) showed it only after the page "Multiplayer" had
  been opened.  On the board of icons the number alone.
