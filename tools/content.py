#!/usr/bin/env python3
"""Export the game's content tables to JSON and write them back into the .rsc
files -- the original engine then plays with the new content.

    content.py export OUTDIR SOURCE1.RSC SOURCE2.RSC     tables -> OUTDIR/*.json
    content.py import INDIR SOURCE1.RSC SOURCE2.RSC OUTDIR   JSON -> OUTDIR/SOURCE1.RSC (+ SOURCE2 copy)
    content.py check SOURCE1.RSC SOURCE2.RSC             export + import in memory, must be byte-identical

Tables (all loaded raw by data_load_tables 1C18:000C and the text caches
in segment 048C):

    films        res 0xC9, 201 x 0x17   texts 0xCF (de) / 0xFC (en): title, description
                 (up to 1000 records in a mod: the decompiled game counts them)
    series       res 0xCA,  51 x 0x39   texts 0x2F / 0xFB: title, description, 10 x (episode title, text)
                 (up to 1000 records in a mod, like the films)
    screenplays  res 0xCB,  51 x 0x1B3  texts 0x2D / 0xFA: 16 strings
    ads          res 0xCC,  51 x 0x18   names 0x2E / 0xF9: advertiser, client
                 (up to 1000 records in a mod)
    news_nodes   res 0xCD, 401 x 0xA    texts 0x31 / 0xF8: headline template ('#n' = phrase of part n)
    news_topics  res 0xCE,  51 x 0x38   phrases 0x2C / 0xF7: 10 per topic
    news_templates res 0x30, 51 x 0xE

Record 0 of every table is unused by the game.  Field names follow
names/symbols.txt; bytes not covered by a named field are kept in
"unknown" (offset -> hex) so that import reproduces them exactly.  Texts are
code page 437.  Import keeps the file layout and appends changed resources
at the end (the index entry is redirected), so an unchanged export imports
to an identical file.

The game starts from a savegame (resource 0x2B, loaded by main): the
fields a savegame stores per film/series/screenplay/ad (quality, topicality,
owner, market flags, price, episodes, next episode, live date, holder) are
written there too, so changes take effect in a new game.

Limits of the original engine: the record counts above are fixed (tables
are DGROUP arrays or fixed allocations); resources are at most 65535 bytes.
Films, series and ads are the exception: the decompiled game
(build/mtv-dec) takes their counts from the size of their tables, up to 999
films (ids below 1000, where the series begin), 999 series (programme ids
1000 + n below 2000) and 999 ads (the schedule marks aired spots with
+1000/+2000/+3000), as long as each language's texts of a table fit in
64 KB; a savegame appends films 201.., series 51.. and ads 51.. at its end,
and it must stay below 64 KB (SAVE_MAX: 18 bytes a film past 200, 41 a
series past 50, 1 an ad past 50 -- 999 films and 50 ads leave room for 374
series).
The start savegame keeps the original counts; the game takes the others of
a new game from the tables.
"""
import sys, os, json, struct

ENC = 'cp437'
FT = {'u8': 'B', 'i8': 'b', 'u16': 'H', 'i16': 'h', 'u32': 'I', 'i32': 'i'}
LANGS = ('de', 'en')

# field: (name, offset, type[, count])
TABLES = {
    'films': dict(res=0xC9, size=0x17, count=201, max=1000, texts=dict(kind='pstr', de=0xCF, en=0xFC, n=2), fields=[
        ('genre', 0x0, 'u8'), ('quality', 0x1, 'u8'), ('wear_rate', 0x2, 'u8'), ('topicality', 0x3, 'u8'),
        ('blocks', 0x4, 'u8'), ('owner', 0x5, 'u8'), ('flags', 0x6, 'u8'), ('flags2', 0x7, 'u8'),
        ('price', 0x8, 'i32'), ('jingle', 0xC, 'u16'), ('live_day', 0xF, 'u16'), ('live_slot', 0x11, 'u16')]),
    'series': dict(res=0xCA, size=0x39, count=51, max=1000, texts=dict(kind='pstr', de=0x2F, en=0xFB, n=22), fields=[
        ('episodes_qtf', 0x0, 'u8', 33),          # episode e at 3e: quality, topicality, flags (bit0 exists)
        ('genre', 0x21, 'u8'), ('episodes', 0x22, 'u8'), ('blocks', 0x23, 'u8'), ('owner', 0x24, 'u8'),
        ('flags', 0x25, 'u8'), ('flags2', 0x26, 'u8'), ('price', 0x27, 'i32'), ('episode_price', 0x2B, 'i32'),
        ('jingle', 0x2F, 'u16'), ('next_episode', 0x31, 'u16'), ('wear_rate', 0x33, 'u8'),
        ('critics', 0x35, 'u8'), ('press', 0x36, 'u8')]),
    'screenplays': dict(res=0xCB, size=0x1B3, count=51, texts=dict(kind='pstr', de=0x2D, en=0xFA, n=16), fields=[
        ('price', 0x0, 'i32'), ('state', 0x4, 'u8'), ('flags', 0x5, 'u8'), ('target', 0x6, 'u16'),
        ('duration_hours', 0x166, 'u16')]),
    'ads': dict(res=0xCC, size=0x18, count=51, max=1000, texts=dict(kind='lens', de=0x2E, en=0xF9, sections=2), fields=[
        ('days', 0x8, 'u8'), ('spots', 0x9, 'u8'), ('min_audience_class', 0xA, 'u8'), ('target_group', 0xB, 'u8'),
        ('genre_rule', 0xC, 'i8', 3), ('holder', 0xF, 'u8'), ('fee', 0x10, 'i32'), ('penalty', 0x14, 'i32')]),
    'news_nodes': dict(res=0xCD, size=0xA, count=401, texts=dict(kind='raw', de=0x31, en=0xF8), fields=[
        ('successors', 0x0, 'u16', 4), ('value', 0x8, 'u8'), ('delay_hours', 0x9, 'u8')]),
    'news_topics': dict(res=0xCE, size=0x38, count=51, texts=dict(kind='lens', de=0x2C, en=0xF7, per=10), fields=[
        ('phrase_value', 0x2D, 'u8', 10)]),
    'news_templates': dict(res=0x30, size=0xE, count=51, fields=[
        ('parts', 0x0, 'u16', 5), ('start_node', 0xA, 'u16'), ('category', 0xD, 'u8')]),
}


PHRASE_SLOTS = 561          # length table of the phrase resources (0x462 bytes)

# A savegame (game_save_load 229E:139A, src/actions.c) is the original's 37 605 bytes and,
# with more than 200 films, 50 series or 50 ads or a game option that needs it, an
# extension (savegame_extra_size): "MTVX" and up to 6 u16 counts, films 201.. (0x12 bytes
# each), series 51.. (0x29), ads 51.. (1), the computer players' state (--ai-improve: 86),
# the ad agency's offers 11..17 (7 each) and the film dealer's 11..17 (8 each).  Its length
# is 16 bits, and the game addresses it through one far pointer: at most SAVE_MAX bytes.
SAVE_BASE = 37605
SAVE_MAX = 0xFFF0
SAVE_OPTIONS = 6 + 2 * 6 + (2 + 3 * 28) + 7 * 7 + 7 * 8   # the head, every option at its largest


def savegame_size(nfilms, nseries, nads):
    """the largest savegame of a game with these counts (every option on)"""
    return (SAVE_BASE + SAVE_OPTIONS + max(nfilms - 200, 0) * 0x12 + max(nseries - 50, 0) * 0x29
            + max(nads - 50, 0))


def phrase_extra_slots(t):
    used = set()
    for i in range(t['count']):
        used.update(range(i * t['texts']['per'] + 1, i * t['texts']['per'] + 1 + t['texts']['per']))
    return [k for k in range(PHRASE_SLOTS) if k not in used]


# ---------------------------------------------------------------- RSC files
def read_rsc(path):
    d = open(path, 'rb').read()
    idx = {}
    for i in range(600):
        off, size = struct.unpack_from('<IH', d, 6 * i)
        if off:
            idx[i] = (off, size)
    return d, idx


class Resources:
    def __init__(self, paths):
        self.files = [read_rsc(p) for p in paths]

    def get(self, i):
        for d, idx in reversed(self.files):     # later files override
            if i in idx:
                off, size = idx[i]
                return d[off:off + size]
        raise KeyError('resource %d' % i)

    def where(self, i):
        for n in range(len(self.files) - 1, -1, -1):
            if i in self.files[n][1]:
                return n
        raise KeyError(i)


# ---------------------------------------------------------------- records
def covered(fields, size):
    cov = bytearray(size)
    for f in fields:
        n = struct.calcsize(FT[f[2]]) * (f[3] if len(f) > 3 else 1)
        for k in range(f[1], f[1] + n):
            cov[k] = 1
    return cov


def unpack_record(t, b):
    r = {}
    for f in t['fields']:
        name, off, typ = f[:3]
        if len(f) > 3:
            r[name] = list(struct.unpack_from('<%d%s' % (f[3], FT[typ]), b, off))
        else:
            r[name] = struct.unpack_from('<' + FT[typ], b, off)[0]
    cov = covered(t['fields'], t['size'])
    unk, k = {}, 0
    while k < t['size']:
        if cov[k]:
            k += 1
            continue
        s = k
        while k < t['size'] and not cov[k]:
            k += 1
        if any(b[s:k]):
            unk['0x%02X' % s] = b[s:k].hex()
    if unk:
        r['unknown'] = unk
    return r


def pack_record(t, r):
    b = bytearray(t['size'])
    for off, hx in r.get('unknown', {}).items():
        v = bytes.fromhex(hx)
        o = int(off, 16)
        b[o:o + len(v)] = v
    for f in t['fields']:
        name, off, typ = f[:3]
        if name not in r:
            continue
        if len(f) > 3:
            vals = r[name]
            if len(vals) != f[3]:
                raise ValueError('%s: %d values expected' % (name, f[3]))
            struct.pack_into('<%d%s' % (f[3], FT[typ]), b, off, *vals)
        else:
            struct.pack_into('<' + FT[typ], b, off, r[name])
    return bytes(b)


# ---------------------------------------------------------------- texts
def parse_pstrs(blk):
    out, i = [], 0
    while i < len(blk):
        n = blk[i]
        out.append(blk[i + 1:i + 1 + n].decode(ENC))
        i += 1 + n
    return out


def build_pstrs(strs):
    b = bytearray()
    for s in strs:
        e = s.encode(ENC)
        if len(e) > 255:
            raise ValueError('text longer than 255 bytes: %r' % s[:40])
        b.append(len(e))
        b += e
    return bytes(b)


def parse_indexed(d, count, kind):
    ix = struct.unpack_from('<%dH' % count, d, 0)
    out = []
    for e in range(1, count):
        blk = d[ix[e - 1]:ix[e]]
        if kind == 'pstr':
            strs = parse_pstrs(blk)
            # entries that are not clean strings (film 200 in the original) stay raw
            out.append(strs if build_pstrs(strs) == blk else {'raw': blk.hex()})
        else:
            out.append(blk.decode(ENC))
    return out, d[ix[-1]:]


def build_indexed(entries, kind, tail=b''):
    count = len(entries) + 1
    blobs = [bytes.fromhex(e['raw']) if isinstance(e, dict) else build_pstrs(e) if kind == 'pstr' else e.encode(ENC)
             for e in entries]
    pos = 2 * count
    ix = [pos]
    for bl in blobs:
        pos += len(bl)
        ix.append(pos)
    d = struct.pack('<%dH' % count, *ix) + b''.join(blobs) + tail
    if len(d) > 0xFFFF:
        raise ValueError('text resource larger than 64 KB')
    return d


def parse_lens(d, nlen, sections):
    out, pos = [], 0
    for _ in range(sections):
        lens = struct.unpack_from('<%dH' % nlen, d, pos)
        pos += 2 * nlen
        sec = []
        for l in lens:
            sec.append(d[pos:pos + l].decode(ENC))
            pos += l
        out.append(sec)
    return out, d[pos:]


def build_lens(secs, tail=b''):
    b = bytearray()
    for sec in secs:
        enc = [s.encode(ENC) for s in sec]
        b += struct.pack('<%dH' % len(enc), *[len(e) for e in enc])
        for e in enc:
            b += e
    return bytes(b) + tail


# ---------------------------------------------------------------- export / import
def export(res):
    out = {}
    for name, t in TABLES.items():
        d = res.get(t['res'])
        n = len(d) // t['size'] if t.get('max') else t['count']     # films: as many as the table holds
        if len(d) != t['size'] * n or not t['count'] <= n <= t.get('max', t['count']):
            raise ValueError('%s: resource size %d, expected %d' % (name, len(d), t['size'] * t['count']))
        recs = [unpack_record(t, d[i * t['size']:(i + 1) * t['size']]) for i in range(n)]
        tails, extras = {}, {}
        tx = t.get('texts')
        if tx:
            for lang in LANGS:
                td = res.get(tx[lang])
                if tx['kind'] in ('pstr', 'raw'):
                    ents, tail = parse_indexed(td, n, tx['kind'])
                    for i, e in enumerate(ents, 1):
                        recs[i].setdefault('text', {})[lang] = e
                elif 'sections' in tx:            # ads: advertiser and client, one entry per record each
                    secs, tail = parse_lens(td, n, tx['sections'])
                    for i in range(n):
                        recs[i].setdefault('text', {})[lang] = [secs[0][i], secs[1][i]]
                else:                             # news phrases: 561 lengths, phrase j of topic t at t*10 + j
                    secs, tail = parse_lens(td, PHRASE_SLOTS, 1)
                    flat = secs[0]
                    for i in range(t['count']):
                        recs[i].setdefault('text', {})[lang] = flat[i * tx['per'] + 1:i * tx['per'] + 1 + tx['per']]
                    extra = {str(k): flat[k] for k in phrase_extra_slots(t) if flat[k]}
                    if extra:
                        extras.setdefault(lang, {}).update(extra)
                if tail:
                    tails[lang] = tail.hex()
        items = []
        for i, r in enumerate(recs):
            if i == 0 and not any(d[:t['size']]) and not any(_has_text(r)):
                continue
            items.append(dict(id=i, **r))
        doc = {'table': name, 'resource': t['res'], 'record_size': t['size'], 'count': n, 'records': items}
        if tails:
            doc['text_tail'] = tails
        if extras:
            doc['text_extra'] = extras
        out[name] = doc
    return out


def _has_text(r):
    for v in r.get('text', {}).values():
        if isinstance(v, (str, dict)):
            yield bool(v)
        else:
            yield any(v)


def build(docs):
    """JSON documents -> {resource id: bytes}"""
    out = {}
    for name, t in TABLES.items():
        doc = docs[name]
        recs = {r['id']: r for r in doc['records']}
        top = t.get('max', t['count'])
        if max(recs) >= top:
            raise ValueError('%s: id %d out of range (at most %d)' % (name, max(recs), top - 1))
        n = max(t['count'], max(recs) + 1) if t.get('max') else t['count']
        out[t['res']] = b''.join(pack_record(t, recs.get(i, {})) for i in range(n))
        tx = t.get('texts')
        if not tx:
            continue
        tails = doc.get('text_tail', {})
        for lang in LANGS:
            tail = bytes.fromhex(tails.get(lang, ''))
            def txt(i, default):
                return recs.get(i, {}).get('text', {}).get(lang, default)
            if tx['kind'] in ('pstr', 'raw'):
                try:
                    out[tx[lang]] = build_indexed([txt(i, [] if tx['kind'] == 'pstr' else '') for i in range(1, n)],
                                                  tx['kind'], tail)
                except ValueError:
                    raise ValueError('%s: the %s texts are larger than 64 KB (the game reads them as one resource)'
                                     % (name, {'de': 'German', 'en': 'English'}[lang]))
            elif 'sections' in tx:
                pairs = [txt(i, ['', '']) for i in range(n)]
                out[tx[lang]] = build_lens([[p[0] for p in pairs], [p[1] for p in pairs]], tail)
            else:
                flat = [''] * PHRASE_SLOTS
                for i in range(t['count']):
                    ph = txt(i, [''] * tx['per'])
                    if len(ph) != tx['per']:
                        raise ValueError('news topic %d: %d phrases expected' % (i, tx['per']))
                    flat[i * tx['per'] + 1:i * tx['per'] + 1 + tx['per']] = ph
                for k, v in doc.get('text_extra', {}).get(lang, {}).items():
                    flat[int(k)] = v
                out[tx[lang]] = build_lens([flat], tail)
    n = {name: len(out[t['res']]) // t['size'] - 1 for name, t in TABLES.items() if t.get('max')}
    size = savegame_size(n['films'], n['series'], n['ads'])
    if size > SAVE_MAX:
        raise ValueError('%d films, %d series and %d ads: a savegame would have %d bytes, at most %d '
                         '(each film past 200 takes 18, each series past 50 41, each ad past 50 1)'
                         % (n['films'], n['series'], n['ads'], size, SAVE_MAX))
    return out


START_SAVE = 0x2B           # main() loads this savegame at startup (game_load(0x2B))


def patch_start_save(save, films, series, scripts, ads):
    """Write the per-item fields that game_save_load (229E:139A) stores into
    the start savegame, from the packed tables.  Bytes the save routine does
    not set (stack leftovers of the original tool) are kept."""
    s = bytearray(save)

    def bits(flags, flags2):
        return ((flags >> 5) & 1) | (((flags >> 7) & 1) << 1) | ((flags2 & 1) << 2) | (((flags2 >> 1) & 1) << 3)
    o = 0
    for i in range(1, 201):
        t, b = films[i * 0x17:(i + 1) * 0x17], o + (i - 1) * 0x12
        s[b + 0], s[b + 1], s[b + 3] = t[1], t[3], t[5]
        s[b + 4] = (s[b + 4] & 0xF0) | bits(t[6], t[7])
        s[b + 5:b + 9] = t[8:12]
        s[b + 0xD] = t[0xE]
        s[b + 0xE:b + 0x12] = t[0xF:0x13]
    o += 200 * 0x12
    for i in range(1, 51):
        t, b = series[i * 0x39:(i + 1) * 0x39], o + (i - 1) * 0x29
        s[b:b + 0x21] = t[0:0x21]
        s[b + 0x21] = t[0x24]
        s[b + 0x22] = (s[b + 0x22] & 0xF0) | bits(t[0x25], t[0x26])
        s[b + 0x23:b + 0x27] = t[0x27:0x2B]
        s[b + 0x27:b + 0x29] = t[0x31:0x33]
    o += 50 * 0x29
    for i in range(1, 51):
        t, b = bytearray(scripts[i * 0x1B3:(i + 1) * 0x1B3]), o + (i - 1) * 0x161
        if not (t[5] & 1) and (t[5] & 4):      # script_init_episodes: block 0 -> 1..9
            for e in range(1, 10):
                t[8 + e * 0x23:8 + (e + 1) * 0x23] = t[8:8 + 0x23]
        s[b] = t[4]
        s[b + 1:b + 1 + 10 * 0x23] = t[8:8 + 10 * 0x23]
    o += 50 * 0x161
    for i in range(1, 51):
        s[o + i - 1] = ads[i * 0x18 + 0xF]
    return bytes(s)


def write_rsc(orig, changed):
    """original file bytes + {id: new bytes} -> new file bytes (append changed)"""
    d = bytearray(orig)
    for i, data in sorted(changed.items()):
        off, size = struct.unpack_from('<IH', d, 6 * i)
        if off and d[off:off + size] == data:
            continue
        if len(data) > 0xFFFF:
            raise ValueError('resource %d larger than 64 KB' % i)
        if off and len(data) <= size:
            d[off:off + len(data)] = data          # fits in place
            struct.pack_into('<IH', d, 6 * i, off, len(data))
        else:
            struct.pack_into('<IH', d, 6 * i, len(d), len(data))
            d += data
    return bytes(d)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'export':
        outdir, paths = sys.argv[2], sys.argv[3:]
        docs = export(Resources(paths))
        os.makedirs(outdir, exist_ok=True)
        for name, doc in docs.items():
            with open(os.path.join(outdir, name + '.json'), 'w', encoding='utf-8') as f:
                json.dump(doc, f, ensure_ascii=False, indent=1)
        print('exported %s -> %s' % (', '.join('%s %d' % (n, len(d['records'])) for n, d in docs.items()), outdir))
    elif cmd == 'import':
        indir, paths, outdir = sys.argv[2], sys.argv[3:-1], sys.argv[-1]
        docs = {n: json.load(open(os.path.join(indir, n + '.json'), encoding='utf-8')) for n in TABLES}
        res = Resources(paths)
        new = build(docs)
        new[START_SAVE] = patch_start_save(res.get(START_SAVE), new[0xC9], new[0xCA], new[0xCB], new[0xCC])
        byfile = {}
        for i, data in new.items():
            byfile.setdefault(res.where(i), {})[i] = data
        os.makedirs(outdir, exist_ok=True)
        for n, p in enumerate(paths):
            orig = open(p, 'rb').read()
            out = write_rsc(orig, byfile.get(n, {}))
            open(os.path.join(outdir, os.path.basename(p)), 'wb').write(out)
            print('%s: %s' % (os.path.basename(p), 'unchanged' if out == orig else '%d bytes (was %d)' % (len(out), len(orig))))
    elif cmd == 'check':
        paths = sys.argv[2:]
        res = Resources(paths)
        docs = json.loads(json.dumps(export(res), ensure_ascii=False))
        new = build(docs)
        new[START_SAVE] = patch_start_save(res.get(START_SAVE), new[0xC9], new[0xCA], new[0xCB], new[0xCC])
        bad = [i for i, data in new.items() if data != res.get(i)]
        print('%d resources rebuilt, %d differ%s' % (len(new), len(bad), (': ' + ' '.join('0x%X' % i for i in bad)) if bad else ''))
        sys.exit(1 if bad else 0)
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == '__main__':
    main()
