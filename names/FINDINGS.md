# Mad TV — game rules recovered from the decompiled code

Collected while naming the decompiled functions (`names/symbols.txt`).  Every
rule below was read from the decompiled C; addresses point at the function
that implements it.  Entries marked (?) are the namers' less certain readings.
These are the formulas the Mad Channels recreation marks as fidelity
checkpoints.

## Time

* The broadcast day runs 17:00–01:00 (28 800 game seconds); `clock_hour()` is
  hours since 17:00.  Schedule slots 1..7 = 18:00–24:00; the schedule holds 2
  days × 7 slots, `9999` marks continuation blocks, series are stored as id+1000.
* Every hour: news :00–:04, programme :05–:54, commercial :55–:59
  (phase counter 0x2730, evaluation 229E:0828 / 14E1:3502).  A programme can
  be changed until minute 5 of its hour, an ad until minute 55.
* The building switches to its night picture at hour index 4 (21:00).

## Money

* New game: human player $200 000 cash, $200 000 credit line, image 34, walk
  speed 4.  Computer players $300 000 (easy/normal) or $750 000 (difficult),
  divided by (debug level + 1), walk speed 3.
* Credit interest 5 % per day (credit / 20).
* Movie price (`movie_price`): price/2 + price × topicality / (2 × max
  topicality), rounded to $1 000.  Series value works the same over produced
  episodes.  Topicality recovers every second day to the next 20 % step of its
  maximum (series episodes: 5 % steps).
* Transmitters: the price is a class: all viewer units in the circle × 61.6
  (one unit = 2 464 viewers), up to the next of {50k, 100k, 250k, 500k}
  (1DA8:3F0D; the tooltip shows it, the money display subtracts it at once,
  the day's settlement 222A:02A1 books it).  Running cost: every day a
  fifth of all the player's transmitter fees (pay_daily_costs, ledger
  "transmitter fees"; the starting network's 100 000 -> 20 000 a day; a
  computer player pays half of it from its transmitter pot).  The
  human's purchase check asks for less (see Original bugs, fix "tx-price").
  At most 32 transmitters; the last one cannot be sold.  Purchases, shopping
  lists and presents are booked in the end-of-day settlement (222A:02A1,
  222A:0424).
* News agencies: $1 000 (agency 1) or $10 000 (agencies 2, 3) per day; they
  drop you when image < 5 / 10 / 15.
* Studio rent charged daily; an empty studio is offered at 1.5 × base rent.
* Boss credit: image < 10 → thrown out; 10–49 → $200 000 once, then haggling
  for $100k/$200k; ≥ 50 → $400 000; the day's Sammy winner gets the full
  amount.  Repayment $100k / $200k / all, limited by cash and rounded down to
  $50k -- not by the credit, and a negative cash gives a negative repayment
  (Original bugs, fix "credit-repay").  In effect, image 10..49: $200 000
  (taken at the start), from day 2 +$100 000 while the credit is <= $200 000
  -- a line of $300 000; image >= 50: $400 000, +$200 000 up to $600 000.  Quitting is accepted with image ≥ 75 or a Sammy win, else you are
  fired (game over).
* A human more than $100 000 in debt is visited by a bailiff who seizes items
  and credits half their value.

## Image, ratings and audience

* Image is a zero-sum share: the three players' images always sum to 100.
  Every hour the ratings winner gains 1 and the third place loses 1 (ties: no
  change) (`image_apply_deltas`).
* Boss rating below 5 → fired.  A fired AI's points go to the others (the
  human gets half while three are active, otherwise all).  Days 2–39: the AI
  with the lowest boss rating gets $100 000 extra budget (24B8:1C8C).
* Audience model (AI estimate 32BA:03BC, same structure as the evaluation):
  pull = programme appeal (series: episode quality / reruns so far today) ×
  trend-genre multiplier / 10, split between stations airing the same genre,
  scaled by a genre × hour audience table (0x7160); news lead-in adds
  news quality × 0.85 per hour ahead × hour audience / 5; total share ≤ 100 %;
  viewers = share × reach / 100 × 2 464.  X-rated films count as 0 while
  hour % 7 < 4.
* Coverage map: 168 × 108 px, 2-bit population density weighted {0,1,10,100},
  2 464 real viewers per unit; a transmitter stamps a 32 × 16 px ellipse.
* Ad minimum-audience classes (table 0xA8, byte +0xA of an ad): 0, 250k,
  500k, 1M, 2.5M, 5M, 10M, 25M, 50M, 100M viewers.  Ad offers are limited to
  ads whose minimum is ≤ 40 % of the largest station's reach.  Unfulfilled
  contracts charge their penalty at the deadline.  Hour ad slot result:
  1000+ad = failed, 2000 = done, 3000+ad = sent OK.
* "TV family" panel (105E:32ED): up to 5 viewers chosen by the programme's
  target-group bits; moods at rating thresholds 10/20/30.

## News

* Stories are chains of template nodes (0x2731): 7 running stories, each
  moves after its countdown to one of the node's 4 random successors (9999 =
  story ends).  New stories start with countdown 3 and pick a random phrase
  variant 1..10 per topic part (1C18:08A2).
* A news item's value = node value × (30 + 70 × average phrase value / 10) /
  100; price class = how many of {15, 30, 40, 50, 60, 100} lie below the value.
  Today's pool holds 20 items, the oldest is dropped (1C18:0490).
* Hourly: stories age by one hour, at least `min_cat1` political items are
  forced (day start: 5 hours, 3 items), the pool goes to every inbox subscribed
  to the item's category.  Values in inboxes and broadcast slots drop to 85 %
  every hour, and to 0 at day start (1C18:1022).
* A player's news score = average value of the 3 broadcast slots (1C18:0BCB).

## Market

* While no station reaches 12 M viewers, up to 3 dealer offers are forced to
  cost ≤ $60 000.  Unsold auction items return to the market after 3 days.
* Film auction requires image ≥ 10.  Film ids < 1000 are movies, ≥ 1000
  series; +10000 marks a just-bought film (full refund on return).
* Library: 12 genres × 4 slots per player ("(Full)" at 4 of a genre).
* Auction minimum raise: 10 % of the current bid rounded down to its leading
  digit (1C18:17E7).  Available money = cash − unpaid new transmitters − own
  bids (the human also loses lots nobody bid on when leaving the agency).
* Series episodes wear by quality × wear / 20 per airing, minimum 1
  (1C18:172F).
* X-rated ads require image ≥ 20; actors require image ≥ 10, fees > $5 000
  need ≥ 15, > $10 000 need ≥ 20; actor cost = fee × episode multiplier.

## Production

* Quality (229E:0B98, forecast 05FF:3DDF): for 10 cast slots actor score =
  rating × weight / 100; scene/equipment from a quality table; quality =
  base × Σscore / Σweight × 1.25 (%).  Forecast text: > 80 good, > 60 neat,
  > 40 poor, else catastrophe.  Film price becomes price × quality / 100; the
  quality also counts towards the Sammy when that category runs.

## Sammy award and Betty

* Sammy (`sammy_daily_update` 1DA8:4481; g_sammy_day DS:7490, category
  DS:7488, points DS:748A + 2p).  A three-night cycle: the night where
  g_sammy_day % 3 becomes 0 draws the category -- before day 5 news or
  culture (50 % each), from day 5 one of the four (25 % each); the next day
  (% 3 == 0) the boss announces it and nobody scores; the day after
  (% 3 == 1) is the only day that counts; that night (% 3 == 2) it is
  awarded; the day after no Sammy runs.  The game starts with no category:
  the first call while the new game is set up draws it, news or culture
  (seen: culture in 3 of 4 starts).  So: 1st Sammy counted on day 2, 2nd
  (news/culture) on day 5, 3rd (any of four) on day 8, then every 3 days --
  shows and series at the earliest in the 3rd.  Checked in the game: a news
  hour on day 1 scores nothing; with the counter poked to the counting day
  it scores.
* Points on the counting day: 1 the best news -- each hour's news share; 2
  cultural engagement -- +10 per hour with a culture film (genre 6) on air;
  3 self-produced shows / 4 self-produced series -- the quality of each
  production when its shooting starts in one's own studio (bought
  programmes do not count).  Score = points (only if > 0) + Betty love / 2
  (news, series), + love (culture), + love / 3 (shows).  The highest score
  wins; a tie extends it by one counting day, no points → not awarded.
  The winner gains {8, 4, 6, 6} image by category, the others lose half of
  it each (all of it with only two stations in the game).  No money: the
  option `--sammy-prize` adds a cash prize (`docs/CHANGES.md`).  The
  messages (0x158 the winner is another, 0x159 you won, 0x15A a tie, 0x2B
  not awarded) go to `g_player` only; a network game tells every human.
* Betty: each present adds its love value, which then drops to 70 % for the
  next time; love is capped at the player's image; the plastic-surgery voucher
  resets love to 0.  Love ≥ 20 → master key (enter rivals' rooms); dropping
  below 20 loses it.  Love ≥ 100 + honeymoon trip → the winning ending
  (105E:447B, "Just Married").
* Betty's culture programme (movie #58) aired daily earns a love letter;
  three days without it → the movie returns to the auction and love halves.

## Events, AI and sabotage

* From day 3: 1/3 chance per day of a terrorist from one of the two consulates
  or a bailiff at the movie agency.  Bombs destroy productions or library
  films and cost the room owner $50 000; bombing the rival consulate empties it.
  The $50 000 (1DA8:2D3B) only hit a room with an owner, i.e. through swapped
  signs at the information board; the amount is also written into text 0x135
  ("$50.000" in both languages) -- option `--terror-damage` (`docs/CHANGES.md`).
* A "moral guardian" confiscates an X-rated movie aired too early.
* Live shows are lost if not aired on their date; live movies #29 and #187
  recur 3 and 5 times.
* Information board (room 13): swapping signs redirects anyone navigating by
  the board; the AI uses this to send bombs/bailiffs to the human's rooms
  (32BA:1800).
* Catching a rival's figure in your room pays a bounty that doubles each
  time, up to $200 000.
* Difficulty controls the AI's programme re-planning passes (32BA:1432).
* Measured (tests/baseline, 100 days, normal): both computer players run
  deep into debt (-2.27M / -0.78M) while expanding reach; only the human is
  ever visited by the bailiff, so AI debt has no consequence.
* Computer player loop (2DD9:000C, 2DD9:3C0D): states 0..12 walk through
  own office, ad agency, movie agency, news room, rivals' rooms (with Betty
  love ≥ 20), production errands, supermarket (gift budget ≥ 30 000), Betty,
  information board, porter (10 %).  It acts only after waiting 300 ticks at
  the door.
* AI budget (2DD9:26EA): pots gifts, films, news, production, transmitters,
  other; base weights {5, 28, 14, 18, 35, 0} × priorities from the daily
  ranking (image, Betty love, library, reach share > 28 % / < 20 %, rating),
  normalised to 100 %.  Image rank sets how many deals per visit (2 × rank).
* AI film purchase score = blocks × topicality × 20 / (price / 1000) with
  topicality ≥ its average quality and < 4 programmes of the genre
  (2DD9:1A3F); auction rebids while the same value is > 2 (2DD9:2386).  It sells
  films whose quality + 3 is below its average once it owns ≥ 14 hour blocks.
* AI ad contracts (2DD9:33CB): < 6 planned spots per day, < 8 contracts, no
  genre requirement, minimum audience as % of reach ≤ its image.
* AI schedule: maximises genre × hour audience × topicality per empty hour,
  replaces weak programmes and upgrades ads hour by hour (2DD9:0349, 0710, 0B13).
* AI news agencies at day start: political if image ≥ 5, show if ≥ 10 and
  news budget > 20 000, sundry if ≥ 15 and > 40 000.  Transmitter search
  scans a 150 × 90 grid (step 3) for the largest new reach within budget.

## Engine notes

* Room ids (dispatcher 05FF:000A): 1 office, 2 news, 3 boss, 4 library,
  5 studio, 8 film agency, 9 screenplay agency, 10 estate agent, 11 ad agency,
  12 supermarket, 13 information board, 14 Betty, 25 transmitter map,
  31 planner, 58 load/save, 63 shop, 64 porter, 65 new-game setup.
* Text: language string table G32(0x8390 + 4n) from resource 199 (German) or
  253 (English).
* Formatting (`str_format` 0E35:0000, the game's sprintf): '#' codes d, z
  (one byte of the arguments), l, s, v (glyph 0xC0 + n), f (gauge), with pad
  flags '0', '_' (pads with '{'), '+c' and a width or '*n' (pixel cut).  An
  unknown code prints "Fuck".  Every call passes as many argument words as
  its format reads (all 200 call sites, both languages).  Text 87 (an
  auction minimum-bid question, used nowhere) reads #s #l #l in English but
  #l #s #l in German.
* Save games are resources inside the .rsc files (slots 0xD2–0xDB, names in
  0xD1); the resource manager rewrites them in place.
* Sound: detector resource 99; driver 98 if a device is found, else 95;
  digital samples off if resource 300 (SOURCE2) is missing; one sample per room.
  The "samples" are AdLib songs: driver 98 is "V1.3 - AdLib (TM) soundcard",
  95 "V1.1 - PC-Beeper" (with it g_snd_digi stays 0: no room music).  The
  PALLADIX drivers have 13 entries, reached through far-jump stubs at
  371E:0083 + 5n that 371E:00E5 patches: 0 init(port), 1 shutdown, 2 name,
  3/4 set/get the tick block (16 words counting at 100/50/25/10 Hz; the
  game's g_timer_ticks is word 4), 5 output on/off, 6 load song (byte 3:
  0 nine melodic channels, 1 with rhythm section, 2 effects bank), 7/8 song
  pointer/size, 9 play (4 start, 8 fade out, 2 stop; with a fade time),
  10 status, 11 sound effect, 12 play n times.  All take stack arguments
  only.  The driver hooks int 08h and sets the PIT to 100 Hz (or the song's
  tempo); its handler counts the game's ticks and plays the song (Phase 3
  stage 6, src/rtl_snd.c).
* Two compression schemes: sprite banks = order-1 context Huffman (37B4);
  intro clips = LZW with growing 9-bit codes, delta frames with opcodes
  'b' end, 'c' run, 'd' skip (365F:000B).
* Blitters clip only vertically; the cursor is drawn on VGA over a saved
  background, eased by 1/5 of the distance per frame; Enter = left click,
  Esc = right click, arrows move the cursor with acceleration 2..16.
* Hidden minigame "Madris" (Tetris clone) on the office computer: 10×20 board,
  7 pieces, points O10 S8 Z8 T6 J6 L6 I4 × (9 − fall delay), ends at 9 999 or a
  full top row, speeds up every 10 lines, keeps a high score.

## Savegames

Found with tests/scenarios/save_*.txt.  The safe in the human's office
(hotspot 0x1E, the picture left of the door) opens room 58: save 0x39 /
load 0x3A, 10 slots = resources 0xD1+n of SOURCE1.RSC (names: 0xD1, 10 × 40
bytes); the setup screen's "Load game" opens it in load mode.  A save is one
resource of 37 605 bytes (layout: 229E:139A game_save_load; the start state,
res 0x2B, has the same layout):

| Offset | Contents |
|--------|----------|
| 0 | films 1..200 × 18 bytes: quality, topicality, owner, flag bits (+6 bits 5/7, +7 bits 0/1), price, live day/slot |
| 3600 | series 1..50 × 41 bytes: 11 episode triples, owner, flags, price, next episode |
| 5650 | screenplays 1..50 × 353 bytes: state and the 10 episode blocks |
| 23300 | ad holder, 1 byte × 50 |
| 23350 | about 75 DGROUP blocks (14 255 bytes): building (figures, elevator queue, floors/doors, door index), player records and finance, programmes (transmitters, library, schedules, ratings, contracts, screenplays, statistics, bag), news, markets (studios, auction, dealer, ad and screenplay offers, shopping lists, gifts, actors), day, `g_player`, clock, game speed, all computer-player tables |
| 37485 | the 3 manager names, 40 bytes each |

Not restored by a load: the elevator cabin (DS:8BDA floor, 012A door, 0128
offset), the timer/PIT phase and the random seed offset
`g_cursor_sprites[6]` (DS:8EAC; the clock value at "New game", 0 after a
load from the setup screen), `g_clock_pause_depth`, UI/session state, and
film flag +7 bit 2 "self-produced" (set at run time, never saved; read from
the code).  The wandering figures are saved but keep walking in the safe
while the clock is paused.  So a loaded game continues like the saved one
for the human but not for the computer players: their random draws are
seeded from the timer (`random_n`), whose phase is not part of the save.

* Every save appends a new copy to SOURCE1.RSC (res_create always seeks to
  the end); the slot's old copy becomes dead space, so the file grows
  without limit (on the original floppy: "Disk full!" eventually).
* Every save contains uninitialised stack bytes (film record bytes 2 and
  9..12 and the high nibble of byte 4, series byte 0x22 high nibble,
  screenplay bytes 0x15F..0x160); loading ignores them, byte-wise
  comparisons of savegames must mask them.
* The overlay text 285 is a developer placeholder (an English note on
  how big the load/save overlay must be).

## Original bugs

* `random_n(0)` (0D5E:0229) divides by zero → "Divide error" (fix "random0");
  seen after a key press at a specific moment of the title sequence (README).
  The game code's only variable bounds are the door counts of floors 2..13
  (background figures, 193D); in a running game every one of those floors
  has a door.
* 1DA8:2D3B (bailiff / terrorist in a library or office): the item count
  sums genres 2..12 (`di` is incremented before the read) while the random
  pick only draws genres 1..10.  When the struck player's library holds only
  series (genre 11), the game hangs in an endless loop — seen with a bailiff
  (debt) and with a terrorist bombing the library; a library with only
  genre-1 films is never hit.  Found by long computer-player runs
  (tools/ai_run.py), confirmed in the machine code.  Fix "intruder" counts
  genres 1..10 (tests/fixes/intruder_*.txt).
* 193D:2424: a switch case returns uninitialized locals (stale stack bytes)
  for room type 0; `src/` returns NULL ("no label") there.
* 32BA:07EC (AI audience estimate 32BA:03BC): the news lead-in (`bp-4A`) is
  computed only for today's hours (index <= 7) but added for every hour, so
  for tomorrow's hours the estimate adds a fifth of whatever an earlier call
  left on the stack (seen: 13883 -> +2776 attractiveness, that station takes
  the whole share).  Found with `--stackchk`.  Where the word comes from
  (last writer, measured on the four affected scenarios): mostly this
  function's own locals from an earlier call -- from the same caller or from
  one of its 13 call sites at another depth (ai_tick runs from game_frame,
  which is called at many depths) -- but also temporaries of Borland helpers
  (LXMUL@/LDIV@, farmalloc) and of the assembler blitters (377A, 3762, 3786,
  37A2) that drawing code ran at that depth in between (ending_fired: a
  pushed DS, 0x3B84 -> +3047; a blitter temporary 51 -> +10).  It depends on
  what was drawn and computed since, and at which depth; the decompiled
  build keeps none of these stack writes, so it cannot be tracked: `src/`
  adds no lead-in in every mode (no fix switch).  Measured again 2026-10-09
  (`STACKCHK_ALL=1`, ending_fired): 3 848 such reads in the first 8 400
  frames, mostly of 0; writers include the function's own store at 07D1
  from calls at other call sites (2DD9:1293, 2DD9:0BAE, 32BA:0D6D) and
  38F7:000D inside an interrupt frame.  The one accepted exception of
  ROADMAP Phase 3's exit criterion.
* 2DD9:1147 / 2DD9:1197 (AI fills an empty hour, 2DD9:0DD9): the half-day
  range of the odd-length check (`bp-12` .. `bp-11`) is set only in the
  series branch, which comes last; the films before it are checked against
  stale stack bytes.  Found with `--stackchk`.  The bytes are the stack word
  40 below the SP at which ai_tick (2DD9:000C) calls its state functions,
  and its last writer is always in the same ai_tick: door_check_entry
  (1DA8:3643), called by ai_ready_at_target (2DD9:3985) only with DI = 1,
  saves that DI there in its prologue -- range 1..0, the scan of the half
  day never runs and the odd-length rule never rejects a film.  If
  ai_place_live_programs (2DD9:4719) places a live film before, its far
  call of sched_mark_scheduled (1DA8:004E, at 2DD9:4844) pushes the return
  segment there instead (load segment + 2DD9 = 2F89: range -119..47).  The
  other calls in between stop 4 bytes above it.  `src/` tracks that word
  (ai_stale_range in ai.c); fix "ai-odd-range" uses the half day of the
  hour (tests/fixes/ai_odd_range.txt).

  The difftest reports a result that depends on a never-written read as
  `UNDEF` (src/README.md).
* 348B:098D: the AI's studio rental passes the loop counter (always 13)
  instead of the chosen studio.  "Studio 13" is the props price table
  `g_prop_prices` (DS:6235): the computer player pays three times a "rent"
  read from there, raises it by half there, writes its number into it and
  never gets a studio, so a computer player that has lost its starting
  studio never produces again (tests/fixes/ai_studio.txt; fix "ai-studio").
* Estate agent (05FF:3CAA, 05FF:3D3D): after giving up a studio whose
  production still runs, or renting one behind a running production, it shows
  "Warning:" with text 0x77/0x78, which are the door signs "Psychiatrist" /
  "Bio-control" (German "Psychiaterbüro" / "Bio-Control").  Fix
  "estate-text": text 242 (when the production ends) / 113.
* 2DD9:0DD9 (2DD9:1289): the AI's audience estimate is called with the
  programme category ([bp-17h], 1..11) instead of the hour being filled
  ([bp-19h]); in 15 original days the accept/reject decision against the
  computer player's minimum flipped 32 of 231 times, X-rated films of
  categories 1–3 and 8–10 always estimate 0.  Fix "ai-fill-hour".
* 2DD9:162A (2DD9:16B7): the series check adds `(char)1000` = −24 to the id
  (`mov byte [bp-16h],0E8h`), so an ad's genre rule is checked against film
  id−24 (or memory before the film table); only matters for contracts with
  genre exclusions and unscheduled series.  Fix "ai-series-rule".
* 2DD9:3085 (2DD9:30B8 `inc si` before the read): average series topicality
  sums episodes 2..n+1 (slot n+1 is empty or, for 10 episodes, the episode
  count), so computer players undervalue most series (series 12: 60 instead
  of 67).  Fix "series-avg".
* 32BA:1800 (AI sabotage at the information board): event_find_target_sign
  writes the target only if a door with the event's target room exists.  A
  terrorist from a consulate whose door is gone (bombed, or rented as a
  studio -- studios 7 and 9 sit behind the consulate doors) leaves the
  target as never-written locals, and the sign swap writes a (floor, door)
  pair to a random address.  Seen in a 100-day run with fix "ai-studio": the
  film dealer's door became a "studio", a computer player walked to floor 0
  and the elevator waited for it for good (game hangs).  `src/` sets the
  target to "none" first (no swap); a human renting studio 7 or 9 can reach
  the original bug as well.
* 0AA4:24C3 (library: case → shelf): taking a programme off the library shelf
  into the case withdraws it like a sale (1C18:12F2 program_withdraw: owner 0,
  "available" set); putting it back on the shelf files it again but restores
  neither, so the dealer's refill (24B8:04B9) treats it as on the market.
  Reproduced after the steps of tests/scenarios/room_library_browse: with the
  dealer shelf emptied, `call 24B8:04B9` offered film 66 in slot 1 while it
  was still in the human's library.  Fix "library-owner" sets the owner when
  the programme is filed again (tests/fixes/library_shelf_owner.txt).  Not
  covered by it: a tape carried in the case stays ownerless -- fix
  "case-owner", see "A tape in the case" below.
* 26A7:292F (news room headlines): the text cache has 24 entries (20 inbox
  items, the 3 broadcast slots, the dragged item) but the lookup searches the
  first 20.  With a full inbox, broadcast slots 2 and 3 show slot 1's
  headline unless their item is also in the inbox (reproduced on day 3 with
  20 inbox items: nodes 173 and 174 showed node 169's text; fix "news-cache"
  searches all 24, tests/fixes/news_headline_cache.txt).  1C18:1022
  (news aging) ages count + 1 inbox entries, one past the last (harmless:
  the next item is copied over it).
* 05FF:2C02 (transmitter purchase): the money check asks for the new viewer
  units × 61.6 (1954 × 61.6 ≈ $120 000 in room_txmap_buy), but the
  transmitter costs its price class ($250 000 there: the tooltip, the money
  display and the settlement 222A:02A1 all use it; the computer players
  compare their budget with it too), so the human can buy a transmitter he
  cannot pay for.  Fix "tx-price" checks the price class
  (tests/fixes/tx_price.txt).
* 0AA4:000E (boss room, both paths into the end sequence: fired, and "I
  quit"): `callf 35F8:058F` (snd_room_music_stop) without pushing its two
  arguments (room `[bp+6]`, player `[bp+8]`), so it reads the two words
  above its return address: the DI and SI that 0AA4:000E's prologue saved,
  i.e. its caller's registers.  A left click reaches the boss from the main
  loop through 05FF:000A, a right click or Esc through room_escape
  0054:2741; neither touches SI/DI, so they are the main loop's (0054:0005):
  SI = 60 (left over from its start-up loop over 60 figure sprite entries),
  DI = 1 (its "button held" flag, set just before it dispatches a click).
  So the call is snd_room_music_stop(room 1, owner 60): it records room 1
  and stops the boss room's sample only if the last room recorded was 0 --
  in tests/scenarios/ending_resigned the music plays on into "THE END".
  Other ways in see other registers: the space key while the boss's answer
  is shown (game_frame 0ED7:0004 DI = 0, hotkey handler 0054:085E SI = 0x20)
  and the end of the day (0054:20E0).  `src/` passes the emulated CPU's
  DI/SI, which the decompiled code leaves to the machine-level main loop
  (and which under the difftest are the original's own); fix "boss-music"
  passes (g_room, g_room_owner) and stops the sample
  (tests/fixes/boss_music.txt).  Found with the logical-clock trial.
* 2DD9:0DD9's never-written half-day range (2DD9:1147 above) also decides
  how often it calls 348B:0003 (ai_count_short_programs): with the
  original's range 1..0 only for series candidates, with the half day (fix
  "ai-odd-range") for films too (once vs. three times on one input).  Since
  `src/` reproduces the range, 348B:0003 counts on the logical clock again.
* Verified 2026-10-11 (the former "readings from the code, not tested"),
  fixed 2026-10-12 behind the switches named below (tests in tests/fixes/):
  * **A tape in the case** (05FF:090A room_library_click → 1C18:12F2
    program_withdraw): a tape taken off the library shelf into the case has
    owner 0 and flag 0x20 ("on the market") for as long as it is carried;
    library_film_to_bag 0AA4:2491 only appends it.  Every dealer_refill
    24B8:04B9 meanwhile can put it on the dealer's shelf (candidates: flag
    0x40 clear, owner 0 or > 3, 0x20 set, price != 0; uniform random pick).
    Refills run when leaving the film agency (room_escape 0054:2741:
    auction_commit_bids, dealer_refill, then bag_unload_to_library
    222A:061E -- the only place the case is unloaded, which sets the owner
    of every case item), at every day change (new_day 0054:1F6F refills all
    10 slots) and at every computer player's dealer visit (ai_tick 2DD9:000C
    film agency step 1).  Tapes bought from the dealer or won at auction
    are safe (0x20 clear / owner set).  Reproduced
    (tests/fixes/case_owner.txt): enter the agency with the tape, leave
    without buying -- the dealer offers it, the unload files it back.  In
    longer runs (--logical-time): the human buys his own tape back (paid
    twice, a duplicate library entry); carried over midnight, computer
    player 3 bought it while it stayed in the human's case, and the next
    unload filed it into the human's library too: one tape in two libraries
    (program_withdraw then clears it from both when either side sells or
    loses it).  On the agency's shelf a carried tape counts as "just
    bought" (its id is in a dealer slot): dropping it on the dealer refunds
    instead of selling (filmagency_return_film 0AA4:2586, booked as a
    negative purchase).  Chance per refilled slot ~1/87 on day 1, ~10 % per
    day change while carried, rising as the pool shrinks.  Fix
    "case-owner": the owner is set back right after program_withdraw in
    05FF:090A (0x20 stays, a sale to the dealer sets owner 0 itself); that
    also covers the shelf put-back of "library-owner".  Swapping refill
    and unload in room_escape alone would not be enough (day change, AI
    visits).
  * **0DD9 genre mask** (2DD9:12C1 `mov al,[bp-7]`, ai_fill_program_slot):
    a candidate is tested against the genre bit of the current best's
    *category* (`l_7`), not its own genre (`l_8`); the siblings 0349, 0B13,
    08D1, 162A all test the programme's own genre.  The first fitting
    programme becomes the best (the fallback) before any test, and the
    best so far decides: while its genre is allowed, excluded genres pass;
    once an excluded one is best, nothing replaces it.  A love film (category 1) is often the fallback, and "no
    love films" is the commonest exclusion (ads 5, 20, 47): the computer
    player airs a love film in exactly the hours its contracts forbid, and
    no ad can be booked there (tests/fixes/ai_fill_genre_mask*.txt: 22:00 gets
    love film 88 and no ads; with the right index action film 129 and ad
    13).  Fix "ai-fill-genre": bit l_8 - 1.
  * **0DD9 best-audience row** (2DD9:1316): the best's score reads
    g_genre_audience (DS:7160, row = genre, series 11..15) with the best's
    category: right for films, but a series best always reads row 11
    (family series: highest at 18-19 h, lowest from 22 h), so late in the
    evening a worse series replaces a better one
    (tests/fixes/ai_fill_series_row.txt: 23:00 gets family series 12 instead
    of whodunit 21, about 9x fewer viewers).  Rare: needs two unscheduled
    series.  Fix "ai-fill-genre": the best's genre (series genre + 10).
  * **348B:0895 ai_library_avg_quality**: the loops run over genres 0..11
    and slots 0..count (`jge` at 348B:0918 / 0961), so every call adds 12
    phantom entries: sum / (n + 12) with clean slots; a full genre counts
    the next genre's first programme twice, a full series shelf reads the
    count bytes at +0x60 as a series index (256/512/768/1024, 512 lands in
    heap text), and a withdrawn programme left behind in the slot past the
    end keeps being counted.  The result decides ai_plan_budget's "worst
    quality" flag (film pot share): it follows library size, not quality.
    In a 22-day idle game the flag was wrong on 17 of 42 AI-days (film pot
    10-15 points too low).  tests/fixes/ai_lib_quality.txt: player 2 with the
    best films (true 29) scores 9 < 10, 11.  Fix "ai-lib-quality":
    `count > slot`.
  * **24B8:170C** (broadcast_phase_eval, programme phase): the hand-over of
    a studio to its queued renter pushes the player loop index (`[bp-34h]`)
    instead of the studio (`[bp-32h]`, as the twin in 1DA8:034E does):
    studio_take_over(p, renter) gives the renter studio 1, 2 or 3 (the
    Laundry room, the Psychiatrist's or the Bio-control door) -- a public
    room becomes his studio, he pays 3 x 1.5 x its rent, a renter of that
    studio loses it -- and the studio he queued for stays free with
    reserved set (below).  Needs a queued renter: only the human queues,
    for a free studio that is still "running" -- what a bailiff or
    terrorist (1DA8:2D3B) leaves.  Fix "studio-takeover": push the studio
    (`[bp-32h]`); tests/fixes/studio_takeover.txt.
  * **Studio reserved byte** (+0x0F; 0 = nothing pending, 0xFF = give-up
    deferred, 1..3 = renter queued): written only by 229E:100C
    studio_request_rent and 229E:104A studio_give_back, never cleared --
    not by studio_take_over 229E:1080, studio_release 229E:1264 or the
    bomb.  A leftover 0xFF releases the studio from its next renter at the
    end of each of his productions (tests/fixes/studio_reserved_ff.txt); a
    leftover queue q blocks the studio for good (the plan ignores clicks,
    the tooltip says "already renting", nobody can rent it again once
    freed), every later series episode end calls studio_take_over(s, q)
    again (q pays 3x again, the rent rises 50 % each time, the screenplay
    is wiped) and every show from it triggers 24B8:170C
    (tests/fixes/studio_reserved_queue.txt).  Not the cause of the studios
    that are never offered (renter 0, below).  Fix "studio-reserved":
    reserved = 0 in studio_take_over and studio_release.
  * Side findings (2026-10-11, not reproduced on their own): a bombed
    studio (1DA8:2D3B) keeps "running" and its end time, so 1DA8:034E later
    finishes screenplay 0 for renter 0xFF and prog_give_to_player(0,
    0xFFFF) writes into "player -1's library" (seen: computer player 2's
    news budget DS:6EAD 1470 -> 0); studio_release 229E:1264 stores the
    unfinished screenplay of a given-up studio in owned_scripts but never
    increments g_scripts_owned_count (it is lost); film 138 "Trendy" aired
    by anyone matches every running studio in 24B8:0C4A (24B8:16D5 cmp
    8Ah; reproduced 2026-10-22: a studio given up mid-production is freed
    at once, the production lost, its end day later finishes screenplay 0
    over a computer player's news budget -- tests/fixes/trendy_studios.txt;
    Trendy's own studio is already the screenplay-target match, screenplay
    24 -> film 138; fix "trendy-studios"); 0DD9
    reads a series' topicality one episode early (series_episode_at
    1C18:1116 before placing: a never-aired series counts 0); 348B:02A3
    gives a fully booked contract genre mask 0 (read from the code).
* 2DD9:362D: computes a per-genre block total whose result is never used (?).

* Five bugs found by the multiplayer inventory (2026-10-10, branch
  `multiplayer`), each with a fix and a test
  (tests/fixes/):
  * 1DA8:3643 `door_check_entry`: the catch bounty is paid only when figure
    0 catches (`a0 == 0`); the only caller with a1 = 0 is the human's door
    check, so a human on station 2 or 3 never gets it (fix "catch-bounty").
  * 2DD9:3C0D `ai_next_goal`, state 5: the sneak visit goes to the rooms of
    owner 1 -- the human only on station 1; otherwise computer player 1
    visits its own rooms and the other one visits it (fix "ai-visit").  The
    state-5 abort in `ai_tick` already tests the human's floor.
  * 24B8:0C4A `broadcast_phase_eval`, ad phase: a computer player that
    aired film 138 "Trendy" copies g_trends into g_ai_trends indexed by its
    player number instead of its AI index (found just before and then
    discarded): player 2 writes g_ai_odd_today (DS:78B4..), player 3 also
    g_ai_pending_gift (DS:78BC: a gift for Betty nobody bought); no computer
    player learns the trends (fix "ai-trendy").  With a series genre (11..15)
    as the second trend the pending gift is 11..15, past gifts 1..10:
    `ai_give_betty_gift` (32BA:1331) then decays "gift 11"'s love byte and
    counts its use -- DS:72A8/72A9, the human's news count (20 -> 270) --
    and `news_age` (1C18:1022) walks 270 inbox slots through the data
    segment every hour until a value above 127 makes its price-class search
    (signed bytes at DS:11EE) endless: the game hangs.  Found with
    tools/ai_run.py --offset 7 (day 11, all other fixes on) and a write
    watch on the trace build.
  * 2DD9:26EA `ai_plan_budget`: the rival is g_ai_player(1 - g_ai_cur), but
    `ai_new_day` plans both computer players with the same g_ai_cur, so for
    one of them the rival is itself: its image rank is never 1, its reach
    share counts its own reach twice (fix "ai-rival").
  * 2DD9:4126 `ai_walk_to`: when the goal is the door the figure already
    targets, "arrived" (act without the 300-tick wait) is set only if the
    walker is AI index 0's player; AI index 1 waits again (fix
    "ai-arrived").
* 048C:025B (film text cache, room 4): `g_film_txt_cache` (DS:63E5) has 49
  slots of 10 bytes (id, title, description); for the player's own library
  it fills slots 3.. with the case's films and then every archive film,
  genre 1..10, with no bound (3 + 10 + 40 = 53).  Nothing refuses a film
  for a full genre (`archive_genre_full` 1DA8:06EA is called nowhere), it
  stays in the case, so 47 films in archive and case (series do not count)
  are reachable: full archive and 7 films in the case, or 37 and 10.  Slots
  49..52 then lie on the series cache's slot 0 (DS:65CF: id + 22 string
  pointers).  Reproduced (decompiled build, harness pokes: 40 archive
  films, 10 in the case): the last four Sundry films (5, 16, 19, 42) were in
  slots 49..52, `movie_title` 1DA8:33A6 returned NULL for them and their
  card showed an empty title and no description; series slot 0 held id 5,
  film 5's title and description as pointers 0/1 and, since 10-byte slots
  do not line up with 4-byte pointers, halves mixed with film ids
  (0008:0010, 0008:7D7D, 0013:7D8F, ...); with series 5 in the archive its
  episode list read "Once upon a time in the West": "$", empty, empty,
  "Paris, Texas", "The everpopular Wim Wender...".  From the code:
  `series_title` 1DA8:344E searches slot 0 first in every room, so the wrong
  title stays until slot 0 is loaded again (the TV's now-on-air title when
  a series is not cached, a studio report); slots 49..52 are never freed
  (`txt_film_cache_free` frees 3..48), about 8 strings leak per visit; no
  crash path found -- slots 0..2 are freed only right after slot 0 was
  loaded, so the mixed pointers are never freed.  The planner (0x1F) loads
  3 + 40 + films aired today that are no longer in the archive: overflow
  only in theory.  Fix "title-cache" (tests/fixes/title_cache.txt).
* 05FF:0359 (film agency, the case; found by the user 2026-10-12, "a film in
  the corner of the case that is not mine"): hotspot 0x21 of room 8 is the
  whole case frame (216,24)-(279,103) (the frame tiles of `ui_draw_bag`
  geometry 0x12EC: tiles 27..34 x 3..12), the click takes slot
  (x-224)/9 + 5*((y-28)/30) -- 7 columns, 3 rows, 0..16 -- while the tapes
  are drawn 5 by 2 from (224,32), and asks only whether g_bag + 2*slot is
  non-zero (the library, 05FF:090A, asks slot < g_bag_count).  g_bag is 15
  words (the seat block carries 0x1E bytes); 10..14 are never written,
  slot 15 is g_program_score(0) (DS:24A1, never written), slot 16 --
  x 278..279, y 88..103, the frame's lower right corner -- g_program_score(1)
  (DS:24A3): station 1's programme score of the hour (topicality x the
  genre's audience at that hour / 100, + news score / 5; 0 when nothing or
  nothing much airs), taken for a film id.  tooltip_update (14E1:000E) has
  the same lookup, so hovering the corner shows that film's card -- without
  a title, since room 8's text cache holds only the case, shelf and auction
  films.  Taking it: `bag_remove(16)` shifts nothing and counts the case
  down (0 -> -1); dropping it on Mr. Jabble sells it at its price (or, for
  an auction film, puts it into a free auction slot with the human as
  seller and pays the price at once), owner 0.  Repeatable: the score stays
  for the hour, the film changes with it.  Reproduced with build/mtv
  (scratch script: new game, walk into the agency, wait until 20:00): the
  starting schedule's series scores 4 -> film 4 (action, USA 1988, X-rated,
  $160 000, no title), sold three times in ten game minutes (200 000 ->
  680 000; twice into auction slots 9 and 10, then to the dealer), the case
  count -1, -2, -3.  The negative count: `filmagency_film_to_bag` and
  `library_film_to_bag` write g_bag[count] -- a film bought next went to
  DS:247D (g_ai_tx_scan of computer player 3, +0x3A) and the count back to
  -2: paid, never in the case, never filed (`bag_unload_to_library` loops
  while slot < count).  In a network game the seat block swaps g_bag and
  its count but not the scores: every seat's corner holds station 1's score.
  Same unchecked pattern, harmless: the ad agency's case (05FF:33F6, slot up
  to 10 = the next player's contract 0) and the screenplay agency's
  (05FF:380B, slot 5) only let a programme bought today be given back -- an
  out-of-range slot ends in the "already signed" / "No returns" warning.
  Fix "case-corner": slot < g_bag_count and column < 5, click and tooltip
  (tests/fixes/case_corner.txt); always on in a network game
  (tests/fixes/case_corner_mp.txt).
* 0AA4:000E (boss, "repay credit", g_boss_menu 5; found by the user
  2026-10-12: "in debt, repay all, and the boss gives money"): the amount is
  the choice ($100 000, $200 000, or g_credit for "all"), lowered to the cash
  if the cash is smaller (signed), then (i32)/50 000 x 50 000 -- truncated
  toward zero; cash -= amount, g_credit -= amount, ledger "credit repaid".
  With a negative cash the amount is negative: cash -230 000 -> -200 000
  "repaid", cash -30 000, credit 200 000 -> 400 000.  Reproduced with
  build/mtv (image 34, whose line is $300 000): -1 000 000 -> cash 0,
  credit 1 200 000, the boss: "There! And now back to work, Archie!" (text
  0x3D; 0 gives 0x3C, "You're one of a kind! ...").  Nothing checks the image, so even image < 10 (thrown out on
  "more money") borrows this way; and the cash never stays below -50 000,
  so the bailiff (cash < -100 000 at the day change, 1DA8:276D) never
  comes; the interest (5 % of the credit a day) is "repaid" the same way.
  Nor is the amount capped at the credit: credit 50 000, "repay $100 000"
  -> credit -50 000 (reproduced), and pay_credit_interest (1DA8:0F5B:
  money -= credit / 20 while the credit is not 0) pays 5 % a day on it;
  "$200 000" again and again deposits any sum (1 M -> +50 000 a day), and
  "more money" (credit <= $200 000) takes it back $100 000 at a time.
  Fix "credit-repay": also at most g_credit, and never below 0
  (tests/fixes/credit_repay.txt, credit_repay_over.txt); always on in a
  network game (credit_repay_mp.txt).

* 2DD9:2B8D (ai_eval_yesterday, mode 0; found 2026-10-14 measuring the AI
  improvement's satellites): a computer player's average rating of yesterday
  is the mean over the 7 hours of viewers x 100 / (reach x 2464), the
  product `imul`-ed in 32 signed bits.  Above 21 474 836 viewers in an hour
  it overflows: a rating of 0 or below (g_avg_rating, DS:24F7).  The
  original's networks never get there; with a satellite (half the map,
  about 110-160 M) a good hour does.  ai_take_contracts (2DD9:311D) then
  takes only ads with a minimum audience <= reach x 2464 x rating / 100 --
  none -- and ai_plan_budget (2DD9:26EA) ranks it last in ratings.
  Measured (tools/ai_run.py --human-plan satellite --ai-improve strong, 10
  games x 60 days): with both satellites the computer players' ratings sank
  to about 1 % (some days below 0), their cash to -75 M on day 60.
  tests/fixes/ai_rating.txt.  Fix "ai-rating": the product in 64 bits.

* 2DD9:1DDA (ai_execute_purchases, an auction bid; found 2026-10-14: the
  measurement's games halted with "unknown opcode" in the sound driver's
  timer handler): a computer player remembers its bids in g_ai_bids
  (DS:756A + player x 0x42: 11 places of 6 bytes, lot and the money it
  reserved; count g_ai_bid_count DS:7730 + player x 2).  Every bid appends an
  entry -- the same lot again after the other computer player outbid it --
  and only 2DD9:2386 removes one, when the human outbids it and it gives up.
  Won, lost or refilled lots stay.  An active bidder passes 11: player 3's
  12th entry lands on g_scripts_owned_count (DS:7672), then g_last_hour,
  the 21st on g_player_ad_count (DS:76A8: a reserve of $280 000 = 0x445C0
  made the human's count 17 856), then g_hour_newsviewers.  adagency_refill
  (24B8:000C) then marks "the human's contracts" 0..17 855 as held: ad ids
  from garbage, writes far past the ad table -- into the sound driver's code,
  whose next timer interrupt runs garbage.  Rare in the original (the
  computer players' film budget is small); with the AI improvement and fix
  ai-rating's ad income 18 of 70 test games (60 days) halted.
  tests/fixes/ai_bids.txt.  Fix "ai-bids": a lot already in the list keeps
  its entry (the reserve added); 10 lots, so 11 places always do.

* 229E:0225 (sched_place_program) with 2DD9:4719 (ai_place_live_programs;
  found 2026-10-14, the measurement's other halts: "machine code at
  0D0D:0D0D / 0909:0909 / 0303:0303 not in this build"): the placement's
  refusals -- the hour is over (text 0x2D), it is being shown (0x2E), a live
  film off its day/hour (0x31) -- call ui_warning: room_enter(-1) and a
  message box (ui_box mode 0: popup_open saves the background) on the
  human's screen, also when a computer player places.  A computer player's
  live film whose hour is over today stays unscheduled, and
  ai_place_live_programs tries it again at every office visit: every try
  opens another box, none is closed (the human's click closes one).
  popup_open takes the first free slot without a bound and bitmap_alloc
  gives 0 once memory runs out; grab_screen then copies the screen to
  0000:xxxx, over the interrupt vectors (DS-independent, colour bytes 03, 09,
  0D), and the next timer interrupt jumps there.  In a played game the
  human sees "Warning" boxes he did not cause.  tests/fixes/ai_warning.txt.
  Fix "ai-warning": no box for a computer player (the placement is refused
  as before).

## Observations (original behaviour, not confirmed as bugs)

Seen while writing the room scenarios (tests/scenarios/) and the catalogue
(tools/catalog.py); intended or not, a remake should decide.

* Transmitter map, sale dialog (mode 4): a right click throws the transmitter
  away, the same as "Throw away".
* News studio: a right click while a news item is dragged only ends the drag
  (room_escape → 26A7:1DF8): an item taken from the inbox is gone, one taken
  from a broadcast slot was already paid for.
* Planner (05FF:0D0E, dropping a programme): the search for the next
  non-zero programme word from the drop slot has no upper bound; dropping
  into empty trailing slots reads past the 15 programme words into the
  episode and ad arrays.
* 1DA8:0CB3 (auction lot): series get a lot code `random_n(7)` = 1..7, films
  4..10, and the code is also stored as the programme's owner, so a series in
  the auction can carry a player number as owner (at game start series 21
  sat in lot 4 with owner 1).
* Estate agent (05FF:3AB7): only studios with renter 0xFF are offered.
  Studios 1–5, 7, 9, 11 and 12 start with renter 0 (behind other rooms'
  doors) and can never be rented by the human; the computer players'
  348B:098D treats 0 as free.
* 1DA8:034E: after a film of a multi-production screenplay (flag bit 2) the
  studio stays "running"; a later give-up is deferred (reserved 0xFF).
* Unused content: headline nodes 65, 96, 99, 171, 185 and 186 have text but
  no story reaches them, node 169 ("Documents seized by bailiff Miller.") is
  never used; screenplays 25–33 and 37–50 are never offered (flag bit 1
  clear), 26–33 also have no texts — their programmes exist but cannot be
  produced.  Headline 124 has #1 in English but #1, #2 in German.
* Film 164, "Live Aid" (live, auction pool), has length 0 -- the only film
  without one.  The computer players' value of a lot (2DD9:20BA, 2DD9:2386:
  hours x topicality x 20 / bid in thousands) is 0, so they never bid; its
  date is `random_n(8 - length)` (1DA8:0CB3) = evening hour 1..8, and hour 8
  (1 a.m.) never matches `(slot - 1) % 7 + 1` in 229E:0225, it is lost;
  placed, 229E:0225 clears no hour (a programme under it keeps its 9999
  continuation hours, which then air as Live Aid); aired, 24B8:0C4A never
  sees its last block (the block count starts at 1), so it is not used up and
  1DA8:135D takes it away at night as missed (text 0x113).  The other ten
  live broadcasts' prices are ~$6 965 per hour and quality point: $700 000 at
  quality 50 is 2 hours.  Data fix "live-aid" (tests/fixes/live_aid.txt).
* Screenplay 20, "Video-Connection" (film 134, a show), asks nothing of its
  two roles (host, category 5; star guests, category 7): their requirements
  at +0x168 are all 0, the only such roles of the 50 screenplays.
  script_role_match_pct 1DA8:2560 returns 0 for a role whose requirements sum
  to 0, so the shooting (229E:0B98) counts only the set: quality 3 at best of
  its base 20.  The requirements are no part of a savegame (only the episode
  blocks are).  Data fix "video-connection" (tests/fixes/video_connection.txt).
* Film flags: `flags2` bit 2 (value 4, set on all live broadcasts and shows,
  and by 229E:09AF/1DA8:034E on a production of a screenplay with flag bit 2)
  means "used up when aired" (24B8:0C4A: bit 2 or genre live, at the last
  block -> program_withdraw) -- the production's price is then 0 (229E:0B98)
  and the studio cards show episode counts; for live broadcasts it is
  redundant with the genre.  The low five bits of `flags` (1..28 on every film but 58
  and 164) are never read by the code.
