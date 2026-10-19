"""Fixes of the original's data (not its code), for the content tables of
tools/content.py.  the former Python editor (editor.py) starts from the tables with these applied
(its "original", every new mod); the game applies the same fixes itself while
it plays the original files with fixes on (src/fixes.c, same texts there --
keep both in step).  Each fix only replaces the original's damaged data, so
data a mod changed stays as it is.  docs/CHANGES.md lists them.
"""
import copy

# film 200: John Carpenter's "Prince of Darkness" (1987).  The English title is the
# placeholder "xxx"; the German text resource is cut off after "Die Fuersten der" (its
# length byte, 26, is that of "Die Fuersten der Dunkelheit") and a table of numbers
# follows, the German description is lost (a new one here); never offered (flags 5).
FILM200_EN_TITLE = 'Prince of Darkness'
FILM200_DE = ['Die Fürsten der Dunkelheit', 'Ein Horrorfilm von John Carpenter mit Teufel, Tod und Schlachthaus-Atmosphäre.']
FILM200_DE_DAMAGED = '1a446965204681727374656e2064657266'      # b'\x1aDie F\x81rsten derf'


def film200(docs):
    r = next((x for x in docs['films']['records'] if x.get('id') == 200), None)
    if not r:
        return False
    done = False
    t = r.setdefault('text', {})
    if isinstance(t.get('en'), list) and t['en'] and t['en'][0] == 'xxx':
        t['en'][0] = FILM200_EN_TITLE
        done = True
    de = t.get('de')
    if isinstance(de, dict) and de.get('raw', '').startswith(FILM200_DE_DAMAGED):
        t['de'] = list(FILM200_DE)
        done = True
    if r.get('flags') == 5 and r.get('genre') == 2 and r.get('price') == 90000:
        r['flags'] |= 0x20                      # on the market
        done = True
    return done


# (film 164, "Live Aid", length 0, is not repaired here: packs keep the original's
# record, and the game's fix live-aid gives it its 2 hours while it plays with fixes --
# src/fixes.c fix_live_aid_record.)
FIXES = [('film-200', film200)]


def apply(docs):
    """content.py documents -> a copy with the data fixes applied, and the names applied"""
    docs = copy.deepcopy(docs)
    return docs, [name for name, fn in FIXES if fn(docs)]
