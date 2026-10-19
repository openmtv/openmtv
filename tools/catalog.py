#!/usr/bin/env python3
"""Browsable catalogue of the content of OpenMTV -- movies, series, advertising contracts, news stories
and screenplays -- generated from the game data, as one self-contained HTML page.

    catalog.py OUT.html [SOURCE1.RSC SOURCE2.RSC] [--verify BIN | --no-verify]

Default game files: ../SOURCE1.RSC and ../SOURCE2.RSC relative to decomp/.
The tables are read with tools/content.py, so the page follows content
edits (content.py import).  The page shows every byte of every record
(named fields, flag bits, unknown bytes labelled as such), the texts in
English and German, the start savegame (resource 0x2B) where it differs
from the tables, the cast pool and sets (resource 0xD0, actor names 0x2A),
and values the game derives from them.  Derived values are Python ports of
the decompiled functions:

    movie_price          1DA8:3F5B   value of a movie (dealer, auction, sale)
    series_price         1DA8:4035   value of a series
    film_apply_wear      1C18:1794   topicality loss of a movie per airing
    series_wear_episode  1C18:172F   topicality loss of an episode per airing
    broadcast_phase_eval 24B8:0C4A   audience share of an hour (news and programme parts)
    ad_target_group_ok   1DA8:4258   does a programme genre satisfy an ad
    viewers_to_pct       1DA8:2682   minimum audience as % of a station's reach
    adagency_refill      24B8:000C   which ads the agency can offer
    ad_audience_text     14E1:3D66   the condition line on the contract card
    news_emit            1C18:0490   value and price class of a news item
    news_age             1C18:1022   hourly news aging
    news_text            26A7:292F   the headline with its phrases
    script_role_match_pct 1DA8:2560  how well an actor fits a role
    studio_start_production 229E:0B98  production quality and the movie's new price

With --verify (default when build/mtv and build/MTV_U.EXE exist) the
machine-level build of the original program is run headless on copies of the
given .rsc files: the functions above are called through the test harness
(`N call SEG:OFF,ARGS`, situations set up with `poke16`) and memory dumps are
compared with the ports.  The results are embedded in the page (about 30 s,
five headless runs).  --no-verify skips this; the page then says that it is
unchecked.
"""
import sys, os, re, json, struct, shutil, subprocess, tempfile, datetime, html

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import content  # noqa: E402

VIEWERS_PER_UNIT = 2464            # 0x9A0: real viewers per reach unit of the coverage map
TEXT_RES = {'de': 199, 'en': 253}  # language string tables (g_text, 430 strings)
LANGS = ('en', 'de')
START_SAVE = 0x2B
INIT_DATA = 0xD0
HOURS = ['18:00', '19:00', '20:00', '21:00', '22:00', '23:00', '24:00']

# program constant DS:00A8 (g_ad_min_viewers), checked against game memory by --verify
AD_MIN_VIEWERS = [0, 250000, 500000, 1000000, 2500000, 5000000, 10000000, 25000000, 50000000, 100000000]

# savegame layout after the per-item records (game_save_load 229E:139A, in order)
SAVE_TAIL = [(0x7B42, 0xB4), (0x8C6C, 0xC), (0x8C54, 2), (0x7C82, 0x6AC), ('pl1', 0xF), ('pl2', 0xF),
             ('pl3', 0xF), (0x79C6, 0xF0), (0x7AB6, 0x50), (0x8A60, 0x68), (0x1D09, 0x738)]
# the last tables of a savegame, from g_trends (DS:0094) to the end (player names)
SAVE_END = [(0x94, 0x14), (0x78A8, 0xC), (0x7488, 0xA), (0x724A, 0x58), (0x78BC, 2), (0x549F, 0xB4), ('names', 0x78)]
# init data (resource 0xD0) layout (game_load_init_data 0054:0A0B)
INIT_LAYOUT = [(0x1D00, 8), (0x6165, 0xD0), (0x8BDC, 0x5C), (0x5013, 0x540), (0x6235, 0x180), (0x268E, 0x60),
               (0x1D09, 0x738), (0x724A, 0x58), (0x7B42, 0xB4), (0x7C82, 0x6AC), (0x7160, 0x80)]


# ---------------------------------------------------------------- integer helpers (Turbo C semantics)
def i8(v):
    v &= 0xFF
    return v - 256 if v > 127 else v


def i16(v):
    v &= 0xFFFF
    return v - 65536 if v > 32767 else v


def i32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v > 0x7FFFFFFF else v


def tdiv(a, b):
    """C division (truncates toward zero)"""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


def round_to(v, m):
    """1DA8:2653 round_to: ((v + m/2) / m) * m, signed long"""
    return i32(tdiv(i32(v + (m >> 1)), m) * m)


# ---------------------------------------------------------------- ports of game functions
def movie_price(price, quality, topicality):
    """1DA8:3F5B: price/2 + price*topicality/(2*quality), rounded to 1000"""
    if price == 0 or (quality & 0xFF) == 0:
        v = price
    else:
        v = tdiv(i32(i8(topicality) * price), i16(i8(quality) << 1)) + tdiv(price, 2)
    return round_to(v, 1000)


def series_price(qtf, episode_price):
    """1DA8:4035: n produced episodes (counted from #1 while bit0 is set) * episode price,
    half fixed, half scaled by sum(topicality)/sum(quality), rounded to 1000"""
    sum_t = sum_q = 0
    si = 1
    while si <= 10:
        if not (qtf[si * 3 + 2] & 1):
            break
        sum_t = (sum_t + i8(qtf[si * 3 + 1])) & 0xFFFF
        sum_q = (sum_q + i8(qtf[si * 3])) & 0xFFFF
        si += 1
    total = i32((si - 1) * episode_price)
    if total == 0 or sum_q == 0:
        v = 0
    else:
        v = tdiv(i32(i16(sum_t) * total), i16(sum_q << 1)) + tdiv(total, 2)
    return round_to(v, 1000)


def film_wear(quality, wear, topicality):
    """1C18:1794: topicality -= quality*wear/5, at least 1 (byte arithmetic)"""
    loss = tdiv(i16(i8(quality) * i8(wear)), 5) & 0xFF
    t = (topicality - loss) & 0xFF
    return 1 if i8(t) < 1 else t


def film_wear_loss(quality, wear):
    return tdiv(i16(i8(quality) * i8(wear)), 5)


def episode_wear(quality, wear, topicality):
    """1C18:172F: topicality -= wear*quality/20, at least 1"""
    loss = tdiv(i16(i8(wear) * i8(quality)), 20) & 0xFF
    t = (topicality - loss) & 0xFF
    return 1 if i8(t) < 1 else t


def ad_target_ok(codes, genre):
    """1DA8:4258 ad_target_group_ok: negative code = genre excluded, positive codes = one of them
    required, none = any.  Genre: movie genre 1..10, series 10 + series genre."""
    di = 1
    for c in codes:
        c = i8(c)
        if c < 0 and ((-c) & 0xFFFF) == genre:
            di = 0
    if di:
        di = 2
        for c in codes:
            c = i8(c)
            if c > 0 and di == 2:
                di = 0
            if (c & 0xFFFF) == genre:
                di = 1
        if di == 2:
            di = 1
    return di


def viewers_to_pct(reach, viewers):
    """1DA8:2682: (viewers + T/2) / T with T = reach * 2464 / 100"""
    v = i32(reach * VIEWERS_PER_UNIT)
    return tdiv(i32(tdiv(v, 200) + viewers), tdiv(v, 100)) & 0xFFFF


def ad_max_class(reaches):
    """24B8:000C: ads are offered up to the last class whose minimum is below 40 % of the largest reach"""
    limit = tdiv(i32(max(reaches) * VIEWERS_PER_UNIT) * 2, 5)
    k = 1
    while True:
        if k <= 10 and AD_MIN_VIEWERS[k - 1] >= limit:
            break
        if k > 10:
            break
        k += 1
    return k - 1


def hour_scores(progs, genre_aud, hour, trends, news):
    """24B8:0C4A phase 2 after the per-station pull: progs = {player: (pull, genre code)} of the stations
    still in the game.  Returns ({player: final score}, {player: share %})."""
    score = {p: v[0] & 0xFFFF for p, v in progs.items()}
    genre = {p: v[1] for p, v in progs.items()}
    for p in sorted(progs):                       # trend bonus (3 active trend genres)
        for g, mult in trends[:3]:
            if g == genre[p]:
                score[p] = ((mult * score[p]) & 0xFFFF) // 10
    for p in sorted(progs):                       # same-genre split, then genre x hour audience
        if genre[p] == 0:
            continue
        same = sum(score[q] for q in progs if genre[q] == genre[p]) & 0xFFFF   # earlier stations already scaled
        a = genre_aud[genre[p] * 8 + hour]
        if i16(a) < i16(same):
            score[p] = tdiv(i16(score[p] * a), i16(same)) & 0xFFFF
        score[p] = tdiv(i16(score[p] * a), 100) & 0xFFFF
        if score[p]:
            score[p] = (score[p] + tdiv(news.get(p, 0), 5)) & 0xFFFF
    total = sum(score.values()) & 0xFFFF
    cap = min(95, i16(total)) if total else 0
    share = {p: (tdiv(i16(score[p] * cap), i16(total)) & 0xFFFF) if total else 0 for p in progs}
    return score, share


def solo_share(pull, gcode, genre_aud):
    """share % of a programme that is the only one on air (no news lead-in, no trend), hours 1..7"""
    return [hour_scores({1: (pull, gcode)}, genre_aud, h, [], {})[1][1] for h in range(1, 8)]


# ---------------------------------------------------------------- game texts
def game_strings(res):
    out = {}
    for lang in LANGS:
        parts = res.get(TEXT_RES[lang]).split(b'\0')
        out[lang] = [p.decode(content.ENC) for p in parts]
    return out


def clean(s):
    return ''.join(ch if ch >= ' ' else '·' for ch in s)


def text_pair(t, lang, n):
    """strings of a record in one language; a corrupt entry (raw bytes) is decoded as far as it goes"""
    e = (t or {}).get(lang)
    if e is None:
        return [''] * n, False
    if isinstance(e, dict):
        blk = bytes.fromhex(e['raw'])
        out, i = [], 0
        while i < len(blk) and len(out) < n:
            k = blk[i]
            out.append(clean(blk[i + 1:i + 1 + k].decode(content.ENC)))
            i += 1 + k
        return (out + [''] * n)[:n], True
    return (list(e) + [''] * n)[:n], False


# game strings used by the info cards: 39 "(Program)", 40 "(Full)", 77/78/79/80 ad condition, 376/377 series card,
# 382/383/385 ad card, 412-419 and 425 movie card
UI_STRINGS = [39, 40, 77, 78, 79, 80, 376, 377, 382, 383, 385, 412, 413, 414, 415, 416, 417, 418, 419, 425]


# ---------------------------------------------------------------- start savegame and init data
def parse_start_save(save):
    """per-item fields of the start savegame -> {films: {id: {...}}, series: {...}, ads: {...}}, reach"""
    films, series, ads = {}, {}, {}
    o = 0
    for i in range(1, 201):
        b = o + (i - 1) * 0x12
        bits = save[b + 4]
        films[i] = dict(quality=save[b], topicality=save[b + 1], owner=save[b + 3],
                        fl5=bits & 1, fl7=(bits >> 1) & 1, f20=(bits >> 2) & 1, f21=(bits >> 3) & 1,
                        price=struct.unpack_from('<i', save, b + 5)[0], b0e=save[b + 0xD],
                        live_day=struct.unpack_from('<H', save, b + 0xE)[0],
                        live_slot=struct.unpack_from('<H', save, b + 0x10)[0])
    o += 200 * 0x12
    for i in range(1, 51):
        b = o + (i - 1) * 0x29
        bits = save[b + 0x22]
        series[i] = dict(qtf=list(save[b:b + 0x21]), owner=save[b + 0x21],
                         fl5=bits & 1, fl7=(bits >> 1) & 1, f20=(bits >> 2) & 1, f21=(bits >> 3) & 1,
                         price=struct.unpack_from('<i', save, b + 0x23)[0],
                         next_episode=struct.unpack_from('<H', save, b + 0x27)[0])
    o += 50 * 0x29 + 50 * 0x161
    for i in range(1, 51):
        ads[i] = save[o + i - 1]
    o += 50
    pos = {}
    for a, n in SAVE_TAIL:
        pos[a] = o
        o += n
    reach = [struct.unpack_from('<I', save, pos[0x1D09] + p * 0x1CE + 4)[0] for p in range(4)]
    t = len(save) - sum(n for _, n in SAVE_END)
    trends = [list(struct.unpack_from('<HH', save, t + 4 * k)) for k in range(4)]
    return films, series, ads, reach, trends


def parse_init_data(d):
    pos, o = {}, 0
    for a, n in INIT_LAYOUT:
        pos[a] = (o, n)
        o += n
    ha = list(d[pos[0x1D00][0]:pos[0x1D00][0] + 8])
    ga = list(d[pos[0x7160][0]:pos[0x7160][0] + 0x80])
    return ha, ga


# ---------------------------------------------------------------- field documentation (shown per record)
# tools/editor/fields.json (the content editor's too: build/openmtv editor, launcher/editor.go):
# FIELDS kind -> [[offset, size, key, label, format, meaning], ...]; FIELDS_DE, the same in
# German for the editor's German interface: kind -> key -> [label, meaning]
with open(os.path.join(HERE, 'editor', 'fields.json'), encoding='utf-8') as _f:
    _FIELDS = json.load(_f)
FIELDS, FIELDS_DE = _FIELDS['en'], _FIELDS['de']


# ---------------------------------------------------------------- catalogue
def ad_condition(rule, S, lang):
    """14E1:3D66 ad_audience_text + 14E1:3E49 ad_genre_list: the condition line of the contract card"""
    s = S[lang]
    if rule[0] == 0:
        return ''
    sign = 1 if rule[0] > 0 else -1
    parts = []
    for k in range(3):
        c = rule[k]
        if c == 0:
            parts.append('')
            continue
        v = c * sign
        name = s[0x11A + v] if v > 10 else (s[0x1A + v] if 0x1A + v >= 0 else '?')
        if k == 0 and rule[2] != 0:
            parts.append(name + ', ')
        elif k < 2 and rule[k + 1] != 0:
            parts.append(s[80].replace('#s', name))
        else:
            parts.append(name)
    return s[78 if sign > 0 else 77].replace('#s', ''.join(parts))


def market_of(flags, owner, price_ok=True):
    """24B8:04B9 / 24B8:0905: on the market (bit 5), not held by a station, priced; bit 6 = auction"""
    if not (flags & 0x20) or not price_ok:
        return 'none'
    if 1 <= owner <= 3:
        return 'owned'
    return 'auction' if flags & 0x40 else 'dealer'


# ---------------------------------------------------------------- news
NEWS_PRICES = [1000, 2000, 3000, 4000, 6000, 8000]   # DS:129C, by price class (news_price 1DA8:3F48)
NEWS_STEPS = [15, 30, 40, 50, 60, 100]               # DS:11E8 (emission) and DS:11EE (aging), price class thresholds
AI_NEWS_TARGET = [30, 50, 70, 90]                    # DS:15FC: news value a computer player aims for, by image rank
NEWS_AGENCIES = [[1, 1000, 5], [2, 10000, 10], [3, 10000, 15]]   # category, daily fee, image needed (05FF:25A4)
NEWS_EVENT_NODES = [166, 167, 168]                   # news_add_event (1C18:069A) callers in 1DA8:2D3B and 1DA8:276D
NEWS_TOPIC_EVENTS = 16                               # the two consulates, variant = terrorist 1 or 2


def news_value(node_value, counted):
    """news_emit 1C18:0490: node value x (30 + 70 x average phrase value / 10) / 100 over the parts below 1000"""
    nv = i8(node_value) & 0xFFFF
    if not counted:
        return nv
    s = sum(i8(v) & 0xFFFF for v in counted) & 0xFFFF
    avg = tdiv(i16(s * 70), i16(10 * len(counted))) & 0xFFFF
    return tdiv(i16(nv * ((avg + 30) & 0xFFFF)), 100) & 0xFFFF


def news_class(value, steps=NEWS_STEPS):
    k = 0
    while k < len(steps) and i16(value) > steps[k]:
        k += 1
    return k


def news_price(cls):
    return NEWS_PRICES[cls] if 0 <= cls < len(NEWS_PRICES) else 0


def news_age(value, pct=85):
    """news_age 1C18:1022: hourly value x 85 / 100 (inboxes and broadcast slots), 0 at day start"""
    return tdiv(i16(value * pct), 100) & 0xFFFF


def news_headline(text, parts, variants, topics, lang):
    """news_text 26A7:1516: every '#n' takes the phrase of part n (topic = code % 1000, chosen variant)"""
    def rep(m):
        k = int(m.group(1)) - 1
        if k >= len(parts) or not parts[k]:
            return '?'
        t = topics.get(parts[k] % 1000)
        v = variants[k]
        if not t or not 1 <= v <= 10:
            return '?'
        return t['ph'][lang][v - 1]
    return re.sub(r'#(\d)', rep, text)


def news_score(slot_values, hour_aud_h):
    """news_rate_player 1C18:0BCB then 24B8:0C4A phase 1: average of the 3 slots, times the hour's news audience"""
    avg = tdiv(i16(sum(slot_values)), 3) & 0xFF
    return tdiv(i16(i8(avg) * hour_aud_h), 100) & 0xFF


def news_shares(scores):
    """24B8:0C4A phase 1: total = min(95, sum) (news_total_share 1DA8:0146); share = score x total / sum"""
    total_raw = sum(i8(v) for v in scores.values())
    cap = min(95, total_raw)
    if total_raw == 0:
        return {p: 0 for p in scores}
    return {p: tdiv(i16(i8(v) * cap), i16(total_raw)) & 0xFFFF for p, v in scores.items()}


def build_news(docs, S, save, res):
    raw_nodes = res.get(0xCD)
    raw_topics = res.get(0xCE)
    raw_tpl = res.get(0x30)
    topics = {}
    for r in docs['news_topics']['records']:
        i = r['id']
        ph = {lang: (list(r.get('text', {}).get(lang, [])) + [''] * 10)[:10] for lang in LANGS}
        b = raw_topics[i * 0x38:(i + 1) * 0x38]
        if not any(ph['en']) and not any(ph['de']) and not any(r['phrase_value']):
            continue
        topics[i] = dict(id=i, ph=ph, pv=list(r['phrase_value']), b2c=b[0x2C], b37=b[0x37], raw=b.hex(), used=[])
    nodes = {}
    for r in docs['news_nodes']['records']:
        i = r['id']
        t = r.get('text', {})
        if not t.get('en') and not t.get('de') and not any(r['successors']):
            continue
        nodes[i] = dict(id=i, succ=list(r['successors']), v=r['value'], d=r['delay_hours'],
                        t={lang: t.get(lang, '') for lang in LANGS}, raw=raw_nodes[i * 0xA:(i + 1) * 0xA].hex(),
                        stories=[])
    stories = []
    for r in docs['news_templates']['records']:
        i = r['id']
        b = raw_tpl[i * 0xE:(i + 1) * 0xE]
        if r['start_node'] <= 0 and not any(r['parts']):
            continue
        parts = list(r['parts'])
        pinfo = []
        for k, code in enumerate(parts):
            if not code:
                continue
            mode = code // 1000
            tp = code % 1000
            pinfo.append(dict(k=k + 1, code=code, topic=tp, mode=mode, counted=(code < 1000)))
            if tp in topics and i not in topics[tp]['used']:
                topics[tp]['used'].append(i)
        # BFS over the story's headline chain
        order, depth, seen = [], {}, set()
        queue = [r['start_node']] if r['start_node'] > 0 else []
        depth[r['start_node']] = 0
        while queue:
            n = queue.pop(0)
            if n in seen or n not in nodes:
                continue
            seen.add(n)
            order.append(n)
            nodes[n]['stories'].append(i)
            for s in nodes[n]['succ']:
                if s and s != 9999 and s not in depth:
                    depth[s] = depth[n] + 1
                    queue.append(s)
        # how many headlines until the story ends (shortest), and whether it can loop
        ends = [n for n in order if 9999 in nodes[n]['succ']]
        INF = 10 ** 6
        dist = {}
        changed = True
        for n in order:
            dist[n] = 1 if 9999 in nodes[n]['succ'] else INF
        while changed:
            changed = False
            for n in order:
                for s in nodes[n]['succ']:
                    if s in dist and dist[s] + 1 < dist[n]:
                        dist[n] = dist[s] + 1
                        changed = True
        state = {}

        def has_cycle(n):
            state[n] = 1
            for s in nodes[n]['succ']:
                if s in nodes and s in seen:
                    if state.get(s) == 1 or (s not in state and has_cycle(s)):
                        return True
            state[n] = 2
            return False
        loops = bool(order) and has_cycle(order[0])
        longest = None
        if not loops and order:
            memo = {}

            def lp(n):
                if n in memo:
                    return memo[n]
                best = 1 if 9999 in nodes[n]['succ'] else 0
                for s in nodes[n]['succ']:
                    if s in nodes and s in seen:
                        best = max(best, 1 + lp(s))
                memo[n] = best
                return best
            longest = lp(order[0])
        # example phrase choices: first and last usable variant of every random part
        examples = []
        for pick in (0, -1):
            var = [0] * 5
            for p in pinfo:
                k = p['k'] - 1
                if p['mode'] == 1 and k > 0:
                    var[k] = var[k - 1]
                else:
                    t = topics.get(p['topic'])
                    ok = [v + 1 for v in range(10) if t and t['pv'][v]]
                    var[k] = ok[pick] if ok else 0
            examples.append(var)
        counted_min = [min([v for v in topics[p['topic']]['pv'] if v] or [0]) for p in pinfo if p['counted'] and p['topic'] in topics]
        counted_max = [max(topics[p['topic']]['pv']) for p in pinfo if p['counted'] and p['topic'] in topics]
        chain = []
        for n in order:
            nd = nodes[n]
            vmin, vmax = news_value(nd['v'], counted_min), news_value(nd['v'], counted_max)
            ex = []
            for var in examples:
                cnt = [topics[p['topic']]['pv'][var[p['k'] - 1] - 1] for p in pinfo
                       if p['counted'] and p['topic'] in topics and var[p['k'] - 1]]
                val = news_value(nd['v'], cnt)
                ex.append(dict(t={lang: news_headline(nd['t'][lang], parts, var, topics, lang) for lang in LANGS},
                               v=val, cls=news_class(val)))
            chain.append(dict(n=n, depth=depth.get(n, 0), vmin=vmin, vmax=vmax, cmin=news_class(vmin),
                              cmax=news_class(vmax), ex=ex, toEnd=(dist[n] if dist[n] < INF else None)))
        stories.append(dict(
            id=i, cat=r['category'], parts=parts, pinfo=pinfo, start=r['start_node'], b0c=b[0xC], raw=b.hex(),
            chain=chain, examples=examples, ends=ends, shortest=(dist.get(r['start_node']) if order and dist.get(r['start_node'], INF) < INF else None),
            longest=longest, loops=loops,
            vmin=min([c['vmin'] for c in chain] or [0]), vmax=max([c['vmax'] for c in chain] or [0])))
    # news the game writes itself
    events = []
    for n in NEWS_EVENT_NODES:
        if n not in nodes:
            continue
        ex = []
        for var in (1, 2):
            ex.append({lang: news_headline(nodes[n]['t'][lang], [NEWS_TOPIC_EVENTS, 0, 0, 0, 0], [var, 0, 0, 0, 0],
                                           topics, lang) for lang in LANGS})
        v = i8(nodes[n]['v']) & 0xFFFF
        events.append(dict(n=n, ex=ex, v=v, cls=news_class(v)))
    reached = set(n for s in stories for c in s['chain'] for n in [c['n']])
    unreached = [n for n in sorted(nodes) if n not in reached and n not in NEWS_EVENT_NODES and
                 (nodes[n]['t']['en'] or nodes[n]['t']['de'])]
    # story slots of the start savegame (category per slot)
    t = len(save) - sum(n for _, n in SAVE_END)
    st = t - 0xC8 - 0x93
    slots = [save[st + k * 0x15 + 0x14] for k in range(7)]
    return dict(topics=[topics[k] for k in sorted(topics)], nodes=nodes, stories=stories, events=events,
                unreached=unreached, slotCats=slots, prices=NEWS_PRICES, steps=NEWS_STEPS, aiTarget=AI_NEWS_TARGET,
                agencies=NEWS_AGENCIES, cats={lang: [''] + [S[lang][289 + c] for c in (1, 2, 3)] for lang in LANGS},
                today={lang: S[lang][259] for lang in LANGS}, old={lang: S[lang][268] for lang in LANGS},
                emptyNodes=sum(1 for r in docs['news_nodes']['records'] if r['id'] and not r.get('text', {}).get('en')))


# ---------------------------------------------------------------- screenplays, cast and props
SCRIPT_SIZE = 0x1B3
ACTOR_TEXT_RES = 0x2A        # actor names and catchphrases (actor_text 24B8:1C4C)
FIRST_ACTOR_ID = 153         # name id of actor text entry 0


def parse_actor_texts(d):
    if not d:
        return []
    n = struct.unpack_from('<H', d, 0)[0] // 2
    offs = struct.unpack_from('<%dH' % n, d, 0)

    def s(o):
        e = d.find(b'\0', o)
        return d[o:e if e >= 0 else len(d)].decode(content.ENC)
    return [(s(offs[k]), s(offs[k + 1])) for k in range(0, n - 1, 2)]


def build_cast(res, S, save):
    """actor pool [category 1..7][slot 0..15] from resource 0xD0 (DS:5013), today's star guests (category 7) from the
    start savegame (DS:549F), names from resource 0x2A; props (DS:6235 prices, DS:268E quality) from resource 0xD0"""
    d0 = res.get(INIT_DATA)
    pos, o = {}, 0
    for a, n in INIT_LAYOUT:
        pos[a] = o
        o += n
    pool = bytearray(d0[pos[0x5013]:pos[0x5013] + 0x540])
    t = len(save) - sum(n for _, n in SAVE_END)
    sg = t + sum(n for _, n in SAVE_END[:5])       # DS:549F = category 7 slots 1..15
    pool[6 * 0xC0 + 0xC:7 * 0xC0] = save[sg:sg + 0xB4]
    try:
        names = parse_actor_texts(res.get(ACTOR_TEXT_RES))
    except KeyError:
        names = []
    actors = []
    for g in range(7):
        for a in range(16):
            r = pool[g * 0xC0 + a * 0xC:g * 0xC0 + (a + 1) * 0xC]
            nid, fee = struct.unpack_from('<HI', r, 0)
            if not nid and not fee:
                continue
            k = nid - FIRST_ACTOR_ID
            nm, ph = names[k] if 0 <= k < len(names) else ('<not set>', '')
            actors.append(dict(cat=g + 1, slot=a, nid=nid, name=nm, phrase=ph, fee=fee, humor=r[6], a7=r[7],
                               look=r[8], skill=r[9], icon=struct.unpack_from('<H', r, 10)[0], raw=bytes(r).hex()))
    prices = d0[pos[0x6235]:pos[0x6235] + 0x180]
    equip = d0[pos[0x268E]:pos[0x268E] + 0x60]
    props = []
    for tp in range(16, 39):
        k = tp - 15
        props.append(dict(type=tp, name={lang: S[lang][192 + tp] for lang in LANGS},
                          price=[struct.unpack_from('<I', prices, k * 16 + 4 * lv)[0] for lv in range(4)],
                          q=list(equip[k * 4:k * 4 + 4])))
    cats = {lang: [''] + [S[lang][145 + c] for c in range(1, 8)] for lang in LANGS}
    levels = {lang: [''] + [S[lang][236 + lv] for lv in (1, 2, 3)] for lang in LANGS}
    attrs = {lang: S[lang][363].split('~') for lang in LANGS}
    return dict(actors=actors, props=props, cats=cats, levels=levels, attrs=attrs, pool=bytes(pool))


def role_match(req, actor):
    """script_role_match_pct 1DA8:2560: sum(min(requirement, attribute)) / sum(requirement) x 100 over
    humor (+0), appearance (+2) and skill (+3); attribute +1 is skipped"""
    att = [actor[0], actor[1], actor[2], actor[3]]
    have = need = 0
    for k in (0, 2, 3):
        have += min(i8(req[k]) & 0xFFFF, i8(att[k]) & 0xFFFF)
        need += i8(req[k]) & 0xFFFF
    return tdiv(i16(have * 100), i16(need)) & 0xFFFF if need else 0


def pool_actor(pool, cat, slot):
    r = pool[(cat - 1) * 0xC0 + slot * 0xC:(cat - 1) * 0xC0 + (slot + 1) * 0xC]
    return [r[6], r[7], r[8], r[9]], struct.unpack_from('<I', r, 2)[0], struct.unpack_from('<H', r, 0)[0]


def production_quality(block, script_raw, equip, choice):
    """studio_start_production 229E:0B98: roles add match% x weight / 100, props add min(weight, prop quality);
    quality = base x score / sum(weights) x 125 / 100.  choice[j] (slot j = 1..10) = the chosen actor's 4
    attribute bytes for a role, the level 0..3 for a prop."""
    score = wsum = 0
    for j in range(1, 11):
        tp, w = i8(block[3 * j + 1]), i8(block[3 * j + 2]) & 0xFFFF
        if 1 <= tp <= 15:
            r = tp - 1
            att = choice.get(j) or [0, 0, 0, 0]
            m = role_match(script_raw[0x168 + 4 * r:0x168 + 4 * r + 4], att)
            score = (score + (tdiv(i16(m * w), 100) & 0xFFFF)) & 0xFFFF
            wsum = (wsum + w) & 0xFFFF
        elif tp >= 16:
            lv = choice.get(j) or 0
            eq = equip[(tp - 15) * 4 + lv] if 0 <= (tp - 15) * 4 + lv < len(equip) else 0
            score = (score + min(w, i8(eq) & 0xFFFF)) & 0xFFFF
            wsum = (wsum + w) & 0xFFFF
    q = tdiv(i16((i8(block[0]) & 0xFFFF) * score), i16(wsum)) & 0xFFFF if wsum else 0
    return tdiv(i16(q * 125), 100) & 0xFFFF


def build_scripts(docs, S, res, films, series, cast):
    raw_all = res.get(0xCB)
    pool = cast['pool']
    d0 = res.get(INIT_DATA)
    pos, o = {}, 0
    for a, n in INIT_LAYOUT:
        pos[a] = o
        o += n
    equip = d0[pos[0x268E]:pos[0x268E] + 0x60]
    prices = d0[pos[0x6235]:pos[0x6235] + 0x180]
    fmap = {f['id']: f for f in films}
    smap = {s['id']: s for s in series}
    actors_by_cat = {}
    for a in cast['actors']:
        actors_by_cat.setdefault(a['cat'], []).append(a)
    out = []
    for r in docs['screenplays']['records']:
        i = r['id']
        if i == 0:
            continue
        raw = bytearray(raw_all[i * SCRIPT_SIZE:(i + 1) * SCRIPT_SIZE])
        tx = {}
        for lang in LANGS:
            tx[lang], _ = text_pair(r.get('text'), lang, 16)
        flags, target = r['flags'], r['target']
        is_series = bool(flags & 1)
        multi = bool(flags & 4)
        copied = False
        if not is_series and multi:            # script_init_episodes 1C18:0313: block 0 -> blocks 1..9 at load
            copied = True
            for e in range(1, 10):
                raw[8 + e * 0x23:8 + (e + 1) * 0x23] = raw[8:8 + 0x23]
        if is_series:
            prog = smap.get(target)
            n_ep = prog['episodes'] if prog else 0
            mult = prog['blocks'] if prog else 0
            ptitle = prog['t'] if prog else None
        else:
            prog = fmap.get(target)
            n_ep = 10 if multi else 1
            mult = prog['blocks'] if prog else 0
            ptitle = prog['t'] if prog else None
        roles = []
        for k in range(15):
            req = list(raw[0x168 + 4 * k:0x168 + 4 * k + 4])
            cat = raw[0x1A4 + k]
            if not any(req) and not cat and not (tx['en'][1 + k] or tx['de'][1 + k]):
                continue
            cands = []
            pool_cats = [cat] if 1 <= cat <= 6 else ([1, 2, 3, 4, 6] if cat == 7 else [])
            for c in pool_cats:
                for a in actors_by_cat.get(c, []):
                    m = role_match(req, [a['humor'], a['a7'], a['look'], a['skill']])
                    cands.append(dict(name=a['name'], cat=c, slot=a['slot'], m=m, fee=a['fee']))
            cands.sort(key=lambda x: (-x['m'], x['fee']))
            roles.append(dict(r=k + 1, req=req, cat=cat, name={lang: tx[lang][1 + k] for lang in LANGS}, best=cands[:3]))
        rolemap = {x['r']: x for x in roles}
        blocks = []
        for e in range(10):
            b = raw[8 + e * 0x23:8 + (e + 1) * 0x23]
            slots = [[b[4 + 3 * j], b[5 + 3 * j], b[6 + 3 * j]] for j in range(10)]
            used = e < max(n_ep, 1) or any(x[0] for x in slots) or b[0]
            if not used:
                continue
            # best choice from the pool (star guests: today's start list) and the best prop level
            best, perfect, cost = {}, 0, 0
            for j in range(1, 11):
                tp, w = b[3 * j + 1], b[3 * j + 2]
                if 1 <= tp <= 15:
                    cat = raw[0x1A4 + tp - 1]
                    top = None
                    for c in ([cat] if 1 <= cat <= 6 else [1, 2, 3, 4, 6] if cat == 7 else []):
                        for a in actors_by_cat.get(c, []):
                            att = [a['humor'], a['a7'], a['look'], a['skill']]
                            m = role_match(raw[0x168 + 4 * (tp - 1):0x168 + 4 * (tp - 1) + 4], att)
                            if top is None or (m, -a['fee']) > (top[0], -top[2]):
                                top = (m, att, a['fee'])
                    if top:
                        best[j] = top[1]
                        cost += top[2] * mult
                elif tp >= 16:
                    k = tp - 15
                    lv = max((1, 2, 3), key=lambda lv: (equip[k * 4 + lv] if k * 4 + lv < len(equip) else 0, -lv))
                    best[j] = lv
                    if k * 16 + 4 * lv + 4 <= len(prices):
                        cost += struct.unpack_from('<I', prices, k * 16 + 4 * lv)[0] * mult
            # a perfect cast: every role 100 %
            score = wsum = 0
            for j in range(1, 11):
                tp, w = i8(b[3 * j + 1]), b[3 * j + 2]
                if 1 <= tp <= 15:
                    score += w
                    wsum += w
                elif tp >= 16:
                    k = tp - 15
                    score += min(w, max(equip[k * 4 + 1:k * 4 + 4] or [0]))
                    wsum += w
            perfect = tdiv(tdiv(b[0] * score, wsum) * 125, 100) if wsum else 0
            blocks.append(dict(e=e + 1, base=b[0], b1=b[1], mult=b[2], b3=b[3], b34=b[34], slots=slots,
                               bestQ=production_quality(b, raw, equip, best), perfectQ=perfect,
                               cost=cost, copy=(copied and e > 0), hex=bytes(b).hex()))
        prog_price = prog['price'] if prog and not is_series else None
        out.append(dict(
            id=i, t=tx, price=r['price'], state=r['state'], flags=flags, target=target, series=is_series, multi=multi,
            offered=bool(flags & 2), duration=r['duration_hours'], episodes=n_ep, mult=mult, ptitle=ptitle,
            kind=('series' if is_series else 'show' if multi else 'film'),
            pprice=prog_price, pflags2=(prog['flags2'] if prog and not is_series else None),
            roles=roles, blocks=blocks, raw=bytes(raw_all[i * SCRIPT_SIZE:(i + 1) * SCRIPT_SIZE]).hex(),
            req=bytes(raw[0x168:0x1A4]).hex(), rcat=bytes(raw[0x1A4:0x1B3]).hex(),
            bestQ=max([x['bestQ'] for x in blocks] or [0]), perfectQ=max([x['perfectQ'] for x in blocks] or [0]),
            base1=(blocks[0]['base'] if blocks else 0)))
    return out


def build(paths):
    res = content.Resources(paths)
    docs = content.export(res)
    S = game_strings(res)
    ss_films, ss_series, ss_ads, reach, trends = parse_start_save(res.get(START_SAVE))
    # new_day at the start of a new game (trend_pick_new 24B8:1B5C) shifts the genres one slot down; the
    # multipliers stay with the slots, so the first evening's three trends are known
    trends_day1 = [[trends[k + 1][0], trends[k][1]] for k in range(3)]
    hour_aud, genre_aud = parse_init_data(res.get(INIT_DATA))
    raw = {k: res.get(r) for k, r in (('films', 0xC9), ('series', 0xCA), ('ads', 0xCC))}

    # screenplay -> produced film / series
    produced = {}
    for r in docs['screenplays']['records']:
        if r['id'] == 0 or not (r['target'] or r['flags']):
            continue
        kind = 'series' if r['flags'] & 1 else 'film'
        produced[(kind, r['target'])] = dict(script=r['id'], price=r['price'], offered=bool(r['flags'] & 2),
                                             multi=bool(r['flags'] & 4))

    human_reach = reach[1]
    max_class = ad_max_class(reach[1:4])
    diffs = {'film_diff': [], 'series_diff': [], 'ad_diff': []}

    # ---- films
    films = []
    for r in docs['films']['records']:
        i = r['id']
        if i == 0:
            continue
        b = raw['films'][i * 0x17:(i + 1) * 0x17]
        tx, corrupt = {}, []
        for lang in LANGS:
            tx[lang], bad = text_pair(r.get('text'), lang, 2)
            if bad:
                corrupt.append(lang)
        f = dict(id=i, t=tx, corrupt=corrupt, raw=b.hex(), genre=r['genre'], quality=r['quality'],
                 wear_rate=r['wear_rate'], topicality=r['topicality'], blocks=r['blocks'], owner=r['owner'],
                 flags=r['flags'], flags2=r['flags2'], price=r['price'], jingle=r['jingle'], b0e=b[0xE],
                 live_day=r['live_day'], live_slot=r['live_slot'], c13=b[0x13], c14=b[0x14], c15=b[0x15], c16=b[0x16])
        st = ss_films[i]
        start = {}
        for key in ('quality', 'topicality', 'owner', 'price', 'b0e', 'live_day', 'live_slot'):
            if f[key] != st[key]:
                start[key] = st[key]
        fl0 = (f['flags'] & ~0xA0) | (st['fl5'] << 5) | (st['fl7'] << 7)
        f20 = (f['flags2'] & ~0x03) | st['f20'] | (st['f21'] << 1)
        if fl0 != f['flags']:
            start['flags'] = fl0
        if f20 != f['flags2']:
            start['flags2'] = f20
        if start:
            diffs['film_diff'].append(i)
        g = lambda k: start.get(k, f[k])    # noqa: E731  value at the start of a new game
        q, top, price = g('quality'), g('topicality'), g('price')
        seq_t, seq_v = [top], [movie_price(price, q, top)]
        for _ in range(4):
            seq_t.append(film_wear(q, f['wear_rate'], seq_t[-1]))
            seq_v.append(movie_price(price, q, seq_t[-1]))
        f.update(start=start, value=seq_v[0], loss=film_wear_loss(q, f['wear_rate']), seqT=seq_t, seqV=seq_v,
                 solo=solo_share(i8(top) & 0xFFFF, f['genre'], genre_aud),
                 market=market_of(g('flags'), g('owner'), price != 0),
                 gift=(f['genre'] == 6 and g('owner') == 0 and bool(g('flags') & 0x20)),
                 prod=produced.get(('film', i)))
        films.append(f)

    # ---- series
    series = []
    for r in docs['series']['records']:
        i = r['id']
        if i == 0:
            continue
        b = raw['series'][i * 0x39:(i + 1) * 0x39]
        tx = {}
        for lang in LANGS:
            tx[lang], _ = text_pair(r.get('text'), lang, 22)
        s = dict(id=i, t=tx, raw=b.hex(), ep0=b[0:3].hex(), episodes_qtf=r['episodes_qtf'], genre=r['genre'],
                 episodes=r['episodes'], blocks=r['blocks'], owner=r['owner'], flags=r['flags'], flags2=r['flags2'],
                 price=r['price'], episode_price=r['episode_price'], jingle=r['jingle'],
                 next_episode=r['next_episode'], wear_rate=r['wear_rate'], b34=b[0x34], critics=r['critics'],
                 press=r['press'], b37=b[0x37:0x39].hex())
        st = ss_series[i]
        start = {}
        if st['qtf'] != s['episodes_qtf']:
            start['episodes_qtf'] = st['qtf']
        for key in ('owner', 'price', 'next_episode'):
            if st[key] != s[key]:
                start[key] = st[key]
        fl0 = (s['flags'] & ~0xA0) | (st['fl5'] << 5) | (st['fl7'] << 7)
        f20 = (s['flags2'] & ~0x03) | st['f20'] | (st['f21'] << 1)
        if fl0 != s['flags']:
            start['flags'] = fl0
        if f20 != s['flags2']:
            start['flags2'] = f20
        if start:
            diffs['series_diff'].append(i)
        g = lambda k: start.get(k, s[k])    # noqa: E731
        qtf = g('episodes_qtf')
        eps = []
        for e in range(1, 11):
            q, t, fl = qtf[3 * e], qtf[3 * e + 1], qtf[3 * e + 2]
            seq = [t]
            for _ in range(3):
                seq.append(episode_wear(q, s['wear_rate'], seq[-1]))
            eps.append(dict(q=q, t=t, f=fl, loss=tdiv(i16(i8(s['wear_rate']) * i8(q)), 20), seq=seq))
        made = 0
        while made < 10 and qtf[3 * (made + 1) + 2] & 1:
            made += 1
        worn = list(qtf)
        for e in range(1, made + 1):
            worn[3 * e + 1] = episode_wear(qtf[3 * e], s['wear_rate'], qtf[3 * e + 1])
        nq = [qtf[3 * e] for e in range(1, min(s['episodes'], 10) + 1)]
        s.update(start=start, eps=eps, made=made, avgq=(int(round(sum(nq) / float(len(nq)))) if nq else 0),
                 value=series_price(qtf, s['episode_price']), valueWorn=series_price(worn, s['episode_price']),
                 solo=solo_share(i8(qtf[4]) & 0xFFFF, 10 + s['genre'], genre_aud),
                 market=market_of(g('flags'), g('owner')), gift=(18 <= i <= 20 and g('owner') == 0),
                 prod=produced.get(('series', i)))
        series.append(s)

    # ---- ads
    ads = []
    for r in docs['ads']['records']:
        i = r['id']
        if i == 0:
            continue
        b = raw['ads'][i * 0x18:(i + 1) * 0x18]
        tx = {}
        for lang in LANGS:
            tx[lang], _ = text_pair(r.get('text'), lang, 2)
        rule = [i8(c) for c in r['genre_rule']]
        a = dict(id=i, t=tx, raw=b.hex(), p_name=b[0:4].hex(), p_slogan=b[4:8].hex(), days=r['days'],
                 spots=r['spots'], min_audience_class=r['min_audience_class'], target_group=r['target_group'],
                 genre_rule=rule, holder=r['holder'], fee=r['fee'], penalty=r['penalty'])
        start = {}
        if ss_ads[i] != a['holder']:
            start['holder'] = ss_ads[i]
            diffs['ad_diff'].append(i)
        cls = a['min_audience_class']
        minv = AD_MIN_VIEWERS[cls - 1] if 1 <= cls <= 10 else None
        pct = viewers_to_pct(human_reach, minv) if minv is not None else None
        a.update(start=start, minv=minv, pct=pct, image=max(pct or 0, 20 if rule[0] != 0 else 0),
                 accepts=[ad_target_ok(r['genre_rule'], gg) for gg in range(16)],
                 offered=bool(any(b[0:4]) and a['fee'] != 0 and cls <= max_class),
                 rivals=all(c <= 0 for c in rule), cond={lang: ad_condition(rule, S, lang) for lang in LANGS})
        ads.append(a)

    save = res.get(START_SAVE)
    news = build_news(docs, S, save, res)
    cast = build_cast(res, S, save)
    scripts = build_scripts(docs, S, res, films, series, cast)
    cast.pop('pool')
    genres = {lang: [''] + [S[lang][26 + gg] for gg in range(1, 11)] + [S[lang][292 + gg] for gg in range(1, 6)]
              for lang in LANGS}
    countries = {lang: [S[lang][318 + c] for c in range(10)] for lang in LANGS}
    ui = {lang: dict((str(n), S[lang][n]) for n in UI_STRINGS) for lang in LANGS}
    meta = dict(
        files=[dict(name=os.path.basename(p), size=os.path.getsize(p)) for p in paths],
        generated=datetime.date.today().isoformat(),
        genres=genres, countries=countries, ui=ui, fields=FIELDS,
        hourAud=hour_aud, genreAud=[genre_aud[gg * 8:(gg + 1) * 8] for gg in range(16)],
        minViewers=AD_MIN_VIEWERS, reach=reach[1:4], humanReach=human_reach, viewersPerUnit=VIEWERS_PER_UNIT,
        maxClass=max_class, giftFilms=[f['id'] for f in films if f['gift']], startDiff=diffs, hours=HOURS,
        trendsSave=trends, trendsDay1=trends_day1)
    return dict(meta=meta, films=films, series=series, ads=ads, news=news, scripts=scripts, cast=cast), genre_aud


# ---------------------------------------------------------------- verification against the game
def run_game(binary, gdir, lines, tmp, name):
    """run the headless build on fresh copies of the game files in gdir (a run may write to the .rsc files)"""
    run_dir = os.path.join(tmp, 'g_' + name)
    if os.path.exists(run_dir):
        shutil.rmtree(run_dir)
    shutil.copytree(gdir, run_dir)
    gdir = run_dir
    script = os.path.join(tmp, name + '.txt')
    lines = sorted(lines, key=lambda x: x[0])
    with open(script, 'w') as f:
        for fr, cmd in lines:
            f.write('%d %s\n' % (fr, cmd))
    p = subprocess.run([binary, '--headless', '--nosound', '--game', gdir, '--args', 'e', '--script', script],
                       cwd=tmp, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=900)
    out = p.stdout.decode('latin-1')
    err = p.stderr.decode('latin-1')
    m = re.search(r'bytes at ([0-9A-Fa-f]{4})', err)
    return out, err, (int(m.group(1), 16) if m else 0x01B0)


NEW_GAME = [(400, 'key esc'), (1000, 'click 64 100'), (1300, 'click 172 185')]


class Mem:
    def __init__(self, path, loadseg):
        self.m = open(path, 'rb').read()
        self.ds = (loadseg + 0x39D4) << 4

    def g8(self, off):
        return self.m[self.ds + off]

    def g16(self, off):
        return struct.unpack_from('<H', self.m, self.ds + off)[0]

    def g32(self, off):
        return struct.unpack_from('<I', self.m, self.ds + off)[0]

    def far(self, off):
        o, s = struct.unpack_from('<HH', self.m, self.ds + off)
        return (s << 4) + o

    def film(self, i):
        a = self.far(0x24A9) + i * 0x17
        return self.m[a:a + 0x17]

    def series(self, i):
        a = self.far(0x2646) + i * 0x39
        return self.m[a:a + 0x39]


def parse_calls(out):
    res = []
    for ln in out.splitlines():
        m = re.match(r'CALL frame \d+ ([0-9A-Fa-f]{4}:[0-9A-Fa-f]{4})((?:,-?\w+)*) (\d+) (\d+)$', ln.strip())
        if m:
            args = [int(a, 0) for a in m.group(2).split(',')[1:]]
            res.append((m.group(1).upper(), args, i32(int(m.group(3)) | (int(m.group(4)) << 16))))
    return res


def check(name, fn, how, pairs):
    """pairs: list of (label, expected, actual)"""
    bad = [(lab, e, a) for lab, e, a in pairs if e != a]
    return dict(name=name, fn=fn, how=how, n=len(pairs), ok=len(pairs) - len(bad),
                bad=[dict(item=lab, port=e, game=a) for lab, e, a in bad[:8]])


class Script:
    """script lines at one frame, in order (the call harness holds timers, interrupts and frame events and
    runs calls with DS = ES = DGROUP)"""
    def __init__(self, start=2000, extra=()):
        self.lines = list(NEW_GAME) + list(extra)
        self.f = start

    def at(self, cmd):
        self.lines.append((self.f, cmd))
        return self.f


def verify(binary, paths, data, genre_aud):
    exe = os.path.join(ROOT, 'build', 'MTV_U.EXE')
    tmp = tempfile.mkdtemp(prefix='catalog-')
    try:
        gdir = os.path.join(tmp, 'game')
        os.makedirs(gdir)
        for p, n in zip(paths, ('SOURCE1.RSC', 'SOURCE2.RSC')):
            shutil.copy(p, os.path.join(gdir, n))
        shutil.copy(exe, gdir)
        result = dict(binary=os.path.relpath(binary, ROOT), checks=[], observed={}, noise=0)
        D = lambda n: os.path.join(tmp, n)   # noqa: E731
        films = {f['id']: f for f in data['films']}
        series = {s['id']: s for s in data['series']}
        ads = {a['id']: a for a in data['ads']}

        def calls3(sc, cmd):
            sc.at('call ' + cmd)

        def results(out):
            got = {}
            for fn, args, v in parse_calls(out):
                got.setdefault('%s,%s' % (fn, ','.join(map(str, args))), []).append(v)
            return got

        def value(got, cmd, k=0):
            vals = got.get(cmd, [])
            return vals[k] if k < len(vals) else None

        # ---- run A1: game start; value functions called through the harness
        sc = Script()
        sc.at('dump ' + D('start.bin'))
        for i in range(1, 201):
            calls3(sc, '1DA8:3F5B,%d' % i)
        for s in range(1, 51):
            calls3(sc, '1DA8:4035,%d' % s)
        for a in range(1, 51):
            for g in range(1, 16):
                calls3(sc, '1DA8:4258,%d,%d' % (a, g))
        for p in (1, 2, 3):
            for v in AD_MIN_VIEWERS:
                calls3(sc, '1DA8:2682,%d,%d,%d' % (p, v & 0xFFFF, v >> 16))
        role_calls = []                  # (script, slot) of every role slot of episode 1, actors 1..15
        for x in data['scripts']:
            if not x['blocks']:
                continue
            b = bytes.fromhex(x['blocks'][0]['hex'])
            for j in range(1, 11):
                if 1 <= b[3 * j + 1] <= 15:
                    role_calls.append((x['id'], j))
                    for a in range(1, 16):
                        sc.at('call 1DA8:2560,%d,1,%d,%d' % (x['id'], j, a))
        sc.at('quit')
        out, err, seg = run_game(binary, gdir, sc.lines, tmp, 'run_a1')
        got = results(out)
        if not got or not os.path.exists(D('start.bin')):
            raise RuntimeError('game run A1 produced no results: ' + err[-400:])
        m0 = Mem(D('start.bin'), seg)
        pairs = [('movie %d' % i, films[i]['value'], value(got, '1DA8:3F5B,%d' % i)) for i in range(1, 201)]
        result['checks'].append(check('Movie value at game start (column "Price")', '1DA8:3F5B',
                                      'call 1DA8:3F5B,id for ids 1–200', pairs))
        pairs = [('series %d' % i, series[i]['value'], value(got, '1DA8:4035,%d' % i)) for i in range(1, 51)]
        result['checks'].append(check('Series value at game start (column "Value")', '1DA8:4035',
                                      'call 1DA8:4035,id for ids 1–50', pairs))
        pairs = []
        for a in range(1, 51):
            for g in range(1, 16):
                v = value(got, '1DA8:4258,%d,%d' % (a, g))
                pairs.append(('ad %d genre %d' % (a, g), ads[a]['accepts'][g], None if v is None else v & 0xFFFF))
        result['checks'].append(check('Ad accepts programme genre (50 ads × 15 genres)', '1DA8:4258',
                                      'call 1DA8:4258,ad,genre', pairs))
        pairs, pct1 = [], {}
        for p in (1, 2, 3):
            r = m0.g32(0x1D09 + p * 0x1CE + 4)
            for c, v in enumerate(AD_MIN_VIEWERS):
                g = value(got, '1DA8:2682,%d,%d,%d' % (p, v & 0xFFFF, v >> 16))
                g = None if g is None else g & 0xFFFF
                if p == 1:
                    pct1[c + 1] = g
                pairs.append(('station %d, %d viewers' % (p, v), viewers_to_pct(r, v), g))
        result['checks'].append(check('Minimum audience as % of reach (3 stations × 10 classes)', '1DA8:2682',
                                      'call 1DA8:2682,station,lo,hi', pairs))
        pairs = [('ad %d' % a['id'], a['pct'], pct1.get(a['min_audience_class'])) for a in data['ads']]
        result['checks'].append(check('Column "% of reach" (human player at game start)', '1DA8:2682',
                                      'the calls above, station 1', pairs))
        pairs = [('class %d' % (k + 1), AD_MIN_VIEWERS[k], i32(m0.g32(0xA8 + 4 * k))) for k in range(10)]
        pairs += [('reach station %d' % p, data['meta']['reach'][p - 1], m0.g32(0x1D09 + p * 0x1CE + 4)) for p in (1, 2, 3)]
        pairs += [('genre x hour %d' % k, genre_aud[k], m0.g8(0x7160 + k)) for k in range(0x80)]
        pairs += [('hour audience %d' % k, data['meta']['hourAud'][k], m0.g8(0x1D00 + k)) for k in range(8)]
        result['checks'].append(check('Tables in game memory: audience classes, reach, genre × hour, news audience',
                                      'DS:00A8, 1D09, 7160, 1D00', 'dump at frame 2000', pairs))
        offers = [m0.g16(0x19A0 + k * 7) for k in range(1, 11)]
        pairs = [('ad %d' % a, True, ads[a]['offered']) for a in offers if a]
        result['checks'].append(check('Ads on the agency shelf at game start are marked "offered"', '24B8:000C',
                                      'dump DS:19A0', pairs))
        obs = dict(films_owned=[], series_owned=[], dealer=[], auction=[], ad_offers=offers)
        for i in range(1, 201):
            o = m0.film(i)[5]
            if 1 <= o <= 3:
                obs['films_owned'].append([i, o])
        for i in range(1, 51):
            o = m0.series(i)[0x24]
            if 1 <= o <= 3:
                obs['series_owned'].append([i, o])
        for k in range(1, 11):
            prog, price = struct.unpack_from('<HI', m0.m, m0.ds + 0x19ED + k * 8)
            if prog:
                obs['dealer'].append([prog, price])
            prog, bid = struct.unpack_from('<HI', m0.m, m0.ds + 0x1B0D + k * 9)
            if prog:
                obs['auction'].append([prog, bid, m0.m[m0.ds + 0x1B0D + k * 9 + 7]])
        result['observed'] = obs
        val = lambda prog: films[prog]['value'] if prog < 1000 else series[prog - 1000]['value']   # noqa: E731
        pairs = [('dealer %d' % prog, val(prog), price) for prog, price in obs['dealer']]
        pairs += [('auction %d' % prog, val(prog), bid) for prog, bid, _ in obs['auction']]
        result['checks'].append(check('Dealer prices and auction start bids at game start', '24B8:04B9, 1DA8:0CB3',
                                      'dump DS:19ED, DS:1B0D', pairs))
        pairs = [('movie %d' % i, [films[i][k] if k not in films[i]['start'] else films[i]['start'][k]
                                   for k in ('quality', 'topicality', 'price')],
                  [m0.film(i)[1], m0.film(i)[3], struct.unpack_from('<i', m0.film(i), 8)[0]]) for i in range(1, 201)]
        result['checks'].append(check('Quality, topicality and price at game start (start savegame)', '0054:0005',
                                      'dump at frame 2000', pairs))

        # news, cast and screenplay tables in memory
        N = data['news']
        pairs = [('news price class %d' % k, NEWS_PRICES[k], m0.g16(0x129C + 2 * k)) for k in range(6)]
        pairs += [('price step %d' % k, NEWS_STEPS[k], m0.g8(0x11E8 + k)) for k in range(6)]
        pairs += [('price step copy %d' % k, NEWS_STEPS[k], m0.g8(0x11EE + k)) for k in range(6)]
        pairs += [('AI news target %d' % k, AI_NEWS_TARGET[k], m0.g16(0x15FC + 2 * k)) for k in range(4)]
        pairs += [('story slot %d category' % k, N['slotCats'][k], m0.g8(0x70C9 + k * 0x15 + 0x14)) for k in range(7)]
        raw_nodes = bytes.fromhex(''.join(N['nodes'][n]['raw'] for n in sorted(N['nodes'])))
        pairs += [('headline node %d' % n, N['nodes'][n]['raw'], m0.m[m0.ds + 0x2731 + n * 0xA:m0.ds + 0x2731 + n * 0xA + 0xA].hex())
                  for n in sorted(N['nodes'])]
        pairs += [('phrase values list %d' % t['id'], t['pv'], list(m0.m[m0.ds + 0x5593 + t['id'] * 0x38 + 0x2D:m0.ds + 0x5593 + t['id'] * 0x38 + 0x37]))
                  for t in N['topics']]
        pairs += [('story %d parts and category' % st['id'], [st['parts'], st['cat']],
                   [list(struct.unpack_from('<5H', m0.m, m0.ds + 0x3AA1 + st['id'] * 0xE)), m0.g8(0x3AA1 + st['id'] * 0xE + 0xD)])
                  for st in N['stories']]
        result['checks'].append(check('News tables in memory: prices, price steps, AI targets, story slots, %d headline nodes, '
                                      'phrase values, stories' % len(N['nodes']), 'DS:129C, 11E8, 11EE, 15FC, 70C9, 2731, 5593, 3AA1',
                                      'dump at frame 2000', pairs))
        cast = data['cast']
        pairs = []
        for a in cast['actors']:
            if a['cat'] == 7:
                continue                 # star guests: drawn again by the new day at game start
            o = 0x5013 + (a['cat'] - 1) * 0xC0 + a['slot'] * 0xC
            pairs.append(('actor %s (category %d slot %d)' % (a['name'], a['cat'], a['slot']), a['raw'], m0.m[m0.ds + o:m0.ds + o + 0xC].hex()))
        for pr in cast['props']:
            k = pr['type'] - 15
            pairs.append(('set %d prices' % pr['type'], pr['price'], [m0.g32(0x6235 + k * 16 + 4 * lv) for lv in range(4)]))
            pairs.append(('set %d quality' % pr['type'], pr['q'], list(m0.m[m0.ds + 0x268E + k * 4:m0.ds + 0x268E + k * 4 + 4])))
        sp = m0.far(0x715C)
        for x in data['scripts']:
            mem = m0.m[sp + x['id'] * SCRIPT_SIZE:sp + (x['id'] + 1) * SCRIPT_SIZE]
            raw = bytearray.fromhex(x['raw'])
            if x['kind'] == 'show':                       # script_init_episodes copies block 1 to 2..10 at load
                for e in range(1, 10):
                    raw[8 + e * 0x23:8 + (e + 1) * 0x23] = raw[8:8 + 0x23]
            skip = set([4])                               # runtime state
            pairs.append(('screenplay %d' % x['id'], bytes(b for k, b in enumerate(raw) if k not in skip).hex(),
                          bytes(b for k, b in enumerate(mem) if k not in skip).hex()))
        result['checks'].append(check('Cast pool, sets and %d screenplay records in memory' % len(data['scripts']),
                                      'DS:5013, 6235, 268E, g_scripts', 'dump at frame 2000', pairs))
        smap = {x['id']: x for x in data['scripts']}
        offers = [m0.g16(0x196A + k * 9) for k in range(1, 6)]
        pairs = [('screenplay %d on the agency shelf' % o, True, smap[o]['offered']) for o in offers if o]
        result['checks'].append(check('Screenplays on the agency shelf at game start are marked "offered"', '24B8:02C1',
                                      'dump DS:196A', pairs))
        pairs = []
        for sid, j in role_calls:
            mem = m0.m[sp + sid * SCRIPT_SIZE:sp + (sid + 1) * SCRIPT_SIZE]
            tp = mem[8 + 3 * j + 1]
            cat = mem[0x1A4 + tp - 1]
            req = mem[0x168 + 4 * (tp - 1):0x168 + 4 * (tp - 1) + 4]
            for a in range(1, 16):
                o = m0.ds + 0x5013 + (cat - 1) * 0xC0 + a * 0xC
                att = list(m0.m[o + 6:o + 10]) if 1 <= cat <= 7 else [0, 0, 0, 0]
                g = value(got, '1DA8:2560,%d,1,%d,%d' % (sid, j, a))
                pairs.append(('screenplay %d slot %d actor %d' % (sid, j, a), role_match(req, att), None if g is None else g & 0xFFFF))
        result['checks'].append(check('Actor match for every role of episode 1 (all 15 actors of its category)', '1DA8:2560',
                                      'call 1DA8:2560,screenplay,1,slot,actor', pairs))
        pairs = []
        for x in data['scripts']:
            for r in x['roles']:
                for b in r['best']:
                    if b['cat'] == 7 or not 1 <= b['cat'] <= 6:
                        continue
                    o = m0.ds + 0x5013 + (b['cat'] - 1) * 0xC0 + b['slot'] * 0xC
                    pairs.append(('screenplay %d role %d: %s' % (x['id'], r['r'], b['name']), b['m'],
                                  role_match(r['req'], list(m0.m[o + 6:o + 10]))))
        result['checks'].append(check('Detail "best matches" (page values against the port on game memory)', '1DA8:2560',
                                      'dump at frame 2000', pairs))

        # ---- run A3: productions.  A studio and the screenplay's cast/set choices are poked, then
        # studio_start_production is called; the film or episode quality it writes is compared with the port.
        cases = [(1, 1), (5, 1), (16, 1), (17, 1), (21, 1), (8, 1), (8, 3), (9, 2), (12, 1), (34, 1), (35, 2), (36, 1)]
        seg_rel = (sp >> 4) - seg
        base_off = sp - (sp >> 4 << 4)
        sc = Script()
        plan = []
        for st, (sid, e) in enumerate(cases, 1):
            x = smap.get(sid)
            if not x or (x['series'] and e > x['episodes']):
                continue
            blk = sp + sid * SCRIPT_SIZE + 8 + (e - 1) * 0x23
            choice = {}
            for j in range(1, 11):
                tp = m0.m[blk + 3 * j + 1]
                if 1 <= tp <= 15:
                    choice[j] = (sid * 7 + j * 5 + e) % 15 + 1
                elif tp >= 16:
                    choice[j] = (sid + j) % 3 + 1
                else:
                    continue
                a = blk + 3 * j + 3 - (sp >> 4 << 4)
                sc.at('poke16 %X:%X %d' % (seg_rel, base_off + a - base_off, choice[j] | (m0.m[blk + 3 * j + 4] << 8)))
            so = 0x6165 + st * 0x10
            size = m0.g8(so + 2)
            sc.at('poke16 39D4:%X %d' % (so, sid))
            sc.at('poke16 39D4:%X %d' % (so + 2, size | (1 << 8)))
            sc.at('poke16 39D4:%X %d' % (so + 4, e | (e << 8)))
            sc.at('call 229E:0B98,%d' % st)
            sc.at('dump ' + D('prod%d.bin' % st))
            plan.append((st, sid, e, choice))
        sc.at('quit')
        out, err, seg3 = run_game(binary, gdir, sc.lines, tmp, 'run_a3')
        equip = m0.m[m0.ds + 0x268E:m0.ds + 0x268E + 0x60]
        pairs, pairs_p = [], []
        for st, sid, e, choice in plan:
            x = smap[sid]
            mem = m0.m[sp + sid * SCRIPT_SIZE:sp + (sid + 1) * SCRIPT_SIZE]
            blk = bytearray(mem[8 + (e - 1) * 0x23:8 + e * 0x23])
            att = {}
            for j, c in choice.items():
                blk[3 * j + 3] = c
                tp = blk[3 * j + 1]
                if tp <= 15:
                    cat = mem[0x1A4 + tp - 1]
                    o = m0.ds + 0x5013 + (cat - 1) * 0xC0 + c * 0xC
                    att[j] = list(m0.m[o + 6:o + 10]) if 1 <= cat <= 7 else [0, 0, 0, 0]
                else:
                    att[j] = c
            q = production_quality(blk, mem, equip, att)
            if not os.path.exists(D('prod%d.bin' % st)):
                pairs.append(('screenplay %d episode %d' % (sid, e), q, None))
                continue
            mq = Mem(D('prod%d.bin' % st), seg3)
            lab = 'screenplay %d episode %d (studio %d)' % (sid, e, st)
            if x['series']:
                sr = mq.series(x['target'])
                pairs.append((lab, [q, q], [sr[3 * e], sr[3 * e + 1]]))
            else:
                fb = mq.film(x['target'])
                pairs.append((lab, [q, q], [fb[1], fb[3]]))
                f0 = m0.film(x['target'])
                p0 = struct.unpack_from('<i', f0, 8)[0]
                want = 0 if (f0[7] & 4) or blk[0] == 0 else i32(tdiv(i32(q * p0), 100))
                pairs_p.append((lab, want, struct.unpack_from('<i', fb, 8)[0]))
        result['checks'].append(check('Production quality (%d productions with poked cast and sets)' % len(plan), '229E:0B98',
                                      'poke studio and choices, call 229E:0B98,studio, dump', pairs))
        result['checks'].append(check('Movie price after production', '229E:0B98', 'same dumps', pairs_p))

        # ---- run A2: two forced airings of everything (wear functions), values after each
        sc = Script()
        sc.at('dump ' + D('w0.bin'))
        for k in (1, 2):
            for i in range(1, 201):
                sc.at('call 1C18:1794,%d' % i)
            for i in range(1, 51):
                for e in range(1, 11):
                    sc.at('call 1C18:172F,%d,%d' % (i, e))
            sc.at('dump ' + D('w%d.bin' % k))
            for i in range(1, 201):
                calls3(sc, '1DA8:3F5B,%d' % i)
            for i in range(1, 51):
                calls3(sc, '1DA8:4035,%d' % i)
        sc.at('quit')
        out, err, seg = run_game(binary, gdir, sc.lines, tmp, 'run_a2')
        got = results(out)
        if not os.path.exists(D('w2.bin')):
            raise RuntimeError('game run A2 produced no results: ' + err[-400:])
        mw = [Mem(D('w%d.bin' % k), seg) for k in range(3)]
        for k in (1, 2):
            pt, pv, st, sv = [], [], [], []
            for i in range(1, 201):
                a, b = mw[k - 1].film(i), mw[k].film(i)
                pt.append(('movie %d' % i, film_wear(a[1], a[2], a[3]), b[3]))
                pv.append(('movie %d' % i, movie_price(struct.unpack_from('<i', b, 8)[0], b[1], b[3]),
                           value(got, '1DA8:3F5B,%d' % i, k - 1)))
            for i in range(1, 51):
                a, b = mw[k - 1].series(i), mw[k].series(i)
                for e in range(1, 11):
                    st.append(('series %d ep %d' % (i, e), episode_wear(a[3 * e], a[0x33], a[3 * e + 1]), b[3 * e + 1]))
                sv.append(('series %d' % i, series_price(list(b[0:0x21]), struct.unpack_from('<i', b, 0x2B)[0]),
                           value(got, '1DA8:4035,%d' % i, k - 1)))
            n = 'one airing' if k == 1 else 'a second airing'
            result['checks'].append(check('Movie topicality after %s' % n, '1C18:1794',
                                          'call 1C18:1794,id for all 200, dump', pt))
            result['checks'].append(check('Movie value after %s' % n, '1DA8:3F5B', 'call 1DA8:3F5B,id after the wear', pv))
            result['checks'].append(check('Episode topicality after %s' % n, '1C18:172F',
                                          'call 1C18:172F,series,episode for 50 × 10, dump', st))
            result['checks'].append(check('Series value after %s of every episode' % n, '1DA8:4035',
                                          'call 1DA8:4035,id after the wear', sv))
            if k == 1:
                pairs = [('movie %d' % i, films[i]['seqV'][1], value(got, '1DA8:3F5B,%d' % i, 0)) for i in range(1, 201)]
                result['checks'].append(check('Detail "value after 1 airing" (movies)', '1C18:1794 + 1DA8:3F5B',
                                              'page value against the calls above', pairs))
                pairs = [('series %d' % i, series[i]['valueWorn'], value(got, '1DA8:4035,%d' % i, 0)) for i in range(1, 51)]
                result['checks'].append(check('Detail "value after every episode aired once" (series)',
                                              '1C18:172F + 1DA8:4035', 'page value against the calls above', pairs))

        # ---- run B: the first evening; per hour one dump before the :05 evaluation and one after it
        esc = [(f, 'key esc') for f in range(2000, 66000, 300)]
        # the human subscribes to all three agencies and gets news worth 90, 60 and 45 in the broadcast slots
        news_poke = [(2001, 'poke16 39D4:%X %d' % (0x8BDC + 0x17 + 0x12, 0x0101))]
        for k, v in ((1, 90), (2, 60), (3, 45)):
            news_poke.append((2001, 'poke16 39D4:%X %d' % (0x36DB + (0x38 + 4 + k) * 0x1E + 0x16, v)))
        L = list(NEW_GAME) + esc + news_poke
        for f in range(2001, 66000, 20):
            L.append((f, 'peek16 39D4:71EA'))
            L.append((f, 'peek16 39D4:71EC'))
        L.append((66000, 'dump ' + D('b1end.bin')))
        L.append((66000, 'quit'))
        out, err, seg = run_game(binary, gdir, L, tmp, 'run_b1')
        mb = Mem(D('b1end.bin'), seg)
        inbox = []                       # (station, offset, record) of every inbox item
        for pl in (1, 2, 3):
            for i in range(mb.g16(0x72A6 + 2 * pl)):
                o = 0x457F + pl * 0x258 + i * 0x1E
                inbox.append((pl, o, mb.m[mb.ds + o:mb.ds + o + 0x1E]))
        clock = {}
        for ln in out.splitlines():
            m = re.match(r'PEEK frame (\d+) 39D4:(71EA|71EC) (\d+)', ln)
            if m:
                clock.setdefault(int(m.group(1)), {})[m.group(2)] = int(m.group(3))
        pre, post = {}, {}
        for f in sorted(clock):
            slot, minute = clock[f].get('71EA'), clock[f].get('71EC')
            if slot is None or minute is None:
                continue
            h = slot + 7 if slot < 17 else slot - 17
            if not 1 <= h <= 7:
                continue
            if 1 <= minute <= 4:
                pre[h] = f                       # the last frame before minute 5
            elif 8 <= minute <= 50 and h not in post and h in pre:
                post[h] = f
        hours = sorted(h for h in post if h in pre)
        L = list(NEW_GAME) + esc + news_poke
        for h in hours:
            L.append((pre[h], 'dump ' + D('pre%d.bin' % h)))
            L.append((post[h], 'dump ' + D('post%d.bin' % h)))
        dseg = seg + 0x39D4
        L.append((66000, 'dump ' + D('inbox.bin')))
        for pl in (1, 2, 3):
            L.append((66000, 'call 048C:0007,%d' % pl))
            for q, o, it in inbox:
                if q == pl:
                    L.append((66000, 'call 26A7:292F,%d,%d,%d' % (o, dseg, struct.unpack_from('<H', it, 0)[0])))
        L.append((66000, 'dump ' + D('texts.bin')))
        L.append((66001, 'quit'))
        out, err, seg = run_game(binary, gdir, L, tmp, 'run_b2')
        texts = [v for fn, args, v in parse_calls(out) if fn == '26A7:292F']
        pairs_share, pairs_score, pairs_view, sample, pairs_trend = [], [], [], [], []
        for h in hours:
            m = Mem(D('pre%d.bin' % h), seg)
            mp = Mem(D('post%d.bin' % h), seg)
            gaud = [m.g8(0x7160 + k) for k in range(0x80)]
            trends = [(m.g16(0x94 + 4 * k), m.g16(0x96 + 4 * k)) for k in range(3)]
            progs, news, desc = {}, {}, []
            for p in (1, 2, 3):
                if m.g8(0x8BDC + p * 0x17 + 0x14):
                    continue
                sched = lambda k, p=p: m.g16(p * 0x5A + k * 2 + 0x4417)   # noqa: E731
                news[p] = i8(m.g8(0x3F4B + p))
                if sched(h) == 0:
                    progs[p] = (0, m.g8(0x71E6 + p))
                    desc.append('station %d: nothing' % p)
                    continue
                k = h
                while sched(k) == 9999:
                    k -= 1
                prog = sched(k)
                if prog < 1000:
                    fr = m.film(prog)
                    progs[p] = (i8(fr[3]) & 0xFFFF, fr[0])
                    desc.append('station %d: movie %d, topicality %d' % (p, prog, fr[3]))
                else:
                    i = prog - 1000
                    sr = m.series(i)
                    ep = struct.unpack_from('<H', sr, 0x31)[0] or 1
                    airings = sum(1 for kk in range(1, h + 1) if sched(kk) == prog)
                    progs[p] = (tdiv(i8(sr[ep * 3 + 1]), airings) & 0xFFFF, 10 + sr[0x21])
                    desc.append('station %d: series %d episode %d, topicality %d, airing %d that evening' %
                                (p, i, ep, sr[ep * 3 + 1], airings))
            score, share = hour_scores(progs, gaud, h, trends, news)
            pairs_trend.append((HOURS[h - 1], data['meta']['trendsDay1'], [list(t) for t in trends]))
            for p in progs:
                lab = '%s, station %d' % (HOURS[h - 1], p)
                reach = mp.g32(0x1D09 + p * 0x1CE + 4)
                pairs_score.append((lab, score[p], mp.g16(0x24A1 + 2 * p)))
                pairs_share.append((lab, share[p], mp.g16(0x24AD + p * 10)))
                pairs_view.append((lab, i32(tdiv(i32(reach * share[p]), 100) * VIEWERS_PER_UNIT),
                                   i32(mp.g32(0x24AD + p * 10 + 2))))
            sample.append(dict(hour=HOURS[h - 1], what=desc, share=[share.get(p, 0) for p in (1, 2, 3)],
                               trends=[t[0] for t in trends]))
        result['checks'].append(check('Audience pull per station, first evening (%d hours)' % len(hours), '24B8:0C4A',
                                      'dump before :05, recompute, compare DS:24A1 after :05', pairs_score))
        result['checks'].append(check('Audience share per station, first evening', '24B8:0C4A',
                                      'same dumps, DS:24AD', pairs_share))
        result['checks'].append(check('Viewers per station, first evening', '24B8:0C4A', 'same dumps, DS:24AF',
                                      pairs_view))
        result['checks'].append(check('Trend genres and factors of the first evening (from the start savegame)',
                                      '24B8:1B5C', 'same dumps, DS:0094', pairs_trend))
        # news show (phase 1) from the dumps before :05 (ratings then hold the news result)
        pairs_ns, pairs_nsh = [], []
        for h in hours:
            m = Mem(D('pre%d.bin' % h), seg)
            scores = {}
            for p in (1, 2, 3):
                if m.g8(0x8BDC + p * 0x17 + 0x14):
                    continue
                vals = [m.g16(0x36DB + (0x38 + 4 * p + k) * 0x1E + 0x16) for k in (1, 2, 3)]
                scores[p] = news_score(vals, m.g8(0x1D00 + h))
            shares = news_shares(scores)
            for p in scores:
                lab = '%s, station %d' % (HOURS[h - 1], p)
                pairs_ns.append((lab, scores[p], m.g8(0x3F4B + p)))
                pairs_nsh.append((lab, [shares[p], i32(tdiv(i32(m.g32(0x1D09 + p * 0x1CE + 4) * shares[p]), 100) * VIEWERS_PER_UNIT)],
                                  [m.g16(0x24AD + p * 10), i32(m.g32(0x24AD + p * 10 + 2))]))
        result['checks'].append(check('News score per station, first evening (human slots poked to 90/60/45)', '1C18:0BCB, 24B8:0C4A',
                                      'dump after :00, DS:3F4B', pairs_ns))
        result['checks'].append(check('News share and viewers per station', '24B8:0C4A, 1DA8:0146', 'same dumps, DS:24AD', pairs_nsh))
        # inbox at 24:20: value at arrival (port), aged once per hour since, price class
        node_story = {}
        for st in N['stories']:
            for c in st['chain']:
                node_story[c['n']] = st
        tlist = {t['id']: t for t in N['topics']}
        mi = Mem(D('inbox.bin'), seg)
        slot_now = mi.g16(0x71EA)
        pairs_v, pairs_t = [], []
        tm = Mem(D('texts.bin'), seg)
        for k, (pl, o, it) in enumerate(inbox):
            node = struct.unpack_from('<H', it, 0)[0]
            parts = [struct.unpack_from('<HH', it, 2 + 4 * q) for q in range(5)]
            val, hour, day, cat, cls = struct.unpack_from('<HHHBB', it, 0x16)
            st = node_story.get(node)
            if st:
                counted = [tlist[st['parts'][q] % 1000]['pv'][parts[q][1] - 1] for q in range(5)
                           if st['parts'][q] and st['parts'][q] < 1000 and st['parts'][q] % 1000 in tlist and 1 <= parts[q][1] <= 10]
                v0 = news_value(N['nodes'][node]['v'], counted)
                codes = st['parts']
            else:
                v0 = i8(N['nodes'][node]['v']) & 0xFFFF if node in N['nodes'] else None
                codes = [parts[0][0], 0, 0, 0, 0]
            v = v0
            for _ in range((slot_now - hour) % 24 if v is not None else 0):
                v = news_age(v)
            lab = 'station %d inbox: node %d, arrived %02d:00' % (pl, node, hour)
            pairs_v.append((lab, [v, news_class(v) if v is not None else None], [val, cls]))
            # the headline the game builds
            want = news_headline(N['nodes'][node]['t']['en'], codes, [p[1] for p in parts], tlist, 'en') if node in N['nodes'] else None
            got_t = None
            if k < len(texts):
                a = ((texts[k] >> 16) & 0xFFFF) * 16 + (texts[k] & 0xFFFF)
                e = tm.m.find(b'\0', a)
                got_t = tm.m[a:e].decode(content.ENC) if e > a else ''
            pairs_t.append((lab, want, got_t))
        result['checks'].append(check('News value and price class in the three inboxes at %02d:00 (arrival value aged 85 %% per hour)' % slot_now,
                                      '1C18:0490, 1C18:1022', 'dump of the inboxes, DS:457F', pairs_v))
        result['checks'].append(check('Headline text built by the game (English)', '26A7:292F',
                                      'call 048C:0007,1 then 26A7:292F,item,node; read the strings', pairs_t))
        result['evening'] = sample
        result['total'] = sum(c['n'] for c in result['checks'])
        result['failed'] = sum(c['n'] - c['ok'] for c in result['checks'])
        result['ok'] = result['failed'] == 0
        return result
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ---------------------------------------------------------------- page
def render(data, out_path):
    js = json.dumps(data, ensure_ascii=False, separators=(',', ':')).replace('<', '\\u003c')
    page = PAGE.replace('/*__DATA__*/', js)
    page = page.replace('__NOTES__', notes_html(data))
    page = page.replace('__SUMMARY__', summary_html(data))
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(page)
    return len(page.encode('utf-8'))


def esc(s):
    return html.escape(str(s), quote=True)


def money(n):
    return ('−' if n < 0 else '') + '$' + '{:,}'.format(abs(n))


def summary_html(data):
    v = data['meta'].get('verify')
    m = data['meta']
    files = ' + '.join(esc(f['name']) for f in m['files'])
    if v and v.get('checks'):
        ver = ('Checked against the original program: %s comparisons, %s.' %
               ('{:,}'.format(v['total']), 'all equal' if not v['failed'] else '%d differ' % v['failed']))
    elif v and v.get('error'):
        ver = 'The check against the original program failed: %s' % esc(v['error'])
    else:
        ver = 'Not checked against the original program in this build of the page.'
    return ('Read from %s on %s: %d movies, %d series, %d ad contracts, %d news stories with %d headlines, %d screenplays. '
            'Prices and audience figures are those of a new game. %s <a href="#notes">How these values are computed</a>.' %
            (files, esc(m['generated']), len(data['films']), len(data['series']), len(data['ads']), len(data['news']['stories']),
             sum(len(st['chain']) for st in data['news']['stories']), len(data['scripts']), ver))


def notes_html(data):
    m = data['meta']
    v = m.get('verify')
    hr = m['humanReach']
    H = []
    a = H.append
    a('<section class="prose" aria-labelledby="n-formulas"><h2 id="n-formulas">How the game computes these values</h2>')
    a('<p>Each formula is a port of the decompiled function named in brackets (address in the original '
      'program, readable C in <code>src/</code>). Integer arithmetic follows the original: divisions truncate, '
      'money is rounded with <code>round_to</code> (1DA8:2653) to the nearest $1,000.</p>')
    a('<dl class="formulas">')
    rows = [
        ('Movie value', '1DA8:3F5B', 'price / 2 + price × topicality / (2 × quality), rounded to $1,000. '
         'A movie with quality 0 or price 0 is worth its price. The film dealer asks this, an auction starts at it, '
         'and the dealer pays it when you sell. With topicality = quality (every movie at the start) it is the base '
         'price rounded to $1,000.'),
        ('Series value', '1DA8:4035', 'n × episode price, with n = episodes produced, counted from episode 1 until '
         'the first missing one. Half of it is fixed, the other half scales with Σ topicality / Σ quality of '
         'those episodes; rounded to $1,000. A series without produced episodes is worth $0.'),
        ('Wear', '1C18:1794, 1C18:172F', 'After each airing a movie loses quality × wear / 5 topicality, an episode '
         'quality × wear / 20, never below 1. Every second day topicality climbs back to the next step of quality: '
         '20 % steps for movies, 5 % steps for episodes (1DA8:0217).'),
        ('Audience share of an hour', '24B8:0C4A', 'Pull = topicality (series: episode topicality / number of airings '
         'of the series so far that evening). A trend genre multiplies it by its trend slot’s factor. Stations showing the same '
         'genre split the genre’s hour audience A (table below): if A is below their summed pull, pull × A / sum; '
         'then pull × A / 100, plus a fifth of the news score. Total share = sum of all pulls, at most 95 %; each '
         'station gets its pull × total / sum. Viewers = reach × share / 100 × 2,464.'),
        ('Audience strip (18–24)', 'port of 24B8:0C4A', 'The share a programme gets when it is the only one on air: '
         'min(pull, A) × A / 100 with the pull at game start (series: episode 1), no news lead-in and no trend. '
         'The detail view converts it into viewers for the human player’s starting reach '
         '(' + '{:,}'.format(hr * m['viewersPerUnit']) + ' viewers).'),
        ('Trends', '24B8:1B5C', 'Three genres are trends at a time. Every day a new genre (never sundry movies or sundry '
         'series, never one that is already a trend) moves in and the others move one slot on; the slots multiply the pull by %s, '
         'so a genre stays a trend for three days. Factors and genres come from the start savegame; on the first evening '
         'the trends are %s.' % (', '.join('%.1f' % (t[1] / 10.0) for t in m['trendsSave'][:3]),
                                 ', '.join('%s ×%.1f' % (m['genres']['en'][t[0]], t[1] / 10.0) for t in m['trendsDay1']))),
        ('Ad offers', '24B8:000C', 'The agency only offers ads whose audience class lies below 40 %% of the largest '
         'station’s reach. At game start that is class %d or lower (%s viewers).' %
         (m['maxClass'], '{:,}'.format(m['minViewers'][m['maxClass'] - 1]) if m['maxClass'] else '0')),
        ('Signing an ad', '05FF:33F6, 1DA8:2682', 'Your image must reach the minimum audience as a percentage of your reach, '
         '(minimum + T/2) / T with T = reach × 2,464 / 100. Ads with a programme condition also need image 20. '
         'The "% of reach" column uses the human player’s starting reach; the image at the start is 34.'),
        ('A spot that counts', '24B8:0C4A, 1DA8:4258', 'A spot counts if the hour’s viewers, rounded to the nearest '
         '100,000, reach the minimum (class 1: any programme on air) and the programme’s genre passes the condition. '
         'The last spot pays the whole bonus. At the end of the deadline day an unfinished contract costs the penalty.'),
        ('Computer players and ads', '2DD9:33CB', 'The computer players never sign an ad that requires a genre; exclusions are fine.'),
        ('Starting programmes', '1DA8:1538', 'At a new game each station gets one of the series 18, 19 and 20 and three '
         'random cultural programmes from the starting-gift pool (genre 6, on the market, unowned), all free. '
         'The start savegame itself gives nobody anything.'),
        ('Live broadcasts', '24B8:04B9, 1DA8:0CB3', 'A live programme gets its date when it is offered: dealer 2–4 days '
         'ahead, auction 4–6 days, hour slot at random so that it ends by 1:00. Football (29) recurs 3 times, the '
         'Olympics (187) 5 times; other live programmes leave the library after airing and are lost if missed.'),
        ('News value and price', '1C18:0490, 1DA8:3F48', 'Base value of the headline node \u00d7 (30 + 70 \u00d7 average '
         'phrase value / 10) / 100, the average taken over the story parts below 1000. Price class = how many of '
         '{15, 30, 40, 50, 60, 100} the value exceeds; price = $1,000 / 2,000 / 3,000 / 4,000 / 6,000 / 8,000.'),
        ('News aging', '1C18:1022', 'Every hour the value in the inboxes and in the broadcast slots drops to 85 % '
         '(truncated); at the start of a day it becomes 0. The inbox recomputes the price class.'),
        ('News show', '1C18:0BCB, 24B8:0C4A, 1DA8:0146', 'Score = average of the three slot values \u00d7 the news audience '
         'of the hour / 100. Total news share = sum of the scores, at most 95 %; each station gets score \u00d7 total / sum. '
         'The programme of that hour gets score / 5 added to its pull (if it has any).'),
        ('Stories', '1C18:03C1, 1C18:0755, 1C18:08A2', 'Seven story slots (3 political, 2 show business, 2 sundry). A free '
         'slot takes a random unused story of its category; its countdown starts at 3 hours. When a countdown runs out the '
         'story emits its current headline and moves to a random successor, adding that headline\u2019s waiting time. '
         'Hourly the countdowns drop by 1, at the start of a day by 5 and at least 3 political items are forced.'),
        ('Production quality', '229E:0B98, 1DA8:2560', 'Role score = actor match \u00d7 weight / 100 with match = '
         '\u03a3 min(need, actor value) / \u03a3 need over humor, appearance and skill; set score = min(weight, set '
         'quality). Quality = base \u00d7 \u03a3 score / \u03a3 weight \u00d7 125 / 100, written to the movie '
         '(quality = topicality; price \u00d7 quality / 100, or 0 for one-airing shows) or to the episode.'),
    ]
    for t, fn, d in rows:
        a('<div><dt>%s <span class="fn">%s</span></dt><dd>%s</dd></div>' % (esc(t), esc(fn), esc(d)))
    a('</dl>')
    # genre x hour table
    a('<h3 id="n-hours">Hour audience by genre (A, % of viewers, from resource 0xD0)</h3>')
    a('<div class="scroller"><table class="mini"><thead><tr><th scope="col">Genre</th>')
    for hname in m['hours']:
        a('<th scope="col" class="num">%s</th>' % esc(hname[:2]))
    a('</tr></thead><tbody>')
    for g in range(1, 16):
        a('<tr><th scope="row">%s</th>' % esc(m['genres']['en'][g]))
        for h in range(1, 8):
            a('<td class="num">%d</td>' % m['genreAud'][g][h])
        a('</tr>')
    a('<tr><th scope="row">News (all genres)</th>')
    for h in range(1, 8):
        a('<td class="num">%d</td>' % m['hourAud'][h])
    a('</tr></tbody></table></div>')
    a('</section>')

    # verification
    a('<section class="prose" aria-labelledby="n-verify"><h2 id="n-verify">Checked against the original program</h2>')
    if v and v.get('checks'):
        a('<p>The machine-level build (<code>%s</code>, bit-exact with MTV.EXE) was started headless on copies of '
          'the same .rsc files and a new game was begun (<code>400 key esc</code>, <code>1000 click 64 100</code>, '
          '<code>1300 click 172 185</code>). From frame 2000 the game functions were called through the test harness '
          'and memory was dumped; the ports above were run on the same inputs. Where a check needed a situation the first '
          'evening does not produce, it was set up with <code>poke16</code>: a studio with cast and set choices for '
          'the production check, and the human player\u2019s news agencies and broadcast slots for the news checks. '
          '%s comparisons, %s.</p>' %
          (esc(v['binary']), '{:,}'.format(v['total']),
           'all equal' if not v['failed'] else '<strong>%d differ</strong>' % v['failed']))
        a('<div class="scroller"><table class="mini checks"><thead><tr><th scope="col">What</th><th scope="col">Function</th>'
          '<th scope="col">How</th><th scope="col" class="num">Equal</th></tr></thead><tbody>')
        for c in v['checks']:
            st = 'ok' if c['ok'] == c['n'] else 'bad'
            a('<tr><td>%s</td><td><code>%s</code></td><td><code>%s</code></td><td class="num %s">%d / %d</td></tr>' %
              (esc(c['name']), esc(c['fn']), esc(c['how']), st, c['ok'], c['n']))
            for b in c['bad']:
                a('<tr class="badrow"><td colspan="4">%s: port %s, game %s</td></tr>' %
                  (esc(b['item']), esc(b['port']), esc(b['game'])))
        a('</tbody></table></div>')
        if v.get('evening'):
            a('<h3>The first evening used for the audience check</h3><ul class="evening">')
            for e in v['evening']:
                a('<li><strong>%s</strong> %s → shares %s</li>' %
                  (esc(e['hour']), esc('; '.join(e['what'])), esc(' / '.join('%d %%' % s for s in e['share']))))
            a('</ul>')
        o = v.get('observed') or {}
        if o:
            lots = set(x[0] for x in o.get('auction', []))
            a('<h3>What the random new-game setup did in this run</h3><p>Owned at frame 2000: movies %s; series %s. '
              'These owners are drawn at random each new game, so the catalogue shows the pools instead.</p>' %
              (esc(', '.join('%d (station %d)' % (i, p) for i, p in o.get('films_owned', []) if i not in lots) or 'none'),
               esc(', '.join('%d (station %d)' % (i, p) for i, p in o.get('series_owned', []) if 1000 + i not in lots)
                   or 'none')))
    elif v and v.get('error'):
        a('<p>The check could not run: %s</p>' % esc(v['error']))
    else:
        a('<p>This page was generated without <code>--verify</code>; the ports were not compared with the game for '
          'this data.</p>')
    a('</section>')

    # start save
    sd = m['startDiff']
    a('<section class="prose" aria-labelledby="n-start"><h2 id="n-start">Table values and the start of a new game</h2>')
    a('<p>The game does not start from the tables alone: <code>main</code> loads the savegame resource 0x2B, which '
      'overwrites quality, topicality, owner, market flags, price and live date of every movie, the episodes, owner, '
      'flags, list price and next episode of every series, and the holder of every ad. Where it differs from the '
      'table, the detail view shows both values. ')
    if not (sd['film_diff'] or sd['series_diff'] or sd['ad_diff']):
        a('In this data the start savegame agrees with the tables for all 200 movies, 50 series and 50 ads.')
    else:
        a('It differs for movies %s, series %s and ads %s.' % (
            esc(', '.join(map(str, sd['film_diff'])) or 'none'), esc(', '.join(map(str, sd['series_diff'])) or 'none'),
            esc(', '.join(map(str, sd['ad_diff'])) or 'none')))
    a(' Starting reach: %s units per station (%s viewers for the human player).</p>' %
      (esc(' / '.join(str(r) for r in m['reach'])), '{:,}'.format(hr * m['viewersPerUnit'])))
    a('</section>')

    # unknowns and oddities
    a('<section class="prose" aria-labelledby="n-unknown"><h2 id="n-unknown">Fields with unknown meaning</h2><ul>')
    for t in [
        'Movie +0x06 bit 7 and +0x07 bit 0, series +0x25 bit 7 and +0x26 bit 0: written to and read from savegames, '
        'never tested by the game. Zero in all records.',
        'Movie +0x0E: saved and loaded, no other reader. Zero in all records.',
        'Series +0x27 list price: saved and loaded, never read; the value comes from the episode price.',
        'Series +0x34 and +0x37..+0x38, and the 3 bytes of "episode 0": zero, no reader found.',
        'Series episode flag bits 1–7: zero, only bit 0 (episode exists) is tested.',
        'Ad +0x0B: zero, never read; the programme condition is +0x0C..+0x0E.',
        'Ad +0x00..+0x07: far pointers left over from the authoring tool, replaced at load time.',
        'News story +0x0C: equals the number of parts in every record, never read.',
        'Phrase list +0x37: a count (10 for list 1, which has 10 phrases, is stored as 11), never read; +0x00..+0x2B are '
        'text pointers set at load, +0x2C the unused value of variant 0.',
        'Screenplay episode block bytes +1, +3 and +34: never read by the production code found so far (+2 is the '
        'runtime cost multiplier).',
        'Second byte of every role need (+0x168 + 4r + 1) and actor byte +7: stored, but the actor match skips them '
        'and the shop shows only humor, appearance and skill.',
    ]:
        a('<li>%s</li>' % esc(t))
    a('</ul></section>')
    a('<section class="prose" aria-labelledby="n-odd"><h2 id="n-odd">Oddities in the data and code</h2><ul>')
    for t in odd_notes(data):
        a('<li>%s</li>' % t)
    a('</ul></section>')
    return ''.join(H)


def odd_notes(data):
    films = {f['id']: f for f in data['films']}
    out = []
    zero = [f for f in data['films'] if f['blocks'] == 0]
    for f in zero:
        out.append(esc('Movie %d "%s" (live) has length 0 blocks. When it is offered, its hour slot is drawn from 1–8 '
                       '(random_n(8 − length)), and slot 8 is not an hour of the evening (slots 1–7); the end-of-programme test '
                       '(length = blocks aired) can never match, so it is not removed after airing like other live '
                       'events.' % (f['id'], f['t']['en'][0])))
    for f in data['films']:
        if f['corrupt']:
            out.append(esc('Movie %d: the German text entry is not a clean title/description pair (it starts '
                           '"%s" and continues with bytes that look like an index table); the English title is "%s". '
                           'The movie is not on the market (flag bit 5 clear), so players never see it.' %
                           (f['id'], f['t']['de'][0][:20], f['t']['en'][0])))
    seen = ''
    obs = ((data['meta'].get('verify') or {}).get('observed') or {})
    lots = set(p for p, _, _ in obs.get('auction', []))
    for sid, owner in obs.get('series_owned', []):
        if 1000 + sid in lots:
            seen = (' In the verification run series %d (%s) sat in the auction at game start with owner %d.' %
                    (sid, data['series'][sid - 1]['t']['en'][0], owner))
    out.append(esc('Auction lots: movies get the seller code random(7) + 3 = 4..10, series random(7) = 1..7 '
                   '(auction_add_lot 1DA8:0CB3), and the code is written into the owner field. A series in the '
                   'auction can therefore carry the number of a station as its owner.' + seen))
    out.append(esc('Same-genre split (24B8:0C4A): the stations are processed in order 1, 2, 3 and each sums the pulls '
                   'of all stations with its genre, including pulls already reduced earlier in the same loop. When two '
                   'stations show the same genre, the later one is split against a smaller sum.'))
    out.append(esc('Signing an ad needs image 20 when the first condition byte (+0x0C) is non-zero; the description '
                   'of 05FF:33F6 in names/symbols.txt calls these "X-rated ads".'))
    out.append(esc('Movie %d "%s": each airing adds 5 Betty love (up to the image) and the tape then leaves the '
                   'library (24B8:0C4A withdraws it after airing). The daily check 1DA8:0B53 sends the love letter '
                   'only to an owner who still holds it with the film in today’s schedule, so the letter looks '
                   'unreachable (?).' %
                   (58, films[58]['t']['en'][0])))
    N = data['news']
    out.append(esc('News room bug (26A7:292F): the news text cache has 24 entries (20 inbox items, 3 broadcast slots, the '
                   'dragged item), but the lookup searches only the first 20. With a full inbox, broadcast slots 2 and 3 '
                   'show the headline of slot 1 unless their item is also in the inbox. Reproduced with the test harness '
                   'on day 3 with 20 inbox items: slots holding nodes 173 and 174 returned the text of node 169 in slot 1.'))
    out.append(esc('news_age (1C18:1022) ages count + 1 inbox entries, one past the last item (harmless: the next item '
                   'is copied over it).'))
    out.append(esc('Price class 6 would read the word after the price table and cost $0; no headline reaches a value '
                   'above 100, so it never happens with this data.'))
    if N['unreached']:
        out.append(esc('Headline nodes %s have text but no story reaches them; node 169 ("%s") is never used by the code '
                       'either.' % (', '.join(str(n) for n in N['unreached'] if n != 169),
                                    N['nodes'][169]['t']['en'] if 169 in N['nodes'] else '')))
    for st in N['stories']:
        for c in st['chain']:
            t = N['nodes'][c['n']]['t']
            a_, b_ = sorted(set(re.findall(r'#\d', t['en']))), sorted(set(re.findall(r'#\d', t['de'])))
            if a_ != b_:
                out.append(esc('Headline %d (story %d) uses %s in English but %s in German: "%s" / "%s".' %
                               (c['n'], st['id'], ', '.join(a_) or 'no phrase', ', '.join(b_) or 'no phrase', t['en'], t['de'])))
    never = [x['id'] for x in data['scripts'] if not x['offered']]
    out.append(esc('Screenplays %s are never offered by the agency (flag bit 1 clear); screenplays 26\u201333 also have '
                   'no texts. Their movies and series exist but cannot be produced.' % ', '.join(map(str, never))))
    out.append(esc('Actor names and catchphrases (resource 0x2A) exist in one language only; the German version shows '
                   'the same names.'))
    nofee = [a['id'] for a in data['ads'] if a['penalty'] == 0]
    if nofee:
        out.append(esc('Ads without penalty (%s): missing the deadline only cancels the contract (message 0x110).' %
                       ', '.join(map(str, nofee))))
    unused_price = [f['id'] for f in data['films'] if f['flags2'] & 4 and f['price'] and f['prod'] and f['prod']['multi']]
    if unused_price:
        out.append(esc('Shows %s have a base price, but they are one-airing studio shows: production sets their '
                       'price to 0 and they are never on the market, so the stored price is never used.' %
                       ', '.join(map(str, unused_price))))
    return out


PAGE = r'''<title>Mad TV Programme Catalogue</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&family=Libre+Franklin:wght@400;500;600;700;800;900&display=swap');
/* Layout: a printed TV-guide listing. Masthead, channel-style tabs with counts, a filter strip,
   then ruled listings; every programme carries an 18-24 h audience strip and unfolds into its full record. */
:root {
  --paper: #E8ECF0;        /* ground: cool newsprint */
  --sheet: #FBFCFD;        /* listing surface */
  --ink: #141B26;          /* text, rules of the masthead */
  --muted: #586474;        /* secondary text */
  --rule: #C6CED8;         /* hairlines */
  --band: #DFE5EC;         /* header bands, strip wells */
  --signal: #1B5CAE;       /* programme blue: tabs, bars, focus */
  --signal-ink: #FFFFFF;   /* text on programme blue */
  --x: #B3261E;            /* X-rated, refused */
  --live: #985700;         /* live broadcasts */
  --go: #2A7A3B;           /* accepted, offered */
  --news1: #A83B3B;        /* news: political (game colour 22) */
  --news2: #7A3C7A;        /* news: show business (colour 21) */
  --news3: #A4561C;        /* news: sundry (colour 24) */
  --f-display: "Libre Franklin", "Franklin Gothic Medium", "Franklin Gothic", "Arial Narrow", Arial, sans-serif;
  --f-body: "Libre Franklin", "Helvetica Neue", Helvetica, Arial, sans-serif;
  --f-data: "IBM Plex Mono", ui-monospace, "SFMono-Regular", Menlo, Consolas, monospace;
  --gutter: clamp(16px, 3vw, 32px);
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --paper: #0D1219; --sheet: #141B24; --ink: #E3E8EF; --muted: #97A3B3; --rule: #2A3543; --band: #1C2531;
    --signal: #6EA4EB; --signal-ink: #0A111C; --x: #F1786D; --live: #E2A64C; --go: #6BC685;
    --news1: #EE8A86; --news2: #D196D1; --news3: #EFA36B;
    color-scheme: dark;
  }
}
:root[data-theme="dark"] {
  --paper: #0D1219; --sheet: #141B24; --ink: #E3E8EF; --muted: #97A3B3; --rule: #2A3543; --band: #1C2531;
  --signal: #6EA4EB; --signal-ink: #0A111C; --x: #F1786D; --live: #E2A64C; --go: #6BC685;
  --news1: #EE8A86; --news2: #D196D1; --news3: #EFA36B;
  color-scheme: dark;
}
*, *::before, *::after { box-sizing: border-box; }
body { margin: 0; background: var(--paper); color: var(--ink); font: 400 14px/1.45 var(--f-body); -webkit-text-size-adjust: 100%; }
a { color: var(--signal); }
:focus-visible { outline: 2px solid var(--signal); outline-offset: 2px; }
code { font-family: var(--f-data); font-size: .92em; }
.page { max-width: 1340px; margin-inline: auto; padding-inline: var(--gutter); padding-block: 18px 64px; }
.sr-only { position: absolute; width: 1px; height: 1px; overflow: hidden; clip: rect(0 0 0 0); white-space: nowrap; }

/* masthead */
.mast { display: grid; gap: 10px; padding-block: 6px 16px; border-bottom: 4px solid var(--ink); }
.mast-top { display: flex; justify-content: space-between; align-items: center; gap: 12px; flex-wrap: wrap; }
.eyebrow { font: 600 11.5px/1.2 var(--f-data); letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
h1 { margin: 0; display: flex; flex-wrap: wrap; align-items: baseline; gap: 4px 16px; }
.wordmark { font: 900 clamp(34px, 6vw, 60px)/.9 var(--f-display); letter-spacing: -.035em; text-transform: uppercase; }
.h1-sub { font: 700 clamp(17px, 2.2vw, 22px)/1.15 var(--f-display); letter-spacing: -.005em; text-wrap: balance; }
.summary { margin: 0; max-width: 92ch; color: var(--muted); }
.lang { display: inline-flex; border: 1px solid var(--ink); border-radius: 2px; overflow: hidden; }
.lang button { font: 700 12px/1 var(--f-body); letter-spacing: .04em; padding: 7px 11px; background: var(--sheet); color: var(--ink); border: 0; cursor: pointer; }
.lang button + button { border-left: 1px solid var(--ink); }
.lang button[aria-pressed="true"] { background: var(--ink); color: var(--sheet); }

/* channel tabs */
.tabs { position: sticky; top: env(safe-area-inset-top, 0px); z-index: 5; display: flex; gap: 2px; background: var(--paper); padding-block: 12px 0; border-bottom: 2px solid var(--signal); overflow-x: auto; scrollbar-width: none; }
.tab { display: inline-flex; align-items: baseline; gap: 8px; padding: 10px 14px 8px; font: 800 15px/1 var(--f-display); text-transform: uppercase; letter-spacing: .02em; color: var(--muted); text-decoration: none; border-radius: 2px 2px 0 0; white-space: nowrap; }
.tab:hover { color: var(--ink); background: var(--band); }
.tab[aria-current="page"] { background: var(--signal); color: var(--signal-ink); }
.tab .count { font: 500 12px/1 var(--f-data); }

/* filters */
.toolbar { display: flex; flex-wrap: wrap; gap: 10px 14px; align-items: end; padding-block: 16px 10px; }
.fld { display: grid; gap: 5px; font: 700 10.5px/1 var(--f-body); text-transform: uppercase; letter-spacing: .07em; color: var(--muted); min-width: 0; }
.fld input, .fld select { font: 400 14px/1.2 var(--f-body); color: var(--ink); background: var(--sheet); border: 1px solid var(--rule); border-radius: 2px; padding: 7px 8px; min-width: 0; max-width: 100%; text-transform: none; letter-spacing: 0; }
.fld-search { flex: 1 1 240px; }
.fld-search input { width: 100%; }
.reset { font: 600 12.5px/1 var(--f-body); padding: 8px 10px; background: transparent; color: var(--signal); border: 1px solid var(--rule); border-radius: 2px; cursor: pointer; }
.reset:hover { background: var(--band); }
.count-line { font: 500 12.5px/1.3 var(--f-data); color: var(--muted); margin: 0 0 8px; }

/* listings */
.scroller { overflow-x: auto; background: var(--sheet); border: 1px solid var(--rule); border-radius: 2px; }
table.listing { border-collapse: collapse; width: 100%; font-size: 13.5px; }
.listing thead th { font: 700 10.5px/1.2 var(--f-body); text-transform: uppercase; letter-spacing: .07em; color: var(--muted); text-align: left; background: var(--band); border-bottom: 1px solid var(--rule); padding: 0; white-space: nowrap; vertical-align: bottom; }
.listing thead th button { all: unset; display: block; padding: 9px 10px 8px; cursor: pointer; }
.listing thead th button:focus-visible { outline: 2px solid var(--signal); outline-offset: -2px; }
.listing thead th.num button { text-align: right; }
.listing thead th[aria-sort="ascending"] button::after { content: " \25B2"; color: var(--signal); }
.listing thead th[aria-sort="descending"] button::after { content: " \25BC"; color: var(--signal); }
.listing td { padding: 8px 10px; border-bottom: 1px solid var(--rule); vertical-align: top; }
.listing td.num { text-align: right; font: 400 12.5px/1.45 var(--f-data); font-variant-numeric: tabular-nums; white-space: nowrap; }
.listing td.id { font: 500 12px/1.45 var(--f-data); color: var(--muted); white-space: nowrap; }
.listing tr.row { cursor: pointer; }
.listing tr.row:hover > td { background: var(--band); }
.listing tr.row.open > td { background: var(--band); border-bottom-color: transparent; }
.listing td.title { min-width: 15em; max-width: 26em; }
.listing td.wrap { min-width: 11em; max-width: 20em; }
.open-btn { all: unset; cursor: pointer; font: 700 14px/1.25 var(--f-body); color: var(--ink); display: inline; }
.open-btn::before { content: "\25B8"; display: inline-block; width: 1.1em; color: var(--muted); transition: transform .12s ease; }
.open-btn[aria-expanded="true"]::before { transform: rotate(90deg); color: var(--signal); }
.open-btn:focus-visible { outline: 2px solid var(--signal); outline-offset: 2px; }
.sub { display: block; color: var(--muted); font-size: 12px; margin-top: 2px; padding-left: 1.1em; }
.muted { color: var(--muted); }
.dim { color: var(--muted); }

/* hour strip: 18 19 20 21 22 23 24 */
.strip { display: inline-grid; grid-template-columns: repeat(7, 9px); gap: 2px; align-items: end; height: 34px; padding: 3px; background: var(--band); border-radius: 2px; vertical-align: middle; }
.strip i { display: block; width: 9px; height: calc(var(--v) * .3px); min-height: 1px; background: var(--signal); }
.strip i.risk { background: repeating-linear-gradient(135deg, var(--x) 0 2px, var(--signal) 2px 4px); }
.strip i.zero { background: var(--rule); }
.strip-head { display: inline-grid; grid-template-columns: repeat(7, 9px); gap: 2px; padding-inline: 3px; font: 500 9px/1 var(--f-data); color: var(--muted); letter-spacing: 0; text-transform: none; }
.strip-head span { text-align: center; }

/* chips */
.chips { display: flex; flex-wrap: wrap; gap: 4px; }
.chip { display: inline-block; font: 700 10px/1 var(--f-body); text-transform: uppercase; letter-spacing: .06em; padding: 3px 5px 2px; border: 1px solid currentColor; border-radius: 2px; color: var(--muted); white-space: nowrap; }
.chip-x { color: var(--x); }
.chip-live { color: var(--live); }
.chip-go { color: var(--go); }
.chip-sig { color: var(--signal); }
.yes { color: var(--go); font-weight: 600; }
.no { color: var(--x); font-weight: 600; }

/* detail */
tr.drow > td { padding: 0; background: var(--sheet); border-bottom: 3px solid var(--ink); }
.detail { position: sticky; left: 0; max-width: calc(100vw - 2 * var(--gutter) - 2px); padding: 18px 16px 22px; display: grid; gap: 22px; }
.d-grid { display: grid; grid-template-columns: minmax(0, 1.05fr) minmax(0, 1fr); gap: 22px 32px; }
@media (max-width: 880px) { .d-grid { grid-template-columns: minmax(0, 1fr); } }
.d-kicker { margin: 0; font: 600 11px/1.3 var(--f-data); text-transform: uppercase; letter-spacing: .08em; color: var(--muted); }
.detail h3 { margin: 4px 0 2px; font: 800 23px/1.12 var(--f-display); letter-spacing: -.01em; text-wrap: balance; }
.d-alt { margin: 0; color: var(--muted); }
.d-desc { margin: 10px 0 0; max-width: 64ch; }
.d-desc-alt { margin: 4px 0 0; max-width: 64ch; color: var(--muted); font-size: 13px; }
.detail h4 { margin: 0 0 6px; padding-bottom: 5px; font: 800 11.5px/1.2 var(--f-body); text-transform: uppercase; letter-spacing: .08em; border-bottom: 2px solid var(--ink); }
.d-block { display: grid; gap: 6px; min-width: 0; align-content: start; }
.d-side { display: grid; gap: 18px; align-content: start; min-width: 0; }
.facts { margin: 0; padding: 0; list-style: none; display: grid; gap: 4px; }
.facts li { padding-left: 12px; position: relative; max-width: 70ch; }
.facts li::before { content: ""; position: absolute; left: 0; top: .62em; width: 5px; height: 1px; background: var(--muted); }
.tblwrap { overflow-x: auto; }
table.kv { border-collapse: collapse; width: 100%; font-size: 13px; }
table.kv th, table.kv td { padding: 4px 8px 4px 0; border-bottom: 1px solid var(--rule); text-align: left; vertical-align: top; }
table.kv thead th { font: 700 10px/1.2 var(--f-body); text-transform: uppercase; letter-spacing: .07em; color: var(--muted); }
table.kv .num { text-align: right; font-family: var(--f-data); font-size: 12.5px; font-variant-numeric: tabular-nums; white-space: nowrap; }
table.kv tbody th { font-weight: 600; }
table.kv thead th.num { font-family: var(--f-body); font-size: 10px; }
table.kv tr.na td, table.kv tr.na th { color: var(--muted); }

/* the game's info card */
.card { margin: 14px 0 0; justify-self: start; width: 100%; max-width: 310px; border: 2px solid var(--ink); background: var(--band); font: 500 12.5px/1.4 var(--f-data); }
.card > div { padding: 1px 10px; }
.card .cb { background: var(--ink); color: var(--sheet); padding: 3px 10px; font-weight: 600; }
.card .ct { font-weight: 600; padding-top: 5px; }
.card .cl { display: flex; justify-content: space-between; gap: 10px; }
.card .cx { color: var(--x); font-weight: 600; text-align: right; }
.card .cend { padding-bottom: 6px; }
.gbar { display: inline-block; width: 40px; height: 7px; margin-right: 6px; background: var(--sheet); border: 1px solid var(--rule); vertical-align: middle; }
.gbar i { display: block; height: 100%; width: var(--w); background: var(--signal); }
.card figcaption { padding: 4px 10px 5px; border-top: 1px solid var(--rule); font: 400 11px/1.3 var(--f-body); color: var(--muted); background: var(--sheet); }

/* raw record */
.fields { display: grid; border-top: 1px solid var(--rule); }
.f { display: grid; grid-template-columns: 4.6em minmax(7em, 11em) minmax(0, 15em) minmax(0, 1fr); gap: 3px 14px; padding: 6px 0; border-bottom: 1px solid var(--rule); font-size: 13px; align-items: baseline; }
.f .off { font: 500 12px/1.4 var(--f-data); color: var(--muted); }
.f .fname { font-weight: 600; }
.f .fval { font: 400 12.5px/1.4 var(--f-data); overflow-wrap: anywhere; }
.f .fdoc { color: var(--muted); }
.f .was { text-decoration: line-through; color: var(--muted); }
.f.unknown .fname::after { content: " ?"; color: var(--x); }
@media (max-width: 880px) {
  .f { grid-template-columns: 4.6em minmax(0, 1fr); }
  .f .fval, .f .fdoc { grid-column: 2; }
}
.rawhex { margin: 8px 0 0; font: 400 12px/1.5 var(--f-data); color: var(--muted); overflow-wrap: anywhere; }
.accept { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 2px 14px; margin: 0; padding: 0; list-style: none; font-size: 13px; }

/* news and production extras */
.extras { display: grid; gap: 26px; padding-top: 22px; }
.extras h3 { margin: 0 0 8px; padding-bottom: 6px; font: 800 17px/1.2 var(--f-display); text-transform: uppercase; letter-spacing: .01em; border-bottom: 3px solid var(--ink); text-wrap: balance; }
.extras p { margin: 0 0 8px; max-width: 84ch; }
.extras .scroller { margin-top: 6px; }
.cat1 { color: var(--news1); } .cat2 { color: var(--news2); } .cat3 { color: var(--news3); }
.ncard { margin: 0; max-width: 470px; border: 2px solid var(--ink); border-left: 8px solid currentColor; background: var(--band); font: 500 12.5px/1.4 var(--f-data); }
.ncard .nh { padding: 6px 10px 2px; color: var(--ink); font-weight: 600; }
.ncard .nl { padding: 0 10px 6px; color: var(--muted); white-space: pre-wrap; }
.ncards { display: grid; gap: 8px; }
.ph { display: inline-block; padding: 0 4px; border: 1px dashed var(--muted); border-radius: 2px; font: 500 11.5px/1.5 var(--f-data); color: var(--muted); }
.pv { font: 500 11px/1 var(--f-data); color: var(--muted); }
.phr { display: inline; }
.phr + .phr::before { content: " \00b7 "; color: var(--muted); }
.bars3 { display: inline-grid; grid-template-columns: repeat(3, auto); gap: 2px 8px; font: 400 12px/1.3 var(--f-data); }

/* notes */
.notes { padding-top: 6px; }
.prose h2, .prose h3 { scroll-margin-top: 64px; }
.prose h2 { margin: 30px 0 10px; font: 800 19px/1.2 var(--f-display); text-transform: uppercase; letter-spacing: .01em; padding-bottom: 6px; border-bottom: 3px solid var(--ink); text-wrap: balance; }
.prose h3 { margin: 22px 0 8px; font: 800 14px/1.3 var(--f-body); text-transform: uppercase; letter-spacing: .05em; }
.prose > p, .prose > ul, .prose > dl { max-width: 84ch; }
.prose li { margin-bottom: 6px; }
dl.formulas { margin: 0; }
dl.formulas > div { padding: 9px 0; border-bottom: 1px solid var(--rule); }
dl.formulas dt { font-weight: 700; }
dl.formulas dd { margin: 3px 0 0; }
.fn { font: 500 11.5px/1 var(--f-data); color: var(--muted); margin-left: 6px; }
table.mini { border-collapse: collapse; width: 100%; font-size: 13px; }
table.mini th, table.mini td { padding: 6px 10px; border-bottom: 1px solid var(--rule); text-align: left; vertical-align: top; }
table.mini thead th { font: 700 10.5px/1.2 var(--f-body); text-transform: uppercase; letter-spacing: .07em; color: var(--muted); background: var(--band); }
table.mini .num { text-align: right; font-family: var(--f-data); font-variant-numeric: tabular-nums; white-space: nowrap; }
table.mini td.ok { color: var(--go); font-weight: 600; }
table.mini td.bad, table.mini tr.badrow td { color: var(--x); }
.evening { padding-left: 18px; }

@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after { transition: none !important; animation: none !important; scroll-behavior: auto !important; }
}
</style>

<div class="page">
  <header class="mast">
    <div class="mast-top">
      <span class="eyebrow">Rainbow Arts, 1991 &middot; from the original game data</span>
      <div class="lang" role="group" aria-label="Language of the game texts">
        <button type="button" id="lang-en" aria-pressed="true">English</button>
        <button type="button" id="lang-de" aria-pressed="false">Deutsch</button>
      </div>
    </div>
    <h1><span class="wordmark">Mad TV</span> <span class="h1-sub">Movies, series, ads, news and screenplays</span></h1>
    <p class="summary">__SUMMARY__</p>
  </header>

  <nav class="tabs" aria-label="Sections">
    <a class="tab" id="tab-movies" href="#movies">Movies <span class="count" id="tc-movies"></span></a>
    <a class="tab" id="tab-series" href="#series">Series <span class="count" id="tc-series"></span></a>
    <a class="tab" id="tab-ads" href="#ads">Ads <span class="count" id="tc-ads"></span></a>
    <a class="tab" id="tab-news" href="#news">News <span class="count" id="tc-news"></span></a>
    <a class="tab" id="tab-scripts" href="#scripts">Screenplays <span class="count" id="tc-scripts"></span></a>
    <a class="tab" id="tab-notes" href="#notes">How it works</a>
  </nav>

  <section class="sec" id="sec-movies" aria-labelledby="h-movies">
    <h2 class="sr-only" id="h-movies">Movies</h2>
    <div class="toolbar" id="tb-movies"></div>
    <p class="count-line" id="cnt-movies" aria-live="polite"></p>
    <div class="scroller"><table class="listing" id="tbl-movies"><thead></thead><tbody></tbody></table></div>
  </section>

  <section class="sec" id="sec-series" aria-labelledby="h-series" hidden>
    <h2 class="sr-only" id="h-series">Series</h2>
    <div class="toolbar" id="tb-series"></div>
    <p class="count-line" id="cnt-series" aria-live="polite"></p>
    <div class="scroller"><table class="listing" id="tbl-series"><thead></thead><tbody></tbody></table></div>
  </section>

  <section class="sec" id="sec-ads" aria-labelledby="h-ads" hidden>
    <h2 class="sr-only" id="h-ads">Advertising contracts</h2>
    <div class="toolbar" id="tb-ads"></div>
    <p class="count-line" id="cnt-ads" aria-live="polite"></p>
    <div class="scroller"><table class="listing" id="tbl-ads"><thead></thead><tbody></tbody></table></div>
  </section>

  <section class="sec" id="sec-news" aria-labelledby="h-news" hidden>
    <h2 class="sr-only" id="h-news">News stories</h2>
    <div class="toolbar" id="tb-news"></div>
    <p class="count-line" id="cnt-news" aria-live="polite"></p>
    <div class="scroller"><table class="listing" id="tbl-news"><thead></thead><tbody></tbody></table></div>
    <div class="extras" id="ex-news"></div>
  </section>

  <section class="sec" id="sec-scripts" aria-labelledby="h-scripts" hidden>
    <h2 class="sr-only" id="h-scripts">Screenplays</h2>
    <div class="toolbar" id="tb-scripts"></div>
    <p class="count-line" id="cnt-scripts" aria-live="polite"></p>
    <div class="scroller"><table class="listing" id="tbl-scripts"><thead></thead><tbody></tbody></table></div>
    <div class="extras" id="ex-scripts"></div>
  </section>

  <section class="sec notes" id="sec-notes" aria-label="How the values are computed" hidden>
__NOTES__
  </section>
</div>

<script type="application/json" id="catalog-data">/*__DATA__*/</script>
<script>
(function () {
  'use strict';
  var D = JSON.parse(document.getElementById('catalog-data').textContent);
  var M = D.meta;
  var HOURS = ['18', '19', '20', '21', '22', '23', '24'];
  var lang = 'en';
  try { var saved = localStorage.getItem('madtv-catalog-lang'); if (saved === 'en' || saved === 'de') lang = saved; } catch (e) { /* storage unavailable */ }

  // ------------------------------------------------------------ helpers
  function esc(s) {
    return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; });
  }
  function money(n) {
    if (n == null) return '—';
    var s = '$' + Math.abs(n).toLocaleString('en-US');
    return n < 0 ? '−' + s : s;
  }
  function num(n) { return n == null ? '—' : Number(n).toLocaleString('en-US'); }
  function hx(n, w) { var s = Number(n).toString(16).toUpperCase(); while (s.length < (w || 2)) s = '0' + s; return '0x' + s; }
  function pad2(n) { return (n < 10 ? '0' : '') + n; }
  function otherLang() { return lang === 'en' ? 'de' : 'en'; }
  function langName(l) { return l === 'de' ? 'German' : 'English'; }
  function gname(g, l) { return (M.genres[l || lang] || [])[g] || ('genre ' + g); }
  function txt(r, k, l) {
    var t = r.t[l || lang];
    var v = t && t[k];
    return v || '';
  }
  function title(r) { return txt(r, 0) || txt(r, 0, otherLang()) || '(no title)'; }
  function millions(v) {
    if (v == null) return '—';
    if (v === 0) return 'any';
    var m = v / 1e6;
    return (m >= 1 ? String(Math.round(m * 10) / 10) : String(Math.round(m * 100) / 100)) + ' M';
  }
  // format_millions 1DA8:412F mode 3, as on the contract card
  function gameMillions(v) {
    function rt(x, m) { return Math.floor((x + Math.floor(m / 2)) / m) * m; }
    if (v >= 1000000) { var a = rt(v, 100000); return Math.floor(a / 1e6) + ',' + Math.floor((a % 1e6) / 100000); }
    var b = rt(v, 10000); return Math.floor(b / 1e6) + ',' + pad2(Math.floor((b % 1e6) / 10000));
  }
  function ui(n) { return (M.ui[lang] || M.ui.en)[String(n)] || ''; }
  // Turbo C-style format strings of the game texts: #s #d #02d #_8l #40f (bar); '~' separates lines
  function gfmt(s, args) {
    var i = 0;
    return esc(s).replace(/#([_*+]?)(\d*)([sdlf])/g, function (m, fl, w, t) {
      var a = args[i++];
      if (t === 'f') return '<b class="gbar" aria-hidden="true"><i style="--w:' + Math.max(0, Math.min(100, a / 40 * 100)) + '%"></i></b>';
      if (t === 's') return esc(a == null ? '' : a);
      if (t === 'l') return Number(a).toLocaleString('en-US');
      if (w && w.charAt(0) === '0') { var z = String(a); while (z.length < +w) z = '0' + z; return z; }
      return String(a);
    }).split('~');
  }
  function hourLabel(slot) { return (slot >= 1 && slot <= 7) ? (HOURS[slot - 1] + ':00') : ('slot ' + slot); }
  function viewersAtStart(share) { return Math.trunc(Math.trunc(M.humanReach * share) / 100) * M.viewersPerUnit; }
  function marketLabel(m) { return { dealer: 'Film dealer', auction: 'Auction', none: 'Not on the market', owned: 'Owned' }[m] || m; }

  function strip(solo, xrated, label) {
    var cells = solo.map(function (v, h) {
      var cls = v === 0 ? 'zero' : (xrated && h < 3 ? 'risk' : '');
      return '<i class="' + cls + '" style="--v:' + v + '"></i>';
    }).join('');
    var aria = label + ': ' + solo.map(function (v, h) { return HOURS[h] + ':00 ' + v + ' %'; }).join(', ');
    return '<span class="strip" role="img" aria-label="' + esc(aria) + '" title="' + esc(aria) + '">' + cells + '</span>';
  }
  function stripHead(label) {
    return '<span>' + esc(label) + '</span><br><span class="strip-head" aria-hidden="true">' +
      HOURS.map(function (h, k) { return '<span>' + (k === 0 || k === 6 ? h : (k === 3 ? '21' : '·')) + '</span>'; }).join('') + '</span>';
  }
  function trendOf(g) {
    var t = (M.trendsDay1 || []).filter(function (x) { return x[0] === g; })[0];
    return t ? (t[1] / 10).toFixed(1) : '';
  }
  function chip(t, cls) { return '<span class="chip ' + (cls || '') + '">' + esc(t) + '</span>'; }

  // ------------------------------------------------------------ record fields
  function bits(v, names) {
    var out = [];
    for (var b = 0; b < 8; b++) if (v & (1 << b)) out.push(names[b] || ('bit ' + b));
    return out.length ? out.join(', ') : 'none';
  }
  var FFLAGS = ['Dad watches', 'Mum watches', 'Grandpa watches', 'Kid watches (until 23:00)', 'Teen watches', 'on the market', 'auction', 'unknown bit 7'];
  var FFLAGS2 = ['unknown bit 0', 'in a schedule', 'one airing only', 'X-rated', 'bit 4', 'bit 5', 'bit 6', 'bit 7'];
  var SFLAGS2 = ['unknown bit 0', 'in a schedule', 'bit 2', 'bit 3', 'bit 4', 'bit 5', 'bit 6', 'bit 7'];
  function fmtField(kind, fmt, v, r) {
    switch (fmt) {
      case 'genre': return v + ' · ' + esc(gname(v));
      case 'sgenre': return v + ' · ' + esc(gname(10 + v));
      case 'blocks': return v + (v === 1 ? ' block (1 hour)' : ' blocks (' + v + ' hours)');
      case 'owner':
        if (v === 0) return '0 · market';
        if (v <= 3) return v + ' · station ' + v + (v === 1 ? ' (human by default)' : '');
        return v + ' · auction lot code';
      case 'fflags': return hx(v) + ' · ' + esc(bits(v, FFLAGS));
      case 'fflags2': return hx(v) + ' · ' + esc(bits(v, FFLAGS2));
      case 'sflags2': return hx(v) + ' · ' + esc(bits(v, SFLAGS2));
      case 'money': return money(v);
      case 'pic': return v === 0 ? '0 · genre picture (resource ' + hx(0xDC + r.genre) + ')' : 'resource ' + hx(v);
      case 'spic': return v === 0 ? '0 · genre picture (resource ' + hx(0xE6 + r.genre) + ')' : 'resource ' + hx(v) + (v === 0xE2 ? ' (the cultural-programme picture)' : '');
      case 'slot': return v === 0 ? '0 · not set' : v + ' · ' + hourLabel(v);
      case 'hex': return esc(String(v).replace(/(..)/g, '$1 ').trim());
      case 'eps': return 'see the episode table';
      case 'bar': return v + ' / 10';
      case 'cls': return v + ' · ' + (r.minv == null ? '?' : num(r.minv) + ' viewers');
      case 'rule': return '[' + v.join(', ') + '] · ' + esc(condText(r) || ui(79));
      case 'holder': return v === 0 ? '0 · free' : v <= 3 ? v + ' · station ' + v : v + ' · on offer';
      case 'c13': return r.genre === 8 ? 'Betty bar ' + v : v + ' · ' + esc((M.countries[lang] || [])[v] || 'country ' + v) + (r.genre === 7 ? ' (not shown for live)' : '');
      case 'c14': return r.genre === 8 ? 'critics bar ' + v : v + ' · 19' + pad2(v) + (r.genre === 7 ? ' (not shown)' : '');
      case 'c15': return (r.genre === 6 ? 'Betty bar ' : r.genre === 8 ? 'not shown ' : 'critics bar ') + v;
      case 'c16': return (r.genre === 6 ? 'critics bar ' : r.genre === 8 ? 'not shown ' : 'box-office bar ') + v;
      default: return esc(v);
    }
  }
  function fieldsHtml(kind, r) {
    var defs = M.fields[kind];
    var rows = defs.map(function (d) {
      var key = d[2], fmt = d[4];
      var v = r[key];
      var val = fmtField(kind, fmt, v, r);
      if (r.start && Object.prototype.hasOwnProperty.call(r.start, key) && fmt !== 'eps') {
        val = '<span class="was">' + val + '</span> → ' + fmtField(kind, fmt, r.start[key], r) + ' <span class="dim">(start savegame)</span>';
      }
      var unknown = /^Unknown/.test(d[5]);
      return '<div class="f' + (unknown ? ' unknown' : '') + '"><code class="off">+' + hx(d[0]).slice(2) + '</code><span class="fname">' + esc(d[3]) +
        '</span><span class="fval">' + val + '</span><span class="fdoc">' + esc(d[5]) + '</span></div>';
    }).join('');
    return '<div class="fields">' + rows + '</div><p class="rawhex">' + esc(r.raw.length / 2) + ' bytes: ' +
      esc(r.raw.replace(/(..)/g, '$1 ').trim()) + '</p>';
  }

  // ------------------------------------------------------------ the game's info cards
  function filmCard(f) {
    var g = f.genre, q = f.quality, top = f.topicality;
    var L = ['<div class="cb">' + esc(gname(g)) + '</div>', '<div class="ct">' + esc(title(f)) + '</div>'];
    var country = (M.countries[lang] || [])[f.c13] || '';
    var lines = [];
    if (g < 6 || g > 8) lines = gfmt(ui(412), [country, f.c14, f.c15 * 4, f.c16 * 4]);
    else if (g === 6) lines = gfmt(ui(413), [country, f.c14, f.c15 * 4, f.c16 * 4]);
    else if (g === 8) lines = gfmt(ui(414), [f.c13 * 4, f.c14 * 4]);
    lines.forEach(function (l) { L.push('<div>' + l + '</div>'); });
    if (g !== 7 && g !== 8) L.push('<div>' + gfmt(ui(417), [q ? Math.trunc(top * 40 / q) : 40])[0] + '</div>');
    L.push('<div class="cl' + (f.flags2 & 8 ? '' : ' cend') + '"><span>' + gfmt(ui(418), [f.blocks])[0] + '</span><span>$ ' + num(f.value) + '</span></div>');
    if (f.flags2 & 8) L.push('<div class="cx cend">' + esc(ui(419)) + '</div>');
    return '<figure class="card">' + L.join('') + '<figcaption>The game’s info card at the start of a new game (layout of tooltip_film 14E1:4172; the description is left out here).</figcaption></figure>';
  }
  function seriesCard(s) {
    var next = s.next_episode || 1;
    var L = ['<div class="cb">' + esc(gname(10 + s.genre)) + '</div>', '<div class="ct">' + esc(title(s)) + '</div>'];
    gfmt(ui(376), [s.critics * 4, s.press * 4]).forEach(function (l) { L.push('<div>' + l + '</div>'); });
    gfmt(ui(377), [s.episodes, s.blocks, next, txt(s, 1 + next) || txt(s, 1 + next, otherLang())]).forEach(function (l) { L.push('<div>' + l + '</div>'); });
    L.push('<div class="cl cend"><span></span><span>$ ' + num(s.value) + '</span></div>');
    return '<figure class="card">' + L.join('') + '<figcaption>The game’s info card at the start of a new game (tooltip_show 14E1:16BB, kind 1).</figcaption></figure>';
  }
  function condText(a) { return (a.cond && (a.cond[lang] || a.cond.en)) || ''; }
  function adCard(a) {
    var L = ['<div class="cb">' + esc(txt(a, 0)) + '</div>', '<div class="ct">' + esc(txt(a, 1)) + '</div>'];
    L.push('<div>' + esc(condText(a) || ' ') + '</div>');
    gfmt(ui(382), [a.fee, a.penalty, a.minv == null ? '?' : gameMillions(a.minv), a.pct == null ? '?' : a.pct]).forEach(function (l) { L.push('<div>' + l + '</div>'); });
    L.push('<div class="cend">' + gfmt(ui(385), [a.spots, a.days])[0] + '</div>');
    return '<figure class="card">' + L.join('') + '<figcaption>The game’s contract card in the ad agency, for the human player at the start (tooltip_show 14E1:16BB, kind 3).</figcaption></figure>';
  }

  // ------------------------------------------------------------ details
  function textBlock(r, kicker, n) {
    var o = otherLang();
    return '<p class="d-kicker">' + kicker + '</p><h3>' + esc(title(r)) + '</h3>' +
      '<p class="d-alt">' + langName(o) + ': ' + esc(txt(r, 0, o) || '—') + '</p>' +
      '<p class="d-desc">' + esc(txt(r, 1) || '—') + '</p>' +
      (txt(r, 1, o) ? '<p class="d-desc-alt">' + esc(txt(r, 1, o)) + '</p>' : '');
  }
  function hourTable(solo, gcode, xrated, note) {
    var rows = solo.map(function (v, h) {
      var risk = xrated && h < 3;
      return '<tr' + (v === 0 ? ' class="na"' : '') + '><th scope="row">' + HOURS[h] + ':00</th><td class="num">' + M.genreAud[gcode][h + 1] +
        ' %</td><td class="num">' + v + ' %</td><td class="num">' + num(viewersAtStart(v)) + '</td><td>' + (risk ? '<span class="no">moral guardian</span>' : '') + '</td></tr>';
    }).join('');
    return '<div class="tblwrap"><table class="kv"><thead><tr><th scope="col">Hour</th><th scope="col" class="num">Genre audience</th><th scope="col" class="num">Share alone</th><th scope="col" class="num">Viewers at start</th><th scope="col"></th></tr></thead><tbody>' +
      rows + '</tbody></table></div><p class="d-alt" style="font-size:12.5px">' + note + '</p>';
  }
  function filmDetail(f) {
    var facts = [];
    var st = f.start || {};
    if (f.market === 'dealer') facts.push('Sold by the film dealer (flag bit 5 set, bit 6 clear). Asking price = current value.');
    else if (f.market === 'auction') facts.push('Sold only through the auction (flag bit 6). Bidding starts at the current value.');
    else facts.push('Not on the market at the start (flag bit 5 clear' + (f.price === 0 ? ', price 0' : '') + ').');
    if (f.gift) facts.push('In the starting-gift pool: at a new game each station draws three cultural programmes from this pool, free.');
    if (f.prod) facts.push('Made in the studio from screenplay ' + f.prod.script + ' (' + money(f.prod.price) + (f.prod.offered ? ', offered by the screenplay agency' : ', never offered') + '). Production sets quality = topicality to the production result' + (f.prod.multi ? ' and the price to 0.' : ' and the price to price × quality / 100.'));
    if (f.genre === 7) facts.push('Live: airs only on the day and hour set when it is offered; ' + (f.id === 29 ? 'recurs 3 times.' : f.id === 187 ? 'recurs 5 times.' : 'leaves the library after airing; lost if missed.'));
    else if (f.flags2 & 4) facts.push('One airing only: it leaves the library after it has aired (flags 2 bit 2).');
    if (f.flags2 & 8) facts.push('X-rated: aired before 21:00 on a day the moral guardian is out, he confiscates it.');
    if (f.id === 58) facts.push('Betty’s own programme: each airing adds 5 to Betty’s love (up to your image).');
    if (f.id === 138) facts.push('After it airs, the station learns the current trend genres.');
    var tr = trendOf(f.genre);
    if (tr) facts.push('Trend genre on the first evening: pull ×' + tr + ' (not included in the strip).');
    if (f.blocks > 0) facts.push('Can start at ' + HOURS[0] + ':00–' + HOURS[Math.max(0, 7 - f.blocks)] + ':00 (must end by 1:00).');
    var fam = [];
    for (var b = 0; b < 5; b++) if (f.flags & (1 << b)) fam.push(['Dad', 'Mum', 'Grandpa', 'Kid', 'Teen'][b]);
    facts.push('TV family in front of the set: ' + (fam.length ? fam.join(', ') : 'nobody') + ' (cosmetic).');
    var wear = f.seqT.map(function (t, k) {
      return '<tr><th scope="row">' + (k === 0 ? 'At start' : 'After ' + k + ' airing' + (k > 1 ? 's' : '')) + '</th><td class="num">' + t + '</td><td class="num">' + money(f.seqV[k]) + '</td></tr>';
    }).join('');
    return '<div class="detail" id="d-movies-' + f.id + '"><div class="d-grid"><div class="d-block">' +
      textBlock(f, 'Movie ' + f.id + ' · ' + esc(gname(f.genre)) + (f.corrupt.length ? ' · corrupt ' + f.corrupt.map(langName).join('/') + ' text in the game data' : '')) +
      filmCard(f) + '</div><div class="d-side">' +
      '<div class="d-block"><h4>Value and wear</h4><div class="tblwrap"><table class="kv"><thead><tr><th scope="col"></th><th scope="col" class="num">Topicality</th><th scope="col" class="num">Value</th></tr></thead><tbody>' + wear + '</tbody></table></div>' +
      '<p class="d-alt" style="font-size:12.5px">Each airing costs ' + f.loss + ' topicality (quality ' + (st.quality != null ? st.quality : f.quality) + ' × wear ' + f.wear_rate + ' / 5), never below 1. Without airings it climbs back every second day in steps of 20 % of quality.</p></div>' +
      '<div class="d-block"><h4>Audience by hour</h4>' + hourTable(f.solo, f.genre, f.flags2 & 8, 'Share when it is the only programme on air, at its starting topicality, no news lead-in, no trend; viewers for the human player’s starting reach.') + '</div>' +
      '<div class="d-block"><h4>Rules</h4><ul class="facts">' + facts.map(function (t) { return '<li>' + esc(t) + '</li>'; }).join('') + '</ul></div>' +
      '</div></div><div class="d-block"><h4>Record: all 23 bytes (resource 0xC9, entry ' + f.id + ')</h4>' + fieldsHtml('film', f) + '</div></div>';
  }
  function seriesDetail(s) {
    var o = otherLang();
    var eps = s.eps.map(function (e, k) {
      var n = k + 1;
      var inRange = n <= s.episodes;
      if (!inRange && !e.q && !e.t && !e.f && !txt(s, 1 + n)) return '';
      var t = txt(s, 1 + n) || txt(s, 1 + n, o);
      var d = txt(s, 11 + n) || txt(s, 11 + n, o);
      return '<tr' + (e.f & 1 ? '' : ' class="na"') + '><th scope="row" class="num">' + n + '</th><td>' + esc(t || '—') + (d ? '<br><span class="dim">' + esc(d) + '</span>' : '') +
        (inRange ? '' : ' <span class="chip">beyond the episode count</span>') + '</td><td class="num">' + e.q + '</td><td class="num">' + e.t + '</td><td class="num">' + hx(e.f) + (e.f & 1 ? ' <span class="yes">ready</span>' : ' <span class="dim">not made</span>') +
        '</td><td class="num">−' + e.loss + '</td><td class="num">' + e.seq.slice(1).join(' → ') + '</td></tr>';
    }).join('');
    var facts = [];
    if (s.market === 'dealer') facts.push('Sold by the film dealer (flag bit 5 set, bit 6 clear) at the series value.');
    else if (s.market === 'auction') facts.push('Sold only through the auction (flag bit 6). Bidding starts at the series value.');
    else facts.push('Not on the market at the start (flag bit 5 clear).');
    if (s.gift) facts.push('Starting series: at a new game every station gets one of the series 18, 19 and 20, free.');
    if (s.prod) facts.push('Made in the studio from screenplay ' + s.prod.script + ' (' + money(s.prod.price) + (s.prod.offered ? ', offered by the screenplay agency' : ', never offered') + '); each production fills one episode’s quality and topicality.');
    facts.push('Value counts produced episodes from episode 1 on: ' + s.made + ' of ' + s.episodes + ' ready → ' + money(s.value) + '. After every ready episode aired once: ' + money(s.valueWorn) + '.');
    facts.push('A second airing of the same series on one evening halves its pull, a third divides it by three.');
    var tr = trendOf(10 + s.genre);
    if (tr) facts.push('Trend genre on the first evening: pull ×' + tr + ' (not included in the strip).');
    var fam = [];
    for (var b = 0; b < 5; b++) if (s.flags & (1 << b)) fam.push(['Dad', 'Mum', 'Grandpa', 'Kid', 'Teen'][b]);
    facts.push('TV family in front of the set: ' + (fam.length ? fam.join(', ') : 'nobody') + ' (cosmetic).');
    return '<div class="detail" id="d-series-' + s.id + '"><div class="d-grid"><div class="d-block">' +
      textBlock(s, 'Series ' + s.id + ' · ' + esc(gname(10 + s.genre))) + seriesCard(s) + '</div><div class="d-side">' +
      '<div class="d-block"><h4>Audience by hour (episode 1)</h4>' + hourTable(s.solo, 10 + s.genre, false, 'Share of episode 1 when it is the only programme on air, first airing of the evening, no news lead-in, no trend.') + '</div>' +
      '<div class="d-block"><h4>Rules</h4><ul class="facts">' + facts.map(function (t) { return '<li>' + esc(t) + '</li>'; }).join('') + '</ul></div>' +
      '</div></div>' +
      '<div class="d-block"><h4>Episodes</h4><div class="tblwrap"><table class="kv"><thead><tr><th scope="col" class="num">#</th><th scope="col">Title and description</th><th scope="col" class="num">Quality</th><th scope="col" class="num">Topicality</th><th scope="col" class="num">Flags</th><th scope="col" class="num">Wear</th><th scope="col" class="num">After 1/2/3 airings</th></tr></thead><tbody>' +
      (eps || '<tr class="na"><td colspan="7">No episodes in the data.</td></tr>') + '</tbody></table></div><p class="d-alt" style="font-size:12.5px">Wear per airing = quality × ' + s.wear_rate + ' / 20, never below 1; recovery every second day in 5 % steps of quality.</p></div>' +
      '<div class="d-block"><h4>Record: all 57 bytes (resource 0xCA, entry ' + s.id + ')</h4>' + fieldsHtml('series', s) + '</div></div>';
  }
  function adDetail(a) {
    var acc = [];
    for (var g = 1; g <= 15; g++) {
      acc.push('<li>' + (a.accepts[g] ? '<span class="yes">✓</span> ' : '<span class="no">✕</span> ') + esc(gname(g)) + '</li>');
    }
    var facts = [];
    facts.push('Air ' + a.spots + ' spot' + (a.spots > 1 ? 's' : '') + ' by the end of the ' + a.days + (a.days === 1 ? 'st' : a.days === 2 ? 'nd' : a.days === 3 ? 'rd' : 'th') + ' day after signing. The last spot pays the bonus of ' + money(a.fee) + '.');
    facts.push(a.penalty ? 'Missing the deadline costs ' + money(a.penalty) + '.' : 'Missing the deadline only cancels the contract.');
    facts.push(a.min_audience_class === 1 ? 'A spot counts after any programme.' : 'A spot counts when the hour has at least ' + num(a.minv) + ' viewers (rounded to 100,000) and the programme passes the condition.');
    facts.push('To sign it you need image ' + a.image + ' at the start (' + (a.pct == null ? '?' : a.pct) + ' % of your starting reach' + (a.genre_rule[0] !== 0 ? '; 20 for any programme condition' : '') + '). The human player starts with image 34.');
    facts.push(a.offered ? 'Can be on the agency shelf from the first day (class ' + a.min_audience_class + ' ≤ ' + M.maxClass + ').' : 'Not offered at the start: the agency waits until a station’s reach makes ' + num(a.minv) + ' viewers less than 40 % of it.');
    facts.push(a.rivals ? 'Computer players may sign it.' : 'Computer players never sign it: it requires a genre.');
    var per = a.spots ? Math.round(a.fee / a.spots) : 0;
    facts.push('Bonus per spot about ' + money(per) + ' (for comparison only; the game pays the bonus at the end).');
    return '<div class="detail" id="d-ads-' + a.id + '"><div class="d-grid"><div class="d-block">' +
      '<p class="d-kicker">Advertiser ' + a.id + '</p><h3>' + esc(txt(a, 0)) + '</h3><p class="d-alt">' + langName(otherLang()) + ': ' + esc(txt(a, 0, otherLang()) || '—') + '</p>' +
      '<p class="d-desc">' + esc(txt(a, 1)) + '</p><p class="d-desc-alt">' + esc(txt(a, 1, otherLang())) + '</p>' + adCard(a) + '</div><div class="d-side">' +
      '<div class="d-block"><h4>Contract</h4><ul class="facts">' + facts.map(function (t) { return '<li>' + esc(t) + '</li>'; }).join('') + '</ul></div>' +
      '<div class="d-block"><h4>Programmes the spot may follow</h4><ul class="accept">' + acc.join('') + '</ul></div>' +
      '</div></div><div class="d-block"><h4>Record: all 24 bytes (resource 0xCC, entry ' + a.id + ')</h4>' + fieldsHtml('ad', a) + '</div></div>';
  }

  // ------------------------------------------------------------ sections
  function genreOptions(from, to) {
    var o = [['', 'All genres']];
    for (var g = from; g <= to; g++) o.push([String(g), gname(g)]);
    return o;
  }
  function searchHay(r, extra) {
    var parts = [String(r.id)];
    ['en', 'de'].forEach(function (l) { (r.t[l] || []).forEach(function (s) { parts.push(s); }); });
    if (extra) parts.push(extra);
    return parts.join(' \u0001 ').toLowerCase();
  }
  function marketChip(r) {
    var c = r.market === 'dealer' ? chip('Dealer', 'chip-sig') : r.market === 'auction' ? chip('Auction', 'chip-sig') : '<span class="dim">—</span>';
    return c + (r.gift ? ' ' + chip('Gift pool', 'chip-go') : '');
  }

  var SECTIONS = {
    movies: {
      rows: D.films, noun: ['movie', 'movies'], detail: filmDetail,
      sort: { key: 'id', dir: 1 },
      cols: [
        { k: 'id', label: '#', cls: 'id', num: true, v: function (f) { return f.id; }, c: function (f) { return String(f.id).padStart(3, '0'); } },
        { k: 'title', label: 'Title', cls: 'title', v: function (f) { return title(f).toLowerCase(); },
          c: function (f) {
            var sub = f.genre === 7 ? 'Live broadcast' : f.genre === 8 ? 'Show' : ((M.countries[lang] || [])[f.c13] || '') + (f.c14 ? ' 19' + pad2(f.c14) : '');
            return '<button type="button" class="open-btn" aria-expanded="false" aria-controls="d-movies-' + f.id + '">' + esc(title(f)) + '</button><span class="sub">' + esc(sub) + '</span>';
          } },
        { k: 'genre', label: 'Genre', v: function (f) { return f.genre; }, c: function (f) { return esc(gname(f.genre)); } },
        { k: 'blocks', label: 'Hours', num: true, v: function (f) { return f.blocks; }, c: function (f) { return f.blocks; } },
        { k: 'quality', label: 'Quality', num: true, v: function (f) { return f.quality; }, c: function (f) { return f.quality; } },
        { k: 'loss', label: 'Wear', title: 'Topicality lost per airing', num: true, v: function (f) { return f.loss; }, c: function (f) { return f.loss ? '−' + f.loss : '0'; } },
        { k: 'value', label: 'Price', title: 'Value at the start of a new game: dealer price, auction start, resale', num: true, v: function (f) { return f.value; }, c: function (f) { return money(f.value); } },
        { k: 'v1', label: 'After 1×', title: 'Value after one airing', num: true, v: function (f) { return f.seqV[1]; }, c: function (f) { return money(f.seqV[1]); } },
        { k: 'market', label: 'At start', v: function (f) { return f.market + (f.gift ? 'a' : 'b'); }, c: marketChip },
        { k: 'solo', label: 'Audience', head: stripHead('Audience'), v: function (f) { return Math.max.apply(null, f.solo); }, c: function (f) { return strip(f.solo, f.flags2 & 8, 'Share alone'); } },
        { k: 'tags', label: 'Notes', v: function (f) { return (f.flags2 & 8 ? 4 : 0) + (f.genre === 7 ? 2 : 0) + (f.prod ? 1 : 0); },
          c: function (f) {
            var t = [];
            if (f.flags2 & 8) t.push(chip('X-rated', 'chip-x'));
            if (f.genre === 7) t.push(chip('Live', 'chip-live'));
            else if (f.flags2 & 4) t.push(chip('One airing'));
            if (f.prod) t.push(chip('Studio'));
            if (f.corrupt.length) t.push(chip('Bad text', 'chip-x'));
            return '<span class="chips">' + t.join('') + '</span>';
          } }
      ],
      filters: [
        { id: 'genre', label: 'Genre', opts: function () { return genreOptions(1, 10); }, test: function (f, v) { return f.genre === +v; } },
        { id: 'market', label: 'At game start', opts: function () { return [['', 'Anywhere'], ['dealer', 'Film dealer'], ['auction', 'Auction'], ['gift', 'Starting-gift pool'], ['none', 'Not on the market']]; },
          test: function (f, v) { return v === 'gift' ? f.gift : f.market === v; } },
        { id: 'rating', label: 'X-rated', opts: function () { return [['', 'Include'], ['only', 'Only X-rated'], ['hide', 'Hide X-rated']]; },
          test: function (f, v) { return v === 'only' ? !!(f.flags2 & 8) : !(f.flags2 & 8); } },
        { id: 'kind', label: 'Kind', opts: function () { return [['', 'All'], ['live', 'Live broadcasts'], ['once', 'One airing only'], ['studio', 'Studio productions'], ['bought', 'Films to buy']]; },
          test: function (f, v) {
            if (v === 'live') return f.genre === 7;
            if (v === 'once') return !!(f.flags2 & 4) || f.genre === 7;
            if (v === 'studio') return !!f.prod;
            return !f.prod && f.genre !== 7 && !(f.flags2 & 4);
          } }
      ]
    },
    series: {
      rows: D.series, noun: ['series', 'series'], detail: seriesDetail,
      sort: { key: 'id', dir: 1 },
      cols: [
        { k: 'id', label: '#', cls: 'id', num: true, v: function (s) { return s.id; }, c: function (s) { return String(s.id).padStart(2, '0'); } },
        { k: 'title', label: 'Title', cls: 'title', v: function (s) { return title(s).toLowerCase(); },
          c: function (s) { return '<button type="button" class="open-btn" aria-expanded="false" aria-controls="d-series-' + s.id + '">' + esc(title(s)) + '</button><span class="sub">' + esc(gname(10 + s.genre)) + '</span>'; } },
        { k: 'made', label: 'Episodes', title: 'Produced / total', num: true, v: function (s) { return s.made * 100 + s.episodes; }, c: function (s) { return s.made + '/' + s.episodes; } },
        { k: 'blocks', label: 'Hours', title: 'Length of one episode', num: true, v: function (s) { return s.blocks; }, c: function (s) { return s.blocks; } },
        { k: 'avgq', label: 'Quality', title: 'Average episode quality', num: true, v: function (s) { return s.avgq; }, c: function (s) { return s.avgq; } },
        { k: 'critics', label: 'Critics', num: true, v: function (s) { return s.critics; }, c: function (s) { return s.critics; } },
        { k: 'press', label: 'Press', num: true, v: function (s) { return s.press; }, c: function (s) { return s.press; } },
        { k: 'wear', label: 'Wear', title: 'Wear rate (episode loses quality × rate / 20)', num: true, v: function (s) { return s.wear_rate; }, c: function (s) { return s.wear_rate; } },
        { k: 'eprice', label: 'Per episode', num: true, v: function (s) { return s.episode_price; }, c: function (s) { return money(s.episode_price); } },
        { k: 'value', label: 'Value', title: 'Series value at the start of a new game', num: true, v: function (s) { return s.value; }, c: function (s) { return money(s.value); } },
        { k: 'market', label: 'At start', v: function (s) { return s.market + (s.gift ? 'a' : 'b'); }, c: marketChip },
        { k: 'solo', label: 'Audience', head: stripHead('Audience'), v: function (s) { return Math.max.apply(null, s.solo); }, c: function (s) { return strip(s.solo, false, 'Episode 1 share alone'); } },
        { k: 'tags', label: 'Notes', v: function (s) { return s.prod ? 1 : 0; }, c: function (s) { return '<span class="chips">' + (s.prod ? chip('Studio') : '') + (s.made === 0 ? chip('Not made') : '') + '</span>'; } }
      ],
      filters: [
        { id: 'genre', label: 'Genre', opts: function () { return genreOptions(11, 15); }, test: function (s, v) { return 10 + s.genre === +v; } },
        { id: 'market', label: 'At game start', opts: function () { return [['', 'Anywhere'], ['dealer', 'Film dealer'], ['auction', 'Auction'], ['gift', 'Starting series'], ['none', 'Not on the market']]; },
          test: function (s, v) { return v === 'gift' ? s.gift : s.market === v; } },
        { id: 'made', label: 'Episodes', opts: function () { return [['', 'All'], ['ready', 'Ready to air'], ['none', 'Not made yet (studio)']]; },
          test: function (s, v) { return v === 'ready' ? s.made > 0 : s.made === 0; } }
      ]
    },
    ads: {
      rows: D.ads, noun: ['ad contract', 'ad contracts'], detail: adDetail,
      sort: { key: 'id', dir: 1 },
      cols: [
        { k: 'id', label: '#', cls: 'id', num: true, v: function (a) { return a.id; }, c: function (a) { return String(a.id).padStart(2, '0'); } },
        { k: 'name', label: 'Advertiser', cls: 'title', v: function (a) { return txt(a, 0).toLowerCase(); },
          c: function (a) { return '<button type="button" class="open-btn" aria-expanded="false" aria-controls="d-ads-' + a.id + '">' + esc(txt(a, 0)) + '</button><span class="sub">' + esc(txt(a, 1)) + '</span>'; } },
        { k: 'spots', label: 'Spots', num: true, v: function (a) { return a.spots; }, c: function (a) { return a.spots; } },
        { k: 'days', label: 'Days', num: true, v: function (a) { return a.days; }, c: function (a) { return a.days; } },
        { k: 'minv', label: 'Min. viewers', num: true, v: function (a) { return a.minv; }, c: function (a) { return millions(a.minv); } },
        { k: 'pct', label: '% of reach', title: 'Minimum audience as % of the human player’s starting reach', num: true, v: function (a) { return a.pct; }, c: function (a) { return a.pct + ' %'; } },
        { k: 'image', label: 'Image', title: 'Image needed to sign it at the start (human starts with 34)', num: true, v: function (a) { return a.image; },
          c: function (a) { return a.image > 34 ? '<span class="no">' + a.image + '</span>' : String(a.image); } },
        { k: 'fee', label: 'Bonus', num: true, v: function (a) { return a.fee; }, c: function (a) { return money(a.fee); } },
        { k: 'penalty', label: 'Penalty', num: true, v: function (a) { return a.penalty; }, c: function (a) { return money(a.penalty); } },
        { k: 'cond', label: 'Programme condition', cls: 'wrap', v: function (a) { return (a.genre_rule[0] > 0 ? 2 : a.genre_rule[0] < 0 ? 1 : 0) * 100 + Math.abs(a.genre_rule[0]); },
          c: function (a) { return a.genre_rule[0] ? '<span class="' + (a.genre_rule[0] < 0 ? 'no' : 'yes') + '" style="font-weight:500">' + esc(condText(a)) + '</span>' : '<span class="dim">' + esc(ui(79)) + '</span>'; } },
        { k: 'offered', label: 'At start', v: function (a) { return a.offered ? 0 : 1; }, c: function (a) { return a.offered ? chip('Offered', 'chip-go') : '<span class="dim">later</span>'; } },
        { k: 'rivals', label: 'Rivals', title: 'Can the computer players sign it?', v: function (a) { return a.rivals ? 0 : 1; }, c: function (a) { return a.rivals ? 'yes' : '<span class="dim">never</span>'; } }
      ],
      filters: [
        { id: 'cond', label: 'Condition', opts: function () { return [['', 'Any'], ['none', 'No condition'], ['req', 'Requires genres'], ['exc', 'Refuses genres']]; },
          test: function (a, v) { return v === 'none' ? a.genre_rule[0] === 0 : v === 'req' ? a.genre_rule[0] > 0 : a.genre_rule[0] < 0; } },
        { id: 'accepts', label: 'After programme', opts: function () { var o = genreOptions(1, 15); o[0] = ['', 'Any genre']; return o; },
          test: function (a, v) { return !!a.accepts[+v]; } },
        { id: 'cls', label: 'Min. viewers', opts: function () { var o = [['', 'Any']]; for (var c = 1; c <= 10; c++) o.push([String(c), 'Class ' + c + ': ' + millions(M.minViewers[c - 1])]); return o; },
          test: function (a, v) { return a.min_audience_class === +v; } },
        { id: 'start', label: 'At game start', opts: function () { return [['', 'All'], ['yes', 'Offered at the start'], ['no', 'Offered later'], ['sign', 'Signable at the start (image 34)']]; },
          test: function (a, v) { return v === 'yes' ? a.offered : v === 'no' ? !a.offered : (a.offered && a.image <= 34); } }
      ]
    }
  };

  // ------------------------------------------------------------ news
  var N = D.news, NODES = N.nodes, TOPICS = {};
  N.topics.forEach(function (t) { TOPICS[t.id] = t; });
  function catName(c) { return (N.cats[lang] || N.cats.en)[c] || ('category ' + c); }
  function catChip(c) { return '<span class="chip cat' + c + '">' + esc(catName(c)) + '</span>'; }
  function topicLabel(id) {
    var t = TOPICS[id];
    if (!t) return 'list ' + id;
    var ph = t.ph[lang].filter(function (x) { return x; });
    return 'List ' + id + ': ' + ph.slice(0, 2).join(', ') + (ph.length > 2 ? ', …' : '');
  }
  function newsPriceRange(a, b) { var x = money(N.prices[a] || 0), y = money(N.prices[b] || 0); return x === y ? x : x + '–' + y; }
  function pattern(text, story) {
    // the headline as stored: '#n' marked with the phrase list of part n
    return esc(text).replace(/#(\d)/g, function (m, k) {
      var code = story ? story.parts[+k - 1] : 0;
      return '<span class="ph" title="phrase of part ' + k + '">#' + k + (code ? ' ' + esc((TOPICS[code % 1000] || { ph: { en: [''] } }).ph[lang][0] || '') + '…' : '') + '</span>';
    });
  }
  function newsCard(text, cat, cls, hour, old) {
    var line = '*** ' + pad2(hour) + ':00 ' + (old ? N.old[lang] : N.today[lang]) + ' ** $ ' + (N.prices[cls] || 0) + ' ***';
    return '<figure class="ncard cat' + cat + '"><div class="nh">' + esc(text) + '</div><div class="nl">' + esc(line) + '</div></figure>';
  }
  function storyTitle(s) { return s.chain.length ? s.chain[0].ex[0].t[lang] || s.chain[0].ex[0].t.en : '(no headline)'; }
  function storyDetail(s) {
    var o = otherLang();
    var parts = s.pinfo.map(function (p) {
      var t = TOPICS[p.topic];
      var how = p.mode === 1 ? 'same variant as part ' + (p.k - 1) : p.mode === 2 ? 'random variant, not counted in the value' : 'random variant, counted in the value';
      var phr = t ? t.ph[lang].map(function (x, v) { return x ? '<span class="phr">' + esc(x) + ' <span class="pv">' + t.pv[v] + '</span></span>' : ''; }).join('') : '—';
      return '<tr><th scope="row" class="num">#' + p.k + '</th><td class="num">' + p.code + '</td><td>List ' + p.topic + '</td><td>' + esc(how) + '</td><td>' + phr + '</td></tr>';
    }).join('');
    var chain = s.chain.map(function (c) {
      var nd = NODES[c.n];
      var next = nd.succ.filter(function (x) { return x; }).map(function (x) { return x === 9999 ? 'end' : String(x); }).join(', ');
      return '<tr><th scope="row" class="num">' + c.n + '</th><td>' + pattern(nd.t[lang] || nd.t[o], s) +
        '<br><span class="dim">' + esc(c.ex[0].t[lang]) + '</span>' + (c.ex[1].t[lang] !== c.ex[0].t[lang] ? '<br><span class="dim">' + esc(c.ex[1].t[lang]) + '</span>' : '') +
        '</td><td class="num">' + nd.v + '</td><td class="num">' + c.vmin + (c.vmax !== c.vmin ? '–' + c.vmax : '') + '</td><td class="num">' + newsPriceRange(c.cmin, c.cmax) +
        '</td><td class="num">' + nd.d + ' h</td><td class="num">' + esc(next) + '</td><td class="num">' + (c.toEnd == null ? '—' : c.toEnd) + '</td></tr>';
    }).join('');
    var cards = s.chain.slice(0, 3).map(function (c, k) { return newsCard(c.ex[0].t[lang], s.cat, c.ex[0].cls, 18 + k, false); }).join('');
    var facts = [];
    facts.push('Category: ' + catName(s.cat) + '. It runs in one of the ' + N.slotCats.filter(function (c) { return c === s.cat; }).length + ' story slots of that category (7 slots in all).');
    facts.push('Starts at headline ' + s.start + ' three hours after it is picked; then every headline waits its own number of hours before the next.');
    facts.push('After each headline one of up to four successors is drawn at random; "end" frees the slot for a new story.');
    facts.push(s.shortest == null ? 'It never reaches an end.' : 'Shortest run: ' + s.shortest + ' headlines' + (s.loops ? '; it can loop, so there is no longest run.' : '; longest: ' + s.longest + '.'));
    facts.push('A story is not picked again until every story of its category has run (start node − 1000 marks it used).');
    var rec = [
      ['+0x00', 'Parts', s.parts.join(', '), 'Five phrase parts: list number + 1000 × mode (0 random and counted, 1 same variant as the part before, 2 random, not counted).'],
      ['+0x0A', 'Start headline', String(s.start), 'First headline node; lowered by 1000 while the story is used up.'],
      ['+0x0C', 'Byte +0x0C', String(s.b0c), 'Unknown: equals the number of parts in every record, never read.'],
      ['+0x0D', 'Category', s.cat + ' · ' + catName(s.cat), '1 political news, 2 show business, 3 sundry: which agency delivers it.']
    ];
    var recHtml = rec.map(function (r) { return '<div class="f' + (/^Unknown/.test(r[3]) ? ' unknown' : '') + '"><code class="off">' + r[0] + '</code><span class="fname">' + esc(r[1]) + '</span><span class="fval">' + esc(r[2]) + '</span><span class="fdoc">' + esc(r[3]) + '</span></div>'; }).join('');
    return '<div class="detail" id="d-news-' + s.id + '"><div class="d-grid"><div class="d-block"><p class="d-kicker">Story ' + s.id + ' · ' + esc(catName(s.cat)) + '</p><h3>' + esc(storyTitle(s)) + '</h3>' +
      '<p class="d-alt">' + langName(o) + ': ' + esc(s.chain.length ? s.chain[0].ex[0].t[o] : '') + '</p>' +
      '<div class="d-block" style="margin-top:12px"><h4>As the news room shows it</h4><div class="ncards">' + cards + '</div><p class="d-alt" style="font-size:12.5px">Example phrases; the hour is when the item arrived, "' + esc(N.old[lang]) + '" replaces "' + esc(N.today[lang]) + '" from the next day on. Price by value class at arrival.</p></div></div>' +
      '<div class="d-side"><div class="d-block"><h4>Rules</h4><ul class="facts">' + facts.map(function (t) { return '<li>' + esc(t) + '</li>'; }).join('') + '</ul></div>' +
      '<div class="d-block"><h4>Phrase parts</h4><div class="tblwrap"><table class="kv"><thead><tr><th scope="col" class="num">Part</th><th scope="col" class="num">Code</th><th scope="col">List</th><th scope="col">Choice</th><th scope="col">Phrases and their values</th></tr></thead><tbody>' + (parts || '<tr class="na"><td colspan="5">No phrase parts.</td></tr>') + '</tbody></table></div></div></div></div>' +
      '<div class="d-block"><h4>Headlines (' + s.chain.length + ')</h4><div class="tblwrap"><table class="kv"><thead><tr><th scope="col" class="num">Node</th><th scope="col">Headline as stored, two examples</th><th scope="col" class="num">Base</th><th scope="col" class="num">Value</th><th scope="col" class="num">Price</th><th scope="col" class="num">Wait</th><th scope="col" class="num">Next</th><th scope="col" class="num">To end</th></tr></thead><tbody>' + chain + '</tbody></table></div>' +
      '<p class="d-alt" style="font-size:12.5px">Value = base × (30 + 70 × average phrase value / 10) / 100 over the counted parts; the range runs from the weakest to the strongest phrases. "To end" is the fewest headlines until the story can end.</p></div>' +
      '<div class="d-block"><h4>Record: 14 bytes (resource 0x30, entry ' + s.id + ')</h4><div class="fields">' + recHtml + '</div><p class="rawhex">14 bytes: ' + esc(s.raw.replace(/(..)/g, '$1 ').trim()) + '</p></div></div>';
  }
  SECTIONS.news = {
    rows: N.stories, noun: ['story', 'stories'], detail: storyDetail,
    sort: { key: 'id', dir: 1 },
    hay: function (s) {
      var parts = [String(s.id)];
      s.chain.forEach(function (c) { var nd = NODES[c.n]; parts.push(nd.t.en, nd.t.de, c.ex[0].t.en, c.ex[0].t.de, c.ex[1].t.en, c.ex[1].t.de); });
      s.pinfo.forEach(function (p) { var t = TOPICS[p.topic]; if (t) parts.push(t.ph.en.join(' '), t.ph.de.join(' ')); });
      return parts.join(' \u0001 ').toLowerCase();
    },
    cols: [
      { k: 'id', label: '#', cls: 'id', num: true, v: function (s) { return s.id; }, c: function (s) { return String(s.id).padStart(2, '0'); } },
      { k: 'title', label: 'Story (first headline, example)', cls: 'title', v: function (s) { return storyTitle(s).toLowerCase(); },
        c: function (s) { return '<button type="button" class="open-btn" aria-expanded="false" aria-controls="d-news-' + s.id + '">' + esc(storyTitle(s)) + '</button><span class="sub">' + s.chain.length + ' headlines</span>'; } },
      { k: 'cat', label: 'Category', v: function (s) { return s.cat; }, c: function (s) { return catChip(s.cat); } },
      { k: 'lists', label: 'Phrase lists', cls: 'wrap', v: function (s) { return s.pinfo.length; },
        c: function (s) { return s.pinfo.length ? s.pinfo.map(function (p) { return '<span class="dim">#' + p.k + '</span> ' + esc(topicLabel(p.topic)); }).join('<br>') : '<span class="dim">none</span>'; } },
      { k: 'n', label: 'Headlines', num: true, v: function (s) { return s.chain.length; }, c: function (s) { return s.chain.length; } },
      { k: 'len', label: 'Run', title: 'Fewest headlines until it can end; loops = can run forever', num: true, v: function (s) { return s.shortest == null ? 999 : s.shortest; },
        c: function (s) { return (s.shortest == null ? '—' : s.shortest + (s.loops ? '+' : '–' + s.longest)); } },
      { k: 'value', label: 'Value', num: true, v: function (s) { return s.vmax; }, c: function (s) { return s.vmin + '–' + s.vmax; } },
      { k: 'price', label: 'Price', num: true, v: function (s) { return s.vmax; },
        c: function (s) { var a = 0, b = 0; s.chain.forEach(function (c, k) { if (!k || c.cmin < a) a = c.cmin; if (c.cmax > b) b = c.cmax; }); return newsPriceRange(a, b); } }
    ],
    filters: [
      { id: 'cat', label: 'Category', opts: function () { return [['', 'All'], ['1', catName(1)], ['2', catName(2)], ['3', catName(3)]]; }, test: function (s, v) { return s.cat === +v; } },
      { id: 'list', label: 'Uses phrase list', opts: function () { return [['', 'Any']].concat(N.topics.map(function (t) { return [String(t.id), topicLabel(t.id)]; })); },
        test: function (s, v) { return s.pinfo.some(function (p) { return p.topic === +v; }); } },
      { id: 'loop', label: 'Run', opts: function () { return [['', 'All'], ['loop', 'Can loop'], ['ends', 'Always ends']]; }, test: function (s, v) { return v === 'loop' ? s.loops : !s.loops; } }
    ]
  };
  function newsExtras() {
    var ha = M.hourAud;
    var vals = [20, 40, 60, 80, 100];
    var rows = vals.map(function (v) {
      return '<tr><th scope="row" class="num">' + v + '</th>' + HOURS.map(function (h, k) {
        var sc = Math.trunc(v * ha[k + 1] / 100);
        return '<td class="num">' + sc + ' % <span class="dim">+' + Math.trunc(sc / 5) + '</span></td>';
      }).join('') + '</tr>';
    }).join('');
    var age = [], v = 100;
    for (var k = 0; k <= 8; k++) { age.push(v); v = Math.trunc(v * 85 / 100); }
    var classes = N.prices.map(function (p, c) {
      var lo = c === 0 ? 0 : N.steps[c - 1] + 1, hi = N.steps[c];
      return '<tr><th scope="row" class="num">' + c + '</th><td class="num">' + lo + '–' + hi + '</td><td class="num">' + money(p) + '</td></tr>';
    }).join('');
    var topics = N.topics.map(function (t) {
      var phr = t.ph[lang].map(function (x, i) { return x ? '<span class="phr">' + esc(x) + ' <span class="pv">' + t.pv[i] + '</span></span>' : ''; }).join('');
      return '<tr><th scope="row" class="num">' + t.id + '</th><td>' + phr + '</td><td class="num">' + (t.used.length ? t.used.join(', ') : '<span class="dim">none</span>') + '</td><td class="num">' + t.b37 + '</td></tr>';
    }).join('');
    var ev = N.events.map(function (e) {
      var why = e.n === 166 ? 'A terrorist sets out (intruder event, 1DA8:276D).' : e.n === 167 ? 'His bomb goes off in the consulate he was sent to (1DA8:2D3B).' : 'His bomb goes off behind another door, for example after the information-board signs were swapped (1DA8:2D3B).';
      return '<tr><th scope="row" class="num">' + e.n + '</th><td>' + esc(e.ex[0][lang]) + '<br><span class="dim">' + esc(e.ex[1][lang]) + '</span></td><td>' + esc(why) + '</td><td class="num">' + e.v + '</td><td class="num">' + money(N.prices[e.cls]) + '</td></tr>';
    }).join('');
    var un = N.unreached.map(function (n) {
      var nd = NODES[n];
      return '<tr><th scope="row" class="num">' + n + '</th><td>' + pattern(nd.t[lang] || nd.t[otherLang()], null) + '</td><td class="num">' + nd.v + '</td><td class="num">' + esc(nd.succ.filter(function (x) { return x; }).map(function (x) { return x === 9999 ? 'end' : x; }).join(', ') || '—') + '</td></tr>';
    }).join('');
    var ag = N.agencies.map(function (a, i) {
      return '<tr><th scope="row">' + catChip(a[0]) + '</th><td class="num">' + money(a[1]) + '</td><td class="num">' + a[2] + '</td><td class="num">' + N.slotCats.filter(function (c) { return c === a[0]; }).length + '</td></tr>';
    }).join('');
    return '<section aria-labelledby="nx-how"><h3 id="nx-how">How a news item is made</h3>' +
      '<p>Seven story slots run at once (' + N.slotCats.map(catName).join(', ') + '). A free slot takes an unused story of its category, draws a phrase for every random part (a variant that has a phrase value and that no other running story uses) and waits three hours. Then the story emits its current headline into today’s pool and moves on to a random successor after that headline’s waiting time. Every hour the pool goes to the inbox of each station subscribed to the item’s agency (20 items at most, the oldest drops out). At the start of a day the stories age by five hours and at least three political items are forced out (news_tick 1C18:03C1).</p>' +
      '<p>Value at arrival = base × (30 + 70 × average phrase value / 10) / 100, averaged over the counted parts (news_emit 1C18:0490). In the inbox and in the broadcast slots the value drops to 85 % every hour and to 0 at the start of the next day (news_age 1C18:1022); the price class follows the value in the inbox.</p></section>' +
      '<section aria-labelledby="nx-price"><h3 id="nx-price">Price by value</h3><div class="scroller" style="max-width:420px"><table class="mini"><thead><tr><th scope="col" class="num">Class</th><th scope="col" class="num">Value</th><th scope="col" class="num">Price</th></tr></thead><tbody>' + classes + '</tbody></table></div>' +
      '<p style="margin-top:8px">You pay when you move an item from the inbox into one of your three broadcast slots (news room, 05FF:25A4). Agencies:</p>' +
      '<div class="scroller" style="max-width:520px"><table class="mini"><thead><tr><th scope="col">Agency</th><th scope="col" class="num">Per day</th><th scope="col" class="num">Image needed</th><th scope="col" class="num">Story slots</th></tr></thead><tbody>' + ag + '</tbody></table></div>' +
      '<p style="margin-top:8px">Computer players keep buying while their three slots average less than ' + N.aiTarget.slice(1).join(' / ') + ' (best / middle / worst image rank) and their news budget lasts (ai_buy_news 32BA:0124).</p></section>' +
      '<section aria-labelledby="nx-show"><h3 id="nx-show">What a news show does for the ratings</h3>' +
      '<p>News score = average value of the three slots × the news audience of the hour / 100. The scores of all stations make up at most 95 % of the audience; each station gets score × total / sum (24B8:0C4A, phase 1). A fifth of the score is added to the pull of the programme that follows, if it has any pull (the lead-in). The news share also counts towards a news Sammy, where half of Betty’s love is added.</p>' +
      '<div class="scroller"><table class="mini"><thead><tr><th scope="col" class="num">Slot average</th>' + HOURS.map(function (h) { return '<th scope="col" class="num">' + h + ':00</th>'; }).join('') + '</tr></thead><tbody>' + rows + '</tbody></table></div>' +
      '<p style="margin-top:8px">Cells: news share when no other station shows news, and <span class="dim">+lead-in</span> for the programme of that hour. A slot value of 100 drops hour by hour: ' + age.slice(0, 7).join(' → ') + '.</p></section>' +
      '<section aria-labelledby="nx-lists"><h3 id="nx-lists">Phrase lists (' + N.topics.length + ')</h3><p>The building blocks of the headlines. Each list has up to ten phrases with a value 1–10; a story part draws one of them. The last column is a byte of the record that the game never reads.</p>' +
      '<div class="scroller"><table class="mini"><thead><tr><th scope="col" class="num">List</th><th scope="col">Phrases with value</th><th scope="col" class="num">Used by stories</th><th scope="col" class="num">+0x37</th></tr></thead><tbody>' + topics + '</tbody></table></div></section>' +
      '<section aria-labelledby="nx-events"><h3 id="nx-events">News the game writes itself</h3><p>Three headlines are not part of any story. The game adds them to today’s pool as political news when the consulate terrorist acts (news_add_event 1C18:069A), with phrase list 16 picked by which terrorist it is. No other event (bailiff, Betty, Sammy, the stations) makes the news.</p>' +
      '<div class="scroller"><table class="mini"><thead><tr><th scope="col" class="num">Node</th><th scope="col">Headline (terrorist 1 / 2)</th><th scope="col">When</th><th scope="col" class="num">Value</th><th scope="col" class="num">Price</th></tr></thead><tbody>' + ev + '</tbody></table></div></section>' +
      '<section aria-labelledby="nx-unused"><h3 id="nx-unused">Headlines no story reaches</h3><p>These nodes have text but no story start or successor leads to them, and no code adds them; node 169 (the bailiff) is never used either. ' + N.emptyNodes + ' of the 400 node records are empty.</p>' +
      '<div class="scroller"><table class="mini"><thead><tr><th scope="col" class="num">Node</th><th scope="col">Headline</th><th scope="col" class="num">Base</th><th scope="col" class="num">Next</th></tr></thead><tbody>' + un + '</tbody></table></div></section>';
  }

  // ------------------------------------------------------------ screenplays, cast, props
  var C = D.cast;
  function castCat(c) { return (C.cats[lang] || C.cats.en)[c] || ('category ' + c); }
  function propName(t) { var p = C.props[t - 16]; return p ? p.name[lang] : 'set ' + t; }
  function kindLabel(x) { return x.kind === 'series' ? 'Series' : x.kind === 'show' ? 'Show, up to 10 airings' : 'Movie'; }
  function scriptTitle(x) { return x.ptitle ? (x.ptitle[lang][0] || x.ptitle.en[0] || '(untitled)') : '(no programme)'; }
  function roleName(x, r) { var ro = x.roles.filter(function (y) { return y.r === r; })[0]; return ro && (ro.name[lang] || ro.name[otherLang()]) || ('Role ' + r); }
  function slotLabel(x, s) {
    if (!s[0]) return '';
    var w = ' <span class="pv">×' + s[1] + '</span>';
    if (s[0] <= 15) return esc(roleName(x, s[0])) + w;
    return esc(propName(s[0])) + ' <span class="dim">(set)</span>' + w;
  }
  function scriptDetail(x) {
    var o = otherLang();
    var facts = [];
    facts.push((x.series ? 'Produces the episodes of series ' : 'Produces movie ') + x.target + ', “' + scriptTitle(x) + '”' + (x.series ? ' (' + x.episodes + ' episodes, one shopping list and one studio run each).' : x.multi ? ': a show that can be produced up to 10 times; each production airs once.' : '.'));
    facts.push(x.offered ? 'The screenplay agency can offer it (flag bit 1).' : 'Never offered by the screenplay agency (flag bit 1 clear).');
    facts.push('Studio time per production: ' + (x.duration || 10) + ' hours' + (x.duration ? '' : ' (0 means 10)') + '.');
    facts.push('Cast and props cost their price × the programme length (' + x.mult + ' block' + (x.mult === 1 ? '' : 's') + ').');
    if (!x.series) facts.push(x.pflags2 & 4 ? 'The movie record has the one-airing flag: after production its price is 0.' : 'After production the movie’s price becomes ' + money(x.pprice || 0) + ' × quality / 100.');
    facts.push('Quality = base × score / weights × 1.25; each role scores its weight × actor match, each set min(weight, set quality). The game rates it: above 80 good, above 60 neat, above 40 poor, else a catastrophe.');
    var roles = x.roles.map(function (r) {
      var best = r.best.map(function (b) { return esc(b.name) + ' <span class="pv">' + b.m + ' %, ' + money(b.fee * x.mult) + (r.cat === 7 ? ', ' + esc(castCat(b.cat)) : '') + '</span>'; }).join('<br>');
      return '<tr><th scope="row" class="num">' + r.r + '</th><td>' + esc(r.name[lang] || r.name[o] || '—') + '</td><td>' + esc(castCat(r.cat)) + '</td><td class="num">' + r.req[0] + ' / ' + r.req[2] + ' / ' + r.req[3] + ' <span class="dim">(' + r.req[1] + ')</span></td><td>' + (best || '<span class="dim">no actor in the pool</span>') + '</td></tr>';
    }).join('');
    var eps = x.blocks.filter(function (b) { return !b.copy; }).map(function (b) {
      var items = b.slots.map(function (s) { return slotLabel(x, s); }).filter(function (t) { return t; }).join('<br>');
      return '<tr><th scope="row" class="num">' + b.e + (x.multi && !x.series ? '–10' : '') + '</th><td class="num">' + b.base + '</td><td>' + (items || '<span class="dim">empty</span>') + '</td><td class="num">' + b.bestQ + '</td><td class="num">' + b.perfectQ + '</td><td class="num">' + money(b.cost) + '</td></tr>';
    }).join('');
    var rec = [
      ['+0x000', 'Price', money(x.price), 'Price at the screenplay agency (shown on its card, paid when bought).'],
      ['+0x004', 'State', String(x.state), 'Runtime: 0 free, 4 on offer, 99 used up; a used-up show screenplay goes back to the pool (24B8:02C1).'],
      ['+0x005', 'Flags', hx(x.flags) + ' · ' + [x.flags & 1 ? 'series' : 'movie', x.flags & 2 ? 'offered' : 'not offered', x.flags & 4 ? 'several productions' : ''].filter(function (t) { return t; }).join(', '), 'Bit 0 series, bit 1 the agency may offer it, bit 2 can be produced several times (shows: episode block 1 copied to 2–10 at load).'],
      ['+0x006', 'Target', String(x.target), (x.series ? 'Series' : 'Movie') + ' that the production fills.'],
      ['+0x008', 'Episode blocks', '10 × 35 bytes', 'Per episode: +0 base quality, +1 unknown, +2 cost multiplier (set to the length when a shopping list is made), +3 unknown, +4.. ten slots of (type 1–15 role / 16+ set, weight, choice), +34 unknown.'],
      ['+0x166', 'Studio hours', String(x.duration), 'Production time in hours, 0 = 10.'],
      ['+0x168', 'Role needs', '15 × 4 bytes', 'Humor, (unused), appearance, skill an actor should have for role 1–15; compared by script_role_match_pct 1DA8:2560.'],
      ['+0x1A4', 'Role categories', x.rcat.replace(/(..)/g, '$1 ').trim(), 'Actor category per role: 1 actor, 2 actress, 3 child, 4 alien, 5 MC, 6 singer, 7 star guest.']
    ];
    var recHtml = rec.map(function (r) { return '<div class="f"><code class="off">' + r[0] + '</code><span class="fname">' + esc(r[1]) + '</span><span class="fval">' + esc(r[2]) + '</span><span class="fdoc">' + esc(r[3]) + '</span></div>'; }).join('');
    var blocks = x.blocks.filter(function (b) { return !b.copy; }).map(function (b) {
      return '<div class="f"><code class="off">ep ' + b.e + '</code><span class="fname">Block</span><span class="fval">' + esc(b.hex.replace(/(..)/g, '$1 ').trim()) + '</span><span class="fdoc">base ' + b.base + ', +1 = ' + b.b1 + ', multiplier ' + b.mult + ', +3 = ' + b.b3 + ', +34 = ' + b.b34 + '</span></div>';
    }).join('');
    return '<div class="detail" id="d-scripts-' + x.id + '"><div class="d-grid"><div class="d-block"><p class="d-kicker">Screenplay ' + x.id + ' · ' + esc(kindLabel(x)) + '</p><h3>' + esc(scriptTitle(x)) + '</h3>' +
      '<p class="d-alt">' + langName(o) + ': ' + esc(x.ptitle ? (x.ptitle[o][0] || '—') : '—') + '</p>' +
      '<p class="d-desc">' + esc(x.t[lang][0] || '—') + '</p>' + (x.t[o][0] ? '<p class="d-desc-alt">' + esc(x.t[o][0]) + '</p>' : '') + '</div>' +
      '<div class="d-side"><div class="d-block"><h4>Rules</h4><ul class="facts">' + facts.map(function (t) { return '<li>' + esc(t) + '</li>'; }).join('') + '</ul></div></div></div>' +
      '<div class="d-block"><h4>Roles (' + x.roles.length + ')</h4><div class="tblwrap"><table class="kv"><thead><tr><th scope="col" class="num">#</th><th scope="col">Role</th><th scope="col">Category</th><th scope="col" class="num">Needs ' + esc(C.attrs[lang].join(' / ')) + '</th><th scope="col">Best matches in the pool (match, fee per episode)</th></tr></thead><tbody>' + (roles || '<tr class="na"><td colspan="5">No roles.</td></tr>') + '</tbody></table></div>' +
      '<p class="d-alt" style="font-size:12.5px">Match = Σ min(need, actor value) / Σ need over humor, appearance and skill. The value in brackets is the fourth byte, which the match skips. Star guests change every day; the list shows every actor who can be one.</p></div>' +
      '<div class="d-block"><h4>Episodes: cast and sets</h4><div class="tblwrap"><table class="kv"><thead><tr><th scope="col" class="num">Ep.</th><th scope="col" class="num">Base</th><th scope="col">Slots with weight</th><th scope="col" class="num">Best cast</th><th scope="col" class="num">Perfect</th><th scope="col" class="num">Cost (best cast, best sets)</th></tr></thead><tbody>' + eps + '</tbody></table></div>' +
      '<p class="d-alt" style="font-size:12.5px">"Best cast": the best match in the pool for every role and the best set level; "Perfect": every role matched 100 %. Cost = fees and set prices × length.</p></div>' +
      '<div class="d-block"><h4>Record: 435 bytes (resource 0xCB, entry ' + x.id + ')</h4><div class="fields">' + recHtml + blocks + '</div><p class="rawhex">Role needs: ' + esc(x.req.replace(/(..)/g, '$1 ').trim()) + '</p></div></div>';
  }
  SECTIONS.scripts = {
    rows: D.scripts, noun: ['screenplay', 'screenplays'], detail: scriptDetail,
    sort: { key: 'id', dir: 1 },
    hay: function (x) {
      var parts = [String(x.id), String(x.target)];
      ['en', 'de'].forEach(function (l) { parts.push(x.t[l].join(' ')); if (x.ptitle) parts.push(x.ptitle[l][0]); });
      return parts.join(' \u0001 ').toLowerCase();
    },
    cols: [
      { k: 'id', label: '#', cls: 'id', num: true, v: function (x) { return x.id; }, c: function (x) { return String(x.id).padStart(2, '0'); } },
      { k: 'title', label: 'Produces', cls: 'title', v: function (x) { return scriptTitle(x).toLowerCase(); },
        c: function (x) { return '<button type="button" class="open-btn" aria-expanded="false" aria-controls="d-scripts-' + x.id + '">' + esc(scriptTitle(x)) + '</button><span class="sub">' + (x.series ? 'Series ' : 'Movie ') + x.target + '</span>'; } },
      { k: 'kind', label: 'Kind', v: function (x) { return x.kind; }, c: function (x) { return esc(kindLabel(x)); } },
      { k: 'episodes', label: 'Episodes', num: true, v: function (x) { return x.episodes; }, c: function (x) { return x.episodes; } },
      { k: 'price', label: 'Price', num: true, v: function (x) { return x.price; }, c: function (x) { return money(x.price); } },
      { k: 'offered', label: 'Agency', v: function (x) { return x.offered ? 0 : 1; }, c: function (x) { return x.offered ? chip('Offered', 'chip-go') : '<span class="dim">never</span>'; } },
      { k: 'roles', label: 'Roles', num: true, v: function (x) { return x.roles.length; }, c: function (x) { return x.roles.length; } },
      { k: 'duration', label: 'Studio h', title: 'Production time in hours', num: true, v: function (x) { return x.duration || 10; }, c: function (x) { return x.duration || 10; } },
      { k: 'base', label: 'Base', title: 'Base quality of episode 1', num: true, v: function (x) { return x.base1; }, c: function (x) { return x.base1; } },
      { k: 'bestQ', label: 'Best cast', title: 'Quality with the best match in the pool for every role (best episode)', num: true, v: function (x) { return x.bestQ; }, c: function (x) { return x.bestQ; } },
      { k: 'perfectQ', label: 'Perfect', title: 'Quality with every role matched 100 % (best episode)', num: true, v: function (x) { return x.perfectQ; }, c: function (x) { return x.perfectQ; } }
    ],
    filters: [
      { id: 'kind', label: 'Kind', opts: function () { return [['', 'All'], ['show', 'Shows'], ['series', 'Series'], ['film', 'Movies']]; }, test: function (x, v) { return x.kind === v; } },
      { id: 'agency', label: 'Agency', opts: function () { return [['', 'All'], ['yes', 'Offered'], ['no', 'Never offered']]; }, test: function (x, v) { return v === 'yes' ? x.offered : !x.offered; } }
    ]
  };
  function scriptExtras() {
    var actors = C.actors.map(function (a) {
      return '<tr><td>' + esc(castCat(a.cat)) + '</td><td class="num">' + a.slot + '</td><td>' + esc(a.name) + (a.phrase ? '<br><span class="dim">' + esc(a.phrase) + '</span>' : '') + '</td><td class="num">' + money(a.fee) + '</td><td class="num">' + a.humor + '</td><td class="num">' + a.look + '</td><td class="num">' + a.skill + '</td><td class="num dim">' + a.a7 + '</td></tr>';
    }).join('');
    var props = C.props.map(function (p) {
      return '<tr><th scope="row" class="num">' + p.type + '</th><td>' + esc(p.name[lang]) + '</td>' + [1, 2, 3].map(function (lv) { return '<td class="num">' + money(p.price[lv]) + ' <span class="pv">q ' + p.q[lv] + '</span></td>'; }).join('') + '<td class="num dim">' + money(p.price[0]) + ' / ' + p.q[0] + '</td></tr>';
    }).join('');
    var lv = C.levels[lang] || C.levels.en;
    return '<section aria-labelledby="sx-how"><h3 id="sx-how">How a production works</h3>' +
      '<p>Buy a screenplay at the screenplay agency, rent a studio at the estate agent, then make one shopping list per episode (at most five open lists) and buy its cast and sets in the shop. Starting the shoot computes the quality right away (studio_start_production 229E:0B98): each role adds its weight × the actor’s match / 100, each set adds the smaller of its weight and the set quality, and quality = base × sum / sum of weights × 125 / 100. Actors refuse a station with an image below 10, below 15 if their fee is over $5,000, below 20 if it is over $10,000.</p></section>' +
      '<section aria-labelledby="sx-cast"><h3 id="sx-cast">Cast pool (' + C.actors.length + ' entries)</h3><p>From resource 0xD0 (names from resource 0x2A, which has them in one language only). The same person can appear in several categories with other values. Star guests (category 7) are drawn every day: slot n takes actor n of a random category among 1–4 and 6 (actors_daily_offer 1DA8:1451). ' + (C.actors.some(function (a) { return a.cat === 7; }) ? 'The star guests below are the start savegame’s; a new game draws new ones at once.' : 'The start savegame has no star guests; a new game draws them at once.') + '</p>' +
      '<div class="scroller"><table class="mini"><thead><tr><th scope="col">Category</th><th scope="col" class="num">Slot</th><th scope="col">Name</th><th scope="col" class="num">Fee</th><th scope="col" class="num">' + esc(C.attrs[lang][0]) + '</th><th scope="col" class="num">' + esc(C.attrs[lang][1]) + '</th><th scope="col" class="num">' + esc(C.attrs[lang][2]) + '</th><th scope="col" class="num">+7 (unused)</th></tr></thead><tbody>' + actors + '</tbody></table></div></section>' +
      '<section aria-labelledby="sx-props"><h3 id="sx-props">Sets and props</h3><p>Price per production (× programme length) and set quality by level, from resource 0xD0 (DS:6235, DS:268E). Level 0 is not sold.</p>' +
      '<div class="scroller"><table class="mini"><thead><tr><th scope="col" class="num">Type</th><th scope="col">Set</th><th scope="col" class="num">' + esc(lv[1]) + '</th><th scope="col" class="num">' + esc(lv[2]) + '</th><th scope="col" class="num">' + esc(lv[3]) + '</th><th scope="col" class="num">Level 0</th></tr></thead><tbody>' + props + '</tbody></table></div></section>';
  }
  var EXTRAS = { news: newsExtras, scripts: scriptExtras };
  function renderExtras(name) {
    var el = document.getElementById('ex-' + name);
    if (el && EXTRAS[name]) el.innerHTML = EXTRAS[name]();
  }

  var state = {};
  Object.keys(SECTIONS).forEach(function (name) {
    var S = SECTIONS[name];
    state[name] = { q: '', f: {}, sort: S.sort.key, dir: S.sort.dir, open: {}, built: false };
    S.rows.forEach(function (r) {
      var extra = '';
      if (name === 'ads') extra = (r.cond.en || '') + ' ' + (r.cond.de || '');
      r._hay = S.hay ? S.hay(r) : searchHay(r, extra);
    });
  });

  function buildToolbar(name) {
    var S = SECTIONS[name], st = state[name];
    var tb = document.getElementById('tb-' + name);
    var h = '<label class="fld fld-search" for="q-' + name + '"><span>Search</span><input type="search" id="q-' + name + '" autocomplete="off" placeholder="Title, text or number" value="' + esc(st.q) + '"></label>';
    S.filters.forEach(function (fl) {
      var id = 'f-' + name + '-' + fl.id;
      h += '<label class="fld" for="' + id + '"><span>' + esc(fl.label) + '</span><select id="' + id + '">' +
        fl.opts().map(function (o) { return '<option value="' + esc(o[0]) + '"' + ((st.f[fl.id] || '') === o[0] ? ' selected' : '') + '>' + esc(o[1]) + '</option>'; }).join('') +
        '</select></label>';
    });
    h += '<button type="button" class="reset" id="reset-' + name + '">Clear filters</button>';
    tb.innerHTML = h;
    document.getElementById('q-' + name).addEventListener('input', function (e) { st.q = e.target.value; renderBody(name); });
    S.filters.forEach(function (fl) {
      document.getElementById('f-' + name + '-' + fl.id).addEventListener('change', function (e) { st.f[fl.id] = e.target.value; renderBody(name); });
    });
    document.getElementById('reset-' + name).addEventListener('click', function () { st.q = ''; st.f = {}; buildToolbar(name); renderBody(name); });
  }
  function buildHead(name) {
    var S = SECTIONS[name], st = state[name];
    var thead = document.querySelector('#tbl-' + name + ' thead');
    thead.innerHTML = '<tr>' + S.cols.map(function (c) {
      var sort = st.sort === c.k ? (st.dir > 0 ? 'ascending' : 'descending') : 'none';
      return '<th scope="col" class="' + (c.num ? 'num' : '') + '" aria-sort="' + sort + '"><button type="button" data-sort="' + c.k + '"' + (c.title ? ' title="' + esc(c.title) + '"' : '') + '>' + (c.head || esc(c.label)) + '</button></th>';
    }).join('') + '</tr>';
  }
  function filtered(name) {
    var S = SECTIONS[name], st = state[name];
    var q = st.q.trim().toLowerCase();
    var words = q ? q.split(/\s+/) : [];
    var rows = S.rows.filter(function (r) {
      for (var i = 0; i < words.length; i++) if (r._hay.indexOf(words[i]) < 0) return false;
      for (var k = 0; k < S.filters.length; k++) {
        var fl = S.filters[k], v = st.f[fl.id];
        if (v && !fl.test(r, v)) return false;
      }
      return true;
    });
    var col = S.cols.filter(function (c) { return c.k === st.sort; })[0] || S.cols[0];
    rows.sort(function (a, b) {
      var x = col.v(a), y = col.v(b);
      if (x == null) x = -Infinity;
      if (y == null) y = -Infinity;
      if (x < y) return -st.dir;
      if (x > y) return st.dir;
      return a.id - b.id;
    });
    return rows;
  }
  function renderBody(name) {
    var S = SECTIONS[name], st = state[name];
    var rows = filtered(name);
    var n = S.cols.length;
    var html = rows.map(function (r) {
      var open = !!st.open[r.id];
      var tr = '<tr class="row' + (open ? ' open' : '') + '" data-id="' + r.id + '">' + S.cols.map(function (c) {
        return '<td class="' + (c.num ? 'num ' : '') + (c.cls || '') + '">' + c.c(r) + '</td>';
      }).join('') + '</tr>';
      if (open) tr += '<tr class="drow"><td colspan="' + n + '">' + S.detail(r) + '</td></tr>';
      return tr;
    }).join('');
    var tbody = document.querySelector('#tbl-' + name + ' tbody');
    tbody.innerHTML = html || '<tr><td colspan="' + n + '" class="dim">Nothing matches these filters.</td></tr>';
    tbody.querySelectorAll('tr.row.open .open-btn').forEach(function (b) { b.setAttribute('aria-expanded', 'true'); });
    var total = S.rows.length;
    var noun = total === 1 ? S.noun[0] : S.noun[1];
    document.getElementById('cnt-' + name).textContent = rows.length === total ? total + ' ' + noun : rows.length + ' of ' + total + ' ' + noun;
  }
  function toggle(name, id) {
    var st = state[name];
    if (st.open[id]) delete st.open[id]; else st.open[id] = true;
    renderBody(name);
    var btn = document.querySelector('#tbl-' + name + ' tr.row[data-id="' + id + '"] .open-btn');
    if (btn) btn.focus({ preventScroll: true });
  }
  function wire(name) {
    var table = document.getElementById('tbl-' + name);
    table.querySelector('thead').addEventListener('click', function (e) {
      var b = e.target.closest('button[data-sort]');
      if (!b) return;
      var st = state[name], k = b.getAttribute('data-sort');
      if (st.sort === k) st.dir = -st.dir; else { st.sort = k; st.dir = (k === 'title' || k === 'name' || k === 'id' || k === 'genre') ? 1 : -1; }
      buildHead(name);
      renderBody(name);
    });
    table.querySelector('tbody').addEventListener('click', function (e) {
      var tr = e.target.closest('tr.row');
      if (!tr) return;
      if (e.target.closest('.detail')) return;
      var sel = window.getSelection && window.getSelection();
      if (sel && String(sel).length > 0 && !e.target.closest('.open-btn')) return;
      toggle(name, +tr.getAttribute('data-id'));
    });
  }
  function ensure(name) {
    if (!SECTIONS[name] || state[name].built) return;
    buildToolbar(name); buildHead(name); renderBody(name); wire(name); renderExtras(name);
    state[name].built = true;
  }

  // ------------------------------------------------------------ tabs, language
  var NAMES = ['movies', 'series', 'ads', 'news', 'scripts', 'notes'];
  function show(name) {
    if (NAMES.indexOf(name) < 0) name = 'movies';
    NAMES.forEach(function (n) {
      document.getElementById('sec-' + n).hidden = n !== name;
      var t = document.getElementById('tab-' + n);
      if (n === name) t.setAttribute('aria-current', 'page'); else t.removeAttribute('aria-current');
    });
    ensure(name);
  }
  function fromHash() {
    var h = (location.hash || '').replace(/^#/, '');
    if (h.indexOf('n-') === 0) { show('notes'); var el = document.getElementById(h); if (el) el.scrollIntoView(); return; }
    if (h.indexOf('nx-') === 0 || h.indexOf('sx-') === 0) { show(h.charAt(0) === 'n' ? 'news' : 'scripts'); var el2 = document.getElementById(h); if (el2) el2.scrollIntoView(); return; }
    show(h || 'movies');
  }
  window.addEventListener('hashchange', fromHash);
  function setLang(l) {
    lang = l;
    try { localStorage.setItem('madtv-catalog-lang', l); } catch (e) { /* storage unavailable */ }
    document.getElementById('lang-en').setAttribute('aria-pressed', String(l === 'en'));
    document.getElementById('lang-de').setAttribute('aria-pressed', String(l === 'de'));
    Object.keys(SECTIONS).forEach(function (n) { if (state[n].built) { buildToolbar(n); buildHead(n); renderBody(n); renderExtras(n); } });
  }
  document.getElementById('lang-en').addEventListener('click', function () { setLang('en'); });
  document.getElementById('lang-de').addEventListener('click', function () { setLang('de'); });
  document.getElementById('tc-movies').textContent = D.films.length;
  document.getElementById('tc-series').textContent = D.series.length;
  document.getElementById('tc-ads').textContent = D.ads.length;
  document.getElementById('tc-news').textContent = D.news.stories.length;
  document.getElementById('tc-scripts').textContent = D.scripts.length;
  setLang(lang);
  fromHash();
})();
</script>
'''


def main():
    argv = sys.argv[1:]
    binary = os.path.join(ROOT, 'build', 'mtv')
    do_verify = os.path.exists(binary) and os.path.exists(os.path.join(ROOT, 'build', 'MTV_U.EXE'))
    args = []
    k = 0
    while k < len(argv):
        o = argv[k]
        if o == '--no-verify':
            do_verify = False
        elif o == '--verify':
            do_verify = True
            if k + 1 < len(argv) and not argv[k + 1].startswith('-') and not argv[k + 1].lower().endswith(('.html', '.rsc')):
                binary = os.path.abspath(argv[k + 1])
                k += 1
        elif o in ('-h', '--help'):
            args = []
            break
        else:
            args.append(o)
        k += 1
    if len(args) not in (1, 3):
        print(__doc__)
        sys.exit(2)
    out = args[0]
    if len(args) == 3:
        paths = args[1:3]
    else:
        paths = [os.path.join(ROOT, '..', 'SOURCE1.RSC'), os.path.join(ROOT, '..', 'SOURCE2.RSC')]
    paths = [os.path.abspath(p) for p in paths]
    data, genre_aud = build(paths)
    if do_verify:
        try:
            data['meta']['verify'] = verify(binary, paths, data, genre_aud)
        except Exception as e:      # the page is still useful without the check
            data['meta']['verify'] = dict(error='%s: %s' % (type(e).__name__, e))
    else:
        data['meta']['verify'] = None
    d = os.path.dirname(os.path.abspath(out))
    if not os.path.isdir(d):
        os.makedirs(d)
    size = render(data, out)
    v = data['meta']['verify']
    if not v:
        ver = ''
    elif v.get('error'):
        ver = ', verify failed: ' + v['error']
    else:
        ver = ', verify: %d comparisons, %d differ' % (v['total'], v['failed'])
    print('%s: %d movies, %d series, %d ads, %d bytes%s' % (out, len(data['films']), len(data['series']),
                                                          len(data['ads']), size, ver))


if __name__ == '__main__':
    main()
