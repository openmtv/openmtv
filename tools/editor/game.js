/* OpenMTV editor -- the game's side: its text (metrics, wrap, templates), its glyphs, its
 * info cards, its rules (prices, pools, required fields) and its TV monitor.
 * Content tables come from tools/content.py (the game's records as JSON), labels from
 * the game's own strings, previews are drawn with the game's glyphs (resource 3) and
 * its card templates, like the game draws them. */
'use strict';

const PAGE_ERRORS = [];                          /* script errors, for the self-test */
window.addEventListener('error', (e) => PAGE_ERRORS.push(`${e.message} (${(e.filename || '').split('/').pop()}:${e.lineno})`));
const $ = (s, el = document) => el.querySelector(s);
const $$ = (s, el = document) => [...el.querySelectorAll(s)];
function h(tag, attrs, ...kids) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'class') el.className = v;
    else if (k === 'style') el.style.cssText = v;
    else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
    else if (k === 'value') el.value = v;
    else if (k === 'checked') el.checked = !!v;
    else if (k === 'html') el.innerHTML = v;
    else el.setAttribute(k, v === true ? '' : v);
  }
  for (const k of kids.flat()) {
    if (k === null || k === undefined || k === false) continue;
    el.append(k instanceof Node ? k : document.createTextNode(String(k)));
  }
  return el;
}
async function api(method, url, body) {
  const headers = { 'X-UI-Lang': UI.lang };                 /* the server answers in the interface language */
  if (body) headers['Content-Type'] = 'application/json';
  const r = await fetch(url, { method, headers, body: body ? JSON.stringify(body) : undefined });
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || r.statusText);
  return j;
}
const clone = (o) => JSON.parse(JSON.stringify(o));
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

/* ---------------------------------------------------------------- state */
const STATIONS = ['Market', 'Mad-TV', 'Fun-TV', 'Sun-TV'];
const GENRE_COLORS = ['#888', '#e0407a', '#e07a10', '#8a4ad0', '#f0d020', '#305ad0', '#30a040', '#f4f4f4', '#ff8ad8', '#30d0e0', '#909090'];
const SERIES_COLORS = ['#888', '#f0a040', '#30d0e0', '#c08040', '#305ad0', '#909090'];
const st = {
  meta: null, mods: [], mod: null, lang: 'en', plang: null, section: 'films',
  data: {}, orig: {}, dirty: new Set(), sel: {}, filter: {}, view: {}, tab: {}, mode: 'form',
  glyphImg: [], glyphCache: {}, undo: [], redo: [], base: {}, railFolded: false,
  packs: [], unlocked: new Set(), forking: null, cardScale: 3, stageRow: {}, packInfo: {},
};
const editLang = () => st.plang || st.lang;     /* the previews' language: the one being edited */

/* ---------------------------------------------------------------- the game's text: metrics, wrap, format */
function chW(ch) { const m = st.meta.metrics; return ch in m ? m[ch] : m['.']; }
function textW(s) { let w = 0; for (const ch of s) w += chW(ch); return w; }
function wrap(s, width) {
  const out = [];
  for (const para of String(s).split('~')) {
    let line = '';
    for (const word of para.split(' ')) {
      const cand = line ? line + ' ' + word : word;
      if (textW(cand) > width && line) { out.push(line); line = word; } else line = cand;
    }
    out.push(line);
  }
  return out;
}
function missingChars(s) {
  const m = st.meta.metrics, bad = new Set();
  for (const ch of String(s).replace(/#\d/g, '')) if (!(ch in m) && ch !== '~') bad.add(ch);
  return [...bad];
}
/* str_format templates: #s #d #l #NNf (bar), widths and pads; '~' = new line */
function gfmt(tpl, args) {
  const lines = [[]];
  let buf = '', ai = 0;
  const flush = () => { if (buf) { lines[lines.length - 1].push({ s: buf }); buf = ''; } };
  for (let i = 0; i < tpl.length;) {
    const c = tpl[i];
    if (c === '~') { flush(); lines.push([]); i++; continue; }
    if (c === '#') {
      let j = i + 1, pad = '', width = '';
      if ('0+_'.includes(tpl[j])) pad = tpl[j++];
      while (/[0-9]/.test(tpl[j] || '')) width += tpl[j++];
      const type = tpl[j++];
      const a = args[ai++];
      if (type === 'f') { flush(); lines[lines.length - 1].push({ bar: +width || 40, v: a }); }
      else {
        let s = type === 's' || typeof a === 'string' ? String(a ?? '') : String(Math.trunc(a ?? 0));
        if (width) s = s.padStart(+width, pad === '0' ? '0' : ' ');
        buf += s;
      }
      i = j; continue;
    }
    buf += c; i++;
  }
  flush();
  return lines;
}

/* glyphs of resource 3, tinted per colour */
async function loadGlyphs() {
  const n = Math.max(...Object.values(st.meta.glyphs)) + 1;
  st.glyphImg = await Promise.all([...Array(n).keys()].map((k) => new Promise((ok) => {
    const im = new Image(); im.onload = () => ok(im); im.onerror = () => ok(null); im.src = `/api/sprite/3/${k}.png`;
  })));
}
function glyph(k, color) {
  const key = k + color;
  if (st.glyphCache[key]) return st.glyphCache[key];
  const im = st.glyphImg[k];
  if (!im) return null;
  const c = document.createElement('canvas'); c.width = im.width; c.height = im.height;
  const g = c.getContext('2d'); g.drawImage(im, 0, 0);
  g.globalCompositeOperation = 'source-in'; g.fillStyle = color; g.fillRect(0, 0, c.width, c.height);
  return (st.glyphCache[key] = c);
}
function drawText(g, s, x, y, color) {
  const map = st.meta.glyphs;
  for (const ch of String(s)) {
    if (ch !== ' ') { const gl = glyph(map[ch] ?? map['.'], color); if (gl) g.drawImage(gl, x, y); }
    x += chW(ch);
  }
  return x;
}

/* ---------------------------------------------------------------- the info card, as the game draws it */
/* measured on the game's cards: box 141 px, text at +7, a line holds what is narrower than
 * 16 columns of 8 px (text_fit_chars 0E35:0901); films show 4 description lines, series 3 */
const CARD_W = 141, TEXT_X = 7, TEXT_W = 127, FILM_LINES = 4, SERIES_LINES = 3;
const PAL = { red: '#de2424', grey: '#818181', band: '#b2b2b2', black: '#000', blue: '#a1daf2' };
function drawCard(canvas, rows, scale = 3) {
  const H = 2 + rows.length * 8 + 2;
  canvas.width = CARD_W; canvas.height = H;
  canvas.style.width = CARD_W * scale + 'px'; canvas.style.height = H * scale + 'px';
  const g = canvas.getContext('2d');
  g.fillStyle = PAL.black; g.fillRect(0, 0, CARD_W, H);
  let y = 1;
  rows.forEach((r) => {
    g.fillStyle = PAL[r.bg || 'grey']; g.fillRect(1, y, CARD_W - 2, 8);
    let x = TEXT_X;
    for (const seg of r.segs || [{ s: r.text || '' }]) {
      if (seg.bar) {                     /* #40f: 41 x 5 outline, 3 rows of value, ticks every 10 px */
        const w = seg.bar, v = Math.max(0, Math.min(w - 1, Math.round(seg.v || 0)));
        g.fillStyle = PAL.black; g.fillRect(x, y + 1, w + 1, 5);
        g.fillStyle = PAL.blue; g.fillRect(x + 1, y + 2, w - 1, 3);
        g.fillStyle = PAL.red; g.fillRect(x + 1, y + 2, v, 3);
        g.fillStyle = PAL.black;
        for (let t = 10; t < w; t += 10) g.fillRect(x + t, y + (t === w / 2 ? 3 : 4), 1, t === w / 2 ? 2 : 1);
        x += w + 4;
      } else x = drawText(g, seg.s, x, y, r.color || PAL.black);
    }
    if (r.right) drawText(g, r.right, CARD_W - 5 - textW(r.right), y, PAL.black);
    y += 8;
  });
  g.fillStyle = PAL.black; g.fillRect(0, y, CARD_W, 1);
}
/* the row's text, for comparing two cards */
const rowText = (r) => (r.segs ? r.segs.map((s) => (s.bar ? `[${Math.round(s.v || 0)}]` : s.s)).join('') : r.text || '') + '|' + (r.right || '') + '|' + (r.bg || '');
const fmtRows = (lines, bg, edit) => lines.map((segs, k) => ({ bg, segs, edit: Array.isArray(edit) ? edit[k] : edit }));

/* ---------------------------------------------------------------- game rules shown next to the values (catalog.py ports) */
const i8 = (v) => ((v & 0xFF) ^ 0x80) - 0x80;
const tdiv = (a, b) => Math.trunc(a / b);
const i16 = (v) => ((v & 0xFFFF) ^ 0x8000) - 0x8000;
/* the share of the hours 18..24 a programme gets alone on air -- no rival, no news lead-in, no
   trend: broadcast_phase_eval 24B8:0C4A as tools/catalog.py solo_share ports it (checked there
   against the original program).  pull: a film's topicality, a series' first episode's;
   gcode: the film genre 1..10, a series 10 + its genre; the audience of a genre by hour comes
   from the game's start data (DS:7160) */
function soloShare(pull, gcode) {
  const ga = st.meta.genre_aud || [];
  return [1, 2, 3, 4, 5, 6, 7].map((hour) => {
    if (!gcode) return 0;
    let s = pull & 0xFFFF;
    const same = s, a = ga[gcode * 8 + hour] || 0;
    if (i16(a) < i16(same)) s = tdiv(i16(s * a), i16(same)) & 0xFFFF;
    s = tdiv(i16(s * a), 100) & 0xFFFF;
    if (!s) return 0;
    const cap = Math.min(95, i16(s));
    return tdiv(i16(s * cap), i16(s)) & 0xFFFF;
  });
}
const filmSolo = (r) => soloShare(i8(r.topicality) & 0xFFFF, r.genre);
const seriesSolo = (r) => soloShare(i8(r.episodes_qtf[4]) & 0xFFFF, 10 + r.genre);
const roundTo = (v, m) => tdiv(v + (m >> 1), m) * m;
function moviePrice(price, quality, top) {
  const v = (price === 0 || (quality & 0xFF) === 0) ? price : tdiv(i8(top) * price, i8(quality) << 1) + tdiv(price, 2);
  return roundTo(v, 1000);
}
function seriesPrice(qtf, epPrice) {
  let t = 0, q = 0, si = 1;
  while (si <= 10 && (qtf[si * 3 + 2] & 1)) { t += i8(qtf[si * 3 + 1]); q += i8(qtf[si * 3]); si++; }
  const total = (si - 1) * epPrice;
  return roundTo(total === 0 || q === 0 ? 0 : tdiv(t * total, q << 1) + tdiv(total, 2), 1000);
}
const money = (v) => '$ ' + numFmt(v);

/* ---------------------------------------------------------------- record helpers */
function unkGet(rec, off) {
  for (const [k, hx] of Object.entries(rec.unknown || {})) {
    const b = parseInt(k, 16), n = hx.length / 2;
    if (off >= b && off < b + n) return parseInt(hx.substr((off - b) * 2, 2), 16);
  }
  return 0;
}
function unkSet(rec, base, len, off, v) {        /* the uncovered run base..base+len holds off */
  rec.unknown = rec.unknown || {};
  const key = '0x' + base.toString(16).toUpperCase().padStart(2, '0');
  let hx = rec.unknown[key] || '00'.repeat(len);
  const p = (off - base) * 2;
  hx = hx.slice(0, p) + (v & 0xFF).toString(16).padStart(2, '0') + hx.slice(p + 2);
  if (/^0+$/.test(hx)) delete rec.unknown[key]; else rec.unknown[key] = hx;
  if (!Object.keys(rec.unknown).length) delete rec.unknown;
}
function txt(rec, i, lang = st.lang) {
  const t = rec.text && rec.text[lang];
  if (i === null) return typeof t === 'string' ? t : '';
  if (Array.isArray(t)) return t[i] ?? '';
  if (typeof t === 'string') return t;
  return '';
}
function setTxt(rec, i, v, lang) {
  rec.text = rec.text || {};
  if (i === null) { rec.text[lang] = v; return; }
  if (!Array.isArray(rec.text[lang])) rec.text[lang] = [];
  rec.text[lang][i] = v;
}
const records = (table) => st.data[table].records.filter((r) => r.id > 0);
const recById = (table, id) => st.data[table].records.find((r) => r.id === id);
const origById = (table, id) => st.orig[table].records.find((r) => r.id === id);
const changed = (table, rec) => !same(rec, origById(table, rec.id));

/* a screenplay produces a film, or a series when its flags bit 0 is set (the target is then a series id) */
const spIsSeries = (sp) => (sp.flags & 1) !== 0;
const spTarget = (sp) => sp.target && recById(spIsSeries(sp) ? 'series' : 'films', sp.target);
function targetTitle(sp, lang = st.lang) {
  const r = spTarget(sp);
  if (!r) return '';
  return spIsSeries(sp) ? T('{title} (series)', { title: txt(r, 0, lang) }) : txt(r, 0, lang);
}

/* ---------------------------------------------------------------- screenplays: the production
 * read from the code (studio_new_shopping_list 229E:09AF, studio_start_production 229E:0B98,
 * script_role_match_pct 1DA8:2560, script_count_scenes 1DA8:2293); content.py keeps these
 * bytes in the record's "unknown".  Episode e (1..10): 0x23 bytes at 8 + (e-1) * 0x23 -- +0
 * its base quality, +2 the programme's length (the game writes it there when it issues the
 * list), then the shopping list: entry k (1..10) at +1 + 3k, item, weight, what was bought (0
 * in the data); an item 0 ends it.  Items 1-15 are the screenplay's roles, 16 special effects,
 * 17-41 sets -- a studio (size 1-3) takes a screenplay whose first list has no more sets than
 * its size.  Role t (1..15): requirements at 0x168 + 4(t-1) -- humor, a fourth attribute that
 * counts nowhere, appearance, skill -- and its actor category at 0x1A3 + t; texts 1..15 are
 * the roles' names.  Shooting: quality = base x sum(scores) / sum(weights) x 125 / 100; a role
 * scores weight x the actor's fit (the sum of min(requirement, attribute) / the sum of
 * requirements), a set or the effects their weight up to the quality of the level bought.
 * A film is one list (with flag bit 2 the game copies it to the other nine at load,
 * script_init_episodes 1C18:0313: up to 10 productions); a series one per episode of its
 * series (script_episode_count 1DA8:01CB). */
const SP_ROLES = 15, SP_ITEMS = 10, SP_SETS = 3, SP_QMAX = 80;
const SP_EP = (e) => 8 + (e - 1) * 0x23;
const SP_REQ = (t) => 0x168 + (t - 1) * 4;
const SP_CAT = (t) => 0x1A3 + t;
const SP_ATTRS = [0, 2, 3];                      /* the requirement bytes the fit reads: humor, appearance, skill */
const SP_KEYS = ['id', 'price', 'state', 'flags', 'target', 'duration_hours'];
const spIsSet = (item) => item >= 17 && item <= 41;
/* a byte of the production: the record's runs 0x08..0x165 and 0x168..0x1B2 (content.py's "unknown") */
function spSet(r, off, v) {
  if (off < 0x166) unkSet(r, 0x08, 0x15E, off, v); else unkSet(r, 0x168, 0x4B, off, v);
}
/* after a change: the runs in their order and the record's keys as the export writes them, so an
   entry set back is the original again (same JSON) */
function spTidy(r) {
  const u = r.unknown || {}, out = {};
  for (const k of SP_KEYS) if (k in r) out[k] = r[k];
  const runs = Object.keys(u).filter((k) => !/^0*$/.test(u[k])).sort((a, b) => parseInt(a, 16) - parseInt(b, 16));
  if (runs.length) out.unknown = Object.fromEntries(runs.map((k) => [k, u[k]]));
  for (const k of Object.keys(r)) if (!(k in out) && k !== 'unknown') out[k] = r[k];
  for (const k of Object.keys(r)) delete r[k];
  Object.assign(r, out);
}
function spList(r, e) {
  const out = [];
  for (let k = 1; k <= SP_ITEMS; k++) {
    const o = SP_EP(e) + 1 + 3 * k, item = unkGet(r, o);
    if (!item) break;
    out.push({ item, weight: unkGet(r, o + 1), got: unkGet(r, o + 2) });
  }
  return out;
}
/* the list written; past its end item 0, the other bytes the original's where its slot is
   empty too (item 0 counts nowhere: screenplays 11 and 14 have weights there), else 0 -- a
   list set back is the original again */
function spPutList(r, e, list) {
  const orig = origById('screenplays', r.id);
  for (let k = 1; k <= SP_ITEMS; k++) {
    const o = SP_EP(e) + 1 + 3 * k, x = list[k - 1], keep = !x && orig && !unkGet(orig, o);
    spSet(r, o, x ? x.item : 0);
    spSet(r, o + 1, x ? x.weight : keep ? unkGet(orig, o + 1) : 0);
    spSet(r, o + 2, x ? x.got : keep ? unkGet(orig, o + 2) : 0);
  }
  spTidy(r);
}
/* the lists the game uses: a film's one, a series' one per episode of its series */
function spEpisodes(r, byId = recById) {
  if (!spIsSeries(r)) return 1;
  const t = r.target && byId('series', r.target);
  return t ? Math.max(0, Math.min(10, t.episodes)) : 0;
}
const spRoleUse = (r, t) => [...Array(spEpisodes(r)).keys()].map((k) => k + 1).filter((e) => spList(r, e).some((x) => x.item === t));
const spRoleSet = (r, t) => !!(unkGet(r, SP_CAT(t)) || [0, 1, 2, 3].some((i) => unkGet(r, SP_REQ(t) + i)) || LANGS.some((l) => txt(r, t, l)));
/* the roles shown: up to the highest one set or in a list the game uses (the lists spRoleUse
   reads: a role in no list shown can be removed) */
function spRoleCount(r) {
  let n = 0;
  for (let t = 1; t <= SP_ROLES; t++) if (spRoleSet(r, t)) n = t;
  for (let e = 1; e <= spEpisodes(r); e++) for (const x of spList(r, e)) if (x.item <= SP_ROLES) n = Math.max(n, x.item);
  return n;
}
/* the game's names: actor categories (text 145 + c), the shop's items (192 + item), the attributes (363) */
const gameStr = (i, lang = st.lang) => ((st.strings || {})[lang] || [])[i] || '';
const actorCat = (c, lang = st.lang) => (c >= 1 && c <= 7 ? gameStr(145 + c, lang) : '-');
const propName = (item, lang = st.lang) => gameStr(192 + item, lang) || T('item {n}', { n: item });
const roleName = (r, t, lang = st.lang) => txt(r, t, lang).trim();
function spItemName(r, item, lang = st.lang) {
  if (item > SP_ROLES) return propName(item, lang);
  const n = roleName(r, item, lang), c = unkGet(r, SP_CAT(item));
  return n ? T('role {n}: {name}', { n: item, name: n }) : T('role {n}: {category}', { n: item, category: actorCat(c, lang) });
}
/* a list's effects and sets more than once -> [{ e, item, n }]: each entry is bought (and paid)
   on its own, a set takes a studio place each time (the original has none twice; roles often) */
function spDupes(r) {
  const out = [];
  for (let e = 1; e <= spEpisodes(r); e++) {
    const n = {};
    for (const x of spList(r, e)) if (x.item > SP_ROLES) n[x.item] = (n[x.item] || 0) + 1;
    for (const [item, k] of Object.entries(n)) if (k > 1) out.push({ e, item: +item, n: k });
  }
  return out;
}
/* "1-3, 5" */
function numRanges(ns) {
  const out = [];
  for (let i = 0; i < ns.length; i++) {
    let j = i;
    while (j + 1 < ns.length && ns[j + 1] === ns[j] + 1) j++;
    out.push(j > i ? `${ns[i]}-${ns[j]}` : String(ns[i]));
    i = j;
  }
  return out.join(', ');
}
/* what an offered screenplay misses before the agency may offer it -> [{ key, msg } or { key, kind, n }]
   (spCompact words the kinds: one message for the episodes or roles of a kind); byId: the tables
   its target is looked up in (origById for the original's own gaps) */
function spMissing(r, byId = recById) {
  const out = [], series = spIsSeries(r), tgt = r.target && byId(series ? 'series' : 'films', r.target);
  if (!(r.price > 0)) out.push({ key: 'price', msg: T('price') });
  if (!txt(r, 0, 'en').trim()) out.push({ key: 'desc-en', msg: T('English description') });
  if (!txt(r, 0, 'de').trim()) out.push({ key: 'desc-de', msg: T('German description') });
  if (!tgt) out.push({ key: 'target', msg: T('the programme it becomes') });
  else {
    const o = { id: tgt.id };
    if (!txt(tgt, 0, 'en').trim()) out.push({ key: 'target-title-en', msg: series ? T('English title of series {id}', o) : T('English title of film {id}', o) });
    if (!txt(tgt, 0, 'de').trim()) out.push({ key: 'target-title-de', msg: series ? T('German title of series {id}', o) : T('German title of film {id}', o) });
    if (!(tgt.blocks >= 1)) out.push({ key: 'target-length', msg: series ? T('episode length of series {id}', o) : T('length of film {id}', o) });
    if (series && !(tgt.episodes >= 1)) out.push({ key: 'target-episodes', msg: T('episodes of series {id}', o) });
  }
  const used = new Set();
  for (let e = 1; e <= spEpisodes(r, byId); e++) {
    const list = spList(r, e);
    if (!(unkGet(r, SP_EP(e)) >= 1)) out.push({ key: `ep${e}-quality`, kind: 'quality', n: e });
    if (!list.length) out.push({ key: `ep${e}-list`, kind: 'list', n: e });
    if (e === 1 && list.filter((x) => spIsSet(x.item)).length > SP_SETS) out.push({ key: 'ep1-sets', msg: T('at most 3 sets in the first list (a studio holds 3)') });
    for (const x of list) if (x.item <= SP_ROLES) used.add(x.item);
  }
  for (const t of [...used].sort((a, b) => a - b)) {
    const c = unkGet(r, SP_CAT(t));
    if (!(c >= 1 && c <= 7)) out.push({ key: `role${t}-category`, kind: 'category', n: t });
    if (!SP_ATTRS.some((i) => unkGet(r, SP_REQ(t) + i))) out.push({ key: `role${t}-req`, kind: 'req', n: t });
  }
  return out;
}
function spCompact(r, list) {
  const series = spIsSeries(r), out = [], at = {};
  const word = {
    quality: (ns) => (series ? TP(ns.length, 'base quality (episode {list})', 'base quality (episodes {list})', { list: numRanges(ns) }) : T('base quality')),
    list: (ns) => (series ? TP(ns.length, 'shopping list (episode {list})', 'shopping list (episodes {list})', { list: numRanges(ns) }) : T('shopping list')),
    category: (ns) => TP(ns.length, 'actor category of role {list}', 'actor category of roles {list}', { list: numRanges(ns) }),
    req: (ns) => TP(ns.length, 'requirements of role {list}', 'requirements of roles {list}', { list: numRanges(ns) }),
  };
  for (const x of list) {
    if (!x.kind) { out.push(x); continue; }
    if (!(x.kind in at)) { at[x.kind] = out.length; out.push({ key: x.kind, keys: [], ns: [] }); }
    const g = out[at[x.kind]];
    g.keys.push(x.key); g.ns.push(x.n);
  }
  for (const g of out) if (g.ns) { g.msg = word[g.key](g.ns); delete g.ns; }
  return out;
}

/* ---------------------------------------------------------------- slots: in use, free, removable
 * The engine's tables have fixed slots; an entry takes part in the game only through these
 * gates (read from the code): the film dealer and the auction offer films/series with the
 * market flag (bit 5) and no owner (24B8:04B9, 24B8:0905); an owner 1-3 holds it in a start
 * library; a screenplay produces its target; the ad agency offers ads with a bonus
 * (24B8:000C); the screenplay agency offers screenplays with flag bit 1 (24B8:02C1; nothing
 * sets it, so 25-33 and 37-50 are never offered in the original); the code uses films 29 and
 * 187 (live football), 58 (Betty's culture programme) and screenplay 24 by number. */
const SPECIAL = {};
onUiLang(() => Object.assign(SPECIAL, {
  films: { 29: T('the recurring live football (code uses it by number)'), 187: T('a recurring live broadcast (code uses it by number)'), 58: T("Betty's culture programme (code uses it by number)") },
  screenplays: { 24: T('always offered first (code uses it by number)') },
  news_nodes: { 166: T('a game event (code uses it by number)'), 167: T('a game event (code uses it by number)'), 168: T('a game event (code uses it by number)') } }));

const CULTURE = 6, POOL = 0x10;                 /* flags2 bit 4: the mod's starting pool (decompiled build) */
const filmPool = () => records('films').filter((f) => f.flags2 & POOL);
const seriesPool = () => records('series').filter((s) => s.flags2 & POOL);
/* a new game (newgame_starting_programs) gives each station one series and 3 films: from the
   marked ones while one is free, then from the original pools (series 18-20, cultural
   programmes on the market); it loops for ever when both run out */
const origStartSeries = () => [18, 19, 20].map((i) => recById('series', i)).filter((s) => s && !s.owner);
const origStartFilms = () => records('films').filter((f) => f.genre === CULTURE && (f.flags & 0x20) && !f.owner);
const freeStart = (pool, orig) => new Set([...pool.filter((r) => !r.owner), ...orig].map((r) => r.id)).size;
function startCounts() {
  return { films: freeStart(filmPool(), origStartFilms()), series: freeStart(seriesPool(), origStartSeries()) };
}
function gameChecks() {
  const out = [];
  const { films: nf, series: ns } = startCounts();
  if (nf < 9) out.push(T('A new game gives each station 3 films (the marked ones, then cultural programmes on the market): only {n} are free, it would hang with fewer than 9.', { n: nf }));
  if (ns < 3) out.push(T('A new game gives each station one series (the marked ones, then series 18-20): only {n} are free, it would hang with fewer than 3.', { n: ns }));
  return out;
}
/* What an entry needs before it goes into the game -> [{ key, msg }] (empty: complete).
   Films and series: on the market, in a start library or the starting pool (series 18-20:
   a new game may give them away); not the special films the code uses by number, not
   produced entries, whose values come from the production.  Ads: with a fee (the agency
   offers it) or held at the start.  The form marks the fields by these keys; the same rules
   in launcher/checks.go (incomplete) keep a mod with an incomplete entry off the air. */
function textsMissing(r, ad) {                    /* the two texts as the form calls them: title and description, or advertiser and slogan */
  const names = ad ? { en: [T('English advertiser'), T('English slogan')], de: [T('German advertiser'), T('German slogan')] }
    : { en: [T('English title'), T('English description')], de: [T('German title'), T('German description')] };
  const out = [];
  for (const l of ['en', 'de']) {
    if (!txt(r, 0, l).trim()) out.push({ key: 'title-' + l, msg: names[l][0] });
    if (!txt(r, 1, l).trim()) out.push({ key: 'desc-' + l, msg: names[l][1] });
  }
  return out;
}
const REQUIRED = {
  series: {
    hint: () => T('titles, descriptions, episode price'),
    inGame: (r) => !!((r.flags & 0x20) || (r.owner >= 1 && r.owner <= 3) || (r.flags2 & POOL) || [18, 19, 20].includes(r.id)),
    check(r) {
      const out = textsMissing(r);
      if (!(r.genre >= 1 && r.genre <= 5)) out.push({ key: 'genre', msg: T('genre') });
      if (!(r.episodes >= 1 && producedEpisodes(r) >= 1)) out.push({ key: 'episodes', msg: T('at least one produced episode') });
      if (!(r.blocks >= 1)) out.push({ key: 'blocks', msg: T('episode length (at least 1 hour)') });
      if (!(r.episode_price > 0)) out.push({ key: 'episode_price', msg: T('episode price') });
      return out;
    },
  },
  ads: {
    hint: () => T('advertiser and slogan in both languages'),
    inGame: (r) => !!(r.fee || r.holder),
    check(r) {
      const out = textsMissing(r, true);
      if (!(r.spots >= 1)) out.push({ key: 'spots', msg: T('spots') });
      if (!(r.days >= 1)) out.push({ key: 'days', msg: T('days') });
      if (!(r.min_audience_class >= 1 && r.min_audience_class <= 10)) out.push({ key: 'min_audience_class', msg: T('minimum audience') });
      return out;
    },
  },
  films: {
    hint: () => T('titles, descriptions, price'),
    keys: ['title-en', 'title-de', 'desc-en', 'desc-de', 'genre', 'blocks', 'quality', 'topicality', 'price'],
    inGame: (r) => !!((r.flags & 0x20) || (r.owner >= 1 && r.owner <= 3) || (r.flags2 & POOL)),
    check(r) {
      const out = textsMissing(r);
      if (!(r.genre >= 1 && r.genre <= 10)) out.push({ key: 'genre', msg: T('genre') });
      if (!(r.blocks >= 1)) out.push({ key: 'blocks', msg: T('length (at least 1 hour)') });
      if (!(r.quality >= 1)) out.push({ key: 'quality', msg: T('quality (at least 1)') });
      if (!(r.topicality >= 1 && r.topicality <= r.quality)) out.push({ key: 'topicality', msg: T('topicality (1 up to the quality)') });
      if (!(r.price > 0)) out.push({ key: 'price', msg: T('price') });
      return out;
    },
  },
  /* screenplays: offered at the agency -- and 24, which it offers first (by number, 24B8:02C1);
     their production as the game reads it (spMissing) */
  screenplays: {
    inGame: (r) => !!(r.flags & 2) || r.id === 24,
    special: true,
    check: spMissing,
    compact: spCompact,
  },
};
/* -> [{ key, msg }]; a message for several keys (screenplays: one for the episodes or roles of a
   kind) holds them in keys.  raw: the check alone ('orig': of an original entry, its references
   looked up in the original's tables) */
function missing(sec, r, raw = false) {
  const req = REQUIRED[sec.key];
  if (!req || !req.inGame(r) || (!req.special && (SPECIAL[sec.table] || {})[r.id])) return [];
  if (!raw) {                                    /* what the original itself lacks (film 164: length 0 -- the game's data fix live-aid repairs it while it plays) does not block */
    const o = origById(sec.table, r.id), had = new Set(o ? missing(sec, o, 'orig').map((x) => x.key) : []);
    const m = missing(sec, r, true).filter((x) => !had.has(x.key));
    return req.compact ? req.compact(r, m) : m;
  }
  return req.check(r, raw === 'orig' ? origById : recById);
}

/* ---------------------------------------------------------------- the cards' rows (geometry measured against the game) */
/* films; a row's "edit" names the fields behind it (the in-game view opens them) */
function filmRows(r, lang) {
  const genre = st.meta.film_genres[lang][r.genre - 1] || '?';
  const rows = [{ bg: 'red', text: txt(r, 0, lang), edit: 'title' }];
  const desc = wrap(txt(r, 1, lang), TEXT_W);
  for (let k = 0; k < FILM_LINES; k++) rows.push({ text: desc[k] || '', edit: 'desc' });
  rows.push({ bg: 'band', text: genre, edit: 'genre' });
  const c13 = unkGet(r, 0x13), c14 = unkGet(r, 0x14), c15 = unkGet(r, 0x15), c16 = unkGet(r, 0x16);
  const country = st.meta.countries[lang][c13] || '';
  const card = st.meta.card[lang];
  /* the year byte counts from 1900; the game prints 20nn for 100..199 (status_panel.c) */
  const y20 = c14 >= 100, year = (t) => (y20 ? t.replace('19#', '20#') : t), yy = y20 ? c14 - 100 : c14;
  let tpl;
  if (r.genre === 6) tpl = fmtRows(gfmt(year(card[413]), [country, yy, c15 << 2, c16 << 2]), 'grey', ['origin', 'c15', 'c16']);
  else if (r.genre === 8) tpl = fmtRows(gfmt(card[414], [c13 << 2, c14 << 2]), 'grey', ['c13', 'c14']);
  else tpl = fmtRows(gfmt(year(card[412]), [country, yy, c15 << 2, c16 << 2]), 'grey', ['origin', 'c15', 'c16']);
  rows.push(...tpl);
  if (r.genre === 7) rows.push(...fmtRows(gfmt(card[416], [Math.max(0, r.live_day - 1), `${(r.live_slot || 1) + 17}:00`]), 'grey', 'live'));
  rows.push({ bg: 'band', segs: gfmt(card[418], [r.blocks])[0], right: '$ ' + moviePrice(r.price, r.quality, r.topicality), edit: 'price' });
  rows.push(...fmtRows(gfmt(card[417], [r.quality ? Math.round((r.topicality * 40) / r.quality) : 0]), 'grey', 'topicality'));
  if (r.flags2 & 8) rows.push({ text: card[419], color: '#de2424', edit: 'rating' });
  return rows;
}
/* series */
function seriesRows(r, lang) {
  const card = st.meta.card[lang];
  const rows = [{ bg: 'red', text: txt(r, 0, lang), edit: 'title' }];
  const desc = wrap(txt(r, 1, lang), TEXT_W);
  for (let k = 0; k < SERIES_LINES; k++) rows.push({ text: desc[k] || '', edit: 'desc' });
  rows.push({ bg: 'band', text: st.meta.series_genres[lang][r.genre - 1] || '?', edit: 'genre' });
  rows.push(...fmtRows(gfmt(card[376], [r.critics << 2, r.press << 2]), 'grey', ['critics', 'press']));
  const ep = Math.max(1, r.next_episode || 1);
  const lines = gfmt(card[377], [r.episodes, r.blocks, ep, txt(r, 1 + ep, lang)]);
  rows.push({ bg: 'band', segs: lines[0], right: '$ ' + seriesPrice(r.episodes_qtf, r.episode_price), edit: 'price' });
  rows.push(...fmtRows(lines.slice(1), 'grey', 'episode'));
  return rows;
}
/* ads */
function adRows(r, lang) {
  const card = st.meta.card[lang];
  const rows = [{ bg: 'red', text: txt(r, 0, lang), edit: 'title' }];
  for (const l of wrap(txt(r, 1, lang), TEXT_W)) rows.push({ text: l, edit: 'desc' });
  const cls = st.meta.audience_classes[r.min_audience_class - 1] ?? 0;
  rows.push(...fmtRows(gfmt(card[382], [r.fee, r.penalty, String(cls).replace('.', lang === 'de' ? ',' : '.'), '?']), 'band', ['fee', 'penalty', 'audience']));
  rows.push(...fmtRows(gfmt(card[385], [r.spots, r.days]), 'grey', 'spots'));
  const codes = (r.genre_rule || []).filter((c) => c);
  const gname = (c) => (Math.abs(c) <= 10 ? st.meta.film_genres[lang][Math.abs(c) - 1] : st.meta.series_genres[lang][Math.abs(c) - 11]) || '?';
  let cond;
  if (!codes.length) cond = card[79];
  else if (codes[0] < 0) cond = codes.map((c) => gfmt(card[77], [gname(c)])[0][0].s).join(' ');
  else cond = gfmt(card[78], [codes.map(gname).join(gfmt(card[80], [''])[0][0]?.s || ' or ')])[0][0].s;
  for (const l of wrap(cond, TEXT_W)) rows.push({ text: l, edit: 'rule' });
  return rows;
}
/* news: which template (story) a node belongs to -- from its start node along the successors */
const NEWS_END = 9999;                          /* a successor 9999 ends the story (news_advance_story 1C18:0755) */
function newsTemplateOf(id) {
  for (const t of records('news_templates')) {
    const seen = new Set(), todo = [t.start_node];
    while (todo.length) {
      const n = todo.pop();
      if (!n || n === NEWS_END || seen.has(n)) continue;
      if (n === id) return t;
      seen.add(n);
      const nd = recById('news_nodes', n);
      if (nd) todo.push(...nd.successors);
    }
  }
  return null;
}
/* news_text 26A7:292F: every #n takes a phrase of the topic of part n (topic = code % 1000) */
function newsHeadline(r, lang, variants) {
  const t = newsTemplateOf(r.id);
  return (r.text?.[lang] || '').replace(/#(\d)/g, (m, n) => {
    const topic = t && recById('news_topics', t.parts[+n - 1] % 1000);
    const v = (variants && variants[+n - 1]) || 1;
    return topic ? (topic.text?.[lang] || [])[v - 1] || (topic.text?.[lang] || [])[0] || '<' + n + '>' : '<' + n + '>';
  });
}

/* ---------------------------------------------------------------- the TV monitor (the bottom panel's office TV)
 * res 9 sprite 0 (120 x 77, the left half of the bottom panel, at screen 0,122); the
 * programme's picture (88 x 61, resources 220-238) goes at +8,+2 (tv_show_picture 0054:3855
 * puts it at screen 8,124), with the logo res 15 sprite 0 drawn on it at +5,+5. */
const TV = { w: 116, h: 77, picX: 8, picY: 2, logoX: 5, logoY: 5 };
const TV_DARK = 0xDC, TV_NEWS = 0xEC, TV_ADS = 0xED, TV_NONE = 0xEE;   /* tv_update_picture 0054:3738 */
const FILM_PIC = 0xDC, SERIES_PIC = 0xE6;                              /* + genre */
const imgCache = {};
/* ---------------------------------------------------------------- the TV family (the office, bottom right) */
/* which members watch, where they sit, how they look: tv_family_update (0054:39CF) with
   tvfam_draw_room and tvfam_draw_viewer0..4, and the first frame of tvfam_render (105E:32ED) --
   a still; the game animates it (blinks, eating, knitting).  Checked against the game's own
   picture (an evening of the scenario room_evening_air: the same 104 x 58 pixels).
   bits: the entry's family bits (flags 0..4), q: what the game takes as the programme's quality
   (a film's topicality, a series' next episode's), gcode: film genre 1..10, series 10 + genre,
   hour: 1..7 = 18:00..24:00.  -> { draws: [[bank, sprite, x, y]], shown, left } (members by bit) */
const FAMILY_BITS = [1, 0, 2, 4, 3];             /* the game's order of the members: their flag bit */
const FAMILY_CHILD_ANIM = [0, 3, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 3];  /* DS:0D9D */
function familyScene(bits, q, gcode, hour) {
  const d = [], room = (t, x, y) => d.push([0xF, (t - 2) >> 3, x, y]);
  for (let x = 0; x <= 96; x += 8) { room(0x2A, x, 0); room(0x2A, x, 8); room(0x32, x, 0x32); }
  for (let x = 0x1F; x <= 67; x += 8) room(0x22, x, 0xE);
  room(0x12, 0, 0xE); room(0x1A, 0x43, 0xE);
  const w = FAMILY_BITS.map((b) => (bits >> b) & 1);
  if (hour >= 6) w[4] = 0;                       /* the child goes to bed at 23:00 */
  let X = w[1] ? 0x22 : 0xD, n = 0;
  const shown = [], left = [];
  for (let si = 0; si < 5; si++) {
    if (!w[si]) continue;
    if (n >= 3) { left.push(FAMILY_BITS[si]); continue; }   /* three on the sofa at most */
    n++; shown.push(FAMILY_BITS[si]);
    if (si === 0) {                                /* grandma, knitting */
      const x = X; X += 0x1A;
      const m = q > 30 ? (gcode === 4 ? 6 : gcode === 5 ? 2 : 3) : q > 20 ? 0 : q > 10 ? 5 : 4;
      d.push([0xB0, 0, x, 4]);
      if (m) d.push([0xB0, (m === 4 ? 2 : m === 6 ? 3 : m) + 3, x + 7, 0xD]);
      d.push([0xB0, 1, x + 0xA, 0x10]);
    } else if (si === 1) {                         /* father: awake before 22:00 or with a good programme */
      const x = 6, awake = hour < 5 || q > 30;
      d.push([0xB4, 0, x, 0x12]);
      if (awake) { d.push([0xB4, 8, x + 8, 7]); if (q > 30) d.push([0xB4, 12, x + 8, 0x11]); }
      else d.push([0xB4, 2, x + 8, 7], [0xB4, 5, x + 7, 0x18], [0xB4, 6, x + 0xC, 0x2C]);
    } else if (si === 2) {                         /* grandpa */
      const x = X; X += 0x1A;
      d.push([0xB1, 0, x, 0xB]);
      if (hour < 4 || q > 5) {
        if (q > 7) {
          const m = q > 30 ? (gcode === 4 ? 0xB : 0xE) : q > 20 ? 9 : q > 10 ? 0xD : 0xC;
          d.push([0xB1, 8, x + 6, 5]);
          if (![9, 0xB, 0xC].includes(m)) d.push([0xB1, m - 1, x + 6, 8]);
        } else d.push([0xB1, 14, x + 6, 5]);
      } else d.push([0xB1, 3, x + 6, 4]);
    } else if (si === 3) {                         /* the son */
      const x = X; X += 0x19;
      const m = q > 30 ? 9 : q > 20 ? 8 : q > 10 ? 5 : q > 5 ? 6 : 7;
      d.push([0xB2, 0, x, 0], [0xB2, (m === 6 || m === 7 ? 5 : m === 9 ? 8 : m) - 1, x + 7, 0]);
    } else {                                       /* the child */
      const x = X; X += 0x19;
      const m = q > 20 ? 0xD : q > 10 ? 0 : 0xC;
      d.push([0xB3, 0, x + 7, 0xB]);
      if (m) d.push([0xB3, m - 1, x + 7, 0x13]);
      d.push([0xB3, 1, x + 1, 0x23]);
      if (q <= 20) d.push([0xB3, 2, x + 1, 0x18]);
      d.push([0xB3, 7, x + 2, 0x26], [0xB3, 13, x, 0xB], [0xB3, 14, x + 0x10, 0xB]);
      if (q > 20) d.push([0xB3, FAMILY_CHILD_ANIM[1] - 1, x + 1, 0x18]);
    }
  }
  d.push([0xF, 1, 0x54, 4], [0xF, 8, 0, 0], [0xF, 9, 0x5E, 0], [0xF, 10, 0, 0x33]);
  return { draws: d, shown, left, bed: hour >= 6 && !!((bits >> 3) & 1) };
}
const familyImgs = {};
/* draw a scene into a 104 x 58 canvas (shown at scale S); the sprites load once */
function drawFamily(canvas, scene, S) {
  canvas.width = 104; canvas.height = 58;
  canvas.style.width = 104 * S + 'px'; canvas.style.height = 58 * S + 'px';
  const want = scene.draws.map(([b, k]) => `${b}/${k}`);
  return Promise.all(want.map((key) => (familyImgs[key] = familyImgs[key] || loadImg(`/api/sprite/${key}.png`)))).then((imgs) => {
    const g = canvas.getContext('2d');
    g.imageSmoothingEnabled = false;
    g.fillStyle = '#000'; g.fillRect(0, 0, 104, 58);
    scene.draws.forEach(([, , x, y], i) => g.drawImage(imgs[i], x, y));
    canvas.dataset.drawn = want.join(',');
  });
}
/* the programme as the family sees it: { q, gcode } of a film or series record */
const familyInput = (table, r) => (table === 'series'
  ? { q: i8(r.episodes_qtf[Math.max(1, r.next_episode || 1) * 3 + 1]), gcode: 10 + r.genre }
  : { q: i8(r.topicality), gcode: r.genre });
function loadImg(src) {
  return imgCache[src] || (imgCache[src] = new Promise((ok) => {
    const im = new Image(); im.onload = () => ok(im); im.onerror = () => ok(null); im.src = src;
  }));
}
function drawMonitor(canvas, pic, scale = 2) {
  canvas.width = TV.w; canvas.height = TV.h;
  canvas.style.width = TV.w * scale + 'px'; canvas.style.height = TV.h * scale + 'px';
  canvas.dataset.pic = pic;
  return Promise.all([loadImg('/api/sprite/9/0.png'), loadImg(`/api/picture/${pic}.png`), loadImg('/api/sprite/15/0.png')]).then(([tv, p, logo]) => {
    if (+canvas.dataset.pic !== pic) return;           /* a newer picture was asked for meanwhile */
    const g = canvas.getContext('2d');
    g.clearRect(0, 0, TV.w, TV.h);
    if (tv) g.drawImage(tv, 0, 0);
    if (p) g.drawImage(p, TV.picX, TV.picY);
    if (logo) g.drawImage(logo, TV.picX + TV.logoX, TV.picY + TV.logoY);
  });
}
