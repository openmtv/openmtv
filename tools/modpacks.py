#!/usr/bin/env python3
"""Mods played together as packs (the former Python editor (editor.py), ./play.sh --mod A+B+...).

A mod is a folder of complete tables (mods/NAME/*.json; the former Python editor (editor.py)
fills a table it leaves out from the original).  Played together,
the packs are laid over the original game's data in their order:

  * an original entry a pack changes takes that pack's version; where two
    packs change the same entry differently, the later pack's version counts
    (reported as a conflict);
  * the entries a pack adds -- films, series and ads beyond the original's
    200, 50 and 50 -- are appended: the first pack keeps its numbers, every
    later pack's new entries follow the highest number so far, in their own
    order.  No pack occupies fixed numbers, so packs that each start at
    film 201 combine;
  * a screenplay a pack changes that turns into one of that pack's new films
    or series (its "target") follows the new number.

One pack alone is played exactly as saved.  Standard library only.

    combine(original, [(name, docs), ...]) -> (docs, report)
"""
import copy

SEP = '+'                                  # between the names: build/mods/A+B, --mod A+B
GROWING = ('films', 'series', 'ads')       # tables a pack can add entries to
LIMIT = 999                                # their highest number (content.py)


def combine(original, packs):
    """original: {table: doc} (the game's data); packs: [(name, {table: doc})].
    Returns the tables of the packs played together and a report:
      {'packs': [{'name', 'added': {table: [first, last, n]}, 'moved': {table: n}}],
       'conflicts': [{'table', 'id', 'packs': [names], 'wins': name}]}"""
    docs = {t: copy.deepcopy(d) for t, d in original.items()}
    orig = {t: {r['id']: r for r in d['records']} for t, d in original.items()}
    result = {t: {r['id']: r for r in d['records']} for t, d in docs.items()}
    changed_by = {t: {} for t in docs}           # table -> id -> [(pack, record)]
    report = {'packs': [], 'conflicts': []}
    for k, (name, pdocs) in enumerate(packs):
        mapping, added, moved = {}, {}, {}
        # the new entries first: their numbers in the combination
        for t in GROWING:
            if t not in pdocs:
                continue
            new = sorted((r for r in pdocs[t]['records'] if r['id'] not in orig[t]), key=lambda r: r['id'])
            mapping[t] = {}
            for r in new:
                nid = r['id'] if k == 0 else max(result[t]) + 1
                if nid in result[t] and k == 0:
                    raise ValueError('pack %s: %s #%d twice' % (name, t, r['id']))
                if nid > LIMIT:
                    raise ValueError('%s: more than %d with pack %s' % (t, LIMIT, name))
                mapping[t][r['id']] = nid
                rec = copy.deepcopy(r)
                rec['id'] = nid
                result[t][nid] = rec
            if new:
                ids = [mapping[t][r['id']] for r in new]
                added[t] = [min(ids), max(ids), len(ids)]
                n = sum(1 for r in new if mapping[t][r['id']] != r['id'])
                if n:
                    moved[t] = n
        # then the changes of original entries
        for t, pdoc in pdocs.items():
            if t not in orig:
                continue
            for r in pdoc['records']:
                o = orig[t].get(r['id'])
                if o is None or r == o:
                    continue
                rec = copy.deepcopy(r)
                if t == 'screenplays' and rec.get('target'):
                    kind = 'series' if rec.get('flags', 0) & 1 else 'films'
                    rec['target'] = mapping.get(kind, {}).get(rec['target'], rec['target'])
                changed_by[t].setdefault(r['id'], []).append((name, rec))
                result[t][r['id']] = rec
        report['packs'].append({'name': name, 'added': added, 'moved': moved})
    for t, ids in changed_by.items():
        for i, versions in sorted(ids.items()):
            if len(versions) > 1 and any(v != versions[-1][1] for _, v in versions[:-1]):
                report['conflicts'].append({'table': t, 'id': i, 'packs': [n for n, _ in versions], 'wins': versions[-1][0]})
    for t in docs:
        docs[t]['records'] = [result[t][i] for i in sorted(result[t])]
    return docs, report
