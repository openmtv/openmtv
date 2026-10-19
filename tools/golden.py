"""What the launcher's tests expect: tests/golden/NAME.json, the results the Go launcher
gave when they were recorded (2026-10-19 first checked against the Python launcher it
replaced, tools/deprecated/ until then).  A test compares with them; after an intended
change it records them anew (--record) -- then look at the diff before committing.

    g = golden.Golden('buildtest', record)
    ok, detail = g.check(key, value)              # the value itself: paths, messages, codes
    ok, detail = g.check(key, value, hashed=True) # only its SHA-256: answers that hold the
                                                  # original's data (tables, texts, pictures)
    g.save()                                      # (only when recording)

Standard library only."""
import hashlib
import json
import os

DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'tests', 'golden')


def digest(value):
    if isinstance(value, bytes):
        return 'sha256:' + hashlib.sha256(value).hexdigest()
    return 'sha256:' + hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False).encode('utf-8')).hexdigest()


def short(v, n=600):
    s = json.dumps(v, ensure_ascii=False, sort_keys=True)
    return s if len(s) <= n else s[:n] + ' ...'


class Golden:
    def __init__(self, name, record):
        self.name, self.record = name, record
        self.path = os.path.join(DIR, name + '.json')
        self.data, self.seen = {}, set()
        if not record and os.path.exists(self.path):
            with open(self.path, encoding='utf-8') as f:
                self.data = json.load(f)

    def check(self, key, value, hashed=False):
        """-> (whether VALUE is what KEY expects, what differs)"""
        if key in self.seen:
            raise KeyError('golden: %s twice in one run' % key)
        self.seen.add(key)
        v = digest(value) if hashed else json.loads(json.dumps(value, ensure_ascii=False))
        if self.record:
            self.data[key] = v
            return True, ''
        if key not in self.data:
            return False, '    (not in tests/golden/%s.json: --record it)\n    got: %s' % (self.name, short(v))
        want = self.data[key]
        if v == want:
            return True, ''
        if hashed:
            return False, '    expected %s, got %s: %s' % (want, v, short(value if not isinstance(value, bytes) else len(value)))
        return False, '    expected: %s\n    got:      %s' % (short(want), short(v))

    def save(self):
        if not self.record:
            return
        os.makedirs(DIR, exist_ok=True)
        with open(self.path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(self.data, f, ensure_ascii=False, indent=1, sort_keys=True)
            f.write('\n')
        print('golden: %d results recorded in %s' % (len(self.data), os.path.relpath(self.path)))
