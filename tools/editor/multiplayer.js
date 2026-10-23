/* OpenMTV editor -- "Multiplayer": the lobby of a network game (multiplayer stage 5,
 * the former Python lobby (mplobby.py), docs/multiplayer.md).  Before a game: on the internet (the public server's
 * open games; the default) or in the LAN (this machine runs the server; the others find it or
 * type its address), the list of open games kept fresh by itself while it is shown, the
 * player's name, open a game or join one.  In the lobby: the invitation (the game's code or
 * this machine's addresses), the three stations in the game's colours and who sits where (each
 * player's version, original files and built content packs checked against the host's), the
 * game as the host sets it (language, Classic, fixes, difficulty, fourth speed, the ad and film offers, the amounts of money -- start money,
 * credit line, terror damage, Sammy prize money --, the
 * content packs: a selection of its own), ready, start -- the band on top holds the invitation,
 * who is ready and the big button, the game is in the three tabs of the page "Play" (gameTabs:
 * packs, rules, fixes; no side column; Classic -- a choice in the tab "Fixes" -- travels as
 * "fixes = none", the host's choice of fixes kept in the file).  Before a game the saved network games too, a
 * card each (go on as the host, rename, delete here only).  Network games have a settings file
 * of their own (multiplayer.ini, apart from the single player's): the display and the sound in
 * it are the page "Settings"'s, the same as the single player's (launcher/server.go writes both
 * files), the mouse always System.  The host's game is kept too: the rules in that file, the
 * difficulty and the packs in this browser.  At the start every launcher starts its game with
 * its own display and sound and the host's game.  The page asks its
 * server for the lobby's state while it is shown.  The self-test is dry (nothing started, no
 * list asked for by itself) and brings its own second player (/api/mp/bot). */
'use strict';
st.mp = st.mp || { snap: null, found: null, rooms: null, finding: false, listAt: 0, listErr: null, where: 'internet', address: '', code: '', listed: true,
  sig: '', err: null, timer: null, fixGroup: 0, onlyChanged: false, building: false, packs: null, saves: null, saveOpen: {}, saveMore: {}, saveMenu: null };
const MP_NAME_KEY = 'madtv-mp-name', MP_PACKS_KEY = 'madtv-mp-packs', MP_DIFF_KEY = 'madtv-mp-difficulty', MP_KNOCK_KEY = 'madtv-mp-knock', MP_SERVER_KEY = 'madtv-mp-server';
const mpDry = () => new URLSearchParams(location.search).has('selftest');
const cleanMpName = (v) => String(v || '').replace(/[^\x20-\x7e]|,/g, '').slice(0, 8);
function mpName() { try { return localStorage.getItem(MP_NAME_KEY) || ''; } catch (e) { return ''; } }
function setMpName(v) { try { localStorage.setItem(MP_NAME_KEY, v); } catch (e) { /* ignore */ } }
/* the public server: ours (the launcher's default, $MADTV_SERVER) unless another one is set here */
function mpServerSet() { try { return localStorage.getItem(MP_SERVER_KEY) || ''; } catch (e) { return ''; } }
const mpServerUrl = () => mpServerSet() || (st.mp.snap && st.mp.snap.server) || 'wss://openmtv.net';
const mpServerName = (url = mpServerUrl()) => String(url).replace(/^wss?:\/\//, '').replace(/\/$/, '');
function setMpServer(v) {
  v = String(v || '').trim();
  if (v && !/^wss?:\/\//.test(v)) v = 'wss://' + v;
  if (v && !/^wss?:\/\/[\w.-]+(:\d+)?\/?$/.test(v)) { status(T('A server: its name, e.g. madtv.example.org, or wss://name:port.'), 'err'); return; }
  if (v === (st.mp.snap && st.mp.snap.server)) v = '';
  try { if (v) localStorage.setItem(MP_SERVER_KEY, v); else localStorage.removeItem(MP_SERVER_KEY); } catch (e) { /* ignore */ }
  st.mp.serverEdit = false; st.mp.rooms = null; st.mp.listErr = null;
  renderEntry(); mpList(true);
}
const mpInternet = () => st.mp.where === 'internet';
/* where the game is held: the public server on the internet (stage 6), or this machine in the LAN */
function setMpWhere(v) { st.mp.where = v; st.mp.listErr = null; renderEntry(); mpList(true); }
/* the host's packs for network games: a selection of their own, kept in this browser */
function mpPacks() {
  if (!st.mp.packs) {
    let v = null;
    try { v = JSON.parse(localStorage.getItem(MP_PACKS_KEY) || 'null'); } catch (e) { /* ignore */ }
    st.mp.packs = Array.isArray(v) ? v.filter((n) => typeof n === 'string') : [];
  }
  return st.mp.packs.filter((n) => modInfo(n));
}
function setMpPacks(packs) {
  st.mp.packs = packs;
  try { localStorage.setItem(MP_PACKS_KEY, JSON.stringify(packs)); } catch (e) { /* ignore */ }
  if (mpIn() && st.mp.snap.role === 'host') mpGame({}, packs.join('+'));
  else renderEntry();
}
const KNOCK_CHOICES = [0, 5, 10, 15, 30];
function mpKnock() { try { const v = localStorage.getItem(MP_KNOCK_KEY); return v !== null && KNOCK_CHOICES.includes(+v) ? +v : 15; } catch (e) { return 15; } }
function mpDifficulty() { try { return [1, 2, 3].includes(+localStorage.getItem(MP_DIFF_KEY)) ? +localStorage.getItem(MP_DIFF_KEY) : 2; } catch (e) { return 2; } }
/* the host's game as the network games' settings file has it (and the difficulty kept here) */
function mpDefaults() {
  const fx = sv('game', 'classic', true) === 'yes' ? 'none' : sv('game', 'fixes', true) || 'all';
  return { lang: sv('game', 'language', true) === 'german' ? 'd' : 'e', fixes: fx, difficulty: mpDifficulty(),
    speed4: sv('game', 'fourth_speed', true) === 'yes', aiimp: sv('game', 'ai_improve', true) || 'off', money: Number(sv('game', 'start_money', true) || 200000),
    credit: sv('game', 'credit_line', true) === CREDIT_ORIGINAL ? 0 : Number(sv('game', 'credit_line', true) || 700000), terror: Number(sv('game', 'terror_damage', true) ?? 50000),
    sammy: SAMMY_KEYS.map((k) => Number(sv('game', k, true) ?? 0)), offers: Number(sv('game', 'ad_offers', true) || 10),
    filmoffers: Number(sv('game', 'film_offers', true) || 10), sertop: sv('game', 'series_topicality', true) !== 'no', knock: mpKnock(), castfit: sv('display', 'cast_fit', true) === 'yes' };
}
/* the host changes the game: in the lobby, and kept for the next one */
function mpGameSet(changes) {
  const keep = {};
  if ('lang' in changes) keep.language = changes.lang === 'd' ? 'german' : 'english';
  if ('fixes' in changes) keep.fixes = changes.fixes;
  if ('speed4' in changes) keep.fourth_speed = changes.speed4 ? 'yes' : 'no';
  if ('sertop' in changes) keep.series_topicality = changes.sertop ? 'yes' : 'no';
  if ('aiimp' in changes) keep.ai_improve = changes.aiimp;
  if ('offers' in changes) keep.ad_offers = String(changes.offers);
  if ('filmoffers' in changes) keep.film_offers = String(changes.filmoffers);
  if ('money' in changes) keep.start_money = String(changes.money);
  if ('credit' in changes) keep.credit_line = changes.credit === 0 ? CREDIT_ORIGINAL : String(changes.credit);   /* 0: the original's credit system */
  if ('terror' in changes) keep.terror_damage = String(changes.terror);
  if ('sammy' in changes) SAMMY_KEYS.forEach((k, i) => { keep[k] = String(changes.sammy[i]); });
  if ('difficulty' in changes) try { localStorage.setItem(MP_DIFF_KEY, String(changes.difficulty)); } catch (e) { /* ignore */ }
  if ('knock' in changes) try { localStorage.setItem(MP_KNOCK_KEY, String(changes.knock)); } catch (e) { /* ignore */ }
  const values = {};
  if (Object.keys(keep).length) values.game = keep;
  if ('castfit' in changes) values.display = { cast_fit: changes.castfit ? 'yes' : 'no' };   /* (the host's own, kept for the next game) */
  if (Object.keys(values).length) api('PUT', SETTINGS_URL(true), { values }).then((d) => { st.mpSettings = d; }).catch((e) => status(e.message, 'err'));
  return mpGame(changes);
}
/* the host: Classic on -- no fixes, for everyone ("fixes = none" in the game) -- or off: the fixes
   as chosen, kept in the network games' file */
async function mpSetClassic(on) {
  let fx = sv('game', 'fixes', true) || 'all';
  const keep = { classic: on ? 'yes' : 'no' };
  if (!on && fx === 'none') { fx = 'all'; keep.fixes = 'all'; }
  try { st.mpSettings = await api('PUT', SETTINGS_URL(true), { values: { game: keep } }); } catch (e) { status(e.message, 'err'); }
  return mpGame({ fixes: on ? 'none' : fx });
}
const mpState = () => (st.mp.snap && st.mp.snap.state) || null;
const mpIn = () => !!(st.mp.snap && st.mp.snap.role && mpState() && !['closed', 'ended'].includes(mpState().phase));
const mpMe = () => (mpState()?.players || []).find((p) => p.id === st.mp.snap.you);
const mpHostP = () => (mpState()?.players || []).find((p) => p.host);
/* the server's codes in the interface language */
function mpProblem(p) {
  const n = { name: p.name };
  return {
    alone: T('Waiting for a second player.'),
    station: T('{name}: a station taken twice.', n),
    version: T('{name} plays another version of the game.', n),
    files: T('{name} has other original game files.', n),
    mods: T('{name} has not built the content packs yet.', n),
    ready: T('{name} is not ready yet.', n),
    savehave: T('{name} does not have the savegame yet.', n),
    savefree: T('Nobody sits on {name}\'s station of the savegame.', n),
    savestation: T('{name} sits on a station that is the computer\'s in the savegame.', n),
  }[p.code] || `${p.code} ${p.name}`;
}
function mpError(e) {
  return {
    station: T('That station is taken.'), full: T('The game is full: three players.'), started: T('The game has started already.'),
    not_yet: T('Not everyone is ready yet.'), host: T('Only the host can do that.'), join: T('Not in the game (any more).'),
    save: T('The server did not take that savegame.'), mod: T('The server did not take those packs.'),
    room: T('No such game (any more): check the code.'), bridge: T('Your game could not reach the server.'),
  }[e.code] || e.msg;
}
const mpWhy = (w) => ({ 'the host closed the game': T('The host closed the game.'), 'the game is over': T('The game is over.'),
  'the connection to the game is gone': T('The connection to the game is gone.'), 'not every player\'s game started': T('Not every player\'s game started.') }[w] || w || '');
/* the page's own log (the launcher's is the build's): keys, so a switch of the language shows them anew */
function mpLog(key, cls = '', fresh = false, params = null) {
  if (fresh) st.mpLog = [];
  st.mpLog = st.mpLog || [];
  st.mpLog.push({ t: new Date().toLocaleTimeString('en-GB'), key, params, cls });
  const el = $('#mplog');
  if (el) fillMpLog(el);
}
function fillMpLog(el) {
  el.innerHTML = '';
  for (const x of st.mpLog || []) el.append(h('div', { class: x.cls }, `${x.t}  ${T(x.key, x.params)}`));
  if (!(st.mpLog || []).length) el.append(h('div', { class: 'hint' }, T('The lobby\'s messages appear here.')));
  const strip = el.closest('.logstrip');             /* (before a game: there once there are any; in the lobby always) */
  if (strip && !strip.hasAttribute('data-always')) strip.classList.toggle('empty', !(st.mpLog || []).length);
}
/* the messages under the band: the page's own log, the game's start (its seat, its command) */
function mpLogStrip(always) {
  const s = st.mp.snap, go = s.go;
  const log = h('div', { id: 'mplog', class: 'log', role: 'log' });
  const strip = h('div', { class: 'logstrip', 'data-always': always ? '' : null }, h('span', { class: 'overline' }, T('Messages')),
    h('div', { class: 'logcol' }, log,
      go && !go.error ? h('div', { class: 'gonote', 'data-mp': 'go' }, go.pid ? T('Your game started: seat {seat}.', { seat: go.seat + 1 }) : T('Your game would start: seat {seat}.', { seat: go.seat + 1 })) : null,
      go && go.error ? h('div', { class: 'gonote err', 'data-mp': 'go' }, T('Your game did not start: {why}', { why: go.error })) : null,
      go && go.ended ? h('div', { class: 'gonote err', 'data-mp': 'ended-why' }, T('Your game ended: {why}', { why: go.ended })) : null,
      s.cmd ? h('code', { class: 'cmd', 'data-mp': 'cmd' }, s.cmd) : null));
  fillMpLog(log);
  return strip;
}
async function mpCall(what, body) {
  try {
    st.mp.snap = await api('POST', `/api/mp/${what}`, body || {});
    return true;
  } catch (e) { status(e.message, 'err'); return false; } finally { renderEntry(); }
}
async function mpRefresh() {
  try { st.mp.snap = await api('GET', '/api/mp'); } catch (e) { return; }
  const e = st.mp.snap.error;
  if (e && JSON.stringify(e) !== JSON.stringify(st.mp.err)) status(mpError(e), 'err');
  st.mp.err = e;
}
/* while the page is shown: the lobby's state, drawn again when it changed (not while typing);
   before a game the open games, asked for again every few seconds (not in the self-test) */
function mpPoll() {
  if (st.mp.timer) return;
  st.mp.timer = setInterval(async () => {
    if (st.section !== 'multiplayer') { clearInterval(st.mp.timer); st.mp.timer = null; return; }
    await mpRefresh();
    const s = st.mp.snap, sig = JSON.stringify([s.role, s.state, s.mods, s.save, s.go, s.connected]);
    const typing = document.activeElement && document.activeElement.closest('#work, #inspector') && ['INPUT', 'SELECT'].includes(document.activeElement.tagName);
    if (sig !== st.mp.sig && !typing) { st.mp.sig = sig; renderEntry(); }
    if (!mpDry()) mpList(false);
  }, 700);
}
/* the open games: the public server's list, or the LAN's answers; drawn in place (a name or a
   code being typed keeps its focus) */
async function mpList(force) {
  if (mpIn() || st.mp.finding || st.section !== 'multiplayer') return;
  const internet = mpInternet();
  if (!force && Date.now() - st.mp.listAt < (internet ? 4000 : 6000)) return;
  st.mp.finding = true;
  let list = null, err = null;
  try { list = await api('GET', internet ? '/api/mp/rooms' + (mpServerSet() ? '?server=' + encodeURIComponent(mpServerSet()) : '') : '/api/mp/find'); } catch (e) { err = e.message; }
  st.mp.finding = false;
  st.mp.listAt = Date.now();
  if (internet !== mpInternet()) return;          /* (switched meanwhile) */
  if (list) st.mp[internet ? 'rooms' : 'found'] = list;
  else if (!st.mp[internet ? 'rooms' : 'found']) st.mp[internet ? 'rooms' : 'found'] = [];
  st.mp.listErr = err;
  const el = $('#mplist');
  if (el) fillMpList(el);
}
async function mpHost() {
  const name = cleanMpName(mpName());
  if (!name) { status(T('Your name first.'), 'err'); return; }
  const internet = mpInternet();
  if (internet) mpLog('Opening a game on {server} ...', '', true, { server: mpServerName() });
  else mpLog('Opening a game ...', '', true);
  const port = mpDry() ? 'free' : undefined;
  st.mp.saves = null;                          /* (the savegames read anew: a game may have saved more) */
  if (await mpCall('host', { name, settings: mpDefaults(), packs: mpPacks().join('+'), dry: mpDry(), port, listen: mpDry() ? '127.0.0.1' : undefined,
    internet, listed: st.mp.listed, server: internet && mpServerSet() ? mpServerSet() : undefined }))
    mpLog('Opened: the others join with {address}.', 'ok', false, { address: mpAddress() });
}
async function mpJoinRoom(code) {
  const name = cleanMpName(mpName());
  code = String(code || '').trim().toLowerCase();
  if (!name) { status(T('Your name first.'), 'err'); return; }
  if (!/^[0-9a-f]{6}$/.test(code)) { status(T('The game\'s code: six letters and digits, e.g. 3fa9c1.'), 'err'); return; }
  mpLog('Joining the game {code} on {server} ...', '', true, { code, server: mpServerName() });
  if (await mpCall('join', { room: code, name, dry: mpDry(), server: mpServerSet() || undefined })) mpLog('Joined.', 'ok');
}
async function mpJoin(address) {
  const name = cleanMpName(mpName());
  if (!name) { status(T('Your name first.'), 'err'); return; }
  if (!address) { status(T('The address of the game: the host\'s machine, e.g. 192.168.1.20.'), 'err'); return; }
  mpLog('Joining {address} ...', '', true, { address });
  if (await mpCall('join', { address, name, dry: mpDry() })) mpLog('Joined.', 'ok');
}
/* the host's game: settings changed (the others as they are), or the packs anew */
async function mpGame(changes, packs) {
  st.mp.building = packs !== undefined;
  if (st.mp.building) renderEntry();
  try { return await mpCall('game', { settings: Object.assign({}, mpState().settings, changes), packs: packs ?? mpPacks().join('+') }); } finally { if (st.mp.building) { st.mp.building = false; renderEntry(); } }
}
/* where the others join: this machine's LAN addresses and the port, or the game's code on the internet */
function mpAddress() {
  const s = st.mp.snap || {};
  if (s.internet) return T('the code {code} (on {server})', { code: s.room || '?', server: mpServerName(s.internet) });
  return mpLanAddresses().join(T(' or '));
}
function mpLanAddresses() {
  const s = st.mp.snap || {}, port = (s.address || ':7640').split(':').pop();
  return (s.addresses && s.addresses.length ? s.addresses : ['127.0.0.1']).map((a) => `${a}:${port}`);
}
function mpCopy(text) {
  const done = () => status(T('Copied: {text}', { text }), 'ok');
  try { navigator.clipboard.writeText(text).then(done, () => status(text, 'ok')); } catch (e) { status(text, 'ok'); }
}

/* the network games saved on this machine (src/mpsave.c: every night, a directory per game) */
async function mpSaves(force) {
  if (st.mp.saves && !force) return st.mp.saves;
  try { st.mp.saves = await api('GET', '/api/mp/saves' + (mpDry() ? '?test=1' : '')); } catch (e) { st.mp.saves = []; }
  return st.mp.saves;
}
const mpSaveNames = (humans) => (Array.isArray(humans) ? humans : []).map((x) => (x && x.name) || T('Station {n}', { n: x && x.station })).join(' · ') || '?';
const mpWhen = (t) => new Date(t * 1000).toLocaleString(UI.lang === 'de' ? 'de-DE' : 'en-GB', { dateStyle: 'medium', timeStyle: 'short' });
/* the host: on from a savegame (its game's directory, the night's file), or a new game */
async function mpChooseSave(game, file) {
  st.mp.building = true;
  renderEntry();
  let ok = false;
  try { ok = await mpCall('save', game ? { game, file } : {}); } finally { st.mp.building = false; renderEntry(); }
  /* a new game again: the host's own rules and packs, not the saved game's */
  if (ok && !game) await mpGame(mpDefaults(), mpPacks().join('+'));
  return ok;
}
/* before a game: on from a saved game -- a game opened as its host, that night chosen */
async function mpGoOn(game, file) {
  await mpHost();
  if (mpIn()) await mpChooseSave(game, file);
}
const mpSavesQ = () => (mpDry() ? '?test=1' : '');
async function mpSavesPost(what, body) {
  try { st.mp.saves = await api('POST', `/api/mp/saves/${what}${mpSavesQ()}`, body); return true; } catch (e) { status(e.message, 'err'); return false; } finally { renderEntry(); }
}
/* a night, or (no night) the whole game: deleted on this machine only, after asking */
function mpDeleteSave(x, n) {
  st.mp.saveMenu = null;
  dialog(n ? T('Delete day {day}?', { day: n.day }) : T('Delete "{game}"?', { game: mpSaveTitle(x) }),
    h('p', {}, n ? T('The savegame of day {day} is deleted.', { day: n.day }) : TP(x.saves.length, 'The game and its one night are deleted.', 'The game and its {n} nights are deleted.'),
      ' ', T('Only on this machine: the other players keep their copies.')),
    [[T('Cancel'), '', null], [T('Delete'), 'red', () => mpSavesPost('delete', { game: x.game, file: n ? n.file : undefined })]]);
}
function mpRenameSave(x) {
  st.mp.saveMenu = null;
  const inp = h('input', { type: 'text', maxlength: '40', value: x.title || '', placeholder: mpSaveNames(x.saves[0].humans), 'aria-label': T('Name'), 'data-mp': 'savetitle' });
  setTimeout(() => inp.focus(), 0);
  dialog(T('Rename the saved game'), h('div', { class: 'fields' }, inp, h('span', { class: 'hint' }, T('Empty: its players\' names again.'))),
    [[T('Cancel'), '', null], [T('Rename'), 'red', () => mpSavesPost('rename', { game: x.game, title: inp.value })]]);
}
async function mpOpenSave(x) {
  st.mp.saveMenu = null;
  try { await api('POST', `/api/mp/saves/open${mpSavesQ()}`, { game: x.game }); } catch (e) { status(e.message, 'err'); }
  renderEntry();
}
const mpSaveTitle = (x) => x.title || mpSaveNames(x.saves[0].humans);
const mpCash = (v) => (typeof v !== 'number' ? '' : Math.abs(v) >= 1e6 ? T('{n} M|money', { n: (v / 1e6).toLocaleString(UI.lang === 'de' ? 'de-DE' : 'en-US', { maximumFractionDigits: 1 }) })
  : T('{n} K|money', { n: numFmt(Math.round(v / 1000)) }));
const mpDate = (s) => { const d = new Date(String(s || '').replace(' ', 'T')); return isNaN(d) ? '' : d.toLocaleDateString(UI.lang === 'de' ? 'de-DE' : 'en-GB', { day: 'numeric', month: 'short' }); };
const mpAvatars = (humans) => h('span', { class: 'avs', 'aria-hidden': 'true' }, ...(humans || []).map((p) => h('i', { class: 's' + p.station }, String(p.name || '?').slice(0, 1).toUpperCase())));
/* a saved game: its players in their stations' colours, its name, when it began, how it is
   played; the latest night; folded or its nights with each player's money.  where: 'start'
   (go on, load, rename, delete) or 'lobby' (the host chooses the night) */
function saveCard(x, where) {
  const lobby = where === 'lobby', newest = x.saves[0], open = !!st.mp.saveOpen[x.game], more = !!st.mp.saveMore[x.game];
  const sv = lobby ? mpState().save : null, chosen = !!(sv && sv.meta && sv.meta.game === x.game), chosenFile = chosen ? sv.meta.file : null;
  const names = mpSaveNames(newest.humans);
  const diff = { 1: T('Easy'), 2: T('Normal'), 3: T('Difficult') }[newest.difficulty];
  const about = [x.title ? names : null, x.created ? T('since {date}', { date: mpDate(x.created) }) : null, diff, x.classic ? T('Classic') : null,
    x.packs.length ? x.packs.join(', ') : T('without packs')].filter(Boolean).join(' · ');
  const busy = st.mp.building || !x.loadable;
  const night = (n) => h('div', { class: 'night' + (n.file === chosenFile ? ' on' : ''), 'data-mpnight': `${x.game}/${n.file}` },
    h('span', { class: 'nd' }, T('day {day}', { day: n.day })), h('span', { class: 'nt' }, mpWhen(n.time)),
    h('span', { class: 'nm' }, ...(n.humans || []).map((p) => h('span', { class: 'nmh' }, h('i', { class: 'sdot s' + p.station }), `${p.name || '?'} ${mpCash(p.money)}`,
      p.out ? h('span', { class: 'pill dark' }, T('fired')) : null))),
    lobby ? (n.file === chosenFile ? h('span', { class: 'pill green' }, icon('ok', 12), T('chosen')) : h('button', { class: 'plate small', 'data-mpchoosenight': n.file, disabled: busy, onclick: () => mpChooseSave(x.game, n.file) }, T('Choose')))
      : [h('button', { class: 'plate small', disabled: !x.loadable, onclick: () => mpGoOn(x.game, n.file) }, T('Load')),
        h('button', { class: 'ib', 'data-mpdelnight': n.file, 'aria-label': T('Delete day {day} ...', { day: n.day }), 'data-tip': T('Delete day {day} ...', { day: n.day }), onclick: () => mpDeleteSave(x, n) }, icon('remove', 15))]);
  const menu = st.mp.saveMenu === x.game ? h('div', { class: 'menu', role: 'menu' },
    h('button', { role: 'menuitem', 'data-mpsaveact': 'rename', onclick: () => mpRenameSave(x) }, T('Rename ...')),
    h('button', { role: 'menuitem', 'data-mpsaveact': 'open', onclick: () => mpOpenSave(x) }, T('Show the folder')),
    h('button', { role: 'menuitem', class: 'danger', 'data-mpsaveact': 'delete', onclick: () => mpDeleteSave(x) }, TP(x.saves.length, 'Delete the game (one night)', 'Delete the game ({n} nights)')),
    h('span', { class: 'hint' }, T('on this machine only; the others keep their copies'))) : null;
  return h('div', { class: 'savecard' + (chosen ? ' on' : '') + (x.loadable ? '' : ' off'), 'data-mpsave': x.game },
    h('div', { class: 'sch' }, mpAvatars(newest.humans), h('span', { class: 'pt' }, h('b', {}, mpSaveTitle(x)), h('span', {}, about)),
      chosen ? h('span', { class: 'pill green', 'data-mpchosen': '1' }, icon('ok', 12), T('chosen: day {day}', { day: sv.meta.day }))
        : h('span', { class: 'newest' }, h('b', {}, T('day {day}', { day: newest.day })), h('span', {}, mpWhen(newest.time)))),
    h('div', { class: 'sca' },
      h('button', { class: 'fold2', 'aria-expanded': open ? 'true' : 'false', 'data-mpsavefold': x.game, onclick: () => { st.mp.saveOpen[x.game] = !open; renderEntry(); } },
        h('span', { class: 'chev' + (open ? ' open' : '') }, icon('unfold', 14)), TP(x.saves.length, 'one night', '{n} nights')),
      x.loadable ? null : h('span', { class: 'reason' }, icon('problem', 14), { tables: T('Not playable here: the tables of its packs are not kept.') }[x.why] || ''),
      h('span', { style: 'flex:1' }),
      lobby ? (chosen ? null : h('button', { class: 'plate small', 'data-mpchoose': x.game, disabled: busy, onclick: () => mpChooseSave(x.game, newest.file) }, T('Choose')))
        : [h('button', { class: 'plate small red', 'data-mpgo': x.game, disabled: !x.loadable, onclick: () => mpGoOn(x.game, newest.file) }, T('Go on')),
          h('span', { class: 'savemenu' }, h('button', { class: 'plate small sq', 'data-mpsavemenu': x.game, 'aria-label': T('More about this game'), 'aria-expanded': menu ? 'true' : 'false',
            onclick: () => { st.mp.saveMenu = menu ? null : x.game; renderEntry(); } }, '⋯'), menu)]),
    open ? h('div', { class: 'nights' }, ...(more ? x.saves : x.saves.slice(0, 4)).map(night),
      x.saves.length > 4 ? h('button', { class: 'link', onclick: () => { st.mp.saveMore[x.game] = !more; renderEntry(); } }, more ? T('fewer') : TP(x.saves.length - 4, 'one older night', '{n} older nights')) : null) : null);
}
/* before a game: the network games saved on this machine, a card each */
function renderMpSavedGames() {
  if (!st.mp.saves) mpSaves().then(() => { if (st.section === 'multiplayer' && !mpIn()) renderEntry(); });
  const games = st.mp.saves || [];
  return h('section', { class: 'step', 'data-mp': 'saved' },
    h('div', { class: 'steph' }, h('h2', {}, T('Saved games')), h('span', { class: 'hint' }, T('on this machine; every network game saves itself each night, on every player\'s machine')),
      h('span', { style: 'flex:1' }), games.length ? h('span', { class: 'hint' }, TP(games.length, 'one game', '{n} games')) : null),
    games.length ? h('div', { class: 'savecards' }, ...games.map((x) => saveCard(x, 'start')))
      : h('div', { class: 'mpempty' }, icon('tv', 22), h('b', {}, T('No network game saved here yet.')), h('span', { class: 'hint' }, T('Every network game saves itself each night, on every player\'s machine.'))));
}

/* ---------------------------------------------------------------- the page */
function renderMultiplayer(w) {
  if (!st.mpSettings) {
    w.append(h('div', { class: 'hint' }, T('Reading the settings ...')));
    loadSettings(false, true).then(() => { if (st.section === 'multiplayer') renderEntry(); }).catch((e) => status(e.message, 'err'));
    return;
  }
  if (!st.mp.snap) { mpRefresh().then(() => { if (st.section === 'multiplayer') renderEntry(); }); w.append(h('div', { class: 'hint' }, T('Reading the lobby ...'))); return; }
  mpPoll();
  if (mpIn()) renderLobby(w);
  else {
    renderMpStart(w);
    if (!mpDry() && !st.mp.listAt) mpList(true);      /* (the first visit: the list at once) */
  }
}
/* the public server: shown, changed here (kept in this browser), back to ours */
function renderMpServer() {
  const set = mpServerSet();
  if (st.mp.serverEdit) {
    const inp = h('input', { type: 'text', 'data-mp': 'server', value: mpServerName(), placeholder: 'openmtv.net', 'aria-label': T('Server'),
      onkeydown: (e) => { if (e.key === 'Enter') setMpServer(e.target.value); if (e.key === 'Escape') { st.mp.serverEdit = false; renderEntry(); } } });
    setTimeout(() => inp.focus(), 0);
    return h('span', { class: 'mpserver edit' }, h('span', {}, T('Server')), inp,
      h('button', { class: 'plate small', 'data-mp': 'server-ok', onclick: () => setMpServer(inp.value) }, T('OK')),
      h('button', { class: 'plate small', 'data-mp': 'server-default', onclick: () => setMpServer('') }, T('Ours')));
  }
  return h('span', { class: 'mpserver' + (set ? ' custom' : '') }, h('span', {}, T('Server')), h('b', { class: 'mono' }, mpServerName()),
    h('button', { class: 'link', 'data-mp': 'server-edit', onclick: () => { st.mp.serverEdit = true; renderEntry(); } }, T('change')));
}
/* a game in the list: the host, its code or address, the stations taken, join */
function mpGameRow(g, internet) {
  const s = st.mp.snap, other = g.version !== s.version, full = g.players >= g.max;
  const id = internet ? g.room : g.address, open = internet || g.phase === 'lobby';
  return h('div', { class: 'mpgame' + (other ? ' other' : '') },
    h('span', { class: 'mpavatar', 'aria-hidden': 'true' }, (g.name || '?').slice(0, 1).toUpperCase()),
    h('span', { class: 'pt' }, h('b', {}, T('The game of {name}', { name: g.name || '?' })),
      h('span', {}, internet ? h('span', { class: 'mono' }, g.room) : h('span', { class: 'mono' }, g.address),
        open ? null : h('span', {}, ' · ' + T('started')),
        other ? h('span', { class: 'pill orange', 'data-tip': g.version }, T('another version ({version})', { version: shortVersion(g.version) })) : null)),
    h('span', { class: 'mpslots', 'data-tip': TP(g.players, 'one player', '{n} players'), 'aria-label': TP(g.players, 'one player', '{n} players') },
      ...[0, 1, 2].slice(0, g.max || 3).map((i) => h('i', { class: i < g.players ? 'on' : '' })),
      h('small', {}, `${g.players}/${g.max || 3}`)),
    h('button', { class: 'plate small' + (other || full || !open ? '' : ' red'), [internet ? 'data-room' : 'data-game']: id, disabled: full || !open,
      onclick: () => (internet ? mpJoinRoom(g.room) : mpJoin(g.address)) }, T('Join')));
}
function fillMpList(el) {
  const internet = mpInternet(), list = internet ? st.mp.rooms : st.mp.found;
  el.innerHTML = '';
  const live = el.parentElement && el.parentElement.querySelector('.mplive');
  if (live) live.classList.toggle('err', !!st.mp.listErr);
  if (st.mp.listErr) {
    el.append(h('div', { class: 'mpempty err' }, icon('problem', 22),
      h('b', {}, internet ? T('{server} does not answer.', { server: mpServerName() }) : T('The search in the LAN failed.')),
      h('span', { class: 'hint' }, st.mp.listErr)));
    return;
  }
  if (!list) {
    el.append(h('div', { class: 'mpempty' }, h('span', { class: 'hint' }, internet ? T('Asking {server} ...', { server: mpServerName() }) : T('Searching the LAN ...'))));
    return;
  }
  if (!list.length) {
    el.append(h('div', { class: 'mpempty' }, icon('tv', 22),
      h('b', {}, internet ? T('No open game yet.') : T('No open game in the LAN yet.')),
      h('span', { class: 'hint' }, internet ? T('New ones appear here by themselves. A game opened unlisted is joined with its code.')
        : T('New ones appear here by themselves. If the host\'s firewall blocks the search, type its address.'))));
    return;
  }
  for (const g of list) el.append(mpGameRow(g, internet));
}
/* before a game: the band -- the name, where, open a game --, the messages, the open games, the
   saved ones */
function renderMpStart(w) {
  const s = st.mp.snap, ended = s.role && mpState() && ['closed', 'ended'].includes(mpState().phase);
  const internet = mpInternet();
  const nameIn = h('input', { type: 'text', 'data-mp': 'name', maxlength: '8', value: mpName(), 'aria-label': T('Your name'), placeholder: T('8 letters at most'),
    oninput: (e) => { const v = cleanMpName(e.target.value); if (v !== e.target.value) e.target.value = v; setMpName(v); } });
  const where = h('div', { class: 'seg mptabs', role: 'group', 'data-mp': 'where' },
    ...[['internet', 'globe', T('On the internet')], ['lan', 'lan', T('In the LAN')]].map(([v, ic, label]) =>
      h('button', { class: st.mp.where === v ? 'on' : '', 'data-value': v, 'aria-pressed': st.mp.where === v ? 'true' : 'false', onclick: () => setMpWhere(v) }, icon(ic, 15), label)));
  const list = h('div', { id: 'mplist', class: 'mplist', 'data-mp': internet ? 'rooms' : 'found' });
  fillMpList(list);
  const joinBy = internet
    ? h('input', { type: 'text', 'data-mp': 'code', maxlength: '6', value: st.mp.code, placeholder: T('the game\'s code, e.g. 3fa9c1'), 'aria-label': T('Code'),
      oninput: (e) => { st.mp.code = e.target.value.trim(); }, onkeydown: (e) => { if (e.key === 'Enter') mpJoinRoom(st.mp.code); } })
    : h('input', { type: 'text', 'data-mp': 'address', value: st.mp.address, placeholder: T('e.g. 192.168.1.20'), 'aria-label': T('Address'),
      oninput: (e) => { st.mp.address = e.target.value.trim(); }, onkeydown: (e) => { if (e.key === 'Enter') mpJoin(st.mp.address); } });
  const listCard = h('section', { class: 'box mpbox' },
    h('div', { class: 'mpcardh' }, h('h2', {}, T('Open games')),
      h('span', { class: 'mplive' + (st.mp.listErr ? ' err' : ''), 'data-tip': T('The list is asked for again every few seconds while this page is shown.') },
        h('i', {}), internet ? T('live from {server}', { server: mpServerName() }) : T('live from the LAN')),
      h('button', { class: 'plate small sq', 'data-mp': internet ? 'rooms-now' : 'find', 'aria-label': T('Ask again now'), 'data-tip': T('Ask again now'), onclick: () => mpList(true) }, icon('reset', 14))),
    list,
    h('div', { class: 'mpjoin' }, h('span', { class: 'lbl' }, internet ? T('Join with a code') : T('Join by address')), joinBy,
      h('button', { class: 'plate small', 'data-mp': internet ? 'joinroom' : 'join', onclick: () => (internet ? mpJoinRoom(st.mp.code) : mpJoin(st.mp.address)) }, T('Join'))));
  const band = h('section', { class: 'mpband' }, h('h1', { class: 'sr' }, T('Play together')),
    h('div', { class: 'mpbandrow' },
      h('div', { class: 'mpwho' }, h('label', { class: 'mpme' }, h('span', {}, T('You play as')), nameIn), where),
      h('div', { class: 'mpopen' },
        internet ? h('button', { class: 'optsw' + (st.mp.listed ? ' on' : ''), role: 'switch', 'aria-checked': st.mp.listed ? 'true' : 'false', 'data-mp': 'listed',
          onclick: () => { st.mp.listed = !st.mp.listed; renderEntry(); } },
        h('span', { class: 'trk' }, h('span', { class: 'knob' })), h('span', { class: 'mtx' }, h('span', {}, T('In the server\'s list')), h('span', { class: 'hint' }, T('otherwise only with the code'))))
          : h('div', { class: 'optsw static' }, h('span', { class: 'hint' }, T('This machine runs the game; the others find it or type its address.'))),
        h('div', { class: 'gocol' }, goButton({ cls: 'red', icon: 'people', label: T('Open a game'), sub: T('packs and rules in the lobby'), attrs: { 'data-mp': 'host' }, onclick: mpHost })))),
    h('div', { class: 'mpbandfoot' }, internet ? renderMpServer() : h('span', {}, T('If the system asks: allow it for the local network.'))));
  w.append(h('div', { class: 'page mppage' },
    ended ? h('div', { class: 'notice', 'data-mp': 'ended' }, h('span', {}, '●'), h('span', {}, mpWhy(mpState().why))) : null,
    band, mpLogStrip(false), listCard, renderMpSavedGames()));
}
/* in the lobby, the start: a new game, or on from a night a network game saved -- the host
   chooses (the saved games as cards, a night each); the others see what is chosen and whether
   its savegame is here */
function renderMpSave(isHost) {
  const saved = mpState().save, mine = st.mp.snap.save || {}, rows = [];
  const chosenMark = h('span', { class: 'pill green', 'data-mpchosen': '1' }, icon('ok', 12), T('chosen'));
  const newRow = (chosen, sub) => h('div', { class: 'mpgame' + (chosen ? ' on' : ''), 'data-mpsave': 'new' },
    h('span', { class: 'mpavatar new', 'aria-hidden': 'true' }, '+'),
    h('span', { class: 'pt' }, h('b', {}, T('A new game')), h('span', {}, sub)),
    chosen ? chosenMark : isHost ? h('button', { class: 'plate small', 'data-mpchoose': 'new', disabled: st.mp.building, onclick: () => mpChooseSave(null) }, T('Choose')) : null);
  if (isHost) {
    if (!st.mp.saves) mpSaves().then(() => { if (st.section === 'multiplayer') renderEntry(); });
    const m = (saved && saved.meta) || {};
    rows.push(newRow(!saved, T('day 1, as set below')));
    for (const x of st.mp.saves || []) rows.push(saveCard(x, 'lobby'));
    if (saved && !(st.mp.saves || []).some((x) => x.game === m.game)) rows.push(chosenSaveCard(m, null));   /* (chosen, but not in the list read before) */
    if (st.mp.saves && !st.mp.saves.length && !saved)
      rows.push(h('div', { class: 'hint', 'data-mp': 'nosaves' }, T('No saved network game here yet. Every network game saves itself each night, on every player\'s machine.')));
  } else if (saved) rows.push(chosenSaveCard(saved.meta || {}, mine));
  else rows.push(newRow(true, T('day 1')));
  return h('section', { class: 'step' },
    h('div', { class: 'steph' }, h('h2', {}, T('New or saved')),
      h('span', { class: 'hint' }, isHost ? T('every network game saves itself each night: go on from any night') : T('as the host chooses'))),
    h('div', { class: 'mplist mpsaves', 'data-mp': 'saves' }, ...rows));
}
/* the saved game chosen as the others see it (or the host's, when not in the list read before):
   who, which day; mine: whether its savegame is here yet */
function chosenSaveCard(m, mine) {
  return h('div', { class: 'savecard on', 'data-mpsave': m.game || '?' },
    h('div', { class: 'sch' }, mpAvatars(m.humans), h('span', { class: 'pt' }, h('b', {}, mpSaveNames(m.humans)),
      h('span', {}, [T('day {day}', { day: m.day }), Array.isArray(m.packs) && m.packs.length ? m.packs.join(', ') : null].filter(Boolean).join(' · '))),
    mine ? h('span', { class: 'pill ' + ({ ok: 'green', failed: 'orange' }[mine.status] || 'yellow'), 'data-mp': 'savehere' }, mine.status === 'ok' ? icon('ok', 12) : null,
      { ok: T('here'), failed: T('not fetched: {why}', { why: mine.msg || '?' }) }[mine.status] || T('fetching ...'))
      : h('span', { class: 'pill green', 'data-mpchosen': '1' }, icon('ok', 12), T('chosen'))));
}
/* in the lobby: the band -- the invitation, who is ready, start (the host) or ready (the others),
   close or leave --, the messages, the stations in the game's colours, new or saved, then the
   game as the host sets it in three tabs (lobbyTabs) */
function renderLobby(w) {
  const s = st.mp.snap, g = mpState(), me = mpMe(), host = mpHostP(), isHost = s.role === 'host', set = g.settings, saved = g.save;
  const bySt = {};
  for (const p of g.players) bySt[p.station] = p;
  const check = (p) => {
    if (!host || p === host) return null;
    const bad = [p.version !== host.version ? T('another version') : null, p.files !== host.files ? T('other original files') : null,
      set.mods && p.mods !== set.mods ? T('packs not built') : null].filter(Boolean);
    return bad.length ? h('span', { class: 'pill orange', 'data-check': 'bad' }, bad.join(', ')) : h('span', { class: 'pill green', 'data-check': 'ok' }, icon('ok', 12), T('same game'));
  };
  const myName = h('input', { type: 'text', maxlength: '8', 'data-mp': 'myname', value: me ? me.name : '', 'aria-label': T('Your name'),
    onchange: (e) => { const v = cleanMpName(e.target.value); setMpName(v); if (v) mpCall('me', { name: v }); } });
  const stations = [1, 2, 3].map((n) => {
    const p = bySt[n], mine = p && p.id === s.you;
    return h('div', { class: `mpst s${n}` + (mine ? ' me' : '') + (p ? '' : ' free'), 'data-station': String(n) },
      h('div', { class: 'mpsth' }, h('span', { class: 'mark' }, String(n)), h('span', { class: 'sname' }, STATIONS[n]),
        mine ? h('span', { class: 'pill dark' }, T('you')) : p ? null : h('span', { class: 'freeword' }, T('free|station'))),
      h('div', { class: 'mpstb' },
        mine ? myName : h('b', { class: 'nm' }, p ? p.name : T('Computer player')),
        h('span', { class: 'meta' }, p ? [p.host ? T('host') : T('guest'), p.platform].filter(Boolean).join(' · ') : T('the computer plays this station')),
        saved ? (() => {            /* whose station it is in the savegame */
          const x = ((saved.meta || {}).humans || []).find((y) => y.station === n);
          return h('span', { class: 'pill ' + (x ? (x.out ? 'dark' : 'yellow') : 'dark'), 'data-savehuman': String(n) },
            x ? (x.out ? T('saved: {name}, fired (watches)', { name: x.name }) : T('saved: {name}', { name: x.name })) : T('saved: the computer'));
        })() : null,
        p ? h('div', { class: 'tags' }, h('span', { class: 'pill ' + (p.ready ? 'green' : 'yellow'), 'data-ready': p.ready ? 'yes' : 'no' }, p.ready ? icon('ok', 12) : null, p.ready ? T('ready') : T('not ready')), check(p))
          : h('button', { class: 'plate small', 'data-sit': String(n), onclick: () => mpCall('me', { station: n }) }, T('Sit here'))));
  });
  /* Classic: "fixes = none" for everyone; the host's own choice of fixes stays in the file */
  const classicOn = set.fixes === 'none';
  const keptFx = sv('game', 'fixes', true) || 'all';
  /* the band: who is ready, the big button -- the host starts once everyone is, the others say they are */
  const probs = g.phase === 'lobby' ? g.problems || [] : [];
  const statusBox = h('div', { class: 'mpstatus ' + (probs.length || g.phase !== 'lobby' ? 'wait' : 'ok'), 'data-mp': 'problems' },
    ...(g.phase !== 'lobby' ? [h('div', {}, g.phase === 'starting' ? T('Starting ...') : T('Playing'))]
      : probs.length ? probs.slice(0, 3).map((p) => h('div', {}, h('span', { class: 'dot' }), mpProblem(p))) : [h('div', {}, icon('ok', 14), T('Everyone is ready.'))]));
  const waitSub = (p) => (p.code === 'ready' ? T('waiting for {name}', { name: p.name }) : p.code === 'alone' ? T('waiting for a second player') : T('not possible yet'));
  let go = null;
  if (g.phase !== 'lobby') go = goButton({ cls: 'wait', label: g.phase === 'starting' ? T('Starting ...') : T('Playing'), disabled: true, attrs: { 'data-mp': isHost ? 'start' : 'ready' } });
  else if (isHost) go = goButton({ cls: probs.length ? 'wait' : 'ready', label: T('Start the game'), disabled: probs.length > 0, attrs: { 'data-mp': 'start' }, onclick: () => mpCall('start'),
    sub: probs.length ? waitSub(probs.find((p) => p.code === 'ready') || probs.find((p) => p.code === 'alone') || probs[0]) : T('everyone is ready') });
  else if (me) go = goButton({ cls: me.ready ? 'ready' : 'todo', icon: me.ready ? 'ok' : 'play', label: me.ready ? T('Ready') : T('I am ready'), sub: me.ready ? T('waiting for the host') : T('the host waits for you'),
    attrs: { 'data-mp': 'ready', 'aria-pressed': me.ready ? 'true' : 'false' }, onclick: () => mpCall('me', { ready: !me.ready }) });
  /* the invitation: the game's code on the internet, this machine's addresses in the LAN */
  const invite = s.internet
    ? h('div', { class: 'mpinvite', 'data-mp': 'invite' }, h('span', { class: 'ov' }, T('Invite with the code')),
      h('div', { class: 'code' }, h('b', { class: 'mono' }, s.room || '?'),
        h('button', { class: 'plate small sq', 'aria-label': T('Copy'), 'data-tip': T('Copy'), onclick: () => mpCopy(s.room || '') }, icon('copy', 14))),
      h('span', { class: 'hint' }, T('on the internet · {server}', { server: mpServerName(s.internet) })))
    : h('div', { class: 'mpinvite', 'data-mp': 'invite' }, h('span', { class: 'ov' }, T('Invite with the address')),
      ...(isHost ? mpLanAddresses() : [s.address]).map((a, i) => h('div', { class: 'code' + (i ? ' more' : '') }, h('b', { class: 'mono' }, a),
        h('button', { class: 'plate small sq', 'aria-label': T('Copy'), 'data-tip': T('Copy'), onclick: () => mpCopy(a) }, icon('copy', 14)))),
      h('span', { class: 'hint' }, T('in the LAN: the others type it, or find the game by searching')));
  const band = h('section', { class: 'mpband lobby' }, h('h1', { class: 'sr' }, isHost ? T('Your game') : T('The game of {name}', { name: host ? host.name : '?' })),
    invite,
    h('div', { class: 'mpact' },
      h('div', { class: 'seatsum' }, h('b', {}, TP(g.players.length, 'One player', '{n} players')),
        h('span', { class: 'seats3', 'aria-hidden': 'true' }, ...[1, 2, 3].map((n) => h('i', { class: bySt[n] ? 's' + n : '' })))),
      statusBox,
      h('div', { class: 'gocol' }, go,
        h('button', { class: 'leave', 'data-mp': 'leave', onclick: () => mpCall('leave') }, isHost ? T('Close the game') : T('Leave the game')))));
  /* the game: the host changes it, the others see it -- the three tabs as on the page "Play" */
  w.append(h('div', { class: 'page mppage' },
    band,
    mpLogStrip(true),
    h('section', { class: 'step' },
      h('div', { class: 'steph' }, h('h2', {}, T('Stations')), h('span', { class: 'hint' }, saved ? T('everyone on a station of the savegame: its humans\' stations, each taken') : T('take a free one; the computer plays the others'))),
      h('div', { class: 'mpstations', 'data-mp': 'stations' }, ...stations)),
    renderMpSave(isHost),
    lobbyTabs(g, isHost, saved, classicOn, keptFx, check)));
}
/* the lobby's three tabs: the packs (and whether every machine built them), the rules, the fixes
   -- the host changes them, the others see them (a savegame fixes most of them) */
function lobbyTabs(g, isHost, saved, classicOn, keptFx, check) {
  const s = st.mp.snap, set = g.settings, host = mpHostP();
  const cur = (key) => (key === 'castfit' ? !!set.castfit : key === 'sertop' ? set.sertop !== false : set[key]);   /* (a server without the key: castfit unset = No, sertop unset = Yes, as the games start) */
  /* what a savegame holds: its value shown, locked (the difficulty and the start money: "the savegame's") */
  const SAVED = ['difficulty', 'speed4', 'aiimp', 'offers', 'filmoffers', 'money', 'credit', 'terror', 'sammy'];
  const fixed = (k) => !!saved && SAVED.includes(k);
  const seg = (key, opts) => h('div', { class: 'seg', role: 'group', 'data-mpset': key },
    ...opts.map(([v, label]) => h('button', { class: cur(key) === v ? 'on' : '', 'data-value': String(v), disabled: !isHost || fixed(key), 'aria-pressed': cur(key) === v ? 'true' : 'false', onclick: () => mpGameSet({ [key]: v }) }, label)));
  const savePill = (k) => h('span', { class: 'pill dark', 'data-mpset': k }, icon('lock', 12), T('the savegame\'s'));
  const row = (key, label, help, ctl, isDef, def, back, extra = {}) => Object.assign({ key: 'mp.' + key, label, help,
    ctl: fixed(key) && (key === 'difficulty' || key === 'money') ? savePill(key) : ctl,
    changed: !isDef && !(fixed(key) && (key === 'difficulty' || key === 'money')), def, back, reset: isHost && !fixed(key) ? () => mpGameSet(back) : null }, extra);
  const yn = [[false, T('No')], [true, T('Yes')]];
  const may = isHost && !saved, sam = Array.isArray(set.sammy) && set.sammy.length === 4 ? set.sammy : [0, 0, 0, 0];
  const money = typeof set.money === 'number' ? set.money : 200000, terror = typeof set.terror === 'number' ? set.terror : 50000;
  const corig = set.credit === 0, credit = typeof set.credit === 'number' && set.credit ? set.credit : 700000;
  const cp = creditParts({ orig: corig, cur: credit, set: may ? (v) => mpGameSet({ credit: v === CREDIT_ORIGINAL ? 0 : v }) : null, attr: 'data-mpset', key: 'credit',
    note: T('As in 1991, for every player. The bug fix credit-repay is off in this game -- otherwise a network game always has it.'),
    jump: () => showFix('lobby', 'credit-repay', (i) => { st.mp.fixGroup = i; }) });
  const rules = [
    [[T('General'), [
      row('lang', T('Language'), T('The game\'s language: the same for everyone.'), seg('lang', [['e', T('English')], ['d', T('German')]]), set.lang !== 'd', T('English'), { lang: 'e' }),
      row('difficulty', T('Difficulty'), T('As in the game\'s own setup.'), seg('difficulty', [[1, T('Easy')], [2, T('Normal')], [3, T('Difficult')]]), (set.difficulty ?? 2) === 2, T('Normal'), { difficulty: 2 })]],
    [T('Start'), [
      row('money', T('Start money'), T('Your cash when a new game starts (also with Classic); a saved game keeps its own. The credit stays $ 200,000.'),
        startMoney(money, isHost ? (v) => mpGameSet({ money: v }) : null, 'data-mpset', 'money'), money === 200000, numFmt(200000), { money: 200000 }, { tag: T('new game only') }),
      row('credit', T('Credit line'), creditHelp(), cp.ctl, !corig && credit === 700000, T('Staircase') + ' ' + numFmt(700000), { credit: 700000 }, { note: cp.note })]],
    [T('Pace'), [row('speed4', T('Fourth speed'), T('A fourth button in the planner\'s Control-Center, twice as fast as the original\'s fastest. Chosen before the start (also with Classic); No: the original\'s three.'),
      seg('speed4', yn), !set.speed4, T('No'), { speed4: false })]],
    [T('Opponents'), [row('aiimp', T('AI improvement'), T('Computer players build up their transmitter network when you build up yours -- a while later (also with Classic). Off: the original.'),
      seg('aiimp', [['off', T('Off')], ['normal', T('Normal')], ['strong', T('Strong')]]), (set.aiimp || 'off') === 'off', T('Off'), { aiimp: 'off' })]]],
    [[T('Shops'), [
      row('offers', T('Ad offers'), adOffersHelp(), adOffers(typeof set.offers === 'number' ? set.offers : 10, may ? (v) => mpGameSet({ offers: v }) : null, 'data-mpset', 'offers'), (set.offers || 10) === 10, '10', { offers: 10 }),
      row('filmoffers', T('Film offers'), filmOffersHelp(), filmOffers(typeof set.filmoffers === 'number' ? set.filmoffers : 10, may ? (v) => mpGameSet({ filmoffers: v }) : null, 'data-mpset', 'filmoffers'), (set.filmoffers || 10) === 10, '10', { filmoffers: 10 }),
      row('castfit', T('Role fit in the shop'), castFitHelp() + ' ' + T('In a network game the host chooses it, the same for everyone.'), seg('castfit', yn), !cur('castfit'), T('No'), { castfit: false }),
      row('sertop', T('Series topicality'), T('The series card shows how topical the series still is: a bar as on the film card, over all its episodes (also with Classic). No: the original\'s card.'), seg('sertop', yn), cur('sertop'), T('Yes'), { sertop: true })]],
    [T('Network game'), [row('knock', T('Knocking'), T('The film dealer, the ad agency and the supermarket take one at a time. Who finds a player inside knocks: that player has this many game minutes (at normal speed one is two seconds), then the room is the knocker\'s. Off: whoever is inside stays as long as they like.'),
      seg('knock', KNOCK_CHOICES.map((n) => [n, n ? T('{n} min', { n }) : T('Off')])), (set.knock ?? 15) === 15, T('{n} min', { n: 15 }), { knock: 15 })]],
    [T('Events and prizes'), [
      row('terror', T('Terror damage'), terrorHelp(), terrorDamage(terror, may ? (v) => mpGameSet({ terror: v }) : null, 'data-mpset', 'terror'), terror === 50000, numFmt(50000), { terror: 50000 }),
      row('sammy', T('Sammy prizes'), T('Prize money for the winner of the Sammy, by its category, you or a computer player (also with Classic). The original pays none: the Sammy brings image only.'),
        h('div', { class: 'sammyctl' }, ...SAMMY_KEYS.flatMap((k, i) => [h('span', {}, sammyShort(i)),
          sammyPrize(i, sam[i], may ? (v) => mpGameSet({ sammy: sam.map((x, j) => (j === i ? v : x)) }) : null, 'data-mpset', 'sammy' + i)])),
        sam.every((x) => !x), numFmt(0), { sammy: [0, 0, 0, 0] })]]]];
  const changed = rules.flat().flatMap((x) => x[1]).filter((r) => r.changed);
  /* the fixes: all, an own choice (kept here), Classic -- for every player */
  const fx = classicOn && isHost && !saved ? keptFx : set.fixes || 'all';   /* (Classic: the host's own choice kept beside it; the others see the game's) */
  const mode = classicOn ? 'classic' : fx !== 'all' ? 'own' : isHost && fixOwn('mp').mode ? 'own' : 'all';
  const forced = set.credit === 0 ? creditForced(() => showCredit('lobby', 'credit')) : null;
  const fixSum = fixesSummary(mode, fx, forced);
  /* the packs: the host chooses them; every machine builds them from its own original files */
  const mods = s.mods || {};
  const titles = Array.isArray(set.packs) ? set.packs : [];
  const notBuilt = !!set.mods && g.players.some((p) => p !== host && p.mods !== set.mods);
  const packSum = (titles.length ? T('Original game') + ' + ' + titles.join(' + ') : T('Original game'))
    + (st.mp.building ? ' · ' + T('building ...') : set.mods ? ' · ' + (notBuilt ? T('not built everywhere yet') : T('built everywhere')) : '');
  const built = !set.mods ? null : isHost ? h('div', { class: 'hint', 'data-mp': 'mods' }, icon('ok', 12), T('Built: {files}', { files: set.mods }))
    : h('div', { class: 'hint' + (['ok', 'building', 'fetching'].includes(mods.status) ? '' : ' err'), 'data-mp': 'mods' },
      { fetching: T('Fetching the host\'s content packs ...'), building: T('Building the host\'s content packs ...'), ok: T('Content packs built here: the same as the host\'s.'),
        differ: T('Content packs built, but the files differ from the host\'s: other original game files?'), failed: T('The content packs could not be built: {why}', { why: mods.msg }) }[mods.status] || '');
  const packsPanel = () => h('div', { class: 'gpanel packspanel' },
    h('div', { class: 'pcol mppacks' },
      ...(isHost && !saved ? packRows({ packs: mpPacks(), select: setMpPacks, manage: false, attr: 'data-mppack', moveAttr: 'data-mpmove', disabled: st.mp.building })
        : [h('div', { class: 'h3' }, T('Packs of this game'), h('span', { class: 'hint' }, saved ? T('The savegame\'s packs: its game was played with them.') : T('as the host chooses'))),
          h('ol', { class: 'mppacklist', 'data-mpset': 'packs' }, h('li', { class: 'base' }, T('The original game')), ...titles.map((t) => h('li', {}, t)))]),
      st.mp.building ? h('div', { class: 'hint', 'data-mp': 'building' }, T('Building the content packs ...')) : built),
    h('div', { class: 'scol' },
      h('div', { class: 'h3' }, T('On the machines'), h('span', { class: 'hint' }, T('every machine builds them from its own original game files; the files must come out the same'))),
      ...g.players.map((p) => h('div', { class: 'vault', 'data-mpmachine': p.name },
        h('span', { class: 'pt' }, h('b', {}, p.name || '?'), h('span', {}, [p.host ? T('host') : T('guest'), p.platform].filter(Boolean).join(' · '))),
        p === host ? (set.mods ? h('span', { class: 'pill green' }, icon('ok', 12), T('built')) : h('span', { class: 'pill dark' }, T('the original game'))) : check(p) || h('span', {})))));
  return gameTabs({ page: 'lobby', heads: [
    { id: 'packs', label: T('Packs'), sub: (saved ? T('from the savegame') + ': ' : '') + packSum, chg: notBuilt },
    { id: 'rules', label: T('Rules'), sub: changed.length ? T('{n} changed: {list}', { n: changed.length, list: changed.map((r) => r.label).join(', ') }) : T('as by default'), chg: changed.length > 0 },
    { id: 'fixes', label: T('Fixes|tab'), sub: (saved ? T('from the savegame') + ': ' : '') + fixSum[0], chg: fixSum[1] }],
  panel: (id) => (id === 'packs' ? packsPanel()
    : id === 'rules' ? rulesPanel({ cols: rules, only: st.mp.onlyChanged, setOnly: (v) => { st.mp.onlyChanged = v; renderEntry(); },
      resetAll: isHost ? () => mpGameSet(Object.assign({}, ...rules.flat().flatMap((x) => x[1]).filter((r) => r.changed && r.reset).map((r) => r.back))) : null,
      lock: !isHost ? T('as {name} sets them, the host', { name: host ? host.name : '?' }) : saved ? T('grey: from the savegame') : null })
      : fixesPanel({ page: 'lobby', mode, mp: true, pick: isHost && !saved ? pickFixModeLobby : null, fx, forced,
        set: isHost && !saved ? (v) => { keepFixOwn('mp', true, v); mpGameSet({ fixes: v }); } : null,
        lock: saved ? 'save' : isHost ? null : 'host', group: st.mp.fixGroup, setGroup: (i) => { st.mp.fixGroup = i; renderEntry(); }, attr: 'data-mpset', key: 'fixes' })) });
}
/* the host's choice of fixes (fixesPanel): all, own (kept here), Classic -- for every player */
async function pickFixModeLobby(m) {
  const set = mpState().settings, fx = set.fixes === 'none' ? sv('game', 'fixes', true) || 'all' : set.fixes || 'all';
  /* (a list of names; "none" is Classic in a network game -- every fix off -- so not an own choice to go back to) */
  const names = (x) => (x && x !== 'all' && x !== 'none' ? x : '');
  if (m === 'classic') return mpSetClassic(true);
  if (m === 'all') keepFixOwn('mp', false, names(fx) || undefined);
  const want = m === 'all' ? 'all' : names(fx) || names(fixOwn('mp').list) || 'all';
  if (m === 'own') keepFixOwn('mp', true, want);
  try { st.mpSettings = await api('PUT', SETTINGS_URL(true), { values: { game: { classic: 'no', fixes: want } } }); } catch (e) { status(e.message, 'err'); }
  return mpGame({ fixes: want });
}
MODES.multiplayer = { page: true, get label() { return T('Multiplayer'); }, render: renderMultiplayer };

SELFTESTS.push(Object.assign(async function selftestMultiplayer(t) {
  const { ok, wait, go2, typeInto } = t;
  /* ---- the lobby: open a game (dry, this machine only), a second player (the bot), the
   * stations, the host's game and packs (the settings in a test file of their own, apart from
   * the single player's), ready, the start (dry: nothing started), leave */
  const keptName = mpName();
  let keptTabsMp = null, keptOwnMp = null;
  try { keptTabsMp = localStorage.getItem(TABS_KEY); keptOwnMp = localStorage.getItem(FIXOWN_KEY); } catch (e) { /* ignore */ }
  let keptPacks = null, keptDiff = null;
  try { keptPacks = localStorage.getItem(MP_PACKS_KEY); keptDiff = localStorage.getItem(MP_DIFF_KEY); } catch (e) { /* ignore */ }
  const until = async (f, ms = 8000) => { const end = Date.now() + ms; while (Date.now() < end) { await mpRefresh(); renderEntry(); if (f()) return true; await wait(200); } return false; };
  st.mp.where = 'internet';
  st.mp.rooms = [];                          /* (the server's list as if asked: no request goes out) */
  st.mpSettings = null;
  await go2('#multiplayer');
  await wait(600);
  if (mpIn()) { await mpCall('leave'); await wait(300); }
  ok($('#work [data-mp="where"] [data-value="internet"]').classList.contains('on') && $('#work [data-mp="rooms"]') && $('#work [data-mp="code"]') && $('#work [data-mp="listed"]') && !$('#work [data-mp="find"]'),
    'multiplayer: on the internet at first -- the server\'s list, a game\'s code, listed or not');
  ok($('#work [data-mp="rooms"]').textContent.includes(T('No open game yet.')), 'multiplayer: an empty list says new games appear by themselves');
  typeInto($('#work [data-mp="name"]'), 'Tester, the long');
  ok($('#work [data-mp="name"]').value === 'Tester t' && mpName() === 'Tester t', 'multiplayer: the name, 8 letters at most, no comma (' + $('#work [data-mp="name"]').value + ')');
  typeInto($('#work [data-mp="name"]'), 'Tester');
  const keptFound = st.mp.found;
  st.mp.found = [];
  st.mp.listAt = Date.now();                 /* (no search right now) */
  st.mp.where = 'lan'; renderEntry();
  await wait(200);
  ok($('#work [data-mp="find"]') && $('#work [data-mp="address"]') && !$('#work [data-mp="rooms"]') && $('#work [data-mp="where"] [data-value="lan"]').classList.contains('on'), 'multiplayer: in the LAN -- the search, an address');
  st.mp.found = keptFound;
  /* the network games' settings file: their own, the mouse always System; the display and the
     sound are the page "Settings"'s (selftestSettings): none on this page */
  const mp0 = await api('GET', SETTINGS_URL(true));
  ok(/selftest-multiplayer\.ini$/.test(mp0.path) && mp0.values.input.mouse === 'system' && !$('#work [data-mp="mine"]') && !$('#work [data-setting]'),
    'multiplayer: no settings of its own on the page (the page "Settings" has them); the network games\' file (' + mp0.path.split(/[\\/]/).pop() + '), the mouse System');
  let refused = false;
  try { await api('PUT', SETTINGS_URL(true), { values: { input: { mouse: 'free' } } }); } catch (e) { refused = true; }
  ok(refused && (await api('GET', SETTINGS_URL(true))).values.input.mouse === 'system', 'multiplayer: the mouse cannot be set to Free for network games');
  await loadSettings(true, true);
  /* open a game: the host's packs a selection of their own (here the self-test's pack) */
  st.mp.packs = ['selftest'];
  try { localStorage.setItem(MP_DIFF_KEY, '2'); } catch (e) { /* ignore */ }
  renderEntry();
  $('#work [data-mp="host"]').click();
  ok(await until(() => mpIn() && mpMe()), 'multiplayer: a game opened (' + (st.mp.snap.address || '-') + ')');
  ok(!$('#inspector').children.length, 'multiplayer: the lobby, no side column');
  ok(mpMe() && mpMe().name === 'Tester' && mpMe().station === 1 && mpMe().host && $('#work [data-mp="start"]').disabled && $('#work [data-mp="start"]').classList.contains('wait')
    && $('#work [data-mp="problems"]').textContent.includes(T('Waiting for a second player.')), 'multiplayer: the host on station 1, alone: no start (waiting)');
  ok($$('#work [data-station]').length === 3 && $('#work [data-station="2"]').textContent.includes(T('Computer player')) && $('#work [data-station="1"]').textContent.includes('Mad-TV')
    && $('#work [data-station="3"]').classList.contains('s3') && $('#work [data-mp="invite"]').textContent.includes(mpLanAddresses()[0]),
    'multiplayer: three stations in the game\'s colours and names, the free ones the computer\'s; the invitation (' + mpLanAddresses()[0] + ')');
  const mtab = async (id) => { setTab('lobby', id); await wait(150); };
  await mtab('packs');
  ok(same(mpState().settings.packs || [], [packTitle('selftest')]) && $('#work [data-mppack="selftest"]').classList.contains('on') && $('#work [data-tabsum="packs"]').textContent.includes(packTitle('selftest')),
    'multiplayer: the host\'s pack chosen in the tab "Packs", in the game; its head says so');
  const bot = await api('POST', '/api/mp/bot', { action: 'join', address: st.mp.snap.address, name: 'Robo' });
  ok(bot.role === 'guest', 'multiplayer: a second player joins (the bot)');
  ok(await until(() => mpState().players.length === 2 && $('#work [data-station="2"]').textContent.includes('Robo')), 'multiplayer: Robo on station 2, shown');
  ok($('#work [data-station="2"] [data-ready="no"]') && $('#work [data-mp="problems"]').textContent.includes(T('{name} is not ready yet.', { name: 'Robo' })) && $('#work [data-mp="start"]').disabled
    && $('#work [data-mp="start"]').textContent.includes(T('waiting for {name}', { name: 'Robo' })), 'multiplayer: Robo not ready: no start, the button waits for him');
  ok(await until(() => $('#work [data-station="2"] [data-check="ok"]'), 180000),     /* (Robo builds the packs: slow on a busy machine) */
    'multiplayer: Robo\'s version, original files and built packs the same as the host\'s');
  await api('POST', '/api/mp/bot', { action: 'me', ready: true });
  ok(await until(() => !$('#work [data-mp="start"]').disabled && $('#work [data-mp="start"]').classList.contains('ready')), 'multiplayer: Robo ready: the start possible (green)');
  await mtab('rules');
  $('#work [data-mpset="difficulty"] [data-value="3"]').click();
  ok(await until(() => mpState().settings.difficulty === 3 && $('#work [data-station="2"] [data-ready="no"]')) && mpDifficulty() === 3, 'multiplayer: the host makes it difficult: Robo not ready again; kept for the next game');
  await mtab('fixes');
  $('#work [data-mpset="fixes"] [data-fix="random0"]').click();
  ok(await until(() => !(mpState().settings.fixes || '').split(',').includes('random0') && mpState().settings.fixes !== 'all')
    && !(await api('GET', SETTINGS_URL(true))).values.game.fixes.split(',').includes('random0'), 'multiplayer: a fix left out -- in the game and kept in the network games\' file');
  $('#work [data-mpset="fixes"] [data-fixmode] [data-value="classic"]').click();
  ok(await until(() => mpState().settings.fixes === 'none' && $('#work [data-fixes="classicnote"]') && $('#work [data-fixes="classicnote"]').classList.contains('open')) && (await api('GET', SETTINGS_URL(true))).values.game.classic === 'yes'
    && (await api('GET', SETTINGS_URL(true))).values.game.fixes !== 'none' && $('#work [data-mpset="fixes"] [data-fix="intruder"]').disabled,
    'multiplayer: Classic -- no fixes in the game, kept in the file, the choice of fixes kept beside it');
  await mtab('packs');
  $('#work [data-mppack="selftest"]').click();
  ok(await until(() => !(mpState().settings.packs || []).length && !mpState().settings.mods) && same(mpPacks(), []), 'multiplayer: the pack taken out on the page: the original game');
  await api('POST', '/api/mp/bot', { action: 'me', ready: true });
  ok(await until(() => !$('#work [data-mp="start"]').disabled), 'multiplayer: ready again');
  $('#work [data-sit="3"]').click();
  ok(await until(() => mpMe() && mpMe().station === 3), 'multiplayer: the host moves to station 3');
  $('#work [data-mp="start"]').click();
  ok(await until(() => st.mp.snap.go && st.mp.snap.cmd), 'multiplayer: started (dry)');
  const cmd = st.mp.snap.cmd || '';
  ok(/--new-game/.test(cmd) && / --names ,Robo,Tester /.test(cmd) && / --humans 2,3 /.test(cmd) && / --seat 1 /.test(cmd) && / --difficulty 3 /.test(cmd) && / --classic /.test(cmd)
    && / --mouse=system/.test(cmd) && /--settings=\S*selftest-multiplayer\.ini/.test(cmd)
    && $('#work [data-mp="go"]').textContent.includes(T('Your game would start: seat {seat}.', { seat: 2 })), 'multiplayer: the command -- stations 2 and 3, the host seat 2 of 2, the mouse System, the network games\' settings (' + cmd.replace(/^\S+/, '').slice(0, 160) + ' ...)');
  await mpCall('leave');
  await api('POST', '/api/mp/bot', { action: 'leave' });
  ok(!mpIn() && $('#work [data-mp="host"]'), 'multiplayer: left; open or join again');
  /* ---- on from a savegame (src/mpsave.c): the host's saved game (the self-test's: its description
   * as the game writes it), the second player fetches it through the server, the stations must be
   * its humans', the start loads it */
  await api('POST', '/api/mp/testsave', { game: 'selftest-game', day: 4 });
  const saves = await api('POST', '/api/mp/testsave', { game: 'selftest-game', day: 5 });
  ok(saves.some((x) => x.game === 'selftest-game' && x.saves[0].day === 5 && x.saves.length === 2 && x.loadable), 'multiplayer: a saved network game here (days 4 and 5)');
  /* before a game: the saved games, a card each -- renamed, a night deleted (here only) */
  st.mp.saves = null;
  renderEntry();
  ok(await until(() => $('#work [data-mp="saved"] [data-mpsave="selftest-game"]')) && $('#work [data-mpsave="selftest-game"]').textContent.includes('Anna · Ben')
    && $('#work [data-mpgo="selftest-game"]') && !$('#work [data-mpsave="selftest-game"] [data-mpnight]'), 'multiplayer: the saved game as a card before a game (Anna · Ben), go on, the nights folded');
  $('#work [data-mpsavefold="selftest-game"]').click();
  await wait(100);
  ok($$('#work [data-mpsave="selftest-game"] [data-mpnight]').length === 2, 'multiplayer: its two nights unfolded');
  await api('POST', '/api/mp/saves/rename?test=1', { game: 'selftest-game', title: 'Testrunde' });
  st.mp.saves = await api('POST', '/api/mp/saves/delete?test=1', { game: 'selftest-game', file: 'tag-004.mtvmp' });
  renderEntry();
  ok($('#work [data-mpsave="selftest-game"]').textContent.includes('Testrunde') && $$('#work [data-mpsave="selftest-game"] [data-mpnight]').length === 1,
    'multiplayer: renamed, the night of day 4 deleted');
  $('#work [data-mp="host"]').click();
  ok(await until(() => mpIn() && mpMe() && $('#work [data-mpsave="selftest-game"]')), 'multiplayer: open again; the saved game in the list');
  ok($('#work [data-mpsave="new"]').classList.contains('on') && $('#work [data-mpsave="selftest-game"]').textContent.includes('Anna · Ben') && $('#work [data-mpsave="selftest-game"]').textContent.includes(T('day {day}', { day: 5 })),
    'multiplayer: a new game chosen; the saved one shows its managers and day');
  await api('POST', '/api/mp/bot', { action: 'join', address: st.mp.snap.address, name: 'Robo' });
  ok(await until(() => mpState().players.length === 2), 'multiplayer: Robo joins again');
  await mtab('rules');
  $('#work [data-mpset="aiimp"] [data-value="strong"]').click();
  ok(await until(() => mpState().settings.aiimp === 'strong' && $('#work [data-mpset="aiimp"] [data-value="strong"]').classList.contains('on')), 'multiplayer: the AI improvement set to strong for a new game');
  ok(mpState().settings.credit === 700000 && $('#work [data-mpset="credit"]').value === numFmt(700000) && !$('#work [data-mpset="credit"]').disabled, 'multiplayer: the credit line ' + numFmt(700000) + ' for a new game, the host may change it');
  /* the host's amounts reach the server (and are kept for the next game) */
  const mpMoney = (key, v) => { const i = $(`#work [data-mpset="${key}"]`); i.value = numFmt(v); i.dispatchEvent(new Event('change')); };
  mpMoney('credit', 1000000);
  ok(await until(() => mpState().settings.credit === 1000000 && $('#work [data-mpset="credit"]').value === numFmt(1000000)), 'multiplayer: the host\'s credit line ' + numFmt(1000000) + ' reaches the server');
  /* the original's credit system: credit 0 in the lobby, kept as credit_line = original; credit-repay held off; back: the line as before */
  const mcs = (v) => $(`#work [data-credit="credit"] [data-value="${v}"]`);
  mcs('original').click();
  ok(await until(() => mpState().settings.credit === 0 && $('#work [data-mpset="credit"]').disabled && $('#work [data-mpset="credit"]').value === numFmt(300000)
    && $('#work [data-creditnote="credit"]').classList.contains('open')), 'multiplayer: the host\'s "Original 1991" reaches the server (credit 0), the note open');
  ok($('#work [data-tabsum="fixes"]').classList.contains('chg') && (mpState().settings.fixes === 'none' ? $('#work [data-tabsum="fixes"]').textContent === T('Classic 1991: none on')
    : !$('#work [data-tabsum="fixes"]').textContent.startsWith(T('all {n} on', { n: (st.mpSettings.fixes || []).length }))), 'multiplayer: the head of the fixes counts credit-repay off (with Classic: none at all)');
  await wait(400);
  ok((await api('GET', SETTINGS_URL(true))).values.game.credit_line === 'original', 'multiplayer: "Original 1991" kept for the next game');
  mcs('line').click();
  ok(await until(() => mpState().settings.credit === 1000000 && $('#work [data-mpset="credit"]').value === numFmt(1000000) && !$('#work [data-creditnote="credit"]').classList.contains('open')),
    'multiplayer: back to the staircase: the line as before');
  ok(mpState().settings.terror === 50000 && JSON.stringify(mpState().settings.sammy) === '[0,0,0,0]' && $('#work [data-mpset="terror"]').value === numFmt(50000)
    && !$('#work [data-mpset="sammy3"]').disabled, 'multiplayer: the terror damage ' + numFmt(50000) + ' and no Sammy prize for a new game, the host may change them');
  mpMoney('terror', 0);
  ok(await until(() => mpState().settings.terror === 0 && $('#work [data-mpset="terror"]').value === numFmt(0)), 'multiplayer: the terror damage 0 reaches the server');
  mpMoney('sammy3', 250000);
  ok(await until(() => JSON.stringify(mpState().settings.sammy) === '[0,0,0,250000]' && $('#work [data-mpset="sammy3"]').value === numFmt(250000)),
    'multiplayer: the Sammy prize for series ' + numFmt(250000) + ' reaches the server');
  const mo = () => $('#work [data-mpset="offers"]');
  ok(mpState().settings.offers === 10 && mo().value === '10' && !mo().disabled, 'multiplayer: the original\'s 10 ad offers for a new game, the host may change them');
  mo().value = '15';
  mo().dispatchEvent(new Event('change'));
  ok(await until(() => mpState().settings.offers === 15 && mo().value === '15'), 'multiplayer: the host\'s 15 ad offers reach the server');
  const mf = () => $('#work [data-mpset="filmoffers"]');
  ok(mpState().settings.filmoffers === 10 && mf().value === '10' && !mf().disabled, 'multiplayer: the original\'s 10 film offers for a new game, the host may change them');
  mf().value = '12';
  mf().dispatchEvent(new Event('change'));
  ok(await until(() => mpState().settings.filmoffers === 12 && mf().value === '12'), 'multiplayer: the host\'s 12 film offers reach the server');
  await wait(400);
  const mk = (await api('GET', SETTINGS_URL(true))).values.game;
  ok(mk.credit_line === '1000000' && mk.terror_damage === '0' && mk.sammy_series === '250000' && mk.sammy_news === '0', 'multiplayer: the host\'s amounts kept for the next game');
  ok(mk.ad_offers === '15' && mk.film_offers === '12', 'multiplayer: the host\'s ad and film offers kept for the next game');
  $('#work [data-mpchoose="selftest-game"]').click();
  ok(await until(() => mpState().save && $('#work [data-mpsave="selftest-game"] [data-mpchosen]')), 'multiplayer: the saved game chosen');
  ok(await until(() => { const b = (mpState().players || []).find((p) => p.name === 'Robo'); return b && b.save === mpState().save.hash; }), 'multiplayer: Robo fetched the savegame through the server');
  ok($('#work [data-savehuman="1"]').textContent.includes('Anna') && $('#work [data-savehuman="3"]').textContent.includes(T('saved: the computer'))
    && $('#work [data-mpset="difficulty"]').textContent.includes(T('the savegame\'s')), 'multiplayer: the stations show whose they are in the savegame; the difficulty is the savegame\'s');
  ok(await until(() => mpState().settings.aiimp === 'off' && $('#work [data-mpset="aiimp"] [data-value="off"]').classList.contains('on') && $('#work [data-mpset="aiimp"] [data-value="strong"]').disabled),
    'multiplayer: the AI improvement is the savegame\'s (off: saved before the option), locked');
  ok(await until(() => mpState().settings.credit === 300000 && $('#work [data-mpset="credit"]').value === numFmt(300000) && $('#work [data-mpset="credit"]').disabled),
    'multiplayer: the credit line is the savegame\'s (the original ' + numFmt(300000) + ': saved before the option), locked');
  ok(await until(() => mpState().settings.terror === 50000 && JSON.stringify(mpState().settings.sammy) === '[0,0,0,0]' && $('#work [data-mpset="terror"]').value === numFmt(50000)
    && $('#work [data-mpset="terror"]').disabled && $('#work [data-mpset="sammy3"]').value === numFmt(0) && $('#work [data-mpset="sammy3"]').disabled),
    'multiplayer: the terror damage and the Sammy prize are the savegame\'s (the original\'s: saved before the options), locked');
  ok(await until(() => mpState().settings.offers === 10 && mo().value === '10' && mo().disabled), 'multiplayer: the ad offers are the savegame\'s (the original\'s 10: saved before the option), locked');
  ok(await until(() => mpState().settings.filmoffers === 10 && mf().value === '10' && mf().disabled), 'multiplayer: the film offers are the savegame\'s (10), locked');
  $('#work [data-sit="3"]').click();
  ok(await until(() => mpMe().station === 3 && $('#work [data-mp="problems"]').textContent.includes(T('{name} sits on a station that is the computer\'s in the savegame.', { name: 'Tester' }))
    && $('#work [data-mp="problems"]').textContent.includes(T('Nobody sits on {name}\'s station of the savegame.', { name: 'Anna' }))), 'multiplayer: station 3 is the computer\'s in the savegame, Anna\'s is free: no start');
  $('#work [data-sit="1"]').click();
  await api('POST', '/api/mp/bot', { action: 'me', ready: true });
  ok(await until(() => mpMe().station === 1 && !$('#work [data-mp="start"]').disabled), 'multiplayer: on the savegame\'s stations, Robo ready: the start possible');
  await mtab('fixes');
  ok($('#work [data-tabsum="fixes"]').textContent === T('from the savegame') + ': ' + T('all {n} on', { n: (st.mpSettings.fixes || []).length })
    && $$('#work [data-mpset="fixes"] [data-fixmode] button').every((b) => b.disabled) && $('#work .fixlockline'), 'multiplayer: the fixes and Classic the savegame\'s');
  await mtab('rules');
  $('#work [data-mp="start"]').click();
  ok(await until(() => st.mp.snap.go && st.mp.snap.cmd), 'multiplayer: started from the savegame (dry)');
  const lcmd = st.mp.snap.cmd || '';
  ok(/ --mp-load \S*selftest-game\Stag-005\.mtvmp/.test(lcmd) && / --mp-save \S*selftest-game/.test(lcmd) && !/--new-game/.test(lcmd) && / --humans 1,2 /.test(lcmd) && / --credit-line=300000 /.test(lcmd)
    && / --terror-damage=50000 /.test(lcmd) && / --sammy-prize=0,0,0,0 /.test(lcmd) && / --ad-offers=10 /.test(lcmd) && / --film-offers=10 /.test(lcmd),
    'multiplayer: the command loads the savegame and goes on saving in its game\'s directory (' + lcmd.replace(/^\S+/, '').slice(0, 200) + ' ...)');
  await mpCall('leave');
  await api('POST', '/api/mp/bot', { action: 'leave' });
  await api('PUT', SETTINGS_URL(true), { values: { game: { fixes: 'all', classic: 'no', credit_line: '700000', terror_damage: '50000', sammy_series: '0', ad_offers: '10', film_offers: '10' } } });
  await api('POST', '/api/mp/saves/delete?test=1', { game: 'selftest-game' });
  st.mp.saves = null;
  await loadSettings(true, true);
  try {
    if (keptTabsMp === null) localStorage.removeItem(TABS_KEY); else localStorage.setItem(TABS_KEY, keptTabsMp);
    if (keptOwnMp === null) localStorage.removeItem(FIXOWN_KEY); else localStorage.setItem(FIXOWN_KEY, keptOwnMp);
  } catch (e) { /* ignore */ }
  st.tabs = {};
  st.fixOwn = {};
  setMpName(keptName);
  st.mp.packs = null;
  try {
    if (keptPacks === null) localStorage.removeItem(MP_PACKS_KEY); else localStorage.setItem(MP_PACKS_KEY, keptPacks);
    if (keptDiff === null) localStorage.removeItem(MP_DIFF_KEY); else localStorage.setItem(MP_DIFF_KEY, keptDiff);
  } catch (e) { /* ignore */ }
  st.mp.where = 'internet';
}, { player: true }));      /* (the player's launcher runs it too: selftest.js playerSelftest) */
