/* OpenMTV editor -- the page of build/openmtv editor (launcher/editor.go): the work bench.
 *   top bar   the selection (what is shown and played), unsaved changes, search, undo,
 *             the game texts' language, save, the editor's settings (interface language)
 *   rail      the sections (SECTIONS, schema.js), the overview, the checks, play (launcher.js)
 *   list      the entries of a section (spine, badge, filters) -- or the table (table.js)
 *   head      the entry: title, where it is in the game, card or all fields, actions
 *   work      the card in the game's look, its rows' values below it (stage.js) -- or the
 *             form: field groups of the section's schema, both languages side by side,
 *             every changed field with its original value and a reset
 *   inspector what is not on the card, the TV monitor, checks, changes, references
 *   status    the last message, for a moment (#status)
 * i18n.js: the interface language (T, English or German, the header's switch); game.js: the
 * game's text, cards, rules, monitor; schema.js: the sections.  The modes plug in (MODES):
 * monitor.js (the Monitor tab), overview.js (mod overview), table.js (table), stage.js (in
 * the game), tree.js (story tree); each brings its self-test (SELFTESTS), selftest.js runs
 * them: /?selftest.  Every text the page shows goes through T(). */
'use strict';

/* the modes: key -> { label (a getter: T), view (a list alternative), sections (only these),
   shelf(sh), render(...) }; 'game' shows one entry: render(sec, r, w, ins) */
const MODES = {};
const SELFTESTS = [];
/* the player's launcher (build/openmtv, launcher/server.go: /api/meta says player) starts as the
   editor's page, at "Play" (#launcher) or wherever its link points (enterEditor: /api/meta?editor=1,
   the tables) -- one board for play, multiplayer and the contents.  The player's page alone (the
   pages "Play" and "Multiplayer", without the editor's data; its "Editor" makes it the editor) is
   what is left when the editor cannot start, and with ?player (its self-test) */
const player = () => !!(st.meta && st.meta.player);
/* the player's own pages: "Play", "Multiplayer", "Settings" */
const PLAYER_PAGES = ['launcher', 'multiplayer', 'settings'];
const playerPage = (hash) => PLAYER_PAGES.includes(String(hash).replace(/^#/, ''));
const hasMode = (k, sec) => !!MODES[k] && (!sec || !MODES[k].sections || MODES[k].sections.includes(sec.key));
function closePopover() { $$('.popover').forEach((p) => p.remove()); }

const ICONS = {
  film: '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M7 4v16M17 4v16M3 9h4M3 15h4M17 9h4M17 15h4"/>',
  series: '<rect x="3" y="7" width="14" height="13" rx="2"/><path d="M7 4h12a2 2 0 0 1 2 2v10"/>',
  ad: '<path d="M3 11v2a1 1 0 0 0 1 1h3l6 4V6L7 10H4a1 1 0 0 0-1 1z"/><path d="M17 9a4 4 0 0 1 0 6"/>',
  script: '<path d="M14 3H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><path d="M14 3v6h6M8 13h8M8 17h5"/>',
  news: '<rect x="3" y="5" width="18" height="14" rx="2"/><path d="M7 9h6M7 13h10M7 16h10"/>',
  topic: '<path d="M20 13l-7 7-9-9V4h7z"/><circle cx="8" cy="8" r="1.5"/>',
  overview: '<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>',
  shield: '<path d="M12 3l8 3v6c0 5-3.5 8-8 9-4.5-1-8-4-8-9V6z"/><path d="M9 12l2 2 4-4"/>',
  problem: '<path d="M12 3l10 18H2z"/><path d="M12 10v5M12 18h.01"/>',
  reset: '<path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/>',
  copy: '<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/>',
  remove: '<path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/>',
  undo: '<path d="M9 14L4 9l5-5"/><path d="M4 9h11a5 5 0 0 1 0 10h-3"/>',
  redo: '<path d="M15 14l5-5-5-5"/><path d="M20 9H9a5 5 0 0 0 0 10h3"/>',
  search: '<circle cx="11" cy="11" r="7"/><path d="M20 20l-4-4"/>',
  play: '<path d="M7 4l13 8-13 8z" fill="currentColor" stroke="none"/>',
  people: '<circle cx="9" cy="8" r="3"/><path d="M3 20a6 6 0 0 1 12 0"/><circle cx="17" cy="9" r="2.5"/><path d="M15.5 14.2A5 5 0 0 1 21 19"/>',
  ok: '<path d="M5 12l5 5L20 7"/>', warn: '<path d="M12 6v8M12 18h.01"/>', err: '<path d="M6 6l12 12M18 6L6 18"/>',
  lock: '<rect x="5" y="11" width="14" height="10" rx="1"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>',
  unlock: '<rect x="5" y="11" width="14" height="10" rx="1"/><path d="M8 11V7a4 4 0 0 1 7.5-2"/>',
  fold: '<path d="M15 6l-6 6 6 6"/>', unfold: '<path d="M9 6l6 6-6 6"/>', plus: '<path d="M12 5v14M5 12h14"/>',
  folder: '<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
  list: '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
  table: '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 10h18M3 15h18M9 4v16"/>',
  tree: '<circle cx="5" cy="6" r="2"/><circle cx="5" cy="18" r="2"/><circle cx="19" cy="12" r="2"/><path d="M7 6h3a4 4 0 0 1 4 4v0a2 2 0 0 0 2 2h1M7 18h3a4 4 0 0 0 4-4v0a2 2 0 0 1 2-2"/>',
  card: '<rect x="3" y="5" width="18" height="14" rx="1"/><path d="M3 9h18"/>',
  form: '<path d="M4 6h16M4 12h16M4 18h10"/>',
  up: '<path d="M6 15l6-6 6 6"/>', down: '<path d="M6 9l6 6 6-6"/>',
  tv: '<rect x="3" y="6" width="18" height="13" rx="2"/><path d="M8 3l4 3 4-3"/>',
  gear: '<path d="M19.22 9.33 L22.05 10.23 L22.05 13.77 L19.22 14.67 L18.99 15.22 L20.36 17.85 L17.85 20.36 L15.22 18.99 L14.67 19.22 L13.77 22.05 L10.23 22.05 L9.33 19.22 L8.78 18.99 L6.15 20.36 L3.64 17.85 L5.01 15.22 L4.78 14.67 L1.95 13.77 L1.95 10.23 L4.78 9.33 L5.01 8.78 L3.64 6.15 L6.15 3.64 L8.78 5.01 L9.33 4.78 L10.23 1.95 L13.77 1.95 L14.67 4.78 L15.22 5.01 L17.85 3.64 L20.36 6.15 L18.99 8.78Z"/><circle cx="12" cy="12" r="3.2"/>',
  globe: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/>',
  lan: '<rect x="9" y="3" width="6" height="5" rx="1"/><rect x="3" y="16" width="6" height="5" rx="1"/><rect x="15" y="16" width="6" height="5" rx="1"/><path d="M12 8v4M6 16v-4h12v4"/>',
};
const icon = (name, size = 18) => h('span', { class: 'icon', html: `<svg width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ICONS[name]}</svg>` });

const cur = () => SECTIONS.find((s) => s.key === st.section) || null;
const secByKey = (k) => SECTIONS.find((s) => s.key === k);
const ALL_TABLES = () => [...new Set(SECTIONS.map((s) => s.table).concat(['news_templates']))];
const What = (sec) => sec.what[0].toUpperCase() + sec.what.slice(1);

/* ---------------------------------------------------------------- slots, pools, usage */
function poolBlock(table, r) {
  if (r.owner) return T('it belongs to a station');
  if (producedBy(table, r.id).length) return T('a screenplay produces it');
  if (!r.blocks || (table === 'series' && !r.episodes)) return T('it has no length or episodes (unfinished)');
  if (table === 'films' && r.genre === 7) return T('live broadcasts have their dates');
  if (table === 'films' && SPECIAL.films[r.id]) return T("the game's code uses it by number");
  return '';
}
function poolRule(table) {
  const marked = table === 'films' ? filmPool() : seriesPool();
  const now = table === 'films' ? TP(marked.length, '{n} film marked', '{n} films marked') : TP(marked.length, '{n} series marked', '{n} series marked|many');
  const rule = table === 'films'
    ? (marked.length ? T('A new game gives each station 3 films: marked ones (any genre) while one is free, then cultural programmes from the market.') : T('Nothing marked: the original rule -- each station gets 3 cultural programmes from the market ({n} now).', { n: origStartFilms().length }))
    : (marked.length ? T('A new game gives each station one series: a marked one while one is free, then one of the series 18, 19, 20.') : T('Nothing marked: the original rule -- each station gets one of the series 18, 19, 20.'));
  return `${now}. ${rule}`;
}
function producedBy(table, id) {
  return records('screenplays').filter((sp) => sp.target === id && spIsSeries(sp) === (table === 'series')).map((sp) => sp.id);
}
function usage(sec, r) {                          /* -> { free, uses: [why it is in use], keep: [why it cannot be removed] } */
  const uses = [], keep = [];
  const special = (SPECIAL[sec.table] || {})[r.id];
  if (special) keep.push(special);
  if (sec.key === 'films' || sec.key === 'series') {
    if (sec.key === 'series' && seriesPool().filter((s) => !s.owner).length < 3 && [18, 19, 20].includes(r.id)) keep.push(T('a new game may give it to a station (series 18-20 fill up the starting pool)'));
    if (r.flags2 & POOL) uses.push(T('starting pool'));
    if (r.owner >= 1 && r.owner <= 3) keep.push(T("in {station}'s start library", { station: STATIONS[r.owner] }));
    const by = producedBy(sec.table, r.id);
    if (by.length) keep.push(T('produced by screenplay {list}', { list: by.join(', ') }));
    if (r.flags & 0x20) uses.push(r.flags & 0x40 ? T('auction pool') : T('film dealer'));
  } else if (sec.key === 'ads') {
    if (r.fee) uses.push(T('ad agency'));
  } else if (sec.key === 'screenplays') {
    if (r.flags & 2) uses.push(T('screenplay agency'));
  } else return { free: false, uses: [T('used')], keep: [T('stories and topics cannot be added or removed yet')] };
  return { free: !uses.length && !keep.length, uses, keep };
}
function removeEntry(sec, r) {
  if (sec.key === 'films' || sec.key === 'series') { r.flags &= ~0x60; r.flags2 &= ~POOL; }
  else if (sec.key === 'ads') { r.fee = 0; r.penalty = 0; }
  else if (sec.key === 'screenplays') r.flags &= ~2;
}
function freeSlots(sec) { return records(sec.table).filter((r) => usage(sec, r).free); }
/* every entry in the game that misses something: -> ['Film #201 "x": German title, price', ...] */
function incomplete() {
  const out = [];
  for (const sec of SECTIONS) {
    if (!REQUIRED[sec.key] || !st.data[sec.table]) continue;
    for (const r of records(sec.table)) {
      const m = missing(sec, r);
      if (m.length) out.push(T('{what} #{id} "{title}": {missing}', { what: What(sec), id: r.id, title: titleOf(sec, r), missing: m.map((x) => x.msg).join(', ') }));
    }
  }
  return out;
}
/* taking an entry out of the game must keep it startable: -> '' or why not */
function guardRemoval(sec, r, before) {
  const was = usage(sec, before), now = usage(sec, r);
  if (now.uses.length < was.uses.length && was.keep.length) return T('#{id} stays in the game: {why}.', { id: r.id, why: was.keep.join('; ') });
  const keep = clone(r);
  restore(r, before);
  const had = gameChecks().length;
  restore(r, keep);
  const g = gameChecks();
  return g.length > had ? T('#{id} stays: {why}', { id: r.id, why: g[g.length - 1] }) : '';
}
const GUARDS = { market: guardRemoval };
function restore(r, snap) { for (const k of Object.keys(r)) if (!(k in snap)) delete r[k]; Object.assign(r, clone(snap)); }

/* ---------------------------------------------------------------- tables that can grow (films now; series and ads once the engine takes more) */
const tableInfo = (t) => st.meta.tables[t];
const maxId = (t) => tableInfo(t).max - 1;
const nextId = (t) => Math.max(...st.data[t].records.map((r) => r.id)) + 1;
const canGrow = (sec) => !!sec && !!sec.blank && tableInfo(sec.table).max > tableInfo(sec.table).count;
const addedEntry = (sec, r) => !origById(sec.table, r.id);
function newEntry(sec) {
  const id = nextId(sec.table);
  if (id > maxId(sec.table)) { status(T('No room for another {what}: the game takes {many} up to #{max}.', { what: sec.what, many: sec.many, max: maxId(sec.table) }), 'err'); return; }
  st.data[sec.table].records.push(sec.blank(id));
  st.section = sec.key; st.sel[sec.key] = id; st.mode = 'form'; st.view[sec.key] = 'list';
  markDirty([sec.table]); renderAll();
  status(REQUIRED[sec.key] ? T('{new}: fill in the fields marked red ({hint}); then it goes into the game.', { new: sec.newOne(id), hint: REQUIRED[sec.key].hint() })
    : T('{new}: fill in its texts and values.', { new: sec.newOne(id) }), 'ok');
}

/* ---------------------------------------------------------------- copy, remove, delete */
function copyEntry(sec, src) {
  if (sec.key === 'screenplays') return copyScreenplay(src);
  const dst = freeSlots(sec).find((r) => r.id !== src.id);
  const add = canGrow(sec) && nextId(sec.table) <= maxId(sec.table) ? nextId(sec.table) : 0;
  if (!dst && !add) { status(T('No free {many} slot: the game has {n}. Remove one first.', { many: sec.many, n: records(sec.table).length }), 'err'); return; }
  if (!add) {
    dialog(T('Copy'), h('div', {}, T('Copy "{title}" into slot #{slot}?', { title: titleOf(sec, src), slot: dst.id }), h('br'),
      h('span', { class: 'hint' }, T('The slot holds "{title}", which is not in the game; it is replaced.', { title: titleOf(sec, dst) }))),
      [[T('Cancel'), 'grey'], [T('Copy'), 'green', () => doCopy(sec, src, dst)]]);
    return;
  }
  dialog(T('Copy'), h('div', {}, T('Copy "{title}" as a new {what} #{id}?', { title: titleOf(sec, src), what: sec.what, id: add }), h('br'),
    h('span', { class: 'hint' }, T("Entries beyond the original's {n} play in the decompiled game (play.sh; not --lifted), up to #{max}.", { n: tableInfo(sec.table).count - 1, max: maxId(sec.table) })),
    dst ? h('div', { class: 'hint', style: 'margin-top:6px' }, T('Or into the free slot #{slot}, replacing "{title}", which is not in the game.', { slot: dst.id, title: titleOf(sec, dst) })) : null),
    [[T('Cancel'), 'grey'], ...(dst ? [[T('Slot #{id}', { id: dst.id }), 'grey', () => doCopy(sec, src, dst)]] : []), [sec.newOne(add), 'green', () => doCopy(sec, src, null, add)]]);
}
function doCopy(sec, src, dst, add) {
  const recs = st.data[sec.table].records;
  const c = clone(src); c.id = dst ? dst.id : add;
  for (const l of LANGS) if (c.text && Array.isArray(c.text[l])) c.text[l][0] = (c.text[l][0] || '') + (sec.key === 'ads' ? '' : ' 2');
  if (sec.key === 'films' || sec.key === 'series') { c.owner = 0; c.flags = (c.flags | 0x20) & ~0x40; c.flags2 &= ~POOL; }
  if (sec.key === 'ads') { c.holder = 0; if (!c.fee) c.fee = 50000; }
  if (dst) recs[recs.indexOf(dst)] = c; else recs.push(c);
  st.sel[sec.key] = c.id; markDirty([sec.table]); renderShelf(); renderEntry();
  status(dst ? T('Copied into slot #{id}: edit it there. It is offered in the game from the next refill.', { id: c.id })
    : T('{new}: edit it there. It is offered in the game from the next refill.', { new: sec.newOne(c.id) }), 'ok');
}
/* an added entry out of the game: away with it (a gap in the ids stays an empty record) */
function deleteAdded(sec, rec) {
  dialog(T('Delete'), h('div', {}, T('Delete {what} #{id} "{title}"?', { what: sec.what, id: rec.id, title: titleOf(sec, rec) }), h('br'),
    h('span', { class: 'hint' }, T('It was added in this pack; a game saved with it in a library loses it.'))),
  [[T('Cancel'), 'grey'], [T('Delete'), 'red', () => {
    const recs = st.data[sec.table].records; recs.splice(recs.indexOf(rec), 1);
    st.sel[sec.key] = null; markDirty([sec.table]); renderShelf(); renderEntry();
    status(T('{what} #{id} deleted.', { what: What(sec), id: rec.id }), 'ok');
  }]]);
}
function removeAsk(sec, rec) {
  const u = usage(sec, rec);
  if (u.keep.length) { status(T('#{id} stays in the game: {why}.', { id: rec.id, why: u.keep.join('; ') }), 'err'); return; }
  const before = clone(rec);
  removeEntry(sec, rec);
  const err = guardRemoval(sec, rec, before);
  if (err) { restore(rec, before); status(err, 'err'); return; }
  markDirty([sec.table]); renderShelf(); renderEntry();
  status(T('#{id} "{title}" is out of the game; its slot is free (Copy or edit to use it again).', { id: rec.id, title: titleOf(sec, rec) }), 'ok');
}
function copyScreenplay(src) {
  const sec = secByKey('screenplays');
  const kind = spIsSeries(src);
  const dst = freeSlots(sec).find((r) => spIsSeries(r) === kind && r.id !== src.id && spTarget(r));
  if (!dst) { status(kind ? T('No unused series screenplay slot left (it brings its own series slot). Remove one first.') : T('No unused film screenplay slot left (it brings its own film slot). Remove one first.'), 'err'); return; }
  const o = { title: titleOf(sec, dst) };
  dialog(T('Copy'), h('div', {}, T('Copy "{title}" into screenplay slot #{slot}?', { title: titleOf(sec, src), slot: dst.id }), h('br'),
    h('span', { class: 'hint' }, kind ? T('The slot holds "{title}" (not offered in the game) with its series; both are replaced.', o) : T('The slot holds "{title}" (not offered in the game) with its film; both are replaced.', o))),
  [[T('Cancel'), 'grey'], [T('Copy'), 'green', () => doCopyScreenplay(src, dst)]]);
}
function doCopyScreenplay(src, dst) {
  const recs = st.data.screenplays.records;
  const kind = spIsSeries(src);
  const table = kind ? 'series' : 'films';
  const tgt = spTarget(dst), from = spTarget(src);
  const t = clone(from); t.id = tgt.id; t.owner = 0; t.flags &= ~0x60;          /* produced, not sold: off the market */
  for (const l of LANGS) if (t.text && Array.isArray(t.text[l])) t.text[l][0] = (t.text[l][0] || '') + ' 2';
  const tr = st.data[table].records; tr[tr.indexOf(tgt)] = t;
  const c = clone(src); c.id = dst.id; c.target = tgt.id; c.flags |= 2;
  recs[recs.indexOf(dst)] = c;
  st.sel.screenplays = c.id; markDirty(['screenplays', table]); renderShelf(); renderEntry();
  const o = { slot: c.id, id: tgt.id, title: txt(t, 0) };
  status(kind ? T('Copied into screenplay slot #{slot}, producing series #{id} "{title}" (the slot was unused in the original).', o)
    : T('Copied into screenplay slot #{slot}, producing film #{id} "{title}" (the slot was unused in the original).', o), 'ok');
}

/* ---------------------------------------------------------------- changes: dirty, undo, refresh */
/* the last message: shown for a moment (an error longer), its text stays in #status */
function status(msg, cls = '') {
  const s = $('#status');
  s.textContent = msg; s.className = cls;
  clearTimeout(status.timer);
  if (!msg) return;
  requestAnimationFrame(() => s.classList.add('show'));
  status.timer = setTimeout(() => s.classList.remove('show'), cls === 'err' ? 9000 : 4500);
}
function snapBase() { st.base = {}; for (const t of ALL_TABLES()) st.base[t] = JSON.stringify(st.data[t].records); st.undo = []; st.redo = []; st.lastKey = null; updateUndo(); }
function markDirty(tables) {
  tables = tables || [cur()?.table].filter(Boolean);
  if (!st.mod && !st.forking) st.forking = forkSelection().catch((e) => status(T('No new pack: {why}', { why: e.message }), 'err')).finally(() => { st.forking = null; });
  for (const t of tables) st.dirty.add(t);
  const entry = {};
  for (const t of new Set([...tables, ...st.dirty])) {
    const j = JSON.stringify(st.data[t].records);
    if (j !== st.base[t]) { entry[t] = st.base[t]; st.base[t] = j; }
  }
  if (Object.keys(entry).length) {
    const key = Object.keys(entry).join() + '|' + (st.undoKey || Math.random()), now = Date.now();
    if (!(key === st.lastKey && now - st.lastTime < 1500 && st.undo.length)) {     /* typing in one field: one step */
      st.undo.push({ tables: entry, section: st.section, id: st.sel[st.section] });
      if (st.undo.length > 300) st.undo.shift();
    }
    st.lastKey = key; st.lastTime = now; st.redo = [];
  }
  st.undoKey = null;
  st.version = (st.version || 0) + 1;
  refreshListRow(); renderRailCounts(); scheduleStatusBar(); updateUndo();
}
/* the tables as the messages name them */
const tableName = (t) => ({ films: T('films'), series: T('series|many'), ads: T('ads'), screenplays: T('screenplays'), news_nodes: T('news_nodes'), news_topics: T('news_topics'), news_templates: T('news_templates') })[t] || t;
function undoStep(from, to, redoing) {
  const e = from.pop();
  if (!e) { status(redoing ? T('Nothing to redo.') : T('Nothing to undo.')); return; }
  const back = {};
  for (const [t, j] of Object.entries(e.tables)) {
    back[t] = JSON.stringify(st.data[t].records);
    st.data[t].records = JSON.parse(j); st.base[t] = j; st.dirty.add(t);
  }
  to.push({ tables: back, section: e.section, id: e.id });
  st.lastKey = null;
  if (e.section) { st.section = e.section; if (e.id) st.sel[e.section] = e.id; }
  st.version = (st.version || 0) + 1;
  renderAll(); updateUndo();
  const tables = Object.keys(e.tables).map(tableName).join(', ');
  status(redoing ? T('Redone: {tables}.', { tables }) : T('Undone: {tables}.', { tables }));
}
const undo = () => undoStep(st.undo, st.redo, false);
const redo = () => undoStep(st.redo, st.undo, true);
function updateUndo() { const u = $('#undo'), r = $('#redo'); if (u) { u.disabled = !st.undo.length; r.disabled = !st.redo.length; } }
/* after a change made in the form: the record is changed already */
function touched(o = {}) {
  markDirty(o.tables);
  if (o.rerender) renderEntry();
  else { runLive(); }
}
function runLive() { for (const f of st.liveWork || []) f(); for (const f of st.liveInsp || []) f(); }

/* ---------------------------------------------------------------- the selection: what is shown and played */
/* the original game alone ([]), or packs laid over it in their order -- the same as the
   launcher's (launcher.js), kept in this browser.  A change goes into a pack of the player's
   own: one own pack chosen alone is changed in place (st.mod); anything else -- several packs,
   the original alone, a protected pack (mod.json "protected", the content packs) -- makes a new
   pack with all of the selection's content first (forkSelection), which is the selection from
   then on.  A protected pack is changed in place only after "Change it directly" (overview). */
const PACKS_KEY = 'madtv-editor-launch-packs';
function keptPacks() {
  try { const v = JSON.parse(localStorage.getItem(PACKS_KEY) || 'null'); return Array.isArray(v) ? v.filter((n) => typeof n === 'string') : null; } catch (e) { return null; }
}
function keepPacks() { try { localStorage.setItem(PACKS_KEY, JSON.stringify(st.packs)); } catch (e) { /* private window */ } }
const modInfo = (name) => st.mods.find((m) => m.name === name);
const packTitle = (name) => (modInfo(name) || {}).title || name;
const packSpec = (packs = st.packs) => packs.join('+') || 'original';
const editTarget = () => {
  const m = st.packs.length === 1 && modInfo(st.packs[0]);
  return m && (!m.protected || st.unlocked.has(m.name)) ? m.name : null;
};
/* the packs a pack holds: itself, and for a complete pack the packs it was made from (as the server checks) */
function packHolds(name, seen = new Set()) {
  if (seen.has(name)) return new Set();
  seen.add(name);
  const m = modInfo(name), out = new Set([name]);
  if (m && m.complete) for (const f of m.from || []) if (modInfo(f)) for (const x of packHolds(f, seen)) out.add(x);
  return out;
}
const holdsOriginal = (packs = st.packs) => packs.some((n) => modInfo(n)?.complete);
async function loadSelection(packs, o = {}) {
  packs = packs.filter((n) => modInfo(n) && !modInfo(n).error);     /* (a pack that cannot be read stays out) */
  if (player()) {                                   /* (no tables: the launcher only plays them) */
    st.packs = packs; keepPacks();
    st.mod = null;
    delete st.packInfo?.[packSpec(packs)];
    renderAll();
    return;
  }
  const tables = ALL_TABLES();
  let docs;
  if (packs.length === 1) docs = await Promise.all(tables.map((t) => api('GET', `/api/mods/${packs[0]}/${t}`)));
  else { const r = await api('GET', `/api/selection/${packSpec(packs)}`); docs = tables.map((t) => r.tables[t]); }
  st.packs = packs; keepPacks();
  tables.forEach((t, i) => { st.data[t] = docs[i]; });
  st.mod = editTarget();
  st.dirty.clear();
  snapBase();
  delete st.packInfo?.[packSpec(packs)];
  renderAll();
  if (!o.quiet) status(st.mod ? T('Pack "{name}" loaded.', { name: packTitle(st.mod) }) : T('Loaded: {list}. A change makes a new pack; these stay as they are.', { list: selectionList() }));
}
const loadMod = (name) => loadSelection([name]);
/* the selection in words: "Original + Neue Filme + Neue Serien" */
const selectionList = (packs = st.packs) => (holdsOriginal(packs) ? [] : [T('Original')]).concat(packs.map(packTitle)).join(' + ');
async function init() {
  const [meta, mods] = await Promise.all([api('GET', '/api/meta'), api('GET', '/api/mods')]);
  st.meta = meta; st.mods = mods;
  document.documentElement.classList.toggle('window', !!meta.window);   /* (the launcher's own window: no browser around it) */
  if (player()) {
    const alone = new URLSearchParams(location.search).has('player');
    const want = editorHash() ? location.hash : playerPage(location.hash) ? location.hash : '#launcher';
    await initPlayer();
    if (!playerPage(want) || !alone) {
      if (playerPage(want)) st.startedAt = want;
      await enterEditor(want);
    }
    return;
  }
  await loadEditorData();
  wireTop();
  fromHash();
  const q = new URLSearchParams(location.search).get('mod');
  let want = q ? q.split('+') : keptPacks();
  if (!want) { let old = null; try { old = localStorage.getItem('madtv-editor-mod'); } catch (e) { /* ignore */ } want = old && modInfo(old) ? [old] : []; }
  await loadSelection(want.filter((n) => modInfo(n)));
  setInterval(() => { api('GET', '/api/hello').catch(() => {}); }, 20000);     /* (the editor ends when its page has gone) */
}
/* the player's launcher: the pages "Play", "Multiplayer" and "Settings"; the server ends when it
   has not heard of the page for a while, so the page says it is there */
async function initPlayer() {
  document.body.classList.add('player');
  $('#subtitle').textContent = T('Launcher');
  fromHash();
  if (!playerPage(st.section)) st.section = 'launcher';
  wireTop();
  const q = new URLSearchParams(location.search).get('mod');
  await loadSelection(q ? q.split('+') : keptPacks() || []);
  setInterval(() => { api('GET', '/api/hello').catch(() => {}); }, 20000);
}
/* what only the editor needs: the game's font and glyphs, the original's tables, the board's
   look as last left */
async function loadEditorData() {
  await document.fonts.load('16px MadTV');
  await loadGlyphs();
  const tables = ALL_TABLES();
  const [orig, strings] = await Promise.all([Promise.all(tables.map((t) => api('GET', `/api/original/${t}`))), api('GET', '/api/strings')]);
  tables.forEach((t, i) => { st.orig[t] = orig[i]; });
  st.strings = strings;                          /* the game's own texts (names of actor categories, the shop's items) */
  try { st.railFolded = localStorage.getItem('madtv-editor-rail') === 'folded'; } catch (e) { /* ignore */ }
  try { st.cardScale = +localStorage.getItem('madtv-editor-scale') || 3; } catch (e) { /* ignore */ }
}
/* a link to the editor: #editor, the overview or an editor's page (#films/12, ...) */
function editorHash() {
  const s = location.hash.slice(1).split('/')[0];
  return s === 'editor' || s === 'overview' || !!secByKey(s);
}
/* the launcher's page becomes the content editor: its data loaded now, the selection with its
   tables; the editor's board (play, multiplayer and settings on it too), at WANT (#editor: its
   first page; #launcher, #multiplayer, #settings: those, as the player's start has it) */
async function enterEditor(want = '#films') {
  if (!player()) return;
  status(T('Loading the editor ...'));
  try {
    const meta = await api('GET', '/api/meta?editor=1');
    st.meta = Object.assign(meta, { version: st.meta.version, play: st.meta.play, window: st.meta.window });   /* (no "player": the editor's page) */
    await loadEditorData();
  } catch (e) { st.meta.player = true; status(T('The editor could not start: {why}', { why: e.message }), 'err'); return; }
  document.body.classList.remove('player');
  $('#subtitle').textContent = T('Editor');
  $('#subtitle').hidden = true;                 /* (one page for all: the name alone in the top bar) */
  history.replaceState(null, '', want === '#editor' ? '#films' : want);
  st.section = 'films';
  fromHash();
  await loadSelection(st.packs, { quiet: playerPage(want) });
  if (playerPage(want)) status('');
}
/* a choice of packs (the launcher, the overview): unsaved changes first saved or discarded */
async function selectPacks(packs) {
  if (same(packs, st.packs)) return true;
  if (st.forking) await st.forking;
  if (st.dirty.size) {
    const what = await askUnsaved();
    if (!what) return false;
    if (what === 'save' && !(await save())) return false;
  }
  await loadSelection(packs);
  return true;
}
const switchMod = (name) => selectPacks([name]);
function askUnsaved() {
  return new Promise((resolve) => {
    let answer = null;
    dialog(T('Unsaved changes'), h('div', {}, T('Unsaved changes in "{mod}". Save them first?', { mod: packTitle(st.mod) })),
      [[T('Cancel'), 'grey'], [T('Discard'), 'grey', () => { answer = 'discard'; }], [T('Save'), 'green', () => { answer = 'save'; }]], () => resolve(answer));
  });
}
/* the first change of a selection that is not one own pack: a new pack with all of its content
   (the server combines it, mod.json "complete" and "from"); the change itself is not saved yet */
async function forkSelection() {
  const from = st.packs.slice();
  let n = 1;
  while (modInfo('pack-' + n)) n++;
  const name = 'pack-' + n, list = selectionList(from);
  await api('POST', '/api/mods', { name, title: T('My pack {n}', { n }), description: T('Made in the editor from: {list}', { list }), from: packSpec(from), complete: true });
  st.mods = await api('GET', '/api/mods');
  st.packs = [name]; keepPacks();
  st.mod = name;
  renderRail();
  if (st.section === 'launcher' || st.section === 'overview') renderEntry();
  status(T('New pack "{title}": all of {list}, with your change. The packs you chose stay as they are.', { title: packTitle(name), list }), 'ok');
}

function wireTop() {
  $('#reset').addEventListener('click', () => discardDialog());
  $('#save').addEventListener('click', save);
  $('#undo').addEventListener('click', undo);
  $('#redo').addEventListener('click', redo);
  $$('.lang').forEach((b) => b.addEventListener('click', () => { st.lang = b.dataset.lang; st.plang = null; renderAll(); }));
  $$('.uilang').forEach((b) => b.addEventListener('click', () => { if (b.dataset.ui !== UI.lang) setUiLang(b.dataset.ui); }));
  const menu = $('#menu'), pop = $('#menupop');
  const openMenu = (on) => { pop.classList.toggle('open', on); menu.setAttribute('aria-expanded', on ? 'true' : 'false'); };
  menu.addEventListener('click', (e) => { e.stopPropagation(); openMenu(!pop.classList.contains('open')); });
  document.addEventListener('click', (e) => { if (!e.target.closest('.menuwrap')) openMenu(false); });
  wireSearch();
  document.addEventListener('keydown', (e) => {
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key === 's') { e.preventDefault(); save(); return; }
    if (mod && (e.key === 'k' || e.key === 'K')) { e.preventDefault(); $('#gsearch').focus(); $('#gsearch').select(); return; }
    if (e.key === 'Escape') { closePopover(); hideResults(); openMenu(false); }
    const inText = e.target.matches('input[type=text], textarea');
    if (mod && !inText && (e.key === 'z' || e.key === 'Z')) { e.preventDefault(); if (e.shiftKey) redo(); else undo(); return; }
    if (mod && !inText && e.key === 'y') { e.preventDefault(); redo(); return; }
    if (e.target.matches('input, textarea, select')) return;
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') { e.preventDefault(); stepSel(e.key === 'ArrowDown' ? 1 : -1); }
  });
  window.addEventListener('beforeunload', (e) => { if (st.dirty.size) { e.preventDefault(); e.returnValue = ''; } });
  document.addEventListener('mouseover', (e) => {
    const t = e.target.closest('[data-tip]');
    const tip = $('#tip');
    if (!t || !t.dataset.tip) { tip.style.display = 'none'; return; }
    tip.innerHTML = '';
    if (t.dataset.tiptitle) tip.append(h('b', {}, t.dataset.tiptitle));
    tip.append(t.dataset.tip);
    tip.style.display = 'block';
  });
  document.addEventListener('mousemove', (e) => {
    const tip = $('#tip');
    if (tip.style.display !== 'block') return;
    const x = Math.min(e.clientX + 16, window.innerWidth - tip.offsetWidth - 8);
    const y = e.clientY + 20 + tip.offsetHeight > window.innerHeight ? e.clientY - tip.offsetHeight - 12 : e.clientY + 20;
    tip.style.left = x + 'px'; tip.style.top = y + 'px';
  });
}

async function save() {
  if (st.forking) await st.forking;
  if (!st.dirty.size) { status(T('Nothing to save.')); return true; }
  if (!st.mod) { status(T('Not saved: {why}', { why: T('no pack to save into') }), 'err'); return false; }
  const problems = gameChecks();
  if (problems.length) { status(T('Not saved: {why}', { why: problems.join(' ') }), 'err'); return false; }
  try {
    const unlock = !!modInfo(st.mod)?.protected || undefined;
    for (const t of [...st.dirty]) {
      await api('PUT', `/api/mods/${st.mod}/${t}`, { records: st.data[t].records, unlock });
      st.dirty.delete(t);
    }
    st.mods = await api('GET', '/api/mods');
    renderRailCounts(); renderStatusBar();
    if (st.section === 'overview' || st.section === 'launcher') renderAll();
    const inc = incomplete();
    if (inc.length) status(T('Saved "{mod}" -- not playable yet: {why}.', { mod: st.mod, why: inc.join('; ') }), 'err');
    else status(T('Saved "{mod}".', { mod: st.mod }), 'ok');
    return true;
  } catch (e) {
    status(T('Not saved: {why}', { why: e.message }), 'err');
    return false;
  }
}
/* the log keeps the messages as keys (the self-test reads them from the playLog calls),
   so a switch of the interface language shows them anew */
function playLog(key, cls = '', fresh = false, params = null) {
  if (fresh) st.log = [];
  st.log = st.log || [];
  st.log.push({ t: new Date().toLocaleTimeString('en-GB'), key, params, cls });
  const el = $('#playlog');
  if (el) fillLog(el);
}
function fillLog(el) {
  el.innerHTML = '';
  for (const x of st.log || []) el.append(h('div', { class: x.cls }, `${x.t}  ${T(x.key, x.params)}`));
  if (!(st.log || []).length) el.append(h('div', { class: 'hint' }, T('Build and start messages appear here.')));
  el.closest('.logstrip')?.classList.toggle('empty', !(st.log || []).length);      /* (the launcher's strip: there once there are any) */
}

/* ---------------------------------------------------------------- dialogs */
function dialog(title, body, buttons, onClose) {
  const ov = $('#overlay');
  ov.innerHTML = '';
  const close = () => { ov.classList.remove('open'); ov.innerHTML = ''; onClose?.(); };
  const box = h('div', { class: 'box dialog' }, h('div', { class: 'boxtitle' }, title),
    h('div', { class: 'body' }, body, h('div', { class: 'buttons' },
      ...buttons.map(([lab, cls, fn]) => h('button', { class: 'plate ' + cls, onclick: async () => { if ((await fn?.()) !== false) close(); } }, lab)))));
  ov.append(box);
  ov.classList.add('open');
  ov.onclick = (e) => { if (e.target === ov) close(); };
  return close;
}
/* "Reset" in the header: the mod as it was last saved -- only the unsaved changes go */
function discardDialog() {
  if (!st.dirty.size) { status(T('Nothing to reset: "{mod}" has no unsaved changes.', { mod: st.mod })); return; }
  dialog(T('Reset'), h('div', {}, T('Discard the unsaved changes in "{mod}"?', { mod: st.mod }), h('br'),
    h('span', { class: 'hint' }, T('The pack goes back to how it was last saved.'))),
  [[T('Cancel'), 'grey'], [T('Discard'), 'red', async () => { await loadSelection(st.packs, { quiet: true }); status(T('"{mod}" as last saved.', { mod: st.mod }), 'ok'); }]]);
}
/* the mod overview: the original game's data for the whole mod or one section -- not saved
 * at once: "Save" keeps it, "Reset" brings the saved mod back */
function resetDialog(onlySec) {
  const sec = onlySec || cur();
  const tables = ALL_TABLES();
  const extra = (ts) => ts.reduce((n, t) => n + st.data[t].records.filter((r) => !st.orig[t].records.some((o) => o.id === r.id)).length, 0);
  const reset = (which) => {
    for (const t of which) { st.data[t] = clone(st.orig[t]); st.dirty.add(t); }
    markDirty(which);
    renderAll();
    status(T('Back to the original -- not saved yet: "Save" keeps it, "Reset" brings the pack back as saved.'), 'ok');
  };
  const nAll = extra(tables), nSec = sec ? extra([sec.table]) : 0;
  dialog(T('Back to the original'), h('div', {}, T('Set the pack "{mod}" back to the original game\'s data?', { mod: st.mod }), h('br'),
    h('span', { class: 'hint' }, T('Not saved yet: "Save" keeps it, "Reset" brings the pack back as saved. (The original here has the damaged film 200 repaired, docs/CHANGES.md.)')),
    ...(nAll ? [h('br'), h('b', {}, TP(nAll, 'One entry the original does not have goes too.', '{n} entries the original does not have go too (new films, series, ads ...).'))] : [])),
  [[T('Cancel'), 'grey'], ...(sec ? [[T('Only {label}', { label: sec.label }) + (nSec ? ` (-${nSec})` : ''), 'blue', () => reset([sec.table])]] : []), [T('Everything'), 'red', () => reset(tables)]]);
}
/* a new, empty pack (the original's tables, changed from there); it is the selection then */
function newModDialog() {
  let n = 1;
  while (modInfo('mymod' + n)) n++;
  const name = h('input', { type: 'text', value: 'mymod' + n, maxlength: 40 });
  const title = h('input', { type: 'text', value: '', placeholder: T('A title for it') });
  dialog(T('New pack'), h('div', { class: 'fields two' },
    h('label', {}, T('Name')), name, h('label', {}, T('Title')), title,
    h('div', { class: 'wide hint' }, T('Letters, digits, - and _. The pack is a folder mods/NAME with the tables as JSON; it starts as the original game and is played on it.'))), [
    [T('Cancel'), 'grey'],
    [T('Create'), 'green', async () => {
      try {
        await api('POST', '/api/mods', { name: name.value.trim(), title: title.value.trim() || name.value.trim() });
        st.mods = await api('GET', '/api/mods');
        if (!(await selectPacks([name.value.trim()]))) return false;
      } catch (e) { status(e.message, 'err'); return false; }
    }],
  ]);
}
function problemsDialog() {
  const list = allProblems();
  const body = h('div', { class: 'checks-list' }, ...(list.length ? list.map((p) => checkRow(p, p.sec ? () => { closeDlg(); go(p.sec, p.id); } : null)) : [checkRow({ level: 'ok', msg: T('No problems: the pack saves, starts and every changed text fits.') })]));
  const closeDlg = dialog(T('Problems ({n})', { n: list.length }), body, [[T('Close'), 'grey']]);
}

/* ---------------------------------------------------------------- problems of the whole mod */
let problemsCache = { v: -1, list: [] };
function allProblems() {
  if (problemsCache.v === st.version && problemsCache.mod === st.mod) return problemsCache.list;
  const out = [];
  for (const m of gameChecks()) out.push({ level: 'err', kind: 'start', msg: T('{msg} (blocks saving)', { msg: m }) });
  for (const sec of SECTIONS) {
    if (!st.data[sec.table]) continue;
    for (const r of records(sec.table)) {
      const m = missing(sec, r);
      if (m.length) out.push({ level: 'err', kind: 'incomplete', sec: sec.key, id: r.id, msg: T('{what} #{id} "{title}": {missing} (keeps the pack off the air)', { what: What(sec), id: r.id, title: titleOf(sec, r), missing: m.map((x) => x.msg).join(', ') }) });
      const o = origById(sec.table, r.id);
      if (o && same(o.text, r.text)) continue;
      for (const t of textFields(sec)) {
        for (const l of LANGS) {
          const v = txt(r, t.i, l);
          if (o && txt(o, t.i, l) === v) continue;          /* the original's own texts are not the mod's problem */
          const bad = missingChars(v);
          if (bad.length) out.push({ level: 'warn', kind: 'font', sec: sec.key, id: r.id, msg: T("{what} #{id} {label} ({lang}): not in the game's font: {chars}", { what: What(sec), id: r.id, label: t.label, lang: l.toUpperCase(), chars: bad.join(' ') }) });
          const c = t.check && v ? t.check(v, l) : null;
          if (c && c.level === 'warn') out.push({ level: 'warn', kind: 'card', sec: sec.key, id: r.id, msg: `${What(sec)} #${r.id} ${t.label} (${l.toUpperCase()}): ${c.msg}` });
        }
      }
    }
  }
  problemsCache = { v: st.version, mod: st.mod, list: out };
  return out;
}
const changedCount = () => SECTIONS.reduce((a, s) => a + (st.data[s.table] ? records(s.table).filter((r) => changed(s.table, r)).length : 0), 0);
function checkRow(p, onclick) {
  return h('div', { class: 'check ' + p.level, onclick, style: onclick ? 'cursor:pointer' : null }, h('span', { class: 'cicon' }, icon(p.level, 14)), h('span', {}, p.msg));
}

/* ---------------------------------------------------------------- unsaved changes, the checks' count */
/* in the launcher's own window: its close asks before unsaved changes go (the browser's
   beforeunload does not reach it) -- the window is told when that changes */
let toldWindow = false;
function tellWindow() {
  const dirty = st.dirty.size > 0;
  if (!st.meta?.window || dirty === toldWindow) return;
  toldWindow = dirty;
  api('POST', '/api/window', { dirty }).catch(() => { toldWindow = !dirty; });
}
let statusTimer = null;
function scheduleStatusBar() { clearTimeout(statusTimer); statusTimer = setTimeout(renderStatusBar, 120); }
function renderStatusBar() {
  const p = player() ? [] : allProblems(), errs = p.filter((x) => x.level === 'err').length;
  const flag = $('#reset');
  flag.textContent = st.dirty.size ? T('Not saved') : '';
  $('#save').classList.toggle('dirty', st.dirty.size > 0);
  tellWindow();
  const rp = $('#rail-problems');
  if (rp) {
    const c = $('.count', rp), okm = $('.okmark', rp);
    c.textContent = p.length || ''; c.className = 'count' + (p.length ? (errs ? ' err' : ' warn') : '');
    okm.style.display = p.length ? 'none' : '';
    rp.dataset.tip = p.length ? TP(p.length, '{n} problem', '{n} problems') : T('no problems');
  }
}

/* ---------------------------------------------------------------- routing */
function fromHash() {
  const parts = location.hash.slice(1).split('/');
  const s = parts[0];
  if (s === 'overview' || MODES[s]?.page) { st.section = s; return; }      /* pages: the mod overview, the launcher */
  if (!secByKey(s)) return;
  st.section = s;
  let k = 1;
  if (+parts[1]) { st.sel[s] = +parts[1]; k = 2; }
  st.view[s] = 'list'; st.mode = 'game'; st.tab[s] = 'all';
  for (const p of parts.slice(k)) {
    if (p === 'table' || p === 'tree') st.view[s] = p;
    else if (p === 'game') st.mode = 'game';
    else if (p === 'form') st.mode = 'form';
    else if (p) { st.tab[s] = p; st.mode = 'form'; }
  }
}
function toHash() {
  let want = '#' + st.section;
  const sec = cur();
  if (sec) {
    const id = st.sel[sec.key];
    if (id) want += '/' + id;
    if ((st.view[sec.key] || 'list') !== 'list') want += '/' + st.view[sec.key];
    if (st.mode === 'form' && sec.card && hasMode('game') && (st.view[sec.key] || 'list') === 'list' && (!st.tab[sec.key] || st.tab[sec.key] === 'all')) want += '/form';
    if (st.tab[sec.key] && st.tab[sec.key] !== 'all') want += '/' + st.tab[sec.key];
  }
  if (location.hash !== want) history.replaceState(null, '', want);
}
window.addEventListener('hashchange', () => { if (player() && editorHash()) enterEditor(location.hash); else { fromHash(); renderAll(); } });
function go(section, id, o = {}) {
  st.section = section;
  if (id) st.sel[section] = id;
  if (o.view) st.view[section] = o.view;
  else if (id && st.view[section] === 'table') st.view[section] = 'list';
  if (o.tab) st.tab[section] = o.tab;
  renderAll();
}
const layoutOf = () => {
  const sec = cur();
  if (!sec) return MODES[st.section]?.page ? st.section : hasMode('overview') ? 'overview' : 'entry';
  const v = st.view[sec.key];
  return v && v !== 'list' && MODES[v]?.view && hasMode(v, sec) ? v : 'entry';
};
function renderAll() {
  $$('.lang').forEach((b) => { b.classList.toggle('on', b.dataset.lang === st.lang); b.setAttribute('aria-pressed', b.dataset.lang === st.lang ? 'true' : 'false'); });
  $('#body').dataset.layout = layoutOf();
  renderRail();
  renderShelf();
  renderEntry();
  renderStatusBar();
  toHash();
}
/* the form and the inspector of the current entry (or the mode's own) */
function renderEntry() {
  closePopover();
  const lay = layoutOf();
  $('#body').dataset.layout = lay;
  st.liveWork = []; st.liveInsp = [];
  const w = $('#work'), ins = $('#inspector');
  w.innerHTML = ''; ins.innerHTML = ''; $('#workhead').innerHTML = '';
  if (MODES[lay]?.page) { MODES[lay].render(w, ins); toHash(); return; }
  if (!cur()) st.section = SECTIONS[0].key;
  const sec = cur();
  if (lay !== 'entry') { MODES[lay].render(sec, w, ins); toHash(); return; }
  const id = st.sel[sec.key];
  const rec = id && recById(sec.table, id);
  if (!rec) { w.append(h('div', { class: 'box pad' }, T('Nothing selected.'))); toHash(); return; }
  if (st.mode === 'game' && sec.card && hasMode('game')) MODES.game.render(sec, rec, w, ins);
  else { renderForm(sec, rec, w); renderInspector(sec, rec, ins); }
  toHash();
}

/* ---------------------------------------------------------------- the rail: play, sections, overview, checks */
/* icons only: folded by hand, or a screen below 1360 px (editor.css) */
const railIcons = () => st.railFolded || matchMedia('(max-width: 1360px)').matches;
function renderRail() {
  const rail = $('#rail'); rail.innerHTML = '';
  rail.classList.toggle('folded', st.railFolded);
  const tip = (label) => (railIcons() ? label : null);
  if (player()) { renderPlayerRail(rail, tip); return; }
  /* the game: play, play together -- first, tinted, so they are seen at once */
  if (hasMode('launcher') || hasMode('multiplayer')) rail.append(h('div', { class: 'boardtitle' }, T('Game')), ...playCards(tip), h('div', { class: 'railsep' }));
  rail.append(h('div', { class: 'boardtitle' }, T('Contents')));
  for (const s of SECTIONS) {
    rail.append(h('button', { class: 'nameplate' + (s.key === st.section ? ' active' : ''), 'data-key': s.key, 'data-tip': tip(s.label), 'aria-label': s.label,
      onclick: () => { st.section = s.key; renderAll(); } }, icon(s.icon, 18), h('span', { class: 'lbl' }, s.label), h('span', { class: 'chg' }), h('span', { class: 'num' }, '')));
  }
  rail.append(h('div', { class: 'railsep' }), h('div', { class: 'boardtitle' }, T('Selection')));
  if (hasMode('overview')) rail.append(h('button', { class: 'nameplate' + (st.section === 'overview' ? ' active' : ''), 'data-key': 'overview', 'data-tip': tip(MODES.overview.label), 'aria-label': MODES.overview.label,
    onclick: () => { st.section = 'overview'; renderAll(); } }, icon('overview', 18), h('span', { class: 'lbl' }, MODES.overview.label)));
  rail.append(h('button', { class: 'nameplate', id: 'rail-problems', 'aria-label': T('Checks'), onclick: problemsDialog }, icon('shield', 18), h('span', { class: 'lbl' }, T('Checks')),
    h('span', { class: 'count' }), h('span', { class: 'okmark' }, icon('ok', 12), T('ok'))));
  /* the end of the launcher (and the editor), the version: at the board's foot -- the end only
     in the browser (in its own window, closing the window ends it) */
  if (hasMode('launcher')) {
    if (!st.meta.window) rail.append(h('button', { class: 'nameplate quitplate', id: 'quitlauncher', 'aria-label': T('Quit the launcher'), 'data-tip': T('Ends the launcher (and the editor): this page stops working. A game started from it plays on.'),
      onclick: quitLauncher }, icon('err', 18), h('span', { class: 'lbl' }, T('Quit the launcher'))));
    const ver = railVersion();
    if (ver) rail.append(ver);
  }
  rail.append(h('button', { class: 'foldbtn', 'data-tip': st.railFolded ? T('Unfold the board') : T('Fold the board to icons'), 'aria-label': st.railFolded ? T('Unfold the board') : T('Fold the board to icons'), onclick: () => {
    st.railFolded = !st.railFolded;
    try { localStorage.setItem('madtv-editor-rail', st.railFolded ? 'folded' : ''); } catch (e) { /* ignore */ }
    renderRail();
  } }, icon(st.railFolded ? 'unfold' : 'fold', 16)));
  renderRailCounts();
}
/* the player's board: play, play together; the editor (this page becomes it); the end */
/* the board's play cards: "Play", "Multiplayer", "Settings" (the display, sound and mouse of both) */
function playCards(tip) {
  const card = (key, cls, ic, size, sub) => h('button', { class: 'playcard' + cls + (st.section === key ? ' active' : ''), 'data-key': key, 'data-tip': tip(MODES[key].label), 'aria-label': MODES[key].label,
    onclick: () => { st.section = key; renderAll(); } }, h('span', { class: 'disc' }, icon(ic, size)), h('span', { class: 'pt' }, h('b', {}, MODES[key].label), h('span', {}, sub)));
  return [hasMode('launcher') ? card('launcher', '', 'play', 13, T('Packs and rules')) : null,
    hasMode('multiplayer') ? card('multiplayer', ' mpcard', 'people', 14, T('Online or in the LAN')) : null,
    hasMode('settings') ? card('settings', ' prefcard', 'gear', 15, T('Display, sound, mouse')) : null].filter(Boolean);
}
function renderPlayerRail(rail, tip) {
  const away = T('The content editor: films, series, ads, news.');
  rail.append(h('div', { class: 'boardtitle' }, T('Game')), ...playCards(tip),
    h('div', { class: 'railsep' }),
    h('button', { class: 'nameplate', 'data-key': 'editor', 'data-tip': away, 'aria-label': T('Editor'), onclick: () => enterEditor() },
      icon('film', 18), h('span', { class: 'lbl' }, T('Editor'))),
    ...(st.meta.window ? [] : [h('button', { class: 'nameplate quitplate', id: 'quitlauncher', 'aria-label': T('Quit the launcher'), 'data-tip': T('Ends the launcher: this page stops working. A game started from it plays on.'),
      onclick: quitLauncher }, icon('err', 18), h('span', { class: 'lbl' }, T('Quit the launcher')))]),
    railVersion());
}
/* the version at the board's foot, as the release page writes it: the commit's first 7 (the
   whole one in its tooltip); on the board of icons (folded, or a window under 1360 px) the
   number alone.  The launcher's (/api/meta), else the lobby's (the Python editor) */
const shortVersion = (v) => (/^[0-9a-f]{8,}$/.test(v) ? v.slice(0, 7) : v);
function railVersion() {
  const v = st.meta.version || st.mp?.snap?.version;
  if (!v) return null;
  return h('div', { class: 'railver', 'data-tip': T('Version {version}', { version: v }) }, h('span', { class: 'lbl' }, T('Version')), ' ', shortVersion(v));
}
function renderRailCounts() {
  $$('#rail .nameplate[data-key]').forEach((el) => {
    const s = secByKey(el.dataset.key);
    if (!s || !st.data[s.table]) return;
    const n = records(s.table).filter((r) => changed(s.table, r)).length;
    el.querySelector('.num').textContent = records(s.table).length;
    const c = el.querySelector('.chg');
    c.textContent = n ? n : ''; c.classList.toggle('on', !!n);
    c.setAttribute('data-tip', n ? T('{n} changed', { n }) : '');
  });
  renderStatusBar();
}

/* ---------------------------------------------------------------- the list column (the shelf) */
const filt = (sec) => (st.filter[sec.key] = st.filter[sec.key] || { q: '', genre: 0, chips: {} });
/* the genre box of the list and the table: "all genres" (0) and the section's genres */
function genreSelect(sec, cur, pick, attrs = {}) {
  return h('select', Object.assign({ 'aria-label': T('Genre'), onchange: (e) => pick(+e.target.value) }, attrs),
    h('option', { value: 0 }, T('all genres')), ...sec.genres().map(([v, n]) => h('option', { value: v, selected: cur === v }, n)));
}
function renderShelf() {
  const sh = $('#shelf'); sh.innerHTML = '';
  const lay = layoutOf();
  if (lay !== 'entry') { MODES[lay].shelf?.(sh); return; }
  if (!cur()) st.section = SECTIONS[0].key;
  const sec = cur(), f = filt(sec);
  sh.append(h('div', { class: 'shelfhead' }, h('h2', { class: 'stitle' }, sec.label), h('span', { id: 'shelfcount' }), viewToggle(sec),
    canGrow(sec) ? h('button', { class: 'plate small', id: 'newentry', 'aria-label': sec.newOne(nextId(sec.table)), 'data-tip': T("A new, empty {what} (up to #{max}); the decompiled game plays {many} beyond the original's", { what: sec.what, max: maxId(sec.table), many: sec.many }), onclick: () => newEntry(sec) },
      icon('plus', 14), T('New')) : null));
  const fb = h('div', { id: 'filters' });
  const row1 = h('div', { class: 'frow' }, h('input', { type: 'text', id: 'search', placeholder: T('Title or #id'), value: f.q, oninput: (e) => { f.q = e.target.value; renderList(); } }));
  if (sec.genres) row1.append(genreSelect(sec, f.genre, (g) => { f.genre = g; renderList(); }));
  fb.append(row1);
  const chips = h('div', { class: 'frow chips' });
  for (const c of chipsOf(sec)) {
    chips.append(h('button', { class: 'chip' + (f.chips[c.key] ? ' on' : ''), 'data-chip': c.key, 'aria-pressed': f.chips[c.key] ? 'true' : 'false',
      onclick: () => { f.chips[c.key] = !f.chips[c.key]; renderShelf(); } }, c.label, c.count ? h('span', { class: 'chipn' }) : null));
  }
  fb.append(chips);
  sh.append(fb, h('div', { id: 'list' }));
  renderList();
}
function viewToggle(sec) {
  const views = ['list'].concat(Object.keys(MODES).filter((k) => MODES[k].view && hasMode(k, sec)));
  if (views.length < 2) return null;
  const now = layoutOf() === 'entry' ? 'list' : layoutOf();
  return h('div', { class: 'seg icons viewtoggle', role: 'group', 'aria-label': T('View') }, ...views.map((v) => {
    const label = v === 'list' ? T('List') : MODES[v].label;
    return h('button', { class: now === v ? 'on' : '', 'data-view': v, 'aria-label': label, 'aria-pressed': now === v ? 'true' : 'false', 'data-tip': label,
      onclick: () => { st.view[sec.key] = v; renderAll(); } }, icon(v === 'list' ? 'list' : v, 16));
  }));
}
function shelfCount() {
  const sec = cur(), el = $('#shelfcount');
  if (!el || !sec) return;
  const n = records(sec.table).length, fr = freeSlots(sec).length;
  const added = records(sec.table).filter((r) => !origById(sec.table, r.id)).length;
  const count = fr ? T('{n} in the game, {free} free', { n: n - fr, free: fr }) : `${n}`;
  el.textContent = added ? T('{count}, {n} new', { count, n: added }) : count;
  const ch = $('#filters [data-chip="changed"] .chipn');
  if (ch) { const k = records(sec.table).filter((r) => changed(sec.table, r)).length; ch.textContent = k ? ' ' + k : ''; }
}
function listed(sec) {
  const f = filt(sec), q = (f.q || '').toLowerCase().replace(/^#/, '');
  const chips = chipsOf(sec).filter((c) => f.chips[c.key]);
  return records(sec.table).filter((r) => {
    if (q && !titleOf(sec, r).toLowerCase().includes(q) && !titleOf(sec, r, st.lang === 'en' ? 'de' : 'en').toLowerCase().includes(q) && String(r.id) !== q) return false;
    if (f.genre && r.genre !== f.genre) return false;
    return chips.every((c) => c.test(r));
  });
}
function listRow(sec, r) {
  const free = usage(sec, r).free, inc = missing(sec, r).length > 0, b = badgeOf(sec, r), ch = changed(sec.table, r);
  const special = (SPECIAL[sec.table] || {})[r.id];
  return h('div', { class: 'tape' + (st.sel[sec.key] === r.id ? ' sel' : '') + (free ? ' free' : '') + (inc ? ' incomplete' : ''), 'data-id': r.id, onclick: () => select(r.id),
    'data-tip': free ? T('Not in the game: a free slot (copy an entry here, or edit it and put it on the market)') : inc ? T('Incomplete: it cannot go on air like this') : null },
  h('span', { class: 'spine', style: `background:${free ? '#555' : sec.spine(r)}` }), h('span', { class: 'id' }, r.id),
  h('span', { class: 'title' }, titleOf(sec, r)),
  h('span', { class: 'badge' + (b ? ' ' + b.cls : ''), 'data-tip': b?.tip || null }, b ? b.text : ''),
  h('span', { class: 'mark' }, special ? h('span', { 'data-tip': special }, icon('lock', 14)) : null, h('span', { class: 'chg' + (ch ? ' on' : ''), 'data-tip': ch ? T('changed') : null })));
}
function renderList() {
  const sec = cur();
  const list = $('#list');
  if (!list || !sec) return;
  list.innerHTML = '';
  const rows = listed(sec);
  const frag = document.createDocumentFragment();
  for (const r of rows) frag.append(listRow(sec, r));
  list.append(frag);
  if (!rows.length) list.append(h('div', { class: 'empty' }, T('Nothing matches.')));
  if (!st.sel[sec.key] && rows.length) { st.sel[sec.key] = rows[0].id; list.firstChild.classList.add('sel'); }
  shelfCount();
  const sel = list.querySelector('.tape.sel');
  if (sel) sel.scrollIntoView({ block: 'nearest' });
}
function refreshListRow() {
  const sec = cur();
  if (!sec) return;
  const id = st.sel[sec.key];
  const row = $(`#list .tape[data-id="${id}"]`);
  const r = recById(sec.table, id);
  if (!row || !r) return;
  row.replaceWith(listRow(sec, r));
  shelfCount();
}
function select(id) {
  st.sel[st.section] = id;
  $$('#list .tape').forEach((el) => el.classList.toggle('sel', +el.dataset.id === id));
  renderEntry();
}
function stepSel(d) {
  const rows = $$('#list .tape');
  const i = rows.findIndex((el) => el.classList.contains('sel'));
  const n = rows[Math.max(0, Math.min(rows.length - 1, i + d))];
  if (n) { select(+n.dataset.id); n.scrollIntoView({ block: 'nearest' }); }
}

/* ---------------------------------------------------------------- global search (Ctrl+K) */
function wireSearch() {
  const inp = $('#gsearch'), res = $('#gresults');
  const run = () => {
    const q = inp.value.trim().toLowerCase().replace(/^#/, '');
    res.innerHTML = '';
    if (!q) { hideResults(); return; }
    let n = 0;
    for (const sec of SECTIONS) {
      for (const r of records(sec.table)) {
        if (n >= 40) break;
        const t = titleOf(sec, r), t2 = titleOf(sec, r, st.lang === 'en' ? 'de' : 'en');
        if (String(r.id) === q || t.toLowerCase().includes(q) || t2.toLowerCase().includes(q)) {
          res.append(h('button', { class: 'gres', onclick: () => { hideResults(); inp.value = ''; go(sec.key, r.id); } },
            h('span', { class: 'gsec' }, sec.label), h('span', { class: 'gid' }, '#' + r.id), h('span', { class: 'gt' }, t)));
          n++;
        }
      }
    }
    if (!n) res.append(h('div', { class: 'gnone' }, T('Nothing found.')));
    res.classList.add('open');
  };
  inp.addEventListener('input', run);
  inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') { const b = $('#gresults .gres'); if (b) b.click(); } });
  document.addEventListener('click', (e) => { if (!e.target.closest('#gsearchbox')) hideResults(); });
}
function hideResults() { $('#gresults')?.classList.remove('open'); }

/* ---------------------------------------------------------------- field widgets (the schema's types) */
/* a field's help: its own text (in the interface language already), or 'kind:key' -- the field
   description of tools/catalog.py (FIELDS, German: FIELDS_DE) */
function helpText(f) {
  if (!f.help) return '';
  const m = /^(\w+):(\w+)$/.exec(f.help);
  if (!m) return f.help;
  if (UI.lang === 'de') {
    const d = ((st.meta.fields_de || {})[m[1]] || {})[m[2]];
    if (d) return d[1];
    UI.missing.add('field help ' + f.help);
  }
  const x = (st.meta.fields[m[1]] || []).find((y) => y[2] === m[2]);
  return x ? x[5] : '';
}
function help(text, title) { return text ? h('span', { class: 'help', 'data-tip': text, 'data-tiptitle': title || '' }, '?') : null; }
const labelOf = (f, r) => (typeof f.label === 'function' ? f.label(r) : f.label);
function fmtVal(f, v, r) {
  if (f.fmt) return f.fmt(v, r);
  if (f.type === 'money') return money(v);
  if (f.type === 'select' || f.type === 'choice') { const o = (typeof f.opts === 'function' ? f.opts(r) : f.opts).find((x) => x[0] === v); return o ? String(o[1]) : String(v); }
  if (f.type === 'toggles') { const on = f.bits.filter(([b]) => v & (1 << b)).map((x) => x[1]); return on.length ? on.join(', ') : T('none'); }
  if (typeof v === 'string') return `"${v}"`;
  return String(v);
}
function fieldGet(f, r) { return f.get ? f.get(r) : r[f.key]; }
function fieldSet(f, r, v) { if (f.set) f.set(r, v); else r[f.key] = v; }
/* one control; apply(v) sets the value (with the field's guard) and reports the change */
function control(sec, r, f, apply, done) {
  const get = () => fieldGet(f, r);
  switch (f.type) {
    case 'select': {
      const opts = typeof f.opts === 'function' ? f.opts(r) : f.opts;
      return h('select', { 'data-field': f.key, onchange: (e) => apply(+e.target.value) }, ...opts.map(([v, n]) => h('option', { value: v, selected: get() === v }, n)));
    }
    case 'number': case 'money': {
      const isMoney = f.type === 'money';
      const inp = h('input', { type: 'number', 'data-field': f.key, min: f.min ?? 0, max: f.max ?? (isMoney ? 99999999 : undefined), step: f.step || (isMoney ? 1000 : 1), value: get(), class: isMoney ? 'money' : '' });
      inp.addEventListener('input', () => {
        let v = Math.trunc(+inp.value || 0);
        const lo = f.min ?? 0, hi = f.max ?? (isMoney ? 99999999 : Infinity);
        v = Math.max(lo, Math.min(hi, v));
        apply(v);
      });
      inp.addEventListener('change', () => { inp.value = get(); });
      return isMoney ? h('span', { class: 'inline' }, '$', inp) : inp;
    }
    case 'bar': return barField(r, f, apply);
    case 'choice': {
      const opts = typeof f.opts === 'function' ? f.opts(r) : f.opts;
      return h('div', { class: 'seg', role: 'radiogroup', 'data-field': f.key }, ...opts.map(([v, n, tip]) => h('button', { role: 'radio', 'aria-checked': get() === v ? 'true' : 'false', class: get() === v ? 'on' : '', 'data-v': v, 'data-tip': tip || null,
        onclick: (e) => { if (apply(v) !== false) { $$('button', e.target.parentNode).forEach((b) => { const on = +b.dataset.v === v; b.classList.toggle('on', on); b.setAttribute('aria-checked', on); }); } } }, n)));
    }
    case 'toggles':
      return h('div', { class: 'toggles', 'data-field': f.key }, ...f.bits.map(([b, n, tip]) => {
        const btn = h('button', { class: 'tog' + (get() & (1 << b) ? ' on' : ''), 'aria-pressed': get() & (1 << b) ? 'true' : 'false', 'data-bit': b, 'data-tip': tip || null,
          onclick: () => { const v = get() ^ (1 << b); if (apply(v) !== false) { btn.classList.toggle('on', !!(get() & (1 << b))); btn.setAttribute('aria-pressed', !!(get() & (1 << b))); } } }, n);
        return btn;
      }));
    case 'info': return h('span', { class: 'info' }, f.text(r));
    case 'custom': return f.render(r, {}, done);
  }
  return h('span', {}, '?');
}
function barField(r, f, apply) {
  const max = f.max;
  const fill = h('div', { class: 'fill' });
  const num = h('input', { type: 'number', class: 'num', min: 0, max, 'data-field': f.key });
  const show = () => { const v = fieldGet(f, r); fill.style.width = (Math.min(v, max) / max) * 100 + '%'; num.value = v; };
  const set = (v) => { apply(Math.max(0, Math.min(max, v))); show(); };
  const track = h('div', { class: 'track' }, fill);
  const fromMouse = (e) => { const b = track.getBoundingClientRect(); set(Math.round(((e.clientX - b.left) / b.width) * max)); };
  track.addEventListener('mousedown', (e) => {
    fromMouse(e);
    const mv = (ev) => fromMouse(ev), up = () => { removeEventListener('mousemove', mv); removeEventListener('mouseup', up); };
    addEventListener('mousemove', mv); addEventListener('mouseup', up);
  });
  num.addEventListener('input', () => { apply(Math.max(0, Math.min(max, Math.trunc(+num.value || 0)))); fill.style.width = (Math.min(fieldGet(f, r), max) / max) * 100 + '%'; });
  num.addEventListener('change', show);
  show();
  return h('span', { class: 'bar' }, track, num);
}
/* a field row: label | control and note | the original value and its reset */
function fieldRow(sec, r, f, ctx) {
  const key = `${sec.key}:${r.id}:${f.key}`;
  const apply = (v) => {
    const before = f.guard ? clone(r) : null;
    fieldSet(f, r, v);
    if (f.guard) {
      const err = GUARDS[f.guard](sec, r, before);
      if (err) { restore(r, before); status(err, 'err'); renderEntry(); return false; }
    }
    st.undoKey = key;
    touched({ rerender: f.rerender });
    return true;
  };
  const done = (o = {}) => { st.undoKey = key; touched(Object.assign({ rerender: f.rerender }, o)); };
  const ctl = control(sec, r, f, apply, done);
  if (f.req) ctx.req[f.req] = ctl.matches?.('input, select') ? ctl : ctl.querySelector?.('input, select') || ctl;
  const lab = h('label', { class: f.req ? 'req' : '', 'data-tip': f.req ? T('Required for an entry in the game') : null }, labelOf(f, r), help(helpText(f), labelOf(f, r)));
  const note = h('span', { class: 'note' });
  const updNote = () => { if (!f.note) return; note.innerHTML = ''; const n = f.note(r); if (n) note.append(...[].concat(n)); };
  updNote();
  if (f.note) st.liveWork.push(updNote);
  const wrapCtl = h('div', { class: 'ctl' }, ctl, f.note ? note : null, f.extra ? f.extra(r) : null);
  return [lab, wrapCtl, origCell(sec, r, f, wrapCtl)];
}
function origCell(sec, r, f, wrapCtl) {
  const o = origById(sec.table, r.id);
  const cell = h('span', { class: 'orig' });
  if (!o || f.type === 'info') return cell;
  const upd = () => {
    cell.innerHTML = '';
    const a = fieldGet(f, o), b = fieldGet(f, r);
    wrapCtl?.classList.toggle('changed', !same(a, b));
    if (same(a, b)) return;
    const shown = fmtVal(f, a, o);
    cell.append(h('span', { class: 'was', 'data-tip': T("The original game's value") }, T('was {value}', { value: shown.length > 24 ? shown.slice(0, 22) + '..' : shown })),
      h('button', { class: 'reset', 'data-tip': T('Back to the original: {value}', { value: shown }), 'aria-label': T('Reset {label}', { label: labelOf(f, r) }), onclick: () => { fieldSet(f, r, clone(a)); st.undoKey = null; touched({ rerender: true }); status(T('{label}: back to {value}.', { label: labelOf(f, r), value: shown })); } }, icon('reset', 14)));
  };
  upd();
  st.liveWork.push(upd);
  return cell;
}
function fieldsGrid(sec, r, fields, ctx) {
  const grid = h('div', { class: 'fields' });
  for (const f of fields) {
    if (f.show && !f.show(r)) continue;
    grid.append(...fieldRow(sec, r, f, ctx));
  }
  return grid;
}
/* texts: both languages side by side */
function textsGrid(sec, r, g, ctx) {
  const items = g.texts.filter((t) => !t.hidden);
  const grid = h('div', { class: 'texts2' });
  for (const lang of LANGS) {
    const col = h('div', { class: 'tcol' + (editLang() === lang ? ' previewed' : ''), 'data-lang': lang },
      h('div', { class: 'tlang' }, LANG_NAME[lang], h('span', { class: 'pv', 'data-tip': T('The preview in the inspector shows this language') }, T('in the preview'))));
    for (const t of items) col.append(...textItem(sec, r, t, lang, ctx));
    grid.append(col);
  }
  return grid;
}
function textItem(sec, r, t, lang, ctx) {
  const multi = t.rows > 1;
  const el = h(multi ? 'textarea' : 'input', multi ? { rows: t.rows } : { type: 'text' });
  el.dataset.text = `${t.i === null ? 'h' : t.i}-${lang}`;
  el.value = txt(r, t.i, lang);
  const lab = h('label', { class: t.req ? 'req' : '' }, t.label);
  const info = h('div', { class: 'tinfo' }), was = h('div', { class: 'wasline' });
  const o = origById(sec.table, r.id);
  const upd = () => {
    const v = el.value;
    info.innerHTML = '';
    const bad = missingChars(v);
    const c = t.check && v ? t.check(v, lang) : null;
    if (c) info.append(h('span', { class: c.level }, c.level === 'warn' ? icon('problem', 14) : null, c.msg));
    if (bad.length) info.append(h('span', { class: 'warn' }, icon('problem', 14), T('Not in the game\'s font (shows as "."): {chars}', { chars: bad.join(' ') })));
    el.classList.toggle('warned', !!(bad.length || (c && c.level === 'warn')));
    was.innerHTML = '';
    if (o && txt(o, t.i, lang) !== v) {
      const ov = txt(o, t.i, lang);
      was.append(h('span', { class: 'was', 'data-tip': ov || T('(empty)') }, ov ? T('was: "{text}"', { text: ov.length > 40 ? ov.slice(0, 38) + '..' : ov }) : T('was: empty')),
        h('button', { class: 'reset', 'data-tip': T('Back to the original text'), onclick: () => { setTxt(r, t.i, ov, lang); touched({ rerender: true }); } }, icon('reset', 14)));
    }
  };
  el.addEventListener('focus', () => { if (editLang() !== lang) { st.plang = lang; $$('.tcol').forEach((c) => c.classList.toggle('previewed', c.dataset.lang === lang)); runLive(); } });
  el.addEventListener('input', () => { setTxt(r, t.i, el.value, lang); st.undoKey = `${sec.key}:${r.id}:text:${t.i}:${lang}`; touched(); });
  if (t.req) ctx.req[`${t.req}-${lang}`] = el;
  upd();
  st.liveWork.push(upd);
  return [lab, el, info, was];
}

/* ---------------------------------------------------------------- the form (③) */
function renderForm(sec, r, w) {
  const ctx = { req: {} };
  const groups = sec.groups;
  let tab = st.tab[sec.key] || 'all';
  if (tab !== 'all' && !groups.some((g) => g.key === tab)) tab = st.tab[sec.key] = 'all';
  $('#workhead').append(entryHead(sec, r, ctx));
  const tabs = h('div', { class: 'tabs', role: 'tablist' });
  const tabBtn = (key, title) => h('button', { role: 'tab', class: 'tab' + (tab === key ? ' on' : ''), 'aria-selected': tab === key ? 'true' : 'false', 'data-tab': key,
    onclick: () => { st.tab[sec.key] = key; renderEntry(); } }, title, h('span', { class: 'tmark' }));
  tabs.append(tabBtn('all', T('All')));
  for (const g of groups) tabs.append(tabBtn(g.key, typeof g.title === 'function' ? g.title(r) : g.title));
  w.append(tabs);
  /* the groups flow into two columns when the form is wide enough (editor.css .groups);
     texts and tables take the whole width */
  const flow = h('div', { class: 'groups' });
  const wide = (g) => !!(g.texts || g.render || g.wide);
  /* "All": the texts first, then the narrow groups side by side, then the wide ones (tables) */
  const shown = groups.filter((g) => (tab === 'all' ? !g.tabOnly : g.key === tab));
  const order = tab === 'all' ? [...shown.filter((g) => g.texts), ...shown.filter((g) => !wide(g)), ...shown.filter((g) => wide(g) && !g.texts)] : shown;
  for (const g of order) {
    const box = groupBox(sec, r, g, ctx);
    if (wide(g)) box.classList.add('wide');
    flow.append(box);
  }
  w.append(flow);
  /* required fields, changed groups: marks on the fields and the tabs */
  const reqBox = $('#workhead .reqbox');
  const marks = () => {
    const m = missing(sec, r), bad = new Set(m.flatMap((x) => x.keys || [x.key]));
    for (const [k, el] of Object.entries(ctx.req)) el.classList.toggle('missing', bad.has(k));
    reqBox.textContent = m.length ? T('Required before it can go on air: {missing}.', { missing: m.map((x) => x.msg).join(', ') }) : '';
    reqBox.style.display = m.length ? 'block' : 'none';
    const o = origById(sec.table, r.id);
    for (const g of groups) {
      const b = $(`.tab[data-tab="${g.key}"] .tmark`, w);
      if (!b) continue;
      const keys = groupReqKeys(g);
      const miss = keys.some((k) => bad.has(k));
      const ch = o && groupFields(g).some((f) => !same(fieldGet(f, o), fieldGet(f, r)));
      b.className = 'tmark' + (miss ? ' miss' : ch ? ' chg' : '');
    }
  };
  marks();
  st.liveWork.push(marks);
}
const groupFields = (g) => (g.fields || []).concat((g.texts || []).flatMap((t) => LANGS.map((l) => textPseudo(t, l))));
const groupReqKeys = (g) => (g.reqKeys || []).concat((g.fields || []).filter((f) => f.req).map((f) => f.req), (g.texts || []).filter((t) => t.req).flatMap((t) => LANGS.map((l) => `${t.req}-${l}`)));
const textPseudo = (t, l) => ({ key: `text-${t.i}-${l}`, label: `${t.label} (${l.toUpperCase()})`, get: (r) => txt(r, t.i, l), set: (r, v) => setTxt(r, t.i, v, l) });
const allFields = (sec) => sec.groups.flatMap(groupFields);
const fieldByKey = (sec, key) => sec.groups.flatMap((g) => g.fields || []).find((f) => f.key === key);
function groupBox(sec, r, g, ctx) {
  const title = typeof g.title === 'function' ? g.title(r) : g.title;
  const box = h('section', { class: 'box group', 'data-group': g.key }, h('div', { class: 'boxtitle band' }, title));
  const done = (o = {}) => touched(o);
  if (g.render) box.append(g.render(r, ctx, done));
  else {
    if (g.texts) box.append(textsGrid(sec, r, g, ctx));
    if (g.fields) box.append(fieldsGrid(sec, r, g.fields, ctx));
  }
  if (g.hint) box.append(h('div', { class: 'hint pad' }, g.hint));
  return box;
}
/* the entry's head: where it is, its title, card or all fields, actions; what it is in the game */
function entryHead(sec, r, ctx) {
  const u = usage(sec, r);
  const slotable = ['films', 'series', 'ads', 'screenplays'].includes(sec.key);
  const orig = origById(sec.table, r.id);
  const title = h('span', { class: 'etitle', role: 'heading', 'aria-level': '1' });
  const btn = (name, label, cls, tip, fn) => h('button', { class: 'plate small ' + cls, 'data-tip': tip, onclick: fn }, icon(name, 14), label);
  const modes = sec.card && hasMode('game') ? h('div', { class: 'seg modetoggle', role: 'group', 'aria-label': T('View') },
    h('button', { class: st.mode !== 'form' ? 'on' : '', 'data-mode': 'game', 'aria-pressed': st.mode !== 'form' ? 'true' : 'false', onclick: () => { st.mode = 'game'; renderEntry(); } }, icon('card', 15), T('Card')),
    h('button', { class: st.mode === 'form' ? 'on' : '', 'data-mode': 'form', 'aria-pressed': st.mode === 'form' ? 'true' : 'false', onclick: () => { st.mode = 'form'; renderEntry(); } }, icon('form', 15), T('All fields'))) : null;
  const bar = h('div', { class: 'ehead' },
    h('div', { class: 'etitlebox' }, h('div', { class: 'overline' }, `${What(sec)} #${r.id} · ${sec.plate}`), title),
    h('div', { class: 'actions' }, modes, modes ? h('span', { class: 'vsep' }) : null,
      slotable ? btn('copy', T('Copy'), 'white', sec.key === 'films' ? T('A copy of this film as a new film (or in a free slot), offered in the game') : T('A copy of this entry in a free slot, offered in the game'), () => copyEntry(sec, r)) : null,
      orig ? btn('reset', T('Original'), 'white', T("Back to the original game's values for this entry"), () => {
        const recs = st.data[sec.table].records; recs[recs.indexOf(r)] = clone(orig); markDirty([sec.table]); renderEntry(); status(T('#{id} is the original again.', { id: r.id })); })
        : u.free ? btn('remove', T('Delete'), 'red', T('An added {what}: delete it from the pack', { what: sec.what }), () => deleteAdded(sec, r)) : null,
      slotable && !u.free ? h('button', { class: 'plate small sq', 'aria-label': T('Remove'), 'data-tip': u.keep.length ? T('Cannot be removed: {why}', { why: u.keep.join('; ') }) : T('Take it out of the game: its slot becomes free'), onclick: () => removeAsk(sec, r) }, icon('remove', 14)) : null));
  const info = h('div', { class: 'einfo' });
  const upd = () => {
    title.textContent = titleOf(sec, r);
    info.innerHTML = '';
    const uu = usage(sec, r);
    if (slotable) {
      if (uu.free) info.append(h('span', { class: 'pill grey' }, T('Not in the game'), ' · ', T('free slot')));
      for (const x of uu.uses) info.append(h('span', { class: 'pill green' }, h('span', { class: 'dot' }), T('In the game:'), ' ', x));
      for (const x of uu.keep) info.append(h('span', { class: 'pill dark' }, icon('lock', 12), x));
    }
    if (sec.genres) {
      const g = (sec.genres().find((x) => x[0] === r.genre) || [0, '?'])[1];
      info.append(h('span', { class: 'pill' }, h('span', { class: 'sq', style: `background:${sec.spine(r)}` }), r.blocks ? T('{genre}, {n} h', { genre: g, n: r.blocks }) : g));
    }
    if (!orig) info.append(h('span', { class: 'pill yellow' }, T('added in this pack')));
    else {
      const n = diffs(sec, r).length;
      info.append(n ? h('span', { class: 'pill yellow' }, TP(n, '{n} field changed', '{n} fields changed')) : h('span', { class: 'pill grey' }, T('as in the original')));
    }
  };
  upd();
  st.liveWork.push(upd);
  return h('div', { class: 'entryhead' }, bar, info, h('div', { class: 'warn reqbox', style: 'display:none' }));
}

/* ---------------------------------------------------------------- what differs from the original: [{ label, from, to, reset }] */
function diffs(sec, r) {
  const o = origById(sec.table, r.id);
  if (!o) return [];
  const out = [];
  const scratch = clone(r);
  for (const f of allFields(sec)) {
    if (f.type === 'info' || (f.show && !f.show(r))) continue;      /* a field the form does not show (another one covers its bytes) */
    const a = fieldGet(f, o), b = fieldGet(f, r);
    fieldSet(f, scratch, clone(a));
    if (!same(a, b)) out.push({ key: f.key, label: labelOf(f, r), from: fmtVal(f, a, o), to: fmtVal(f, b, r), reset: () => fieldSet(f, r, clone(a)) });
  }
  /* what no field covers: by key (and text index) */
  for (const k of new Set([...Object.keys(o), ...Object.keys(scratch)])) {
    if (k === 'id' || same(o[k], scratch[k])) continue;
    if (k === 'text') {
      for (const l of LANGS) {
        const A = o.text?.[l], B = scratch.text?.[l];
        const n = Math.max(Array.isArray(A) ? A.length : 1, Array.isArray(B) ? B.length : 1);
        for (let i = 0; i < n; i++) {
          const a = Array.isArray(A) ? A[i] ?? '' : A ?? '', b = Array.isArray(B) ? B[i] ?? '' : B ?? '';
          if (a !== b) out.push({ key: `text-${i}-${l}`, label: `${textLabel(sec, i)} (${l.toUpperCase()})`, from: `"${a}"`, to: `"${b}"`, reset: () => setTxt(r, Array.isArray(A) ? i : null, a, l) });
        }
      }
    } else out.push({ key: k, label: k === 'unknown' ? T('other bytes') : k, from: JSON.stringify(o[k]), to: JSON.stringify(scratch[k]), reset: () => { r[k] = clone(o[k]); } });
  }
  return out;
}
function textLabel(sec, i) {
  if (sec.key === 'series' && i >= 2) return i <= 11 ? T('Episode {n} title', { n: i - 1 }) : T('Episode {n} text', { n: i - 11 });
  if (sec.key === 'topics') return T('Phrase {n}', { n: i + 1 });
  if (sec.key === 'screenplays') return i ? T('Role {n} name', { n: i }) : T('Description');
  return T('Text {n}', { n: i });
}

/* ---------------------------------------------------------------- the inspector (④): card, TV, checks, changes, references */
function panel(title, ...kids) { return h('section', { class: 'box panel' }, h('div', { class: 'boxtitle band' }, title), ...kids); }
const langTag = () => h('span', {}, editLang().toUpperCase());
/* the preview's card and monitor: the largest whole scale (2..4) its column's width takes
   (card 141 px + its frame and the row marks; monitor 116 px); a resize redraws them */
const inspWidth = () => ((document.getElementById('inspector') || {}).clientWidth || 330) - 28;
const inspScale = () => Math.max(2, Math.min(4, Math.floor((inspWidth() - 32) / CARD_W)));
const monScale = () => Math.max(2, Math.min(3, Math.floor(inspWidth() / 116)));
let inspScales = '';
addEventListener('resize', () => {
  clearTimeout(inspScales.timer);
  inspScales.timer = setTimeout(() => { if (st.data && `${inspScale()},${monScale()}` !== inspScales.v) renderEntry(); }, 150);
});
function renderInspector(sec, r, ins) {
  inspScales = { v: `${inspScale()},${monScale()}`, timer: inspScales.timer };
  if (st.tab[sec.key] === 'monitor' && sec.tvBase && typeof monitorInspector === 'function') { monitorInspector(sec, r, ins); return; }
  for (const p of sec.panels) {
    const el = PANELS[p]?.(sec, r);
    if (el) ins.append(el);
  }
}
const PANELS = {
  /* the audience from 18 to 24 o'clock alone on air (soloShare, game.js), live with the entry:
     seven bars on one scale (full height = 80 %), the best hour named, every hour in its tip;
     X rated: 18-20 hatched and named -- the moral guardian takes the film away before 21:00 */
  audience(sec, r) {
    const box = h('div', { class: 'aud' });
    const upd = () => {
      const series = sec.key === 'series', vals = series ? seriesSolo(r) : filmSolo(r), x = !series && !!(r.flags2 & 8);
      const top = Math.max(...vals), best = vals.indexOf(top);
      const bestOk = x ? Math.max(...vals.slice(3)) : top, bestOkAt = x ? 3 + vals.slice(3).indexOf(bestOk) : best;
      box.innerHTML = '';
      box.append(h('div', { class: 'audhead' }, !top ? T('No audience: its topicality is 0') : x ? T('Best hour from 21:00: {hour}:00, {v} %', { hour: 18 + bestOkAt, v: bestOk }) : T('Best hour {hour}:00, {v} %', { hour: 18 + bestOkAt, v: bestOk })));
      const bars = h('div', { class: 'audbars', role: 'img', 'aria-label': vals.map((v, k) => `${18 + k}:00 ${v} %`).join(', ') });
      vals.forEach((v, k) => {
        const risk = x && k < 3;
        bars.append(h('div', { class: 'audcol' + (k === 3 ? ' after21' : ''), 'data-tip': `${18 + k}:00 -- ${v} %` + (risk ? ' ' + T('(moral guardian)') : ''), 'data-hour': 18 + k },
          h('span', { class: 'audv' }, k === bestOkAt && top ? `${v}` : ''),
          h('span', { class: 'audbar' + (risk ? ' risk' : '') + (v ? '' : ' zero'), style: `height:${Math.max(1, Math.min(64, Math.round((v * 64) / 80)))}px` }),
          h('span', { class: 'audh' }, String(18 + k))));
      });
      box.append(bars);
      if (x) box.append(h('div', { class: 'audwarn' }, icon('problem', 14), best < 3 ? T('X rated: its best hour is before 21:00, when the moral guardian takes it away; from 21:00 at most {v} %.', { v: bestOk }) : T('X rated: 18-20 o\'clock the moral guardian takes it away; its best hours come later anyway.')));
      box.append(h('div', { class: 'hint' }, series ? T("The first episode alone on air: its topicality and the audience of the series genre at each hour; no rival, no news lead-in, no trend.") : T("Alone on air: its topicality and the audience of its genre at each hour; no rival, no news lead-in, no trend.")));
    };
    upd();
    st.liveInsp.push(upd);
    return panel(T('Audience 18-24'), box);
  },
  card(sec, r) {
    const canvas = h('canvas', { class: 'card' });
    const marks = h('div', { class: 'rowmarks' });
    const stepper = sec.key === 'series' ? h('div', { class: 'stepper' }) : null;
    const caption = h('div', { class: 'caption' });
    const draw = () => {
      const lang = editLang();
      const ep = (st.previewEp || {})[r.id];
      const pr = ep ? Object.assign({}, r, { next_episode: ep }) : r;
      const rows = sec.card(pr, lang), S = inspScale();
      drawCard(canvas, rows, S);
      marks.innerHTML = '';
      const o = origById(sec.table, r.id);
      if (o) {
        const orows = sec.card(ep ? Object.assign({}, o, { next_episode: ep }) : o, lang);
        rows.forEach((row, i) => { if (!orows[i] || rowText(row) !== rowText(orows[i])) marks.append(h('span', { class: 'rowmark', style: `top:${(1 + i * 8) * S}px` })); });
      }
      caption.textContent = marks.childNodes.length ? T('Marked: rows that differ from the original') : '';
      if (stepper) {
        const e = Math.max(1, pr.next_episode || 1);
        stepper.innerHTML = '';
        const stepTo = (n) => { st.previewEp = st.previewEp || {}; st.previewEp[r.id] = Math.max(1, Math.min(10, n)); draw(); };
        stepper.append(h('button', { class: 'plate small grey', onclick: () => stepTo(e - 1), 'aria-label': T('Previous episode') }, '<'), h('span', {}, T('Card shows episode {n}', { n: e })),
          h('button', { class: 'plate small grey', onclick: () => stepTo(e + 1), 'aria-label': T('Next episode') }, '>'));
      }
    };
    draw();
    st.liveInsp.push(draw);
    return h('section', { class: 'panel cardpanel' }, h('div', { class: 'phead' }, h('span', {}, T('In the game')), langTag()),
      h('div', { class: 'cardframe' }, h('div', { class: 'cardpos' }, canvas, marks)), stepper, caption);
  },
  tv(sec, r) {
    const c = h('canvas', { class: 'monitor' });
    const cap = h('div', { class: 'caption' });
    const upd = () => {
      const pic = r.jingle || sec.tvBase + (r.genre || 1);
      if (c.dataset.drawn !== `${pic}x${monScale()}`) { c.dataset.drawn = `${pic}x${monScale()}`; drawMonitor(c, pic, monScale()); }
      cap.textContent = r.jingle ? T('Monitor: own picture {n}', { n: r.jingle }) : T('Monitor: genre picture {n}', { n: pic });
    };
    upd();
    st.liveInsp.push(upd);
    const open = () => { st.mode = 'form'; st.tab[sec.key] = 'monitor'; renderEntry(); };
    return h('section', { class: 'panel' }, h('div', { class: 'boxtitle' }, T('On the TV')), h('div', { class: 'tvpanel' }, h('div', { class: 'tvrow', 'data-tip': T("The office TV while it airs (the game's own monitor). Click for the monitor settings."),
      onclick: open }, c),
    h('div', { class: 'tvcap' }, cap, h('button', { class: 'plate small white', onclick: open }, T('Monitor settings ...')))));
  },
  checks(sec, r) {
    const list = h('div', { class: 'checks-list' });
    const upd = () => { list.innerHTML = ''; for (const c of entryChecks(sec, r)) list.append(checkRow(c)); };
    upd();
    st.liveInsp.push(upd);
    return panel(T('Checks'), list);
  },
  changes(sec, r) {
    const list = h('div', { class: 'diffs' });
    const upd = () => {
      list.innerHTML = '';
      if (!origById(sec.table, r.id)) { list.append(h('div', { class: 'hint' }, T('Added in this pack: no original.'))); return; }
      const d = diffs(sec, r);
      if (!d.length) { list.append(h('div', { class: 'hint' }, T('Nothing: as in the original.'))); return; }
      for (const x of d) {
        list.append(h('div', { class: 'diff' }, h('span', { class: 'dl' }, x.label), h('span', { class: 'dv' }, h('span', { class: 'from' }, x.from), ' -> ', h('span', { class: 'to' }, x.to)),
          h('button', { class: 'reset', 'data-tip': T('Back to the original'), onclick: () => { x.reset(); touched({ rerender: true }); } }, icon('reset', 14))));
      }
    };
    upd();
    st.liveInsp.push(upd);
    return panel(T('Changed against the original'), list);
  },
  used(sec, r) {
    const list = h('div', { class: 'used' });
    const upd = () => {
      list.innerHTML = '';
      const u = usage(sec, r);
      if (u.free) list.append(h('div', {}, T('Nothing: a free slot, not in the game')));
      for (const x of u.uses) list.append(h('div', {}, x));
      for (const x of u.keep) list.append(h('div', { class: 'keep' }, icon('lock', 14), x));
      if (sec.key === 'films' || sec.key === 'series') {
        for (const sp of producedBy(sec.table, r.id)) list.append(h('button', { class: 'link', onclick: () => go('screenplays', sp) }, T('Screenplay #{id}: {text}', { id: sp, text: txt(recById('screenplays', sp), 0).slice(0, 30) })));
        if (!(r.owner >= 1 && r.owner <= 3)) list.append(h('div', { class: 'hint' }, T('Not in a start library')));
        const ads = records('ads').filter((a) => a.genre_rule.some((c) => Math.abs(c) === (sec.key === 'series' ? 10 + r.genre : r.genre)));
        if (ads.length) list.append(h('div', { class: 'hint' }, T('Ad conditions name its genre: {list}', { list: ads.map((a) => '#' + a.id).join(' ') })));
      }
    };
    upd();
    st.liveInsp.push(upd);
    return panel(T('Used by'), list);
  },
  target(sec, r) {
    const tgt = spTarget(r);
    if (!tgt) return panel(T('Becomes'), h('div', { class: 'hint pad' }, T('No film or series.')));
    const kind = spIsSeries(r) ? 'series' : 'films', ts = secByKey(kind);
    const canvas = h('canvas', { class: 'card' });
    drawCard(canvas, ts.card(tgt, editLang()), inspScale());
    const o = { id: tgt.id, title: txt(tgt, 0) };
    return h('section', { class: 'panel cardpanel' }, h('div', { class: 'phead' }, h('span', {}, kind === 'series' ? T('Becomes series #{id}', o) : T('Becomes film #{id}', o)), langTag()),
      h('div', { class: 'cardframe' }, canvas),
      h('button', { class: 'plate small grey', onclick: () => go(kind, tgt.id) }, kind === 'series' ? T('Edit series #{id}: {title}', o) : T('Edit film #{id}: {title}', o)));
  },
  headline(sec, r) {
    const c = h('canvas', { class: 'headline' });
    const draw = () => {
      const lines = wrap(newsHeadline(r, editLang(), st.newsVariants), 150);
      c.width = 156; c.height = lines.length * 8 + 4; c.style.width = c.width * 2 + 'px'; c.style.height = c.height * 2 + 'px';
      const g = c.getContext('2d'); g.fillStyle = '#000'; g.fillRect(0, 0, c.width, c.height);
      lines.forEach((l, i) => drawText(g, l, 3, 2 + i * 8, '#f8e010'));
    };
    draw();
    st.liveInsp.push(draw);
    return h('section', { class: 'panel cardpanel' }, h('div', { class: 'phead' }, h('span', {}, T('Headline')), langTag()), h('div', { class: 'cardframe' }, c),
      h('div', { class: 'caption' }, T('Each #n takes the first phrase of its topic')));
  },
  story(sec, r) {
    const t = newsTemplateOf(r.id);
    const preds = records('news_nodes').filter((n) => n.successors.includes(r.id)).map((n) => n.id);
    const box = h('div', { class: 'used' });
    if (t) {
      box.append(h('div', {}, T('Story {id} ({category}), starts at #{start}', { id: t.id, category: newsCategory(t.category), start: t.start_node })));
      t.parts.forEach((p, k) => { if (p) { const tp = recById('news_topics', p % 1000); box.append(h('button', { class: 'link', onclick: () => go('topics', p % 1000) }, T('#{k} = topic {topic}: {phrases}', { k: k + 1, topic: p % 1000, phrases: tp ? (tp.text?.[st.lang] || []).filter(Boolean).slice(0, 2).join(', ') : '?' }))); } });
    } else box.append(h('div', {}, SPECIAL.news_nodes[r.id] || T('No story reaches it.')));
    box.append(h('div', { class: 'hint' }, preds.length ? T('Reached from {list}', { list: preds.map((x) => '#' + x).join(', ') }) : T('Nothing leads here')));
    if (hasMode('tree')) box.append(h('button', { class: 'plate small grey', onclick: () => { st.view.news = 'tree'; renderAll(); } }, T('Show in the story tree')));
    return panel(T('Story'), box);
  },
  stories(sec, r) {
    const ts = records('news_templates').filter((t) => t.parts.some((p) => p && p % 1000 === r.id));
    return panel(T('Used by'), h('div', { class: 'used' }, ts.length ? ts.map((t) => h('button', { class: 'link', onclick: () => go('news', t.start_node, { view: hasMode('tree') ? 'tree' : 'list' }) },
      T('Story {id}: part {part}', { id: t.id, part: t.parts.findIndex((p) => p % 1000 === r.id) + 1 }))) : h('div', {}, T('No story uses this topic'))));
  },
};

/* ---------------------------------------------------------------- custom controls of the schema */
/* the TV family as the game shows it in the office for this programme (game.js familyScene): who
   sits on the sofa at the hour chosen, their faces by its topicality and genre; live */
function familyPicture(table, r) {
  const canvas = h('canvas', { class: 'family', role: 'img' });
  const hours = h('div', { class: 'seg small', role: 'group', 'aria-label': T('Hour') });
  const notes = h('div', { class: 'famnotes' });
  const box = h('div', { class: 'famview' }, canvas, hours, notes);
  const upd = () => {
    const H = st.familyHour || 1, { q, gcode } = familyInput(table, r), names = familyNames();
    const sc = familyScene(r.flags & 0x1F, q, gcode, H);
    const key = `${sc.draws.map((x) => x.join(':')).join()}`;
    if (canvas.dataset.key !== key) { canvas.dataset.key = key; drawFamily(canvas, sc, 2); }
    canvas.setAttribute('aria-label', sc.shown.length ? T('On the sofa at {hour}:00: {names}', { hour: 17 + H, names: sc.shown.map((b) => names[b]).join(', ') }) : T('Nobody of the family watches it'));
    hours.innerHTML = '';
    for (let k = 1; k <= 7; k++) hours.append(h('button', { class: 'mono' + (k === H ? ' on' : ''), 'aria-pressed': k === H ? 'true' : 'false', 'data-famhour': k, onclick: () => { st.familyHour = k; upd(); } }, String(17 + k)));
    notes.innerHTML = '';
    const mood = q > 30 ? T('delighted') : q > 20 ? T('content') : q > 10 ? T('so-so') : T('bored');
    notes.append(h('div', {}, sc.shown.length ? T('Faces: {mood} -- the game reads the topicality ({q}) and the genre.', { mood, q }) : T('Nobody of the family watches it')));
    if (sc.left.length) notes.append(h('div', { class: 'warnline' }, T('Three sit on the sofa at most: {names} stays away.', { names: sc.left.map((b) => names[b]).join(', ') })));
    if (sc.bed) notes.append(h('div', {}, T('The child is in bed from 23:00.')));
  };
  upd();
  st.liveWork.push(upd);
  return box;
}
function poolControl(table, r, done) {
  const block = poolBlock(table, r);
  const on = !!(r.flags2 & POOL);
  const box = h('div', { class: 'poolctl' });
  if (block && !on) box.append(h('span', { class: 'hint' }, T('not possible: {why}', { why: block })));
  else box.append(h('label', { class: 'swrow' }, h('input', { type: 'checkbox', class: 'sw', checked: on, 'data-field': 'pool',
    onchange: () => { r.flags2 ^= POOL; done({ rerender: true }); } }), h('span', {}, table === 'films' ? T('A new game may give this film to a station') : T('A new game may give this series to a station'))));
  box.append(h('div', { class: 'hint' }, poolRule(table)));
  return box;
}
function ruleControl(r, done) {
  const genreCodes = [[0, '-']].concat(st.meta.film_genres[st.lang].map((n, i) => [i + 1, n]), st.meta.series_genres[st.lang].map((n, i) => [i + 11, n]));
  let neg = r.genre_rule.some((c) => c < 0);            /* refused genres are stored negative */
  const sel = (k) => h('select', { 'data-rule': k, onchange: (e) => { const v = +e.target.value; r.genre_rule[k] = neg ? -v : v; done(); } },
    ...genreCodes.map(([v, n]) => h('option', { value: v, selected: Math.abs(r.genre_rule[k]) === v }, n)));
  return h('div', { class: 'rulectl' },
    h('select', { 'data-rule': 'sign', onchange: (e) => { neg = e.target.value === 'no'; r.genre_rule = r.genre_rule.map((c) => (neg ? -Math.abs(c) : Math.abs(c))); done(); } },
      h('option', { value: 'yes', selected: !neg }, T('one of these')), h('option', { value: 'no', selected: neg }, T('none of these'))),
    h('div', { class: 'inline' }, sel(0), sel(1), sel(2)));
}
function targetControl(r, done) {
  const kind = spIsSeries(r) ? 'series' : 'films';
  const targets = [[0, '-']].concat(records(kind).map((f) => [f.id, kind === 'series' ? T('series {id}: {title}', { id: f.id, title: txt(f, 0) }) : T('film {id}: {title}', { id: f.id, title: txt(f, 0) })]));
  const tgt = spTarget(r);
  return h('div', { class: 'inline' }, h('select', { 'data-field': 'target', onchange: (e) => { r.target = +e.target.value; done({ rerender: true }); } },
    ...targets.map(([v, n]) => h('option', { value: v, selected: r.target === v }, n))),
  tgt ? h('button', { class: 'plate small grey', onclick: () => go(kind, tgt.id) }, T('Edit #{id}', { id: tgt.id })) : null);
}
function successorsControl(r, done) {
  const opts = [[0, T('- none')], [NEWS_END, T('end of the story')]].concat(records('news_nodes').map((n) => [n.id, `#${n.id}: ${(txt(n, null) || '').slice(0, 34)}`]));
  const wrapEl = h('div', { class: 'succ' });
  for (let k = 0; k < 4; k++) {
    wrapEl.append(h('div', { class: 'inline' }, h('select', { 'data-succ': k, onchange: (e) => { r.successors[k] = +e.target.value; done({ rerender: true }); } },
      ...opts.map(([v, n]) => h('option', { value: v, selected: r.successors[k] === v }, n))),
    r.successors[k] && r.successors[k] !== NEWS_END ? h('button', { class: 'plate small grey', onclick: () => select(r.successors[k]) }, T('go')) : null));
  }
  return wrapEl;
}
function episodesTable(r, ctx, done) {
  const showText = !!st.epText;
  const tb = h('tbody');
  for (let e = 1; e <= 10; e++) {
    const prod = !!(r.episodes_qtf[e * 3 + 2] & 1);
    const num = (off) => {
      const inp = h('input', { type: 'number', min: 0, max: 100, value: r.episodes_qtf[e * 3 + off], 'data-ep': `${e}-${off}` });
      inp.addEventListener('input', () => { r.episodes_qtf[e * 3 + off] = Math.max(0, Math.min(100, Math.trunc(+inp.value || 0))); st.undoKey = `ep:${r.id}:${e}:${off}`; done(); });
      return inp;
    };
    const tin = (i, l) => {
      const inp = h('input', { type: 'text', value: txt(r, i, l), 'data-text': `${i}-${l}` });
      const ch = () => {
        const bad = missingChars(inp.value);
        inp.classList.toggle('warned', bad.length > 0 || (i <= 11 && prod && textW(`${e}.${inp.value}`) > TEXT_W));
        const o = origById('series', r.id);
        inp.classList.toggle('changedin', !!o && txt(o, i, l) !== inp.value);
      };
      inp.addEventListener('input', () => { setTxt(r, i, inp.value, l); st.undoKey = `ep:${r.id}:${i}:${l}`; ch(); done(); });
      inp.addEventListener('focus', () => { if (editLang() !== l) { st.plang = l; runLive(); } st.previewEp = st.previewEp || {}; if (st.previewEp[r.id] !== e) { st.previewEp[r.id] = e; runLive(); } });
      ch();
      return h('div', { class: 'ttrow' + (i > 11 ? ' text' : '') }, h('span', { class: 'tl', 'data-short': i > 11 ? T('txt') : null }, i > 11 ? T('text {lang}', { lang: l.toUpperCase() }) : l.toUpperCase()), inp);
    };
    tb.append(h('tr', { class: prod ? '' : 'notprod' }, h('td', { class: 'n' }, e),
      h('td', { class: 'prod' }, h('button', { class: 'tog small' + (prod ? ' on' : ''), 'aria-pressed': prod ? 'true' : 'false', 'data-prod': e,
        onclick: () => { r.episodes_qtf[e * 3 + 2] ^= 1; done({ rerender: true }); } }, prod ? T('produced') : T('not yet'))),
      h('td', { class: 'tt' }, tin(1 + e, 'en'), tin(1 + e, 'de'), showText ? tin(11 + e, 'en') : null, showText ? tin(11 + e, 'de') : null),
      h('td', { class: 'qt' }, num(0), num(1))));
  }
  const same4all = () => {
    const v = h('input', { type: 'number', min: 0, max: 100, value: 50 });
    dialog(T('Same quality for all'), h('div', { class: 'fields two' }, h('label', {}, T('Quality and topicality')), v, h('div', { class: 'wide hint' }, T('For every produced episode.'))),
      [[T('Cancel'), 'grey'], [T('Set'), 'green', () => { const q = Math.max(0, Math.min(100, +v.value || 0)); for (let e = 1; e <= 10; e++) if (r.episodes_qtf[e * 3 + 2] & 1) { r.episodes_qtf[e * 3] = q; r.episodes_qtf[e * 3 + 1] = q; } done({ rerender: true }); }]]);
  };
  return h('div', { class: 'pad' },
    h('table', { class: 'grid episodes' }, h('thead', {}, h('tr', {}, ...['#', T('On air|episode'), showText ? T('Title and text: English, Deutsch') : T('Title: English, Deutsch'), T('Q, top.')].map((x) => h('th', {}, x)))), tb),
    h('div', { class: 'inline', style: 'margin-top:8px' },
      h('button', { class: 'plate small white', onclick: same4all }, T('Same quality for all ...')),
      h('button', { class: 'plate small white' + (showText ? ' on' : ''), onclick: () => { st.epText = !st.epText; renderEntry(); } }, showText ? T('Hide episode texts') : T('Show episode texts')),
      h('span', { class: 'hint' }, T('The card shows "n.title" of the next episode.'))));
}
function phrasesTable(r, ctx, done) {
  const tb = h('tbody');
  for (let k = 0; k < 10; k++) {
    const tin = (l) => {
      const inp = h('input', { type: 'text', value: txt(r, k, l), 'data-text': `${k}-${l}` });
      inp.addEventListener('input', () => { setTxt(r, k, inp.value, l); st.undoKey = `ph:${r.id}:${k}:${l}`; inp.classList.toggle('warned', missingChars(inp.value).length > 0); done(); });
      return inp;
    };
    const val = h('input', { type: 'number', min: 0, max: 255, value: r.phrase_value[k], 'data-pv': k });
    val.addEventListener('input', () => { r.phrase_value[k] = Math.max(0, Math.min(255, Math.trunc(+val.value || 0))); st.undoKey = `pv:${r.id}:${k}`; done(); });
    tb.append(h('tr', {}, h('td', { class: 'n' }, k + 1), h('td', {}, tin('en')), h('td', {}, tin('de')), h('td', {}, val)));
  }
  return h('div', { class: 'pad' }, h('table', { class: 'grid' }, h('thead', {}, h('tr', {}, ...['#', T('Phrase, English'), T('Phrase, Deutsch'), T('Value')].map((x) => h('th', {}, x)))), tb),
    h('div', { class: 'hint', style: 'margin-top:8px' }, T("A story's #n takes one of these phrases; its value adds to the story's value.")));
}

/* a screenplay's roles (game.js, "screenplays: the production"): the name in both languages
   (texts 1..15), the actor category the shop sells for it, the three requirements the fit
   reads; a role in a list stays until it is out of the lists */
function rolesTable(r, ctx, done) {
  const n = spRoleCount(r), series = spIsSeries(r), attrs = gameStr(363).split('~');
  const tb = h('tbody');
  for (let t = 1; t <= n; t++) {
    const use = spRoleUse(r, t);
    const name = (l) => {
      const inp = h('input', { type: 'text', value: txt(r, t, l), 'data-text': `${t}-${l}` });
      inp.addEventListener('input', () => { setTxt(r, t, inp.value, l); inp.classList.toggle('warned', missingChars(inp.value).length > 0); st.undoKey = `sprole:${r.id}:${t}:${l}`; done(); });
      inp.classList.toggle('warned', missingChars(inp.value).length > 0);
      return h('div', { class: 'ttrow' }, h('span', { class: 'tl' }, l.toUpperCase()), inp);
    };
    const cat = h('select', { 'data-role-cat': t, 'data-gametext': true, 'aria-label': T('Actor category of role {n}', { n: t }),
      onchange: (e) => { spSet(r, SP_CAT(t), +e.target.value); spTidy(r); st.undoKey = `sprole:${r.id}:${t}:cat`; done(); } },
    ...[0, 1, 2, 3, 4, 5, 6, 7].map((c) => h('option', { value: c, selected: unkGet(r, SP_CAT(t)) === c }, c ? actorCat(c) : '-')));
    const req = h('div', { class: 'spreq', 'data-role-req': t }, ...SP_ATTRS.map((i, k) => {
      const inp = h('input', { type: 'number', min: 0, max: 20, value: unkGet(r, SP_REQ(t) + i), 'data-req': `${t}-${i}`, 'aria-label': `${attrs[k] || ''} (${T('role {n}', { n: t })})` });
      inp.addEventListener('input', () => { spSet(r, SP_REQ(t) + i, Math.max(0, Math.min(20, Math.trunc(+inp.value || 0)))); spTidy(r); st.undoKey = `sprole:${r.id}:${t}:${i}`; done(); });
      inp.addEventListener('change', () => { inp.value = unkGet(r, SP_REQ(t) + i); });
      return inp;
    }));
    ctx.req[`role${t}-category`] = cat;
    ctx.req[`role${t}-req`] = req;
    const remove = () => {
      spSet(r, SP_CAT(t), 0);
      for (const i of [0, 1, 2, 3]) spSet(r, SP_REQ(t) + i, 0);
      spTidy(r);
      for (const l of LANGS) if (txt(r, t, l)) setTxt(r, t, '', l);
      done({ rerender: true });
    };
    tb.append(h('tr', { 'data-role': t }, h('td', { class: 'n' }, t), h('td', { class: 'tt' }, name('en'), name('de')), h('td', {}, cat), h('td', {}, req),
      h('td', { class: 'spuse' }, !use.length ? h('span', { class: 'hint' }, T('in no list')) : series ? TP(use.length, 'episode {list}', 'episodes {list}', { list: numRanges(use) }) : T('in the list')),
      h('td', { class: 'n' }, h('button', { class: 'reset', 'data-role-del': t, disabled: use.length ? true : null, onclick: remove, 'aria-label': T('Remove role {n}', { n: t }),
        'data-tip': use.length ? T('In a shopping list: take it out of the lists first') : T('Remove role {n}', { n: t }) }, icon('remove', 14)))));
  }
  const add = () => {
    const t = spRoleCount(r) + 1;
    spSet(r, SP_CAT(t), series ? 1 : t === 1 ? 5 : 7);
    for (const i of SP_ATTRS) spSet(r, SP_REQ(t) + i, 10);
    spTidy(r);
    done({ rerender: true });
  };
  return h('div', { class: 'pad' },
    n ? h('table', { class: 'grid sproles' }, h('thead', {}, h('tr', {}, ...['#', T('Name: English, Deutsch'), T('Actor category'), attrs.join(' / '), T('Used'), ''].map((x) => h('th', {}, x)))), tb)
      : h('div', { class: 'hint' }, T('No roles yet.')),
    h('div', { class: 'inline', style: 'margin-top:8px' },
      h('button', { class: 'plate small white', id: 'addrole', disabled: n >= SP_ROLES ? true : null, onclick: add }, icon('plus', 14), T('Add role')),
      h('span', { class: 'hint' }, T("Requirements 0-20, as the actors' values: an actor fits by the sum of min(asked, has) over the sum asked. A role counts through its entries in the lists."))));
}
/* a screenplay's shopping lists: one card each -- its base quality, its entries (a role, the
   effects or a set, and how much it weighs in the quality) */
function listsView(r, ctx, done) {
  const series = spIsSeries(r), n = spEpisodes(r), tgt = spTarget(r);
  const box = h('div', { class: 'pad splists' });
  const link = tgt ? h('button', { class: 'plate small grey', onclick: () => go(series ? 'series' : 'films', tgt.id) }, T('Edit #{id}', { id: tgt.id })) : null;
  if (series) {
    box.append(h('div', { class: 'inline sphead' }, h('span', { class: 'hint' }, !tgt ? T('No series chosen yet (Becomes): a list for each of its episodes.')
      : n ? TP(n, 'One list for the one episode of series #{id}.', 'One list for each of the {n} episodes of series #{id}.', { id: tgt.id }) : T('Series #{id} has no episodes yet: set them there, then each gets a list.', { id: tgt.id })), link));
  } else box.append(h('div', { class: 'hint sphead' }, r.flags & 4 ? T('The game takes this list for each of the up to 10 productions.') : T('One production.')));
  const grid = h('div', { class: 'speps' });
  for (let e = 1; e <= n; e++) grid.append(listCard(r, e, ctx, done));
  box.append(grid);
  box.append(h('div', { class: 'hint' }, T('Quality when shot: base quality x the entries\' scores / their weights x 1.25 -- a role scores by the actor\'s fit, the effects and a set by the level bought. A studio holds 1 to 3 sets: the first list decides which studios take the screenplay.')));
  return box;
}
function listCard(r, e, ctx, done) {
  const series = spIsSeries(r), base = SP_EP(e), list = spList(r, e), n = spEpisodes(r), tgt = spTarget(r);
  const put = (l, rerender) => { spPutList(r, e, l); st.undoKey = `splist:${r.id}:${e}:${Math.random()}`; done({ rerender }); };
  const q = h('input', { type: 'number', min: 0, max: SP_QMAX, value: unkGet(r, base), 'data-ep-q': e, 'aria-label': T('Base quality') });
  const best = h('span', { class: 'note' });
  const showBest = () => { const v = unkGet(r, base); best.textContent = v ? T('at best {q}', { q: tdiv(v * 125, 100) }) : ''; };
  q.addEventListener('input', () => { spSet(r, base, Math.max(0, Math.min(SP_QMAX, Math.trunc(+q.value || 0)))); spTidy(r); st.undoKey = `spq:${r.id}:${e}`; showBest(); done(); });
  q.addEventListener('change', () => { q.value = unkGet(r, base); });
  showBest();
  ctx.req[`ep${e}-quality`] = q;
  const items = h('div', { class: 'spitems', 'data-ep-list': e });
  ctx.req[`ep${e}-list`] = items;
  const twice = new Set(list.map((x) => x.item).filter((it, k, all) => it > SP_ROLES && all.indexOf(it) !== k));
  list.forEach((x, k) => {
    const sel = h('select', { 'data-item': `${e}-${k + 1}`, 'data-gametext': true, class: twice.has(x.item) ? 'warned' : null,
      'data-tip': twice.has(x.item) ? T('More than once in this list: bought and paid each time') : null, 'aria-label': T('Entry {k}', { k: k + 1 }), onchange: (ev) => { list[k].item = +ev.target.value; list[k].got = 0; put(list, true); } }, ...itemOptions(r, x.item));
    const w = h('input', { type: 'number', min: 1, max: 10, value: x.weight, 'data-weight': `${e}-${k + 1}`, 'aria-label': T('Weight'), 'data-tip': T('Weight: how much it counts in the quality (1-10)') });
    w.addEventListener('input', () => { list[k].weight = Math.max(1, Math.min(10, Math.trunc(+w.value || 0))); spPutList(r, e, list); st.undoKey = `spw:${r.id}:${e}:${k}`; done(); });
    w.addEventListener('change', () => { w.value = spList(r, e)[k]?.weight ?? ''; });
    items.append(h('div', { class: 'spitem' }, sel, h('span', { class: 'x' }, 'x'), w,
      h('button', { class: 'reset', 'data-item-del': `${e}-${k + 1}`, 'aria-label': T('Remove entry {k}', { k: k + 1 }), onclick: () => { list.splice(k, 1); put(list, true); } }, icon('remove', 14))));
  });
  if (!list.length) items.append(h('div', { class: 'hint' }, T('No entries yet.')));
  const sets = list.filter((x) => spIsSet(x.item)).length;
  const setsInfo = h('span', { class: 'hint' + (e === 1 && sets > SP_SETS ? ' err' : '') }, TP(sets, '{n} set', '{n} sets'));
  if (e === 1) ctx.req['ep1-sets'] = setsInfo;
  const firstFree = () => { for (let t = 1; t <= spRoleCount(r); t++) if (!list.some((x) => x.item === t)) return t; return 16; };
  const others = series ? [...Array(n).keys()].map((k) => k + 1).filter((x) => x !== e && !spList(r, x).length) : [];
  const copyTo = (to) => {
    for (const x of to) { spSet(r, SP_EP(x), unkGet(r, base)); spPutList(r, x, list.map((y) => ({ item: y.item, weight: y.weight, got: 0 }))); }
    st.undoKey = null; done({ rerender: true });
    status(TP(to.length, 'Copied to episode {list}.', 'Copied to episodes {list}.', { list: numRanges(to) }), 'ok');
  };
  const title = series ? [T('Episode {e}', { e }), tgt && txt(tgt, 1 + e) ? h('span', { class: 'hint' }, txt(tgt, 1 + e)) : null] : [T('The list')];
  return h('div', { class: 'spep', 'data-ep': e },
    h('div', { class: 'spephead' }, ...title, setsInfo),
    h('label', { class: 'inline spq' }, T('Base quality'), q, best),
    items,
    h('div', { class: 'inline spfoot' },
      h('button', { class: 'plate small white', 'data-item-add': e, disabled: list.length >= SP_ITEMS ? true : null, onclick: () => { list.push({ item: firstFree(), weight: 5, got: 0 }); put(list, true); } }, icon('plus', 14), T('Entry')),
      series && e > 1 ? h('button', { class: 'plate small white', 'data-ep-prev': e, onclick: () => { spSet(r, base, unkGet(r, SP_EP(e - 1))); spPutList(r, e, spList(r, e - 1).map((y) => ({ item: y.item, weight: y.weight, got: 0 }))); st.undoKey = null; done({ rerender: true }); } }, T('As episode {e}', { e: e - 1 })) : null,
      series && n > 1 ? h('button', { class: 'plate small white sq', 'data-ep-copy': e, disabled: list.length && others.length ? null : true, onclick: () => copyTo(others),
        'aria-label': T('Copy to the episodes without a list'), 'data-tip': T('Copy to the episodes without a list') }, icon('copy', 14)) : null));
}
/* the items of a list entry: the roles (up to the highest one), the effects, the sets of the shop
   (16-38; a higher one only when the entry has it: the shop's tables end at 38) */
function itemOptions(r, cur) {
  const n = Math.max(spRoleCount(r), cur <= SP_ROLES ? cur : 0);
  const roles = [...Array(n).keys()].map((k) => k + 1), props = [...Array(23).keys()].map((k) => k + 16);
  if (cur > 38 && !props.includes(cur)) props.push(cur);
  return [
    roles.length ? h('optgroup', { label: T('Roles') }, ...roles.map((t) => h('option', { value: t, selected: cur === t }, spItemName(r, t)))) : null,
    h('optgroup', { label: T('Studio needs') }, ...props.map((p) => h('option', { value: p, selected: cur === p }, propName(p)))),
  ];
}

/* a news story's category as the editor names it */
const newsCategory = (c) => NEWS_CATEGORY[c] || T('category {n}', { n: c });
/* after a switch of the interface language (setUiLang): everything drawn again in it; the
   records, unsaved changes and undo stay */
function uiLangChanged() {
  problemsCache = { v: -1, list: [] };
  closePopover(); hideResults(); $('#gresults').innerHTML = ''; $('#tip').style.display = 'none';
  if (!st.meta) return;
  renderAll();
  status(T('Interface: English'));
}
async function start() {
  translateStatic();
  status(T('Loading the game files ...'));
  try {
    await init();
    if (new URLSearchParams(location.search).has('selftest')) await selftest();
  } catch (e) { status(T('The editor could not start: {why}', { why: e.message }), 'err'); document.title = 'SELFTEST FAIL: ' + e.message; console.error(e); }
}
