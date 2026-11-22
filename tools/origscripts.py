#!/usr/bin/env python3
"""The original's unfinished screenplays made playable (game rule "Unfertige Drehbücher integrieren",
--original-screenplays / [game] original_screenplays).

The 1991 data holds 23 screenplays the screenplay agency never offers (nothing sets their
flag bit 1): the shows 25-33 have roles and actor categories but no requirements and no
shopping list (26-33 neither price, shooting time, base quality nor agency text), the
series 37-50 have their titles, descriptions and every episode's title and text, nothing
else (no roles, no lists, no price, episode length 0; series 44 "Mary & John" has 8 episode
titles and 0 episodes -- offered, a computer player's rating of it stops the game with
"Divide error", script_avg_quality 2DD9:434F).  The values below fill them; the rule's
proposal page: https://claude.ai/artifact/1Xbwnfn2s74QTyweAdQajS (user's OK 2026-11-17,
the shows' casts a touch harder: the best actor fits 92-95 %, the highlights 30 and 33 100 %).

This file is the one source: it writes the game's table (src/origscripts_data.h,
src/origscripts.c applies it) and the launcher's (launcher/origscripts.json: the editor
shows the screenplays so when the rule is on).  Both apply a screenplay only while the part
of it a savegame does not hold (price, flags, target, shooting time, the roles'
requirements and categories) is the original's -- a pack's own screenplay stays its own.

    python3 tools/origscripts.py           write both files (from the player's SOURCE*.RSC)
    python3 tools/origscripts.py --check   are they up to date (verify.sh, buildtest)

Role requirements: (humor, appearance, skill) -- the three bytes the fit reads (the
record's second byte stays 0).  Lists: (item, weight); items 1-15 roles, 16 the special
effects, 17-38 sets.  A show has one list, a series one per episode."""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import content  # noqa: E402

SETS = {16: 'Spezial-Effekte', 17: 'Sesselrunde', 18: 'Phantasia', 19: 'Zirkus', 20: 'Wüste', 21: 'Saloon',
        22: 'Indianercamp', 23: 'Gefängnis', 24: 'Straßenzug', 25: 'Bar', 26: 'Wohnzimmer', 27: 'Schlafzimmer',
        28: 'Garten', 29: 'Raumschiff', 30: 'Weltall', 31: 'Planet', 32: 'Schiff', 33: 'Strand', 34: 'Kabine',
        35: 'Küche', 36: 'Hafen', 37: 'Büro', 38: 'Flugzeug'}
S = {v: k for k, v in SETS.items()}
FX = 16

def L(*xs):
    """a list: R<n> -> role n, a set name -> its item"""
    out = []
    for x, w in zip(xs[0::2], xs[1::2]):
        out.append((int(x[1:]) if isinstance(x, str) and x[0] == 'R' and x[1:].isdigit() else S[x] if x in S else FX if x == 'FX' else x, w))
    return out

# ---------------------------------------------------------------- shows (film targets 139-147)
SHOWS = {
    25: dict(kind='Quiz', price=25000, dur=3, q=25, keep='Preis, Drehzeit, Basisqualität und deutscher Text sind original',
             desc=None, desc_en='Contestants have to guess words and can win a lot of money.',
             roles={1: (16, 10, 14), 2: (18, 18, 8), 3: (18, 18, 8), 4: (18, 18, 8), 5: (10, 14, 10), 6: (10, 14, 10)},
             lst=L('R1', 10, 'R2', 4, 'R3', 4, 'R4', 4, 'R5', 3, 'R6', 3, 'Phantasia', 4)),
    26: dict(kind='Skandal', price=160000, dur=8, q=45, fsk18=True,
             desc='Zynische Moderatoren, Musik und viel Kunstblut: die geschmackloseste Show aller Zeiten. Erst ab 21 Uhr!',
             desc_en='The most tasteless show in TV history: cynical hosts, music and lots of fake blood. A scandalous hit after 9 p.m.',
             roles={1: (18, 10, 14), 2: (14, 12, 10), 3: (8, 14, 12), 4: (8, 14, 12), 5: (8, 14, 12), 6: (10, 16, 10), 7: (10, 16, 10), 8: (10, 16, 10)},
             lst=L('R1', 10, 'R2', 7, 'R3', 4, 'R4', 4, 'R5', 4, 'R6', 5, 'R7', 5, 'R8', 5, 'FX', 10, 'Phantasia', 6)),
    27: dict(kind='Quiz', price=30000, dur=3, q=22, add_roles={2: 7, 3: 7},
             desc='Gemütliche Weinprobe mit prominenten Gästen. Billig produziert und ideal für den späten Abend.',
             desc_en='A cosy wine tasting with celebrity guests. Cheap to make and ideal for late evenings.',
             roles={1: (14, 10, 14), 2: (12, 12, 6), 3: (12, 12, 6)},
             lst=L('R1', 10, 'R2', 5, 'R3', 5, 'Bar', 5)),
    28: dict(kind='Unterhaltung', price=45000, dur=4, q=28,
             desc='Verkupplungs-Show für einsame Herzen. Ein charmanter Moderator macht aus jedem Paar einen Lacher.',
             desc_en='A dating show for lonely hearts. A charming host turns every couple into a laugh.',
             roles={1: (8, 16, 4), 2: (14, 4, 6), 3: (14, 4, 6), 4: (14, 4, 6), 5: (14, 4, 6)},
             lst=L('R1', 10, 'R2', 5, 'R3', 5, 'R4', 5, 'R5', 5, 'Sesselrunde', 4)),
    29: dict(kind='Spiel', price=35000, dur=4, q=24,
             desc='Spiele-Show rund um Tischsitten. Zwei Moderatoren treten gegeneinander an - der Anstand verliert immer.',
             desc_en='A game show about table manners and etiquette. Two hosts compete - good manners always lose.',
             roles={1: (16, 12, 6), 2: (10, 12, 10), 3: (12, 10, 10)},
             lst=L('R1', 10, 'R2', 8, 'R3', 5, 'Küche', 4, 'Wohnzimmer', 3)),
    30: dict(kind='Highlight', price=480000, dur=14, q=64, star=True,
             desc='Samstagabend-Show mit Wahnsinnspreisen. Mit der richtigen Besetzung ein Quotenhit.',
             desc_en='The big Saturday night show: crazy prizes for crazy stunts, stars and music. A ratings hit with the right cast.',
             roles={1: (20, 8, 16), 2: (18, 10, 12), 3: (16, 16, 12), 4: (16, 16, 12), 5: (16, 16, 12), 6: (16, 16, 12), 7: (16, 16, 12),
                    8: (16, 14, 8), 9: (16, 14, 8), 10: (16, 14, 8)},
             lst=L('R1', 10, 'R2', 8, 'R3', 6, 'R4', 6, 'R5', 6, 'R6', 5, 'R8', 5, 'R9', 5, 'FX', 8, 'Phantasia', 8)),
    31: dict(kind='Trash', price=12000, dur=3, q=16, fsk18=True,
             desc='Billige Trash-Show mit Haustieren und ihren Besitzern. Geschmacklos, aber das Publikum schaut hin.',
             desc_en='A cheap trash show about pets and their owners. Tasteless, but the audience watches.',
             roles={1: (16, 12, 6), 2: (10, 8, 4), 3: (10, 8, 4), 4: (10, 8, 4)},
             lst=L('R1', 10, 'R2', 4, 'R3', 4, 'R4', 4, 'Garten', 3)),
    32: dict(kind='Kinder', price=120000, dur=9, q=38,
             desc='Kinder-Quiz im Raumschiff-Look mit echtem Außerirdischen. Walkmans und Videospiele als Preise.',
             desc_en="A kids' quiz in a spaceship look with a real alien as co-host. Walkmans and video games as prizes.",
             roles={1: (18, 10, 14), 2: (10, 12, 10), 3: (14, 12, 18), 4: (14, 12, 8), 5: (14, 12, 8), 6: (10, 14, 10), 7: (10, 14, 10), 8: (10, 14, 10)},
             lst=L('R1', 10, 'R2', 6, 'R3', 8, 'R4', 4, 'R5', 4, 'R6', 4, 'R7', 4, 'FX', 8, 'Raumschiff', 6, 'Weltall', 5)),
    33: dict(kind='Highlight', price=800000, dur=20, q=76, star=True,
             desc='Bombastische Stars, Kulissen und Effekte - die teuerste Show des Jahres. Nur mit Spitzenbesetzung ein Hit.',
             desc_en='Bombastic stars, sets and effects - the most expensive show of the year. A hit with a top cast only.',
             roles={1: (20, 8, 16), 2: (18, 10, 12), **{t: (16, 18, 14) for t in range(3, 11)}},
             lst=L('R1', 10, 'R2', 8, 'R3', 6, 'R4', 6, 'R5', 6, 'R6', 6, 'R7', 6, 'FX', 10, 'Phantasia', 8, 'Zirkus', 8)),
}

# ---------------------------------------------------------------- series (series targets 37-50)
# roles: t -> (name_de, name_en, category, (humor, appearance, skill)); eps: [(base quality, list)]
SERIES = {
    37: dict(price=150000, dur=18, ep_price=50000, critics=5, press=6, family=13,
             desc='Westernepos um den Treck der Familie Williams und einen geheimnisvollen Colonel. Solide Abendunterhaltung.',
             desc_en="A Western epic about the Williams family's trek and a mysterious colonel. Solid evening entertainment.",
             roles={1: ('Ben Williams, Treckführer', 'Ben Williams, trek leader', 1, (8, 14, 16)),
                    2: ('Martha Williams', 'Martha Williams', 2, (10, 16, 10)),
                    3: ('Colonel Kurtz', 'Colonel Kurtz', 1, (14, 12, 18)),
                    4: ('Benny, Scout', 'Benny, scout', 1, (12, 8, 10)),
                    5: ('Der neue Scout', 'The new scout', 1, (6, 10, 14)),
                    6: ('Captain', 'Captain', 1, (4, 12, 12)),
                    7: ('Häuptling', 'Chief', 1, (6, 14, 14)),
                    8: ('Der Goldsucher', 'The gold digger', 1, (20, 10, 8)),
                    9: ('Sarah Williams', 'Sarah Williams', 2, (8, 16, 8)),
                    10: ('Jimmy Williams', 'Jimmy Williams', 3, (14, 6, 4))},
             eps=[(30, L('R1', 10, 'R2', 6, 'R4', 6, 'R6', 5, 'R7', 5, 'Wüste', 6, 'Indianercamp', 6, 'FX', 8)),
                  (32, L('R1', 10, 'R2', 6, 'R5', 8, 'R10', 4, 'Wüste', 6, 'FX', 5)),
                  (34, L('R1', 10, 'R3', 9, 'R2', 5, 'R5', 5, 'Wüste', 6, 'Saloon', 4)),
                  (35, L('R1', 10, 'R3', 8, 'R5', 6, 'R2', 4, 'Wüste', 7, 'FX', 8)),
                  (36, L('R1', 10, 'R3', 9, 'R7', 6, 'R2', 5, 'R10', 3, 'Indianercamp', 6, 'Wüste', 5, 'FX', 9)),
                  (34, L('R1', 10, 'R3', 9, 'R2', 6, 'R9', 5, 'Wüste', 5)),
                  (36, L('R1', 10, 'R3', 8, 'R8', 8, 'R5', 4, 'Wüste', 6, 'FX', 5)),
                  (38, L('R1', 10, 'R3', 8, 'R2', 6, 'R9', 5, 'R10', 4, 'Garten', 4, 'Wüste', 4)),
                  (40, L('R1', 10, 'R3', 10, 'R2', 6, 'R9', 5, 'R10', 4, 'Straßenzug', 6, 'Saloon', 5))]),
    38: dict(price=360000, dur=30, ep_price=120000, critics=7, press=8, family=17, star=True,
             desc='Aufwendiger Drogen-Krimi, gedreht an Schauplätzen in aller Welt. Mit Spitzenbesetzung ein Straßenfeger.',
             desc_en='A lavish drug-trade thriller shot on locations all over the world. With a top cast it empties the streets.',
             roles={1: ('Gavras', 'Gavras', 1, (12, 14, 18)),
                    2: ('Baker, Dealer', 'Baker, dealer', 1, (8, 10, 16)),
                    3: ('Morrison, Bankier', 'Morrison, banker', 1, (6, 16, 12)),
                    4: ('Inspektor Hale', 'Inspector Hale', 1, (10, 8, 14)),
                    5: ('Nadia, Informantin', 'Nadia, informer', 2, (10, 18, 12)),
                    6: ('Kapitän des Frachters', "Freighter's captain", 1, (14, 6, 12)),
                    7: ('Entführer', 'Hijacker', 1, (4, 8, 14)),
                    8: ('Boss des Rings', 'Boss of the ring', 1, (10, 12, 18)),
                    9: ('Der Rivale', 'The rival', 1, (12, 10, 12)),
                    10: ('Kommandoführer', 'Squad leader', 1, (8, 12, 14)),
                    11: ('Stewardess', 'Stewardess', 2, (8, 18, 6))},
             eps=[(48, L('R1', 10, 'R4', 8, 'R2', 5, 'R9', 5, 'Straßenzug', 6, 'Bar', 5, 'FX', 9)),
                  (50, L('R1', 10, 'R2', 9, 'R4', 5, 'R11', 4, 'Flugzeug', 6, 'Straßenzug', 5, 'FX', 7)),
                  (52, L('R1', 10, 'R2', 6, 'R6', 8, 'Schiff', 7, 'Kabine', 6, 'Hafen', 5, 'FX', 6)),
                  (52, L('R1', 10, 'R6', 8, 'R5', 6, 'Hafen', 7, 'Schiff', 6, 'FX', 7)),
                  (54, L('R1', 10, 'R2', 8, 'R3', 7, 'R5', 6, 'Hafen', 5, 'Büro', 5)),
                  (56, L('R1', 10, 'R7', 8, 'R11', 6, 'R3', 4, 'Flugzeug', 8, 'FX', 9)),
                  (55, L('R1', 10, 'R8', 9, 'R7', 6, 'R5', 5, 'Wüste', 8, 'Gefängnis', 5, 'FX', 8)),
                  (57, L('R1', 10, 'R8', 8, 'R9', 8, 'R5', 5, 'Büro', 5, 'Straßenzug', 6, 'FX', 8)),
                  (58, L('R1', 10, 'R9', 8, 'R5', 6, 'R2', 5, 'Straßenzug', 6, 'Gefängnis', 4, 'FX', 7)),
                  (60, L('R1', 10, 'R8', 9, 'R10', 8, 'R9', 6, 'R2', 5, 'Gefängnis', 5, 'FX', 10))]),
    39: dict(price=60000, dur=10, ep_price=30000, critics=1, press=5, family=16,
             desc='Sechs blutige Gruselgeschichten für die Zeit nach Mitternacht. Billig gedreht, mit Kultpotential.',
             desc_en='Six gory horror tales for the hours after midnight. Cheaply made, lots of fake blood - cult potential.',
             roles={1: ('James, Butler', 'James, butler', 1, (14, 6, 10)),
                    2: ('Lady Ashford', 'Lady Ashford', 2, (6, 16, 8)),
                    3: ('Diamantenhändler', 'Diamond dealer', 1, (8, 10, 12)),
                    4: ('Matthews, Buchhalter', 'Matthews, accountant', 1, (12, 4, 14)),
                    5: ('Graf Theophil', 'Count Theophil', 1, (10, 18, 10)),
                    6: ('Jakob, Diener', 'Jakob, servant', 1, (12, 8, 8)),
                    7: ('Der Ertrunkene', 'The drowned man', 1, (2, 14, 6)),
                    8: ('Der Irre', 'The maniac', 1, (20, 8, 2)),
                    9: ('Mädchen', 'Girl', 2, (4, 14, 8)),
                    10: ('Spaziergänger', 'Passer-by', 1, (8, 4, 2))},
             eps=[(26, L('R1', 10, 'R2', 6, 'R9', 4, 'Wohnzimmer', 5, 'Küche', 4, 'FX', 8)),
                  (28, L('R3', 10, 'R2', 5, 'R6', 4, 'Wohnzimmer', 4, 'FX', 8)),
                  (30, L('R4', 10, 'R9', 6, 'R10', 3, 'Straßenzug', 5, 'FX', 9)),
                  (32, L('R5', 10, 'R6', 8, 'R9', 4, 'Schlafzimmer', 5, 'FX', 7)),
                  (30, L('R7', 10, 'R10', 5, 'R9', 4, 'Strand', 5, 'FX', 8)),
                  (34, L('R8', 10, 'R9', 7, 'R10', 3, 'Garten', 4, 'FX', 10))]),
    40: dict(price=140000, dur=14, ep_price=65000, critics=6, press=7, family=3,
             desc='Frankie will nach oben - und geht dafür über Leichen. Bissige Aufsteiger-Saga in fünf Teilen.',
             desc_en='Frankie wants to get to the top - and walks over dead bodies to get there. A biting rise-and-fall saga in five parts.',
             roles={1: ('Frankie', 'Frankie', 1, (14, 14, 14)),
                    2: ('Tom', 'Tom', 1, (12, 6, 6)),
                    3: ('Fred', 'Fred', 1, (10, 8, 4)),
                    4: ('Ginger', 'Ginger', 2, (8, 18, 6)),
                    5: ('Ale', 'Ale', 2, (12, 16, 4)),
                    6: ('Harry, Bankier', 'Harry, banker', 1, (6, 10, 14)),
                    7: ('Carol', 'Carol', 2, (10, 16, 10)),
                    8: ('Ronald', 'Ronald', 1, (8, 12, 12)),
                    9: ('George', 'George', 1, (12, 10, 10)),
                    10: ('Attentäter', 'Assassin', 1, (2, 8, 16))},
             eps=[(36, L('R1', 10, 'R2', 7, 'R3', 7, 'Bar', 6, 'Wohnzimmer', 3)),
                  (38, L('R1', 10, 'R4', 7, 'R5', 7, 'Schlafzimmer', 5, 'Büro', 4)),
                  (40, L('R1', 10, 'R6', 8, 'R7', 7, 'Büro', 6, 'Bar', 3)),
                  (42, L('R1', 10, 'R8', 7, 'R9', 7, 'R7', 4, 'Büro', 5, 'Straßenzug', 5)),
                  (44, L('R1', 10, 'R10', 8, 'R7', 5, 'R8', 4, 'Straßenzug', 6, 'FX', 6))]),
    41: dict(price=220000, dur=22, ep_price=90000, critics=8, press=6, family=17,
             desc='Der Atomkrieg und die Zeit danach, erzählt am Schicksal der Familie Smith. Packend, beklemmend und effektvoll.',
             desc_en='Nuclear war and its aftermath, told through the Smith family. Gripping, oppressive and full of effects.',
             roles={1: ('Bob Smith, Vater', 'Bob Smith, father', 1, (10, 10, 16)),
                    2: ('Linda Smith, Mutter', 'Linda Smith, mother', 2, (8, 14, 14)),
                    3: ('Sally Smith', 'Sally Smith', 2, (10, 16, 6)),
                    4: ('Tim Smith', 'Tim Smith', 3, (10, 12, 16)),
                    5: ('Der Prophet', 'The prophet', 1, (16, 12, 18)),
                    6: ('Funker', 'Radio operator', 1, (6, 4, 14)),
                    7: ('Ratsvorsitzende', 'Council chairwoman', 2, (14, 14, 18)),
                    8: ('Nachrichtensprecher', 'Newsreader', 1, (8, 18, 6))},
             eps=[(42, L('R1', 10, 'R2', 8, 'R3', 6, 'R4', 6, 'R8', 4, 'Wohnzimmer', 6, 'FX', 10)),
                  (45, L('R1', 10, 'R2', 8, 'R4', 7, 'Wohnzimmer', 5, 'FX', 8)),
                  (48, L('R1', 10, 'R5', 10, 'R2', 6, 'R4', 5, 'Straßenzug', 6, 'FX', 7)),
                  (50, L('R1', 10, 'R6', 8, 'R2', 6, 'R4', 5, 'Büro', 4, 'Wüste', 5, 'FX', 7)),
                  (52, L('R1', 10, 'R7', 8, 'R2', 6, 'R4', 6, 'R5', 5, 'Garten', 6, 'Straßenzug', 4, 'FX', 6))]),
    42: dict(price=90000, dur=20, ep_price=50000, critics=3, press=6, family=21,
             desc='Action-Western: Brannigan findet seine Stadt zerstört und schwört Rache. Viel Pulverdampf für wenig Geld.',
             desc_en='An action Western: Brannigan finds his town in ruins and swears revenge. Lots of gunsmoke for little money.',
             roles={1: ('Brannigan', 'Brannigan', 1, (12, 12, 14)),
                    2: ('Major Perkins', 'Major Perkins', 1, (8, 12, 16)),
                    3: ('Häuptling', 'Chief', 1, (6, 16, 12)),
                    4: ('Mrs. Brannigan', 'Mrs Brannigan', 2, (8, 16, 12)),
                    5: ('Sheriff', 'Sheriff', 1, (14, 8, 14)),
                    6: ('Krieger', 'Warrior', 1, (2, 14, 6)),
                    7: ('Siedler', 'Settler', 1, (8, 4, 2)),
                    8: ('Armee-Captain', 'Army captain', 1, (6, 18, 4))},
             eps=[(30, L('R1', 10, 'R5', 6, 'R7', 4, 'Straßenzug', 7, 'FX', 8)),
                  (32, L('R1', 10, 'R2', 9, 'R3', 7, 'Indianercamp', 7, 'Wüste', 4)),
                  (34, L('R1', 10, 'R4', 6, 'R2', 5, 'R6', 4, 'Garten', 5, 'FX', 8)),
                  (36, L('R1', 10, 'R2', 8, 'R5', 6, 'R7', 4, 'Saloon', 6, 'Straßenzug', 4)),
                  (38, L('R1', 10, 'R2', 9, 'R3', 7, 'R6', 5, 'Wüste', 6, 'FX', 9)),
                  (40, L('R1', 10, 'R3', 8, 'R8', 7, 'R6', 5, 'R7', 4, 'Straßenzug', 6, 'Indianercamp', 5, 'FX', 10))]),
    43: dict(price=400000, dur=28, ep_price=125000, critics=9, press=9, family=19, star=True,
             desc="Psycho-Thriller: Kommissar Weizenbaum jagt 'M', den unsichtbaren Superverbrecher. Mit Kultpotential.",
             desc_en="A psycho thriller: Inspector Weizenbaum hunts 'M', the invisible super criminal. Cult potential.",
             roles={1: ('Kommissar Weizenbaum', 'Inspector Weizenbaum', 1, (16, 12, 18)),
                    2: ("'M'", "'M'", 1, (18, 16, 18)),
                    3: ('Dr. Manson, Psychologe', 'Dr Manson, psychologist', 1, (12, 12, 14)),
                    4: ('Assistentin Kowalski', 'Assistant Kowalski', 2, (14, 14, 18)),
                    5: ('Der Tätowierte', 'The tattooed man', 1, (20, 10, 8)),
                    6: ('Senator Brannigan', 'Senator Brannigan', 1, (8, 18, 6)),
                    7: ('Opfer', 'Victim', 2, (2, 14, 2)),
                    8: ('Polizeichef', 'Police chief', 1, (14, 8, 14))},
             eps=[(50, L('R1', 10, 'R4', 7, 'R7', 5, 'R8', 4, 'Straßenzug', 6, 'FX', 8)),
                  (52, L('R1', 10, 'R5', 9, 'R4', 6, 'Gefängnis', 6, 'Büro', 4)),
                  (53, L('R1', 10, 'R4', 7, 'R7', 5, 'R8', 5, 'Büro', 5, 'Straßenzug', 5, 'FX', 6)),
                  (55, L('R1', 10, 'R6', 7, 'R2', 6, 'R4', 6, 'Büro', 6, 'FX', 7)),
                  (56, L('R1', 10, 'R2', 8, 'R4', 6, 'Bar', 6, 'Straßenzug', 5, 'FX', 7)),
                  (58, L('R1', 10, 'R2', 9, 'R6', 5, 'R8', 5, 'Büro', 5, 'FX', 9)),
                  (58, L('R1', 10, 'R2', 9, 'R4', 6, 'Bar', 5, 'Straßenzug', 5, 'FX', 6)),
                  (60, L('R1', 10, 'R3', 9, 'R4', 6, 'R8', 6, 'Büro', 5, 'FX', 9)),
                  (62, L('R1', 10, 'R3', 10, 'R2', 9, 'Phantasia', 6, 'FX', 10))]),
    44: dict(price=45000, dur=9, ep_price=40000, critics=7, press=4, family=6, episodes=8, gem=True,
             desc='Mary und John, zwei rüstige Alte, legen sich mit ihrem Altersheim an. Herzerwärmend, witzig und billig zu drehen.',
             desc_en='Mary and John, two sprightly oldtimers, take on their old people\'s home. Heart-warming, funny and cheap to make.',
             roles={1: ('Mary', 'Mary', 2, (16, 8, 4)),
                    2: ('John', 'John', 1, (14, 4, 8)),
                    3: ('Pflegerin Sally', 'Nurse Sally', 2, (12, 6, 6)),
                    4: ('Mrs. Johnson, Heimleiterin', 'Mrs Johnson, matron', 2, (14, 4, 16)),
                    5: ('Polizist', 'Policeman', 1, (8, 8, 8)),
                    6: ('Journalistin', 'Journalist', 2, (8, 14, 10)),
                    7: ('Sallys Freundin', "Sally's friend", 2, (4, 10, 14))},
             eps=[(30, L('R1', 10, 'R2', 10, 'R4', 5, 'Wohnzimmer', 4)),
                  (31, L('R1', 10, 'R2', 10, 'R3', 6, 'Schlafzimmer', 4)),
                  (33, L('R1', 10, 'R2', 10, 'R3', 6, 'R4', 5, 'Garten', 4, 'Straßenzug', 3)),
                  (32, L('R1', 10, 'R2', 10, 'R7', 6, 'R5', 5, 'Wohnzimmer', 4, 'Straßenzug', 3)),
                  (34, L('R1', 10, 'R2', 10, 'R4', 7, 'Küche', 4)),
                  (36, L('R1', 10, 'R2', 10, 'R4', 6, 'R6', 5, 'Garten', 4, 'Straßenzug', 3)),
                  (36, L('R1', 10, 'R2', 10, 'R6', 7, 'R4', 6, 'Büro', 4)),
                  (38, L('R1', 10, 'R2', 10, 'R4', 6, 'R3', 5, 'Schlafzimmer', 4))]),
    45: dict(price=110000, dur=12, ep_price=55000, critics=4, press=8, family=18,
             desc='Die Hendersons aus New York sind laut, schrill und unmöglich. Freche Familien-Comedy mit Skandal-Garantie.',
             desc_en='The Hendersons from New York are loud, shrill and impossible. A cheeky family comedy, scandal guaranteed.',
             roles={1: ('Sam Henderson', 'Sam Henderson', 1, (20, 8, 2)),
                    2: ('Laura Henderson', 'Laura Henderson', 2, (18, 2, 8)),
                    3: ('Brad', 'Brad', 1, (16, 18, 10)),
                    4: ('Jimmy', 'Jimmy', 3, (16, 12, 14)),
                    5: ('Brads Lover', "Brad's lover", 1, (6, 18, 4)),
                    6: ('Kommandoführer', 'Commando leader', 1, (14, 8, 14)),
                    7: ('Polizist', 'Policeman', 1, (8, 4, 2))},
             eps=[(34, L('R1', 10, 'R2', 8, 'R3', 7, 'Schlafzimmer', 5, 'Wohnzimmer', 4)),
                  (35, L('R1', 10, 'R4', 8, 'R2', 6, 'Wohnzimmer', 5)),
                  (36, L('R1', 10, 'R3', 8, 'R2', 8, 'Garten', 5)),
                  (37, L('R4', 10, 'R1', 8, 'R2', 6, 'Küche', 6)),
                  (36, L('R1', 10, 'R2', 9, 'R7', 4, 'Wohnzimmer', 4, 'Bar', 4)),
                  (38, L('R3', 10, 'R5', 8, 'R4', 6, 'R1', 5, 'Wohnzimmer', 5)),
                  (38, L('R4', 10, 'R6', 8, 'R1', 6, 'R2', 6, 'Wohnzimmer', 4, 'FX', 6)),
                  (40, L('R1', 10, 'R7', 7, 'R2', 6, 'Gefängnis', 6, 'Büro', 3))]),
    46: dict(price=280000, dur=24, ep_price=120000, critics=8, press=9, family=24, star=True,
             desc='Ein Programm will die Weltherrschaft! Tolle Tricks im Cyberspace - die Serie der Zukunft.',
             desc_en='A computer program wants to rule the world! Breath-taking effects in cyberspace - the series of the future.',
             roles={1: ('Richard, Programmierer', 'Richard, programmer', 1, (2, 8, 16)),
                    2: ('Cybie, das Programm', 'Cybie, the program', 4, (12, 20, 16)),
                    3: ('FBI-Agent', 'FBI agent', 1, (6, 4, 14)),
                    4: ('Dr. Minsky', 'Dr Minsky', 1, (12, 12, 14)),
                    5: ('McCash, Verkaufsleiter', 'McCash, sales manager', 1, (20, 10, 8)),
                    6: ('Kampfprogramm', 'Combat program', 4, (14, 12, 16)),
                    7: ('Jenny, Kollegin', 'Jenny, colleague', 2, (4, 16, 12))},
             eps=[(50, L('R1', 10, 'R2', 8, 'R7', 5, 'Büro', 6, 'Phantasia', 6, 'FX', 10)),
                  (52, L('R1', 10, 'R2', 8, 'R7', 5, 'Phantasia', 7, 'FX', 10)),
                  (54, L('R2', 10, 'R1', 8, 'R3', 5, 'Phantasia', 7, 'Büro', 4, 'FX', 10)),
                  (55, L('R1', 10, 'R3', 8, 'R2', 6, 'Gefängnis', 6, 'Büro', 4, 'FX', 8)),
                  (58, L('R1', 10, 'R2', 9, 'R6', 9, 'Phantasia', 8, 'FX', 10)),
                  (60, L('R4', 10, 'R5', 9, 'R1', 6, 'Büro', 6, 'Phantasia', 6, 'FX', 8))]),
    47: dict(price=200000, dur=20, ep_price=85000, critics=7, press=7, family=3,
             desc='Polit-Thriller: Sgt. Pepper jagt einen Mörder bis ins Weiße Haus. Spannend bis zur letzten Minute.',
             desc_en='A political thriller: Sgt Pepper hunts a murderer all the way to the White House. Gripping to the last minute.',
             roles={1: ('Sgt. Pepper', 'Sgt Pepper', 1, (14, 8, 14)),
                    2: ('Senator Lee', 'Senator Lee', 1, (8, 18, 6)),
                    3: ('Ron Gilbert', 'Ron Gilbert', 1, (6, 20, 8)),
                    4: ('FBI-Direktor', 'FBI director', 1, (16, 12, 18)),
                    5: ('Jonathan Hart', 'Jonathan Hart', 1, (12, 12, 14)),
                    6: ('Präsident Shrub', 'President Shrub', 1, (18, 16, 18)),
                    7: ('Peppers Vorgesetzter', "Pepper's superior", 1, (6, 4, 14)),
                    8: ('Reporterin', 'Reporter', 2, (14, 16, 6))},
             eps=[(42, L('R1', 10, 'R3', 6, 'R8', 5, 'Straßenzug', 6, 'FX', 6)),
                  (44, L('R1', 10, 'R2', 8, 'R8', 5, 'Büro', 6)),
                  (45, L('R1', 10, 'R4', 6, 'R7', 5, 'Straßenzug', 5, 'FX', 8)),
                  (46, L('R1', 10, 'R2', 7, 'R5', 6, 'Bar', 5, 'FX', 8)),
                  (47, L('R1', 10, 'R7', 8, 'R4', 6, 'Gefängnis', 7)),
                  (48, L('R1', 10, 'R5', 7, 'R8', 5, 'Wüste', 5, 'Flugzeug', 4, 'FX', 6)),
                  (49, L('R1', 10, 'R4', 7, 'R7', 5, 'Straßenzug', 6, 'FX', 8)),
                  (50, L('R1', 10, 'R6', 9, 'R4', 7, 'R2', 5, 'Büro', 7))]),
    48: dict(price=70000, dur=12, ep_price=40000, critics=5, press=7, family=13, gem=True,
             desc='Ein zu feiner Sheriff in einer zu rauhen Stadt: Comedy-Western mit Herz. Mit dem richtigen Dandy ein Hit.',
             desc_en='A sheriff too refined for a town too rough: a comedy Western with heart. With the right dandy a hidden gem.',
             roles={1: ('Sheriff Dandy', 'Sheriff Dandy', 1, (6, 20, 8)),
                    2: ('Mary', 'Mary', 2, (4, 16, 12)),
                    3: ('Bürgermeister', 'Mayor', 1, (14, 4, 8)),
                    4: ('Farmer Joe', 'Farmer Joe', 1, (8, 4, 2)),
                    5: ('Pa Johnson', 'Pa Johnson', 1, (20, 8, 2)),
                    6: ('Die Johnson-Brüder', 'The Johnson brothers', 1, (2, 14, 6)),
                    7: ('Saloon-Wirtin', 'Saloon landlady', 2, (18, 2, 8)),
                    8: ('Der falsche Verlobte', 'The false fiancé', 1, (6, 18, 4))},
             eps=[(30, L('R1', 10, 'R3', 6, 'R7', 5, 'Straßenzug', 6, 'Saloon', 4)),
                  (32, L('R1', 10, 'R7', 7, 'R6', 5, 'Saloon', 7)),
                  (33, L('R1', 10, 'R4', 8, 'R6', 6, 'Wüste', 5, 'FX', 4)),
                  (34, L('R1', 10, 'R3', 7, 'R2', 6, 'Straßenzug', 6)),
                  (35, L('R1', 10, 'R5', 9, 'R6', 7, 'Straßenzug', 6, 'FX', 6)),
                  (36, L('R1', 10, 'R5', 8, 'R3', 6, 'Gefängnis', 6)),
                  (36, L('R1', 10, 'R6', 8, 'R5', 6, 'Straßenzug', 5, 'FX', 5)),
                  (38, L('R1', 10, 'R2', 9, 'R8', 7, 'R3', 5, 'Straßenzug', 6))]),
    49: dict(price=160000, dur=18, ep_price=70000, critics=6, press=7, family=11,
             desc='Jason muß einmal um die halbe Welt - und unterwegs stehlen, um sein Erbe zu bekommen. Turbulente Reise-Komödie.',
             desc_en='To get his inheritance Jason must travel halfway round the world, stealing on the way. A turbulent comedy.',
             roles={1: ('Jason', 'Jason', 1, (16, 18, 10)),
                    2: ('Harold', 'Harold', 1, (20, 10, 8)),
                    3: ('Tante Ellie', 'Aunt Ellie', 2, (16, 8, 4)),
                    4: ('Gangsterboss', 'Gang boss', 1, (14, 8, 14)),
                    5: ('Der Bote', 'The messenger', 1, (6, 4, 14)),
                    6: ('Maharadscha', 'Maharaja', 1, (12, 12, 14)),
                    7: ('Taschendieb', 'Pickpocket', 3, (14, 6, 4)),
                    8: ('Stewardess', 'Stewardess', 2, (14, 16, 6))},
             eps=[(38, L('R1', 10, 'R2', 8, 'R3', 6, 'Wohnzimmer', 5, 'Büro', 3)),
                  (40, L('R1', 10, 'R2', 9, 'R4', 7, 'Wüste', 6, 'Gefängnis', 4)),
                  (41, L('R1', 10, 'R2', 9, 'Straßenzug', 6, 'FX', 5)),
                  (42, L('R1', 10, 'R5', 7, 'R2', 6, 'Bar', 6)),
                  (43, L('R1', 10, 'R8', 6, 'R2', 5, 'Flugzeug', 6, 'Wüste', 5)),
                  (44, L('R1', 10, 'R7', 8, 'R2', 7, 'Straßenzug', 6)),
                  (45, L('R1', 10, 'R6', 9, 'R2', 8, 'Phantasia', 6, 'FX', 6)),
                  (46, L('R1', 10, 'R2', 9, 'R3', 4, 'Flugzeug', 6, 'Straßenzug', 4, 'FX', 6))]),
    50: dict(price=180000, dur=20, ep_price=75000, critics=6, press=6, family=17,
             desc='Ingenieur Frank Waters stößt in Afrika auf illegalen Waffenhandel. Abenteuer-Krimi mit exotischen Schauplätzen.',
             desc_en='Engineer Frank Waters uncovers illegal arms dealing in Africa. An adventure thriller with exotic locations.',
             roles={1: ('Frank Waters', 'Frank Waters', 1, (16, 12, 18)),
                    2: ('Leslie', 'Leslie', 2, (16, 20, 14)),
                    3: ('Firmenchef', 'Company boss', 1, (6, 16, 12)),
                    4: ('Gangster', 'Gangster', 1, (20, 8, 2)),
                    5: ('Polizeichef', 'Police chief', 1, (14, 8, 14)),
                    6: ('FBI-Agent', 'FBI agent', 1, (6, 4, 14)),
                    7: ('Ted Zimmers\' Witwe', "Ted Zimmers' widow", 2, (14, 16, 6))},
             eps=[(40, L('R1', 10, 'R7', 6, 'R3', 5, 'Büro', 5, 'Flugzeug', 4)),
                  (42, L('R1', 10, 'R3', 7, 'R5', 6, 'Büro', 5, 'Straßenzug', 4)),
                  (44, L('R1', 10, 'R3', 8, 'R4', 6, 'Hafen', 6, 'FX', 6)),
                  (45, L('R1', 10, 'R4', 9, 'R2', 6, 'Gefängnis', 5, 'FX', 8)),
                  (46, L('R1', 10, 'R2', 9, 'R5', 7, 'Büro', 5)),
                  (48, L('R1', 10, 'R2', 9, 'R6', 6, 'Flugzeug', 6, 'Wüste', 5, 'FX', 8))]),
}

# ---------------------------------------------------------------- the records
SP = content.TABLES['screenplays']
EP = lambda e: 8 + (e - 1) * 0x23          # episode e's block: +0 base quality, entry k at +1 + 3k
REQ = lambda t: 0x168 + (t - 1) * 4         # role t: humor, (unused), appearance, skill
CAT = lambda t: 0x1A3 + t                   # role t's actor category
HEAD = [(0, 4), (5, 8), (0x166, 0x1B3)]     # what a savegame does not hold (it holds state +4, the blocks +8..+0x165)
SERIES_GENRE = {37: 3, 38: 4, 39: 5, 40: 1, 41: 2, 42: 3, 43: 4, 44: 5, 45: 1, 46: 2, 47: 4, 48: 3, 49: 5, 50: 4}
FSK18 = 8                                   # film flags2 bit 3: X rated (the moral guardian before 21:00)


def fnv(b):
    h = 0x811C9DC5
    for x in b:
        h = ((h ^ x) * 0x01000193) & 0xFFFFFFFF
    return h


def head_hash(rec):
    return fnv(b''.join(rec[a:b] for a, b in HEAD))


def write_list(b, e, base, lst):
    b[EP(e)] = base
    for k, (item, w) in enumerate(lst, 1):
        b[EP(e) + 1 + 3 * k] = item
        b[EP(e) + 2 + 3 * k] = w


def screenplay(i, orig):
    """the screenplay's record (bytes) and its texts {lang: 16 strings or None = the game's}"""
    b = bytearray(orig)
    texts = {'de': [None] * 16, 'en': [None] * 16}
    if i in SHOWS:
        s = SHOWS[i]
        flags = 6                            # offered + up to 10 productions, as every show of the original
        for t, c in s.get('add_roles', {}).items():
            b[CAT(t)] = c
        for t, (h, a, k) in s['roles'].items():
            b[REQ(t):REQ(t) + 4] = bytes((h, 0, a, k))
        write_list(b, 1, s['q'], s['lst'])
        for e in range(2, 11):               # up to 10 productions, each its own block: the first one's
            b[EP(e):EP(e) + 0x23] = b[EP(1):EP(1) + 0x23]      # (script_init_episodes 1C18:0313, the start savegame)
        if s.get('desc'):
            texts['de'][0] = s['desc']
        texts['en'][0] = s['desc_en']
    else:
        s = SERIES[i]
        flags = 3                            # a series production, offered
        for t, (nd, ne, c, (h, a, k)) in s['roles'].items():
            b[CAT(t)] = c
            b[REQ(t):REQ(t) + 4] = bytes((h, 0, a, k))
            texts['de'][t], texts['en'][t] = nd, ne
        for e, (base, lst) in enumerate(s['eps'], 1):
            write_list(b, e, base, lst)
        texts['de'][0], texts['en'][0] = s['desc'], s['desc_en']
    struct.pack_into('<i', b, 0, s['price'])
    b[5] = flags
    struct.pack_into('<H', b, 0x166, s['dur'])
    return bytes(b), texts


def series_patch(i):
    s = SERIES[i]
    n = s.get('episodes')
    return dict(id=i, genre=SERIES_GENRE[i], blocks=1, episodes=n, family=s['family'], critics=s['critics'], press=s['press'],
                episode_price=s['ep_price'], price=s['ep_price'] * (n or 0))


def build(res):
    """the layer from the player's game files: per screenplay its original head's hash, the
    record and texts; the series' and films' values"""
    raw = res.get(SP['res'])
    docs = content.export(res)
    series = {r['id']: r for r in docs['series']['records']}
    films = {r['id']: r for r in docs['films']['records']}
    out = {'screenplays': [], 'series': [], 'films': []}
    for i in sorted(list(SHOWS) + list(SERIES)):
        orig = raw[i * SP['size']:(i + 1) * SP['size']]
        if any(orig[EP(e) + 1 + 3 * k] for e in range(1, 11) for k in range(1, 11)):
            raise SystemExit('screenplay %d: the original has shopping lists -- not the 1991 data?' % i)
        rec, texts = screenplay(i, orig)
        out['screenplays'].append(dict(id=i, head=head_hash(orig), rec=rec, texts=texts, target=struct.unpack_from('<H', orig, 6)[0]))
    for i in sorted(SERIES):
        o = series[i]
        p = series_patch(i)
        if o['genre'] != p['genre'] or o['blocks'] or o['price'] or o['episode_price'] or o['critics'] or o['press'] or o['flags']:
            raise SystemExit('series %d: not the original\'s' % i)
        p['episodes_orig'] = o['episodes']
        if p['episodes'] is None:
            p['episodes'] = o['episodes']
            p['price'] = p['episode_price'] * o['episodes']
        out['series'].append(p)
    for i, s in SHOWS.items():
        if s.get('fsk18'):
            f = films[SPF[i]]
            if f['genre'] != 8 or f['flags2'] != 4:
                raise SystemExit('film %d: not the original\'s' % SPF[i])
            out['films'].append(dict(id=SPF[i], genre=8, flags2_orig=4, flags2=4 | FSK18))
    return out


SPF = {25: 139, 26: 140, 27: 141, 28: 142, 29: 143, 30: 144, 31: 145, 32: 146, 33: 147}   # the shows' films


def c_str(s):
    if s is None:
        return 'NULL'
    out = '"'
    for ch in s.encode(content.ENC):
        out += chr(ch) if 32 <= ch < 127 and ch not in (34, 92) else '\\%03o' % ch
    return out + '"'


def c_source(lay):
    L = ['/* generated by tools/origscripts.py from its values and the original\'s SOURCE*.RSC -- do not edit',
         ' * (python3 tools/origscripts.py writes it, --check compares).  The rule "Unfertige Drehbuecher integrieren":',
         ' * src/origscripts.c. */', '',
         '#define ORIGSCRIPTS_N %d' % len(lay['screenplays']),
         '#define ORIGSCRIPTS_SERIES_N %d' % len(lay['series']),
         '#define ORIGSCRIPTS_FILMS_N %d' % len(lay['films']), '',
         '/* per screenplay: its number, the hash (FNV-1a) of the original\'s part a savegame does not',
         ' * hold (+0..+3, +5..+7, +0x166..+0x1B2), the whole record as the rule has it */',
         'static const struct { u16 id; u32 head; u8 rec[0x1B3]; } origscripts_sp[ORIGSCRIPTS_N] = {']
    for s in lay['screenplays']:
        L.append('    {%d, 0x%08X, {' % (s['id'], s['head']))
        for k in range(0, 0x1B3, 24):
            L.append('        ' + ', '.join('%d' % x for x in s['rec'][k:k + 24]) + ',')
        L.append('    }},')
    L.append('};')
    L.append('')
    L.append('/* their 16 texts (the agency text, the roles\' names) in German and English, code page 437;')
    L.append(' * NULL: the game\'s own (25 has its German text) -- an empty one of the game\'s is replaced */')
    L.append('static const char *const origscripts_txt[ORIGSCRIPTS_N][2][16] = {')
    for s in lay['screenplays']:
        L.append('    { /* %d */' % s['id'])
        for lang in ('de', 'en'):
            L.append('        {' + ', '.join(c_str(x) for x in s['texts'][lang]) + '},')
        L.append('    },')
    L.append('};')
    L.append('')
    L.append('/* the series: the original\'s genre and episodes (all else 0), then the rule\'s values */')
    L.append('static const struct { u16 id; u8 genre, episodes_orig, episodes, blocks, family, critics, press; i32 price, episode_price; } origscripts_series[ORIGSCRIPTS_SERIES_N] = {')
    for p in lay['series']:
        L.append('    {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d},' % (p['id'], p['genre'], p['episodes_orig'], p['episodes'], p['blocks'], p['family'],
                                                              p['critics'], p['press'], p['price'], p['episode_price']))
    L.append('};')
    L.append('')
    L.append('/* the films of the two shows the rule rates X (flags2 bit 3): genre, flags2 before and after */')
    L.append('static const struct { u16 id; u8 genre, flags2_orig, flags2; } origscripts_films[ORIGSCRIPTS_FILMS_N] = {')
    for f in lay['films']:
        L.append('    {%d, %d, %d, %d},' % (f['id'], f['genre'], f['flags2_orig'], f['flags2']))
    L.append('};')
    return '\n'.join(L) + '\n'


def go_json(lay):
    sp = []
    for s in lay['screenplays']:
        r = content.unpack_record(SP, s['rec'])
        sp.append(dict(id=s['id'], head='%08x' % s['head'], record=r, text=s['texts']))
    return json.dumps({'about': 'generated by tools/origscripts.py -- the rule "Unfertige Drehbuecher integrieren" (launcher/origscripts.go)',
                       'screenplays': sp, 'series': lay['series'], 'films': lay['films']}, ensure_ascii=False, indent=1) + '\n'


def outputs(game):
    res = content.Resources([os.path.join(game, 'SOURCE1.RSC'), os.path.join(game, 'SOURCE2.RSC')])
    lay = build(res)
    root = os.path.dirname(HERE)
    return {os.path.join(root, 'src', 'origscripts_data.h'): c_source(lay),
            os.path.join(root, 'launcher', 'origscripts.json'): go_json(lay)}


def main():
    game = os.environ.get('GAME') or os.path.dirname(os.path.dirname(HERE))
    check = '--check' in sys.argv[1:]
    bad = 0
    for p, text in outputs(game).items():
        old = open(p, encoding='utf-8').read() if os.path.exists(p) else None
        if check:
            if old != text:
                print('%s: not up to date (python3 tools/origscripts.py)' % p)
                bad = 1
        elif old != text:
            open(p, 'w', encoding='utf-8').write(text)
            print('wrote', p)
    if check and not bad:
        print('origscripts: both tables up to date')
    return bad


if __name__ == '__main__':
    sys.exit(main())
