# Scenario tests

Each `NAME.txt` is an input script (frame-indexed key/mouse events), each
`NAME.expected` what the machine-level build — the original program's
behaviour and timing — prints for it: memory/VRAM hashes at the checkpoints
and the values of the script's `peek`/`call` lines.  `verify.sh` runs
`tools/scenario.py check`, which per scenario requires:

1. the machine-level build prints `NAME.expected` exactly;
2. the decompiled build, run under the per-function differential test
   (`--interp --difftest`), has no decompiled function that differs from the
   original on the way;
3. that difftest run prints `NAME.expected` as well (up to a
   `# difftest-until` cut): after each test the state is rewound and the call
   runs normally, so the difftest plays the recorded game and not another one;
4. the native decompiled build — no interpreter, no per-function test —
   plays the same game as the machine-level build when both run on the
   logical clock (`--logical-time`): same game state (`STATE`: data segment
   below the stack + film/series/screenplay tables), screen and peek/call
   values at every checkpoint.  Instruction time cannot be compared between
   these engines (the decompiled build only approximates the original's
   instruction counts, and the game seeds its random numbers from the
   timer); the logical clock counts function entries and I/O, which both do
   alike.  Where the original computes with stack garbage the decompiled
   build cannot reproduce, `# logical-until FRAME` ends the comparison and
   the header says why.

| Scenario | What it covers |
|----------|----------------|
| `smoke` | a short recorded session (`./play.sh --record`) |
| `ending_fired` | new game, the human only dismisses dialogs; fired on the morning of day 8 → setup screen |
| `ending_resigned` | walk to the boss, quit with image ≥ 75 → "THE END" |
| `ending_married` | give Betty the honeymoon trip with love ≥ 100 → "Just Married" / "The Happy End" |
| `room_boss_credit` | boss: repay the whole credit, take a $200 000 credit, haggle for another $200 000 (test poke image 50), repay $100 000 |
| `room_betty_gift` | supermarket presents catalogue (buy + refund, buy a fur coat into the case), give it to Betty (love 0 → 10); test poke love 20 → master key |
| `room_txmap_buy` | transmitter map: another player's view and info panel, buy a transmitter (reach 2044 → 3998), sell the old one to a computer player's bid (test poke AI budget) |
| `room_board_porter` | information board: swap "Office Archie" / "Office Brian"; porter's gossip; the elevator board ("go" mode) then sends the human to Brian's office |
| `room_adagency_sign` | ad agency: take two offers, give one back, take one back out of the briefcase, sign; "already signed" warning → contract 0 = ad 10 |
| `room_planner_schedule` | office → planner: library film onto 18:00 (the replaced film and series episode move on), waste bin, series onto 23:00, tomorrow's page, move a programme |
| `room_news_buy` | news studio: subscribe to agency 2, buy three news into the broadcast slots, swap two, leave; the 18:00 news block counts viewers |
| `room_evening_air` | ad agency → planner (films swapped, ad placed/moved/removed, fastest speed); 18:00–19:05 run: ratings, ad aired and paid, statistics chart, finance report |
| `room_dealer_buy` | film agency: programme info, buy a film, buy and return one, tape out of the case and back, leave (case unloaded into the library), buy a series, "Not enough money!" |
| `room_library_browse` | own library: genre list, programme info, scheduled film/series refused, film shelf → case → shelf, series episode list |
| `room_dealer_sell` | library → film agency: carry an own film in the case and sell it to Mr. Jabble; a just-bought tape dropped on him is refunded |
| `room_auction_win` | film auction: bid, withdraw, bid, too-expensive lot, win at the day change (clock poke), sell the won film back into the auction |
| `room_scripts_buy` | screenplay agency: talk to the agent, buy two screenplays, return one (refund), leave (shelf refilled), come back: "No returns" |
| `room_estate_rent` | estate agent's studio plan: a slot that is not a free studio is not offered; give up studio 6 (no, then yes), rent it back at 1.5× rent |
| `room_studio_shoplist` | studio: screenplay too big, insert, swap, bin, take out and back; assistant: shopping list, production info, start refused, cancel and reissue the list |
| `room_supermarket_shop` | supermarket "Equipment": cast an actor (replace one, refund), buy a set; presents: buy, return, buy one into the case; costs booked on leaving |
| `room_studio_produce` | studio: quality forecast with full cast, start production (end hour poked to 18:00); the show lands in the library, "production completed" message queued |
| `save_roundtrip` | office safe (room 58): buy a film, save to slot 2, buy another, clock runs on, load slot 2: money, clock, library, film owners, computer player 3's money and AI states as at save time (the elevator cabin floor is not restored) |
| `save_continue` | save, play 4000 frames, load, replay the same input: player 1 ends the same, the computer players differ; with the unsaved elevator and the wandering figures poked back only a random draw (timer-seeded `random_n`) differs |
| `save_slots` | setup screen "Load game": back out, empty slot does nothing, load the shipped slot 1; save twice into slot 3 (each save appends a new copy to SOURCE1.RSC, index repointed), loading gets the second |
| `sammy_win` | the human wins a news Sammy at the day change (test pokes: category, points): image +8, the others −4 each; the boss: "At least we got the Sammy", a credit he otherwise refuses ("since you won the Sammy today"), and quitting with image 42 accepted -> THE END |
| `event_bailiff` | $150 000 in debt at the day change -> the bailiff comes for the human (23:00 clock poke), seizes a film from the library and credits half its value |
| `event_terrorist_board` | sabotage by the human: the terrorist's target sign (consulate "People's Duban") swapped on the information board with Fun-TV's library -> the bomb goes off there: Fun-TV loses a film and $50 000 |
| `event_ai_sabotage` | sabotage by a computer player (test call of its board step `32BA:1800`): it swaps the terrorist's target with the human's studio -> the human loses the studio and $50 000 |
| `german_boss_credit`, `german_betty_gift`, `german_news_buy`, `german_evening_air` | the English room scenarios replayed in German (`--args "d"` in the header): same steps and checked values, German texts in the dialogs, news, statistics and finance screens |

The comments at the top of each file list the steps with their frames and
the observable values (which the `peek` lines print and `.expected` holds).

## Test harness script lines

Besides the recorded input (`key`, `mouse`, `move`, `down`, `up`) a script
may use, all addresses as `SEG:OFF` with SEG relative to the load segment:

| Line | Effect |
|------|--------|
| `N click X Y` / `rclick X Y` / `goto X Y` | home the cursor, move to (X, Y); press once the game has seen the cursor there (≥ 10 frames), release ≥ 5 frames later (`goto`: move only) |
| `N poke16 SEG:OFF VALUE` | write a word: set up conditions that would take weeks of play |
| `N peek8\|peek16\|peek32 SEG:OFF` | print `PEEK frame N SEG:OFF value` |
| `N call SEG:OFF[,ARG...]` | call a game function (far, word arguments), print `CALL frame N … ax dx` |
| `N hash` | print the memory/VRAM hashes (`scenario.py add` inserts them) |
| `N shot FILE.png` | save the screen (PNG or `.ppm`) |
| `N quit` | end the run |

Lines must be in frame order.  DGROUP is `39D4`; far tables: films
`4DA5:0008` (0x17 bytes each), series `4EC7:0008` (0x39), screenplays
`4F7E:0008` (0x1B3).  Things the game does that a script has to allow for:

* While something is dragged the game hit-tests the image's top-left corner,
  i.e. the cursor minus the drag hot spot; in the planner's waste-bin mode the
  click point is the cursor + 5.
* While a transmitter is carried the mouse is clamped to the map
  (48,4)–(200,97), so `goto X Y` lands at (48+X, 4+Y).
* Betty's reply waits in room −6: only a right click returns to her office.
* Walking takes game time: leave enough frames after a door click (or a
  poke of the figure's walk target, DS:7B48/7B54/7B4A/7B46) before the
  next click in the room.

## Adding a scenario

```sh
./play.sh --record /tmp/office.txt         # play; the machine-level build records
python3 tools/scenario.py add /tmp/office.txt office
python3 tools/scenario.py check office
```

or write the script by hand (as the `room_*` ones were) and `add` it the same
way.  Short sessions are better than long ones: one or two rooms each, so a
failure points at a small piece of the game.  After a deliberate runtime
change `python3 tools/scenario.py update` re-records the expected output and
says what changed.

`check` also writes `build/scenario_coverage.txt`: the decompiled functions
whose difftest passed, with the scenarios that reached them.  Every call the
run makes is tested (the first four per function), nested calls included,
except where the test has to be abandoned: a screen transition or wait, a
file write, or a loop on `random_n()` until a condition holds (`new_day`,
`auction_daily`, `adagency_refill`, …) — the timer is frozen during a test,
so such a loop never ends.  The calls inside an
abandoned function are still tested one by one.
