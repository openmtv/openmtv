/* OpenMTV editor -- the launcher ("Play"): the selection -- the original game and the packs laid
 * over it in their order (tools/modpacks.py: each pack's new entries appended after the ones
 * before), the same that the editor shows -- the game's settings (the player's settings.ini,
 * which ./play.sh and the game read; the former Python settings module (gamesettings.py)) and the start (the former Python launcher (play.py); the
 * command is shown as the player types it: ./play.sh, on Windows play.cmd).  The original is a
 * pack too: the base of the others, left out only when a complete pack is chosen (one the
 * editor made from a selection: it holds the original already).  A setting is written at
 * once, in place (the file's comments and order stay) -- "Classic" too ([game] classic: no
 * fixes, the fixes' choice kept for when it is off again).  The page has no side column: a
 * band on top holds Classic and the start, the messages show under it once there are any,
 * the fixes are a block of their own (folded).  The self-test works on a settings file of
 * its own (?test=1) and never starts the game (dry).  Classic, fixes, start money, the big
 * button and pack controls serve the page "Multiplayer" too (multiplayer.js: the network
 * games' own settings file, mp).  What is the player's and not the game's -- the display, the
 * AI upscaling, the sound, the mouse -- is the page "Settings" (renderSettings), for both. */
'use strict';
/* the settings file: the single player's, or (mp) the network games' own (multiplayer.ini) */
const SETTINGS_URL = (mp) => '/api/settings' + (new URLSearchParams(location.search).has('selftest') ? (mp ? '?test=1&mp=1' : '?test=1') : (mp ? '?mp=1' : ''));
const settingsKey = (mp) => (mp ? 'mpSettings' : 'settings');
st.launch = st.launch || { fixOpen: false, fixGroup: 0 };
/* the fixes' descriptions come from the game (build/mtv-dec --fixes=list), in English */
const FIX_DE = {
  'intruder': 'Gerichtsvollzieher/Terrorist bleibt hängen, wenn das Archiv nur Serien hat; Liebesfilme werden nie gepfändet',
  'random0': 'random_n(0) teilt durch null -> "Divide error"',
  'ai-studio': 'ein Computerspieler mietet Studio 13 statt des gewählten',
  'ai-fill-hour': 'ein Computerspieler schätzt das Publikum einer Sendung für die Stunde = ihre Kategorie',
  'ai-series-rule': 'ein Computerspieler prüft die Genre-Regel einer Werbung am Film Nr.-24 statt an der Serie',
  'series-avg': 'die mittlere Aktualität einer Serie zählt die Folgen 2..n+1',
  'estate-text': 'der Makler zeigt Raumnamen ("Warnung: Psychiater") als seine Meldungen',
  'tx-price': 'ein Sender wird gegen neue Zuschauer x 61,6 geprüft statt gegen seinen Preis',
  'library-owner': 'ein Film, der aus dem Archivregal genommen und zurückgelegt wird, bleibt auf dem Markt; der Händler verkauft ihn',
  'news-cache': 'volles Nachrichtenfach: Sendeplätze 2 und 3 zeigen die Schlagzeile von Platz 1',
  'ai-odd-range': 'die Prüfung ungerader Längen eines Computerspielers liest einen Bereich aus Stack-Resten, also nichts',
  'boss-music': 'das Chefbüro hält die Musik vor dem Ende ohne Argumente an: sie spielt weiter',
  'film-200': 'Daten: Film 200 "Prince of Darkness" heißt "xxx", sein deutscher Text ist abgeschnitten, er wird nie angeboten',
  'live-aid': 'Daten: Film 164 "Live Aid" (Live-Sendung in der Auktion) ist 0 Stunden lang: kein Computerspieler bietet darauf, eingeplant verdrängt er nichts (die Sendung darunter läuft als Live Aid weiter), gesendet bleibt er im Archiv und wird nachts als "verpasst" entfernt; sein Termin kann 1 Uhr nachts sein, wo er sich nicht einplanen lässt',
  'video-connection': 'Daten: Die Show "Video-Connection" (Drehbuch 20) stellt an Moderator und Stargäste keine Anforderungen: jede Besetzung passt zu 0 %, nur die Kulisse zählt -- produziert hat die Show höchstens Qualität 3 und praktisch keine Zuschauer',
  'catch-bounty': 'wer einen Rivalen im eigenen Raum erwischt, bekommt die Prämie nur auf Station 1',
  'ai-visit': 'Computerspieler schleichen sich in die Räume von Station 1 statt in die des Menschen',
  'ai-trendy': 'ein Computerspieler, der "Trendy" sendet, schreibt die Trends über andere Werte (ein Betty-Geschenk aus dem Nichts), statt sie zu lernen',
  'ai-rival': 'die Tagesplanung eines Computerspielers nimmt ihn selbst als Rivalen',
  'ai-arrived': 'nur der erste Computerspieler darf an der Tür, an der er schon steht, sofort handeln; der zweite wartet erneut 300 Ticks',
  'title-cache': 'ab 47 Filmen in Archiv und Koffer läuft der Titelspeicher des Archivs über: die letzten Filme haben keinen Titel, eine Serie zeigt einen Filmtitel und Datenmüll als Folgentitel',
  'case-owner': 'ein Band, das vom Archivregal in den Koffer kommt, hat keinen Besitzer: der Filmhändler kann es anbieten, solange man es trägt',
  'credit-repay': 'wer im Minus beim Chef „Kredit zurückzahlen“ wählt, bekommt Geld: der negative Kontostand wird „zurückgezahlt“ und der Chef leiht ihn, über jeden Kreditrahmen hinaus; wer mehr zurückzahlt, als er schuldet, hat einen negativen Kredit und bekommt 5 % Zinsen am Tag. Im Netzspiel immer an',
  'case-corner': 'im Koffer des Filmhändlers liegt in der unteren rechten Rahmenecke ein Film ohne Titel (die eigene Programm-Attraktivität der Stunde als Filmnummer), den man immer wieder an den Händler verkaufen kann; jedes Mal geht ein später gekauftes Band verloren. Im Netzspiel immer an',
  'ai-fill-genre': 'ein Computerspieler prüft beim Füllen einer Stunde das Genre des bisher besten Programms statt das des Kandidaten (Liebesfilme, wo seine Werbeverträge sie ausschließen, und dort keine Werbung) und bewertet Serien nach dem Publikum von Familienserien',
  'ai-lib-quality': 'die Durchschnittsqualität im Archiv eines Computerspielers zählt 12 Phantom-Einträge mit: sein Filmbudget folgt der Archivgröße statt der Qualität',
  'ai-rating': 'die Quote eines Computerspielers vom Vortag läuft über 21,5 Millionen Zuschauern pro Stunde über (Reichweite eines Satelliten): er hält sich für ungesehen und nimmt kaum noch Werbeverträge an',
  'ai-bids': 'die Auktionsgebote eines Computerspielers füllen eine Liste mit 11 Plätzen, die fast nie geleert wird: ein eifriger Bieter schreibt darüber hinaus (Vertragszähler, dann Absturz)',
  'ai-warning': 'will ein Computerspieler einen Live-Film einplanen, dessen Sendezeit vorbei ist, erscheint bei jedem Versuch ein Warnfenster auf dem Bildschirm des Menschen; die Fenster stapeln sich bis zum Absturz',
  'studio-takeover': 'ein vorgemerkter Studiomieter bekommt Studio 1, 2 oder 3 (hinter der Tür eines anderen Raums) statt seines Studios',
  'studio-reserved': 'die Vormerkung eines Studios (wartender Mieter oder spätere Rückgabe) wird nie gelöscht: es wird nach jeder Produktion wieder abgegeben oder erneut übernommen (und bezahlt)',
};
const fixDesc = (f) => (UI.lang === 'de' && FIX_DE[f.name]) || f.desc;
async function loadSettings(force, mp) {
  const k = settingsKey(mp);
  if (!st[k] || force) st[k] = await api('GET', SETTINGS_URL(mp));
  return st[k];
}
async function setSetting(sec, key, val, mp) {
  const k = settingsKey(mp);
  try {
    st[k] = await api('PUT', SETTINGS_URL(mp), { values: { [sec]: { [key]: val } } });
    status(T('Saved in {path}.', { path: st[k].path }), 'ok');
  } catch (e) { status(e.message, 'err'); }
  renderEntry();
}
const sv = (sec, key, mp) => ((st[settingsKey(mp)].values || {})[sec] || {})[key];
/* a settings file's controls: a choice of values, yes/no, a row with its help */
function settingControls(mp) {
  const seg = (sec, key, opts) => h('div', { class: 'seg', role: 'group', 'data-setting': `${sec}.${key}` },
    ...opts.map(([v, label]) => h('button', { class: sv(sec, key, mp) === v ? 'on' : '', 'data-value': v, 'aria-pressed': sv(sec, key, mp) === v ? 'true' : 'false', onclick: () => setSetting(sec, key, v, mp) }, label)));
  const yesno = (sec, key) => seg(sec, key, [['no', T('No')], ['yes', T('Yes')]]);
  return { seg, yesno };
}
const settingRow = (label, tip, ctl) => [h('label', {}, label, help(tip, label)), h('div', { class: 'ctl' }, ctl)];
const setCard = (title, ...rows) => h('section', { class: 'box setcard' }, h('div', { class: 'boxtitle' }, title), h('div', { class: 'fields' }, ...rows.flat()));
const castFitHelp = () => T('In the production shop, how well each actor fits the role picked in the shopping list, in per cent -- the game\'s own rating when shooting starts -- instead of the bars humor, appearance and craft. Green: the best of the five shown; red: below 60 %. Only what you see changes (also with Classic); No: the original\'s panel.');
/* the display: the player's, for a single player and network games alike (the page "Settings":
   launcher/server.go writes it to both files) -- but the role fit, a rule of the game's start
   (the card "Game"; in a network game the host's, in the lobby) */
function displayCard() {
  const { seg, yesno } = settingControls(false), row = settingRow;
  if (sv('display', 'filter') === 'hr' && !displayCard.hr) {   /* (hr went 2026-10-18; the game reads it as hr-pixel -- the file says so too, once) */
    displayCard.hr = true;
    setSetting('display', 'filter', 'hr-pixel');
  }
  const scale = h('select', { 'data-setting': 'display.scale', onchange: (e) => setSetting('display', 'scale', e.target.value) },
    ...[1, 2, 3, 4, 5, 6, 7, 8].map((n) => h('option', { value: String(n), selected: sv('display', 'scale') === String(n) }, T('{n} x 320 x 240 ({w} x {h} pixels)', { n, w: n * 320, h: n * 240 }))));
  return setCard(T('Display'),
    row(T('Fullscreen'), T('Start in fullscreen. F11, Alt+Enter or Cmd+F switch while playing; the game remembers it here.'), yesno('display', 'fullscreen')),
    row(T('Window size'), T('The window: the game\'s 320 x 240 picture times N.'), scale),
    row(T('Aspect'), T('4:3 like a VGA monitor, or square pixels (16:10).'), seg('display', 'aspect', [['4:3', T('4:3 like a VGA monitor')], ['square', T('Square pixels')]])),
    row(T('Filter'), T('Sharp: even pixels at any size; nearest: hard edges; linear: soft; MMPX: smoothed edges and diagonals, made only of the picture\'s own colours; HR pixel: drawn at four times the resolution -- a finer font, frames and bars sharp, the pictures\' pixels as they are.'),
      seg('display', 'filter', [['sharp', T('Sharp')], ['nearest', T('Nearest')], ['linear', T('Linear')], ['mmpx', T('MMPX')], ['hr-pixel', T('HR pixel')]])),
    row(T('Whole-number scaling'), T('Only whole-number scale factors: black borders, every pixel the same size.'), yesno('display', 'integer')));
}
/* the AI pictures (docs/ki-bilder.md, launcher/ki.go): a picture set made here from the game
   files by a model from openmtv.net -- never shipped --, shown with the HR display (Ctrl+K
   switches them while playing).  The card draws itself again while something is made. */
let kiTimer = null, kiFilterWas = null, kiDrawn = null, kiPhaseWas = null;
const kiRowWas = {};
/* what the card shows apart from the progress: while it stays the same, a poll changes only the
   bar and its text -- the card is not drawn again, so a click on it or the keyboard's focus is
   not lost */
const kiShape = (k) => JSON.stringify([k.models, k.supported, k.runtime_here, k.error, (k.job || {}).model, (k.job || {}).phase,
  (k.job || {}).error, ((st[settingsKey(false)] || {}).values || {}).display]);
function kiLabel(job) {
  if (job.phase === 'download') return T('loading: {done} of {total} MB', { done: Math.round(job.done / 1e6), total: Math.round(job.total / 1e6) });
  if (job.phase === 'make') {
    if (!(job.total > 1)) return T('making: starting');
    return T('making: {done} of {total} pictures', { done: job.done, total: job.total })
      + (job.left == null ? '' : ' · ' + (job.left < 60 ? T('under a minute left') : TP(Math.round(job.left / 60), 'about a minute left', 'about {n} minutes left')));
  }
  return job.phase === 'cancelled' ? T('cancelled') : job.phase === 'done' ? T('done') : job.error || '';
}
async function loadKi() {
  /* (a poll that fails keeps what the card showed, with the error under it, and polls on) */
  try { st.ki = await api('GET', '/api/ki'); } catch (e) { st.ki = { ...(st.ki || { models: [] }), error: e.message }; }
  const el = document.getElementById('aicard'), job = st.ki.job || {};
  if (el && kiDrawn === kiShape(st.ki)) {
    const bar = el.querySelector('[data-ki="bar"]'), label = el.querySelector('[data-ki="label"]');
    if (bar) { bar.max = job.total || 1; bar.value = job.done || 0; }
    if (label) label.textContent = kiLabel(job);
  } else if (el) el.replaceWith(aiCard());
  const busy = ['download', 'make'].includes(job.phase), ended = ['download', 'make'].includes(kiPhaseWas) && !busy;
  kiPhaseWas = job.phase;
  clearTimeout(kiTimer);
  if (busy) kiTimer = setTimeout(loadKi, st.ki.error ? 3000 : 700);
  else if (ended && job.phase === 'done') {
    /* (the files it was made with are gone again: only the set stays) */
    const m = (st.ki.models || []).find((x) => x.id === job.model);
    status(T('The picture set is made ({mb} MB); the files for making it are removed again.', { mb: Math.round(((m && m.size) || 0) / 1e6) }), 'ok');
    await loadSettings(true, false);
  }
}
async function kiDo(what, model) {
  try {
    st.ki = await api('POST', `/api/ki/${what}`, { model });
    if (what === 'use' || what === 'delete') {     /* (both settings files changed: the network games' too) */
      await loadSettings(true, false);
      if (st[settingsKey(true)]) await loadSettings(true, true);
      renderEntry();
    }
    if (what === 'use') status(model ? T('AI upscaling is on; it shows with the filter HR pixel.') : T('AI upscaling is off.'), 'ok');
  } catch (e) { status(e.message, 'err'); }
  loadKi();
}
/* "Delete": asked first -- making the set again loads the model again and takes minutes */
function kiAskDelete(m) {
  dialog(T('Delete "{title}"?', { title: m.name }),
    h('p', {}, T('The picture set of {name} is deleted from this computer. Making it again loads about {mb} MB and takes a few minutes.',
      { name: m.name, mb: Math.round((m.download + ((st.ki && st.ki.runtime) || 0)) / 1e6) })),
    [[T('Cancel'), '', null], [T('Delete'), 'red', () => kiDo('delete', m.id)]]);
}
/* a slot that slides open and shut (.slide, as the credit line's note): WAS where it stood
   when last drawn */
function kiSlide(el, open, was) {
  el.inert = !open;
  if (was !== open) setTimeout(() => { void el.offsetHeight; el.classList.toggle('open', open); });   /* (in the page by then) */
  return el;
}
function aiCard() {
  const k = st.ki;
  if (!k) { loadKi(); return h('section', { class: 'box setcard', id: 'aicard' }, h('div', { class: 'boxtitle' }, T('AI upscaling'))); }
  const job = k.job || {}, busy = ['download', 'make'].includes(job.phase);
  const rows = [];
  const intro = h('div', { class: 'hint' }, T('The game\'s pictures redrawn at a finer resolution by an AI model -- made on this computer from your game files, with a model loaded from openmtv.net for the making; afterwards only the pictures stay. They show with the filter HR pixel; Ctrl+K (on the Mac also Cmd+K) switches them off and on while playing.'));
  if (k.supported === false) rows.push(h('div', { class: 'hint', style: 'grid-column:1/-1' }, T('Not on this system: the models run on Windows (x64), macOS (Apple silicon) and Linux (x64).')));
  /* a set in use, the filter not HR: a note slides open */
  const sp = st[settingsKey(false)], filter = sp && ((sp.values || {}).display || {}).filter;
  const noHr = (k.models || []).some((m) => m.used) && !!filter && filter !== 'hr-pixel';
  const was = kiFilterWas === null ? noHr : kiFilterWas;
  kiFilterWas = noHr;
  const note = kiSlide(h('div', { class: 'slide' + (was ? ' open' : ''), 'data-ki': 'filternote' },
    h('div', {}, h('div', { class: 'inlinenote' }, icon('warn', 14),
      h('span', {}, T('AI upscaling shows only with the filter HR pixel.'), ' ',
        h('button', { class: 'link', 'data-ki': 'hr-pixel', onclick: () => setSetting('display', 'filter', 'hr-pixel', false) }, T('Switch to HR pixel')))))), noHr, was);
  rows.unshift(h('div', { style: 'grid-column:1/-1' }, intro, note));
  for (const m of k.models || []) {
    const mine = busy && job.model === m.id;
    const pill = m.used ? h('span', { class: 'pill green' }, T('in use'))
      : m.state === 'ready' ? h('span', { class: 'pill blue' }, T('made'))
      : m.state === 'stale' ? h('span', { class: 'pill dark' }, T('made for other game files'))
      : m.state === 'old' ? h('span', { class: 'pill dark' }, T('made by an older version'))
      : h('span', { class: 'pill grey' }, T('not made'));
    /* a row of its own: the name and its state on the left, the buttons on the right -- the
       same buttons for a state, held while something is made, "Use" and "Do not use" in one
       width (twoLabels): nothing moves --, the credit under it, then the progress sliding open */
    const btns = [];
    if (k.supported !== false) btns.push(h('button', { class: 'plate small', 'data-ki': `make-${m.id}`, disabled: busy, onclick: () => kiDo('make', m.id) },
      m.state === 'none' ? T('Make ({mb} MB to load)', { mb: Math.round((m.download + (k.runtime_here ? 0 : k.runtime || 0)) / 1e6) }) : T('Make again')));
    if (m.state !== 'none') btns.push(h('button', { class: 'plate small', 'data-ki': m.used ? 'use-none' : `use-${m.id}`, onclick: () => kiDo('use', m.used ? '' : m.id) },
      twoLabels(m.used ? T('Do not use') : T('Use'), m.used ? T('Use') : T('Do not use'))));
    if (m.state !== 'none') btns.push(h('button', { class: 'plate small', 'data-ki': `delete-${m.id}`, disabled: mine, onclick: () => kiAskDelete(m) }, T('Delete')));
    const show = job.model === m.id && ['download', 'make', 'failed', 'cancelled', 'done'].includes(job.phase) && !(job.phase === 'done' && !(m.id in kiRowWas));
    const open = show && job.phase !== 'done', rowWas = m.id in kiRowWas ? kiRowWas[m.id] : open;
    kiRowWas[m.id] = open;
    const progress = kiSlide(h('div', { class: 'slide' + (rowWas ? ' open' : ''), 'data-ki': `progress-${m.id}` },
      h('div', {}, show ? h('div', { class: 'kiprog' },
        h('progress', { 'data-ki': mine ? 'bar' : null, max: String(job.total || 1), value: String(job.phase === 'done' ? job.total || 1 : job.done || 0),
          style: job.phase === 'failed' || job.phase === 'cancelled' ? 'display:none' : null }),
        h('span', { class: job.phase === 'failed' ? 'hint err' : 'hint', 'data-ki': mine ? 'label' : null }, kiLabel(job)),
        h('button', { class: 'plate small', 'data-ki': 'cancel', disabled: !mine, style: mine ? null : 'visibility:hidden', onclick: () => kiDo('cancel', m.id) }, T('Cancel'))) : null)), open, rowWas);
    rows.push(h('div', { class: 'kimodel', 'data-kimodel': m.id },
      h('div', { class: 'kihead' }, h('span', { class: 'kiname' }, m.name), pill, h('span', { class: 'kibtns' }, ...btns)),
      h('div', { class: 'hint' }, T('by {author}, {licence}', { author: m.author, licence: m.licence }), ' · ',
        /* (the launcher opens it: the window's web view opens no new windows) */
        h('a', { href: m.link, onclick: (e) => { e.preventDefault(); api('POST', '/api/ki/link', { model: m.id }).catch(() => {}); } }, 'OpenModelDB'),
        m.made ? ' · ' + T('made {when}', { when: m.made }) : '', m.size ? ' · ' + T('{mb} MB', { mb: Math.round(m.size / 1e6) }) : ''),
      progress));
  }
  if (k.error) rows.push(h('div', { class: 'hint err', style: 'grid-column:1/-1' }, k.error));
  const card = setCard(T('AI upscaling'), ...rows);
  card.id = 'aicard';
  kiDrawn = kiShape(k);
  return card;
}
/* a button's text in the width of the wider of two (the other one there, unseen) */
const twoLabels = (shown, other) => h('span', { class: 'twolabels' }, h('span', {}, shown), h('span', { class: 'other', 'aria-hidden': 'true' }, other));
/* the fixes in groups (the game's list, build/mtv-dec --fixes=list; one it adds later: "More") */
const fixGroups = () => [
  [T('Crashes and hangs'), ['intruder', 'random0']],
  [T('Computer players'), ['ai-studio', 'ai-fill-hour', 'ai-series-rule', 'ai-odd-range', 'ai-visit', 'ai-trendy', 'ai-rival', 'ai-arrived', 'ai-fill-genre', 'ai-lib-quality', 'ai-rating', 'ai-bids', 'ai-warning']],
  [T('Rules'), ['tx-price', 'catch-bounty', 'library-owner', 'case-owner', 'case-corner', 'credit-repay', 'studio-takeover', 'studio-reserved', 'series-avg']],
  [T('Texts, data and display'), ['estate-text', 'news-cache', 'boss-music', 'film-200', 'live-aid', 'video-connection', 'title-cache']],
];
/* the bug fixes, a block of their own: folded one line -- how many are on, which are off --,
   unfolded the groups beside the fixes of the one picked, each with what the original does.
   o: fx the choice (all, none or names), set(v) to change it (none: shown only), lock why it
   cannot change -- 'classic' (none; the choice kept), 'save' (the savegame's), 'host' (as the
   host sets it) --, open/setOpen, group/setGroup; attr, key: the block's data attribute;
   forced: a fix another setting holds off -- { name, why, back() } --: off and locked, the
   choice for it kept (all on, all off and a group's all/none leave it as it was) */
function fixesBlock(o) {
  const all = (st.settings || st.mpSettings || {}).fixes || [];
  const fx = o.fx || 'all', names = all.map((f) => f.name);
  const chosen = new Set(fx === 'all' ? names : fx === 'none' ? [] : fx.split(',').map((x) => x.trim()));
  const classic = o.lock === 'classic', edit = !!o.set && !o.lock;
  const forced = !classic && o.forced && names.includes(o.forced.name) ? o.forced : null;
  const held = (n) => !!forced && forced.name === n;
  const isOn = (f) => !classic && chosen.has(f.name) && !held(f.name);
  const keep = (s) => { if (forced) { if (chosen.has(forced.name)) s.add(forced.name); else s.delete(forced.name); } return s; };
  const put = (s) => o.set((s = keep(s)).size === names.length ? 'all' : s.size ? names.filter((n) => s.has(n)).join(',') : 'none');
  const known = new Set(fixGroups().flatMap((g) => g[1]));
  const groups = fixGroups().map(([t, ns]) => [t, all.filter((f) => ns.includes(f.name))])
    .concat([[T('More'), all.filter((f) => !known.has(f.name))]]).filter((g) => g[1].length);
  const on = all.filter(isOn).length, off = names.filter((n) => !chosen.has(n) || held(n)), offFree = off.filter((n) => !held(n));
  const count = { n: on, total: all.length };
  const head = classic ? T('none · set by Classic') : o.lock === 'save' ? T('{n} of {total} on · from the savegame', count)
    : o.lock === 'host' ? T('{n} of {total} on · as the host sets it', count) : T('{n} of {total} on', count);
  const listOf = (ns) => ns.slice(0, 3).join(', ') + (ns.length > 3 ? ' ' + TP(ns.length - 3, 'and one more', 'and {n} more') : '');
  const offList = listOf(off), offFreeList = listOf(offFree);
  const summary = classic ? (o.set ? T('Your choice ({n} of {total}) is kept and counts again without Classic.', { n: chosen.size, total: all.length }) : T('Classic: exactly the original game, for everyone.'))
    : o.lock === 'save' ? T('The saved evening sets them; a new game takes your choice again.')
      : forced ? (offFree.length ? T('off: {list}', { list: offFreeList }) + ' · ' : '') + forced.short
        : off.length ? T('off: {list}', { list: offList }) : T('all on, as by default');
  const g = Math.min(o.group || 0, Math.max(groups.length - 1, 0));
  const box = h('div', { class: 'fixbox', [o.attr || 'data-setting']: o.key || 'game.fixes' },
    h('div', { class: 'fixbar' },
      h('button', { class: 'fold', 'data-fixes': 'fold', 'aria-expanded': o.open ? 'true' : 'false', onclick: () => o.setOpen(!o.open) },
        h('span', { class: 'chev' + (o.open ? ' open' : '') }, icon('unfold', 16)),
        o.lock ? h('span', { class: 'fixlock ' + (classic ? 'k' : 'g') }, icon('lock', 14)) : null,
        h('span', { class: 'fbt' }, h('span', { class: 'cnt' + (classic ? ' k' : o.lock ? ' g' : '') }, head), h('span', { class: 'hint' }, summary)),
        h('span', { class: 'lnk' }, o.open ? T('hide the list') : T('show the list'))),
      edit && offFree.length ? h('button', { class: 'plate small', 'data-fixes': 'all', onclick: () => put(new Set(names)) }, T('All on again')) : null,
      edit && o.open && names.some((n) => chosen.has(n) && !held(n)) ? h('button', { class: 'plate small', 'data-fixes': 'none', onclick: () => put(new Set()) }, T('All off')) : null));
  if (!o.open || !groups.length) return box;
  const [gt, gf] = groups[g];
  box.append(h('div', { class: 'fixbody' },
    h('div', { class: 'fixgroups', role: 'tablist', 'aria-label': T('Groups') }, ...groups.map(([t, fs], i) => h('button', {
      class: 'fg' + (i === g ? ' on' : ''), role: 'tab', 'aria-selected': i === g ? 'true' : 'false', 'data-fixgroup': String(i), onclick: () => o.setGroup(i) },
    h('span', {}, t), h('span', { class: 'gc' + (classic ? ' k' : '') }, `${fs.filter(isOn).length} / ${fs.length}`)))),
    h('div', { class: 'fixpanel', role: 'tabpanel' },
      h('div', { class: 'fph' }, h('b', {}, gt), h('span', { class: 'hint' }, classic ? T('none · set by Classic') : T('{n} of {total} on', { n: gf.filter(isOn).length, total: gf.length })),
        h('span', { style: 'flex:1' }),
        edit ? h('div', { class: 'seg small', role: 'group', 'aria-label': gt },
          h('button', { 'data-fixgroupset': 'all', onclick: () => { const s = new Set(chosen); gf.forEach((f) => s.add(f.name)); put(s); } }, T('all|fixes')),
          h('button', { 'data-fixgroupset': 'none', onclick: () => { const s = new Set(chosen); gf.forEach((f) => s.delete(f.name)); put(s); } }, T('none|fixes'))) : null),
      ...gf.map((f) => held(f.name) ? h('div', { class: 'fx lk', role: 'switch', 'aria-checked': 'false', 'aria-disabled': 'true', 'data-fix': f.name, 'data-held': 'credit' },
        h('span', { class: 'trk' }, h('span', { class: 'knob' })), h('span', { class: 'fb' }, h('span', { class: 'fn' }, f.name), h('span', { class: 'fd' }, fixDesc(f)),
          h('span', { class: 'why' }, icon('lock', 12), forced.why, forced.back ? h('button', { class: 'link', 'data-fixes': 'back', onclick: forced.back }, T('change')) : null)))
        : h('button', { class: 'fx' + (isOn(f) ? ' on' : '') + (classic ? ' lk' : o.lock ? ' ro' : ''), role: 'switch', 'aria-checked': isOn(f) ? 'true' : 'false',
        'data-fix': f.name, disabled: !edit, onclick: () => { const s = new Set(chosen); if (s.has(f.name)) s.delete(f.name); else s.add(f.name); put(s); } },
      h('span', { class: 'trk' }, h('span', { class: 'knob' })), h('span', { class: 'fb' }, h('span', { class: 'fn' }, f.name), h('span', { class: 'fd' }, fixDesc(f))))))));
  return box;
}
/* "Classic": exactly the original game, none of its bugs fixed (the fixes' choice kept for later).
   o: on, set(v) (none: shown only, with a lock), sub (the line under it), attrs */
function classicSwitch(o) {
  return h('button', Object.assign({ class: 'modesw' + (o.on ? ' on' : ''), role: 'switch', 'aria-checked': o.on ? 'true' : 'false', 'data-classic': 'switch', disabled: !o.set,
    onclick: () => o.set(!o.on) }, o.attrs || {}),
  h('span', { class: 'mtrk' }, h('span', { class: 'mknob' })),
  h('span', { class: 'mtx' }, h('b', {}, T('Classic mode'), o.set ? null : icon('lock', 12)), h('span', { class: 'hint' }, o.sub)));
}
/* Classic is on: what it sets and what it leaves */
const classicNote = (text) => h('div', { class: 'classicnote', 'data-classic': 'note' }, icon('lock', 16),
  h('div', {}, h('b', {}, T('Classic: the rules, bugs and timing of the original.')), h('span', {}, text)));
/* the big button: its symbol over two lines -- what it does, how it stands.  cls: red (play),
   ready (green: go), todo (orange: your turn), wait (orange, muted: nothing to do yet) */
const goButton = (o) => h('button', Object.assign({ class: 'go ' + o.cls, disabled: !!o.disabled, onclick: o.onclick }, o.attrs || {}),
  h('span', { class: 'goi' }, icon(o.icon || 'play', 28)), h('span', { class: 'got' }, h('span', { class: 'gol' }, o.label), o.sub ? h('span', { class: 'gos' }, o.sub) : null));
const playCmd = () => (st.settings && st.settings.play) || './play.sh';
/* a pack's changes in a few words: "125 films, 25 ads" */
function packSummary(m) {
  const parts = Object.entries(m.changed || {}).map(([t, n]) => `${n} ${tableName(t)}`);
  return parts.length ? T('changes {list}', { list: parts.join(', ') }) : T('as the original');
}
/* the packs a chosen complete pack holds already: -> { name: holder } */
function heldBy(packs = st.packs) {
  const out = {};
  for (const p of packs) for (const x of packHolds(p)) if (x !== p) out[x] = p;
  return out;
}
/* the list: the original, then every pack; a click takes one in or out, the arrows order them.
   o: packs and select(packs) (the editor's selection by default); manage: delete buttons and
   the unsaved mark (the launcher); attr, moveAttr: the multiplayer page's own; disabled */
function renderPackList(o = {}) {
  const packs = o.packs || st.packs, select = o.select || selectPacks, manage = o.manage !== false && !player();
  const attr = o.attr || 'data-play', moveAttr = o.moveAttr || 'data-move';
  const held = heldBy(packs), complete = holdsOriginal(packs);
  const list = h('div', { class: 'packlist', role: 'group', 'aria-label': T('Contents') });
  const holder = packs.find((n) => modInfo(n)?.complete);
  list.append(h('div', { class: 'packrow' },
    h('button', { class: 'pick' + (complete ? '' : ' on'), [attr]: 'original', disabled: true, 'aria-pressed': complete ? 'false' : 'true' },
      h('span', { class: 'mark' }, complete ? null : icon('lock', 12)),
      h('span', { class: 'pt' }, h('b', {}, T('The original game')), h('span', {}, complete ? T('held in "{pack}" already', { pack: packTitle(holder) }) : T('the base of every pack; left out only for a complete pack')))),
    manage ? h('span', { class: 'order' }) : null, h('span', { class: 'order' })));
  const move = (i, d) => { const p = packs.slice(); [p[i], p[i + d]] = [p[i + d], p[i]]; select(p); };
  for (const m of st.mods) {
    const k = packs.indexOf(m.name), on = k >= 0, inside = !on && held[m.name];
    const tags = h('span', { class: 'tags' },
      m.protected && manage ? h('span', { class: 'pill dark', 'data-tip': T('Never changed by the editor: a change makes a new pack (unless unlocked in the overview).') }, icon('lock', 12), T('protected')) : null,
      m.complete ? h('span', { class: 'pill blue', 'data-tip': T('Holds the original and the packs it was made from: {list}', { list: [T('Original')].concat((m.from || []).map(packTitle)).join(' + ') }) }, T('complete')) : null,
      manage && st.mod === m.name && st.dirty.size ? h('span', { class: 'pill yellow' }, T('not saved')) : null);
    list.append(h('div', { class: 'packrow' },
      h('button', { class: 'pick' + (on ? ' on' : ''), [attr]: m.name, 'aria-pressed': on ? 'true' : 'false', disabled: !!inside || !!o.disabled || (!!m.error && !on),
        onclick: () => select(on ? packs.filter((n) => n !== m.name) : packs.concat([m.name])) },
      h('span', { class: 'mark' }, on ? icon('ok', 14) : null),
      on && packs.length > 1 ? h('span', { class: 'no' }, k + 1) : null,
      h('span', { class: 'pt' }, h('b', {}, m.title || m.name), m.error ? h('span', { class: 'err', 'data-packerror': m.name }, m.error)
        : h('span', {}, inside ? T('held in "{pack}" already', { pack: packTitle(inside) }) : `mods/${m.name} · ${packSummary(m)}`)),
      tags),
      !manage ? null : m.protected ? h('span', { class: 'order' }) : h('span', { class: 'order' }, h('button', { 'data-delete': m.name, 'aria-label': T('Delete "{title}" ...', { title: m.title || m.name }), 'data-tip': T('Delete "{title}" ...', { title: m.title || m.name }), onclick: () => deleteDialog(m.name) }, icon('remove', 14))),
      h('span', { class: 'order' }, on && packs.length > 1 ? [
        h('button', { [moveAttr]: `${m.name}-up`, disabled: k === 0 || !!o.disabled, 'aria-label': T('Earlier'), 'data-tip': T('Earlier'), onclick: () => move(k, -1) }, icon('up', 14)),
        h('button', { [moveAttr]: `${m.name}-down`, disabled: k === packs.length - 1 || !!o.disabled, 'aria-label': T('Later'), 'data-tip': T('Later'), onclick: () => move(k, 1) }, icon('down', 14))] : null)));
  }
  return list;
}
/* what the packs add and where they collide (tools/modpacks.py), from /api/packs/A+B */
function fillPackInfo(box, spec) {
  const label = (t) => (SECTIONS.find((x) => x.table === t) || { label: t }).label;
  const show = (r) => {
    box.innerHTML = '';
    if (r.error) { box.append(h('div', { class: 'hint err' }, r.error)); return; }
    for (const p of r.packs) {
      const added = Object.entries(p.added).map(([t, [a, b, n]]) => T('{what} #{first}-#{last} ({n} new)', { what: label(t), first: a, last: b, n }));
      box.append(h('div', { class: 'packline', 'data-pack': p.name }, h('b', {}, packTitle(p.name)), ': ',
        added.length ? added.join(', ') : T('changes only'), Object.keys(p.moved).length ? h('span', { class: 'hint' }, ' ' + T('(appended after the packs before)')) : null));
    }
    for (const c of r.conflicts.slice(0, 5))
      box.append(h('div', { class: 'hint warn', 'data-conflict': `${c.table}-${c.id}` }, T('{what} #{id}: changed in {packs} -- "{wins}" counts.', { what: label(c.table), id: c.id, packs: c.packs.join(', '), wins: c.wins })));
    if (r.conflicts.length > 5) box.append(h('div', { class: 'hint warn' }, TP(r.conflicts.length - 5, 'One more conflict.', '{n} more conflicts.')));
    if ((r.incomplete || []).length) box.append(h('div', { class: 'hint err' }, T('Not playable yet: {why}', { why: r.incomplete.slice(0, 3).join('; ') })));
  };
  if (st.packInfo[spec]) { show(st.packInfo[spec]); return; }
  box.append(h('div', { class: 'hint' }, T('Reading the packs ...')));
  api('GET', `/api/packs/${spec}`).then((r) => { st.packInfo[spec] = r; if (box.isConnected) show(r); }).catch((e) => { box.innerHTML = ''; box.append(h('div', { class: 'hint err' }, e.message)); });
}
/* an amount of dollars (the start money, the credit line): as the interface language writes them,
   the default always beside them and a way back to it.  o: cur the value, set(dollars) to change
   it (none: shown only), skey the settings key (its schema: default, lowest, highest), label,
   bad() the message for a refused value, step (the value a multiple of it); attr, key: the
   control's data attribute and its value; narrow: the default only in the reset's tip */
function moneyInput(o) {
  const sch = ((st.settings || st.mpSettings).schema || []).find((x) => x.section === 'game' && x.key === o.skey);
  const [def, lo, hi] = sch ? sch.opts : o.fallback;
  const { cur, set } = o, step = o.step || 1, changed = cur !== def;
  const commit = (e) => {
    const v = e.target.value.replace(/[\s.,'’$]/g, '');
    if (/^\d+$/.test(v) && +v >= lo && +v <= hi && +v % step === 0) { if (+v !== cur) set(+v); else e.target.value = numFmt(cur); return; }
    status(o.bad(lo, hi), 'err');
    e.target.value = numFmt(cur);
  };
  return h('div', { class: 'moneyctl' + (changed ? ' changed' : '') },
    h('span', { class: 'unit' }, '$'),
    h('input', { type: 'text', inputmode: 'numeric', [o.attr]: o.key, 'aria-label': o.label, value: numFmt(cur), disabled: !set,
      onchange: commit, onkeydown: (e) => { if (e.key === 'Enter') e.target.blur(); } }),
    o.narrow ? null : h('span', { class: 'was', 'data-default': String(def) }, T('Default {value}', { value: money(def) })),
    set ? h('button', { class: 'reset', 'data-reset': o.key, disabled: !changed, 'aria-label': T('Reset {label}', { label: o.label }),
      'data-tip': changed ? T('Back to the default: {value}', { value: money(def) }) : T('Is the default'), onclick: () => set(def) }, icon('reset', 14)) : null);
}
/* the start money: the human's cash in a new game */
const startMoney = (cur, set, attr = 'data-setting', key = 'game.start_money', narrow = false) => moneyInput({ cur, set, attr, key, narrow,
  skey: 'start_money', fallback: [200000, 0, 100000000], label: T('Start money'),
  bad: (lo, hi) => T('Start money: whole dollars from {lo} to {hi}.', { lo: money(lo), hi: money(hi) }) });
/* the credit line: the most the boss lends while the image is below 50 (+$ 100,000 a day from day 2) */
const creditLine = (cur, set, attr = 'data-setting', key = 'game.credit_line', narrow = false) => moneyInput({ cur, set, attr, key, narrow,
  skey: 'credit_line', fallback: [700000, 300000, 100000000], label: T('Credit line'), step: 100000,
  bad: (lo, hi) => T('Credit line: dollars from {lo} to {hi} in steps of {step}.', { lo: money(lo), hi: money(hi), step: money(100000) }) });
const creditHelp = () => T('The most the boss lends you while your image is below 50: the $ 200,000 of the start, then $ 100,000 more a day from day 2, up to this line (also with Classic). The original: $ 300,000.')
  + ' ' + T('Original 1991: the original\'s credit, without the staircase -- and the bug fix credit-repay off, in network games too.');
/* the credit system: the staircase up to the credit line, or the original's of 1991 (settings
   credit_line = original, a lobby's credit 0): the original's line, and the bug fix credit-repay
   off and locked.  The row keeps its place when it switches (the amount stays where it was, the
   original's in its place); the note under it slides open and shut.  o: orig, cur (the line),
   set(dollars or CREDIT_ORIGINAL; none: shown only), attr, key (the amount's data attribute),
   narrow, note (its text), jump() to the fix.  Returns the row: label, control, note */
const CREDIT_ORIGINAL = 'original';
const creditWas = {};    /* by row: the system it showed last -- its note slides from there */
const creditKept = {};   /* by row: the line before "Original 1991", back to it */
function creditRow(o) {
  const sch = ((st.settings || st.mpSettings || {}).schema || []).find((x) => x.section === 'game' && x.key === 'credit_line');
  const def = sch ? sch.opts[0] : 700000;
  const pick = (orig) => () => {
    if (orig === o.orig) return;
    if (orig) creditKept[o.key] = o.cur;
    o.set(orig ? CREDIT_ORIGINAL : creditKept[o.key] || def);
  };
  const seg = h('div', { class: 'seg', role: 'group', 'aria-label': T('Credit system'), 'data-credit': o.key },
    h('button', { class: o.orig ? '' : 'on', 'data-value': 'line', 'aria-pressed': o.orig ? 'false' : 'true', disabled: !o.set, onclick: pick(false) }, T('Staircase')),
    h('button', { class: o.orig ? 'on' : '', 'data-value': CREDIT_ORIGINAL, 'aria-pressed': o.orig ? 'true' : 'false', disabled: !o.set, onclick: pick(true) }, T('Original 1991')));
  const amount = !o.orig ? creditLine(o.cur, o.set, o.attr, o.key, o.narrow)
    : h('div', { class: 'moneyctl orig' }, h('span', { class: 'unit' }, '$'),
      h('input', { type: 'text', [o.attr]: o.key, 'aria-label': T('Credit line'), value: numFmt(300000), disabled: true }),
      o.narrow ? null : h('span', { class: 'was' }, icon('lock', 12), T('as in 1991')),
      o.set ? h('button', { class: 'reset ghost', tabindex: '-1', 'aria-hidden': 'true' }, icon('reset', 14)) : null);
  const was = o.key in creditWas ? creditWas[o.key] : o.orig;
  creditWas[o.key] = o.orig;
  const note = h('div', { class: 'slide' + (was ? ' open' : ''), 'data-creditnote': o.key },
    h('div', {}, h('div', { class: 'inlinenote' }, icon('lock', 14),
      h('span', {}, o.note, o.jump ? ' ' : '', o.jump ? h('button', { class: 'link', 'data-credit': 'tofix', onclick: o.jump }, T('to the fix')) : null))));
  note.inert = !o.orig;
  if (was !== o.orig) setTimeout(() => { void note.offsetHeight; note.classList.toggle('open', o.orig); });   /* (in the page by then; a timer, as a hidden page gets no frames) */
  /* the choice beside the label, the amount under it where the other amounts stand: room for
     both in a narrow card, the same lines in both states */
  return [h('label', {}, T('Credit line'), help(creditHelp(), T('Credit line'))), h('div', { class: 'ctl' }, seg),
    h('div', { class: 'wide creditbelow' }, h('div', { class: 'ctl' }, amount), note)];
}
/* the fix held off by the original's credit (fixesBlock's forced): its row, why, and back to the credit */
const creditForced = (back) => ({ name: 'credit-repay', why: T('off: the credit line is set to "Original 1991"'), short: T('credit-repay off: the credit line is "Original 1991"'), back });
/* to a fix in the fixes block: unfolded at its group, the row shown */
function showFix(name, setOpen, setGroup) {
  const all = (st.settings || st.mpSettings || {}).fixes || [];
  const groups = fixGroups().map(([, ns]) => all.filter((f) => ns.includes(f.name))).filter((g) => g.length);
  const g = groups.findIndex((fs) => fs.some((f) => f.name === name));
  setGroup(Math.max(g, 0));
  setOpen(true);
  setTimeout(() => {
    const r = $(`#work [data-fix="${name}"]`);
    if (!r) return;
    r.scrollIntoView({ behavior: 'smooth', block: 'center' });
    r.classList.add('flash');
  });
}
/* back to a credit row: shown, its choice focused */
function showCredit(key) {
  const s = $(`#work [data-credit="${key}"]`);
  if (!s) return;
  s.scrollIntoView({ behavior: 'smooth', block: 'center' });
  (s.querySelector('button.on') || s.querySelector('button')).focus({ preventScroll: true });
}
/* the terror damage: what a terrorist's bomb costs the owner of the room it hits */
const terrorDamage = (cur, set, attr = 'data-setting', key = 'game.terror_damage', narrow = false) => moneyInput({ cur, set, attr, key, narrow,
  skey: 'terror_damage', fallback: [50000, 0, 100000000], label: T('Terror damage'),
  bad: (lo, hi) => T('Terror damage: whole dollars from {lo} to {hi}.', { lo: money(lo), hi: money(hi) }) });
const terrorHelp = () => T('The repair costs a terrorist\'s bomb leaves the owner of the room it hits -- a room whose sign someone swapped at the information board, you or a computer player (also with Classic). 0: no costs and no message; the room\'s other losses stay. The original: $ 50,000.');
/* the Sammy's prize money, by its categories (the game's 1..4): the settings' keys, labels, help */
const SAMMY_KEYS = ['sammy_news', 'sammy_culture', 'sammy_shows', 'sammy_series'];
const sammyLabel = (i) => [T('Sammy: news'), T('Sammy: culture'), T('Sammy: shows'), T('Sammy: series')][i];
const sammyHelp = (i) => T('Prize money for the winner of the Sammy for {what}, you or a computer player (also with Classic). The original pays none: the Sammy brings image only.',
  { what: [T('the best news'), T('cultural engagement'), T('self-produced shows'), T('self-produced series')][i] });
const sammyPrize = (i, cur, set, attr = 'data-setting', key = 'game.' + SAMMY_KEYS[i], narrow = false) => moneyInput({ cur, set, attr, key, narrow,
  skey: SAMMY_KEYS[i], fallback: [0, 0, 100000000], label: sammyLabel(i),
  bad: (lo, hi) => T('Sammy prize money: whole dollars from {lo} to {hi}.', { lo: money(lo), hi: money(hi) }) });
/* the ad agency's offers at a time: 10 (the original's desk, three stacks) to 17 (a front row
   of 13, a back row of 4) -- the range from the settings' schema.  cur the number, set(n) to
   change it (none: shown only); attr, key: the control's data attribute and its value */
const adOffersLabel = (n) => n === 10 ? T('{n}: the original\'s desk', { n }) : n <= 13 ? T('{n}: in one row', { n }) : T('{n}: in two rows', { n });
/* a number of offers, 10 to 17 (the range from the settings' schema, skey): a select */
function offersSelect(o) {
  const sch = ((st.settings || st.mpSettings || {}).schema || []).find((x) => x.section === 'game' && x.key === o.skey);
  const [, lo, hi] = sch ? sch.opts : [10, 10, 17];
  return h('select', { [o.attr]: o.key, 'aria-label': o.label, disabled: !o.set, onchange: (e) => o.set(Number(e.target.value)) },
    ...Array.from({ length: hi - lo + 1 }, (_, i) => lo + i).map((n) => h('option', { value: String(n), selected: n === o.cur }, o.text(n))));
}
const adOffers = (cur, set, attr = 'data-setting', key = 'game.ad_offers') => offersSelect({ cur, set, attr, key, skey: 'ad_offers', label: T('Ad offers'), text: adOffersLabel });
/* the film dealer's offers at a time: 10 (the original's counter) to 17 (7 more tapes to the left) */
const filmOffersLabel = (n) => n === 10 ? T('{n}: the original\'s counter', { n }) : TP(n - 10, '{n}: one more tape', '{n}: {k} more tapes', { n, k: n - 10 });
const filmOffers = (cur, set, attr = 'data-setting', key = 'game.film_offers') => offersSelect({ cur, set, attr, key, skey: 'film_offers', label: T('Film offers'), text: filmOffersLabel });
const filmOffersHelp = () => T('How many films and series the film dealer offers at a time (also with Classic). From 11 the row of tapes on his counter grows to the left. The original: 10.');
const adOffersHelp = () => T('How many contracts the ad agency offers at a time (also with Classic). From 11 the folders stand in a front row of up to 13 and a row of up to 4 behind it. The original: 10, in three stacks. More offers need more ads: with the original\'s 50, some places stay empty early in the game.');
/* the page: the band on top (Classic beside the start, the command), the messages, what is
   played, the settings, the fixes */
function renderLauncher(w) {
  if (!st.settings) {
    w.append(h('div', { class: 'hint' }, T('Reading the settings ...')));
    loadSettings().then(() => { if (st.section === 'launcher') renderEntry(); }).catch((e) => status(e.message, 'err'));
    return;
  }
  const { seg, yesno } = settingControls(false), row = settingRow, saves = !player();
  const L = st.launch, spec = st.packs.join('+'), n = st.packs.length;
  const classic = sv('game', 'classic') === 'yes';
  const cash = Number(sv('game', 'start_money') ?? 200000), terror = Number(sv('game', 'terror_damage') ?? 50000);
  const corig = sv('game', 'credit_line') === CREDIT_ORIGINAL, credit = corig ? 700000 : Number(sv('game', 'credit_line') ?? 700000);
  const toFixes = { setOpen: (v) => { L.fixOpen = v; renderEntry(); }, setGroup: (i) => { L.fixGroup = i; } };
  const moneyCard = setCard(T('Money'),
    row(T('Start money'), T('Your cash when a new game starts (also with Classic); a saved game keeps its own. The credit stays $ 200,000.'), startMoney(cash, (v) => setSetting('game', 'start_money', String(v)))),
    creditRow({ orig: corig, cur: credit, set: (v) => setSetting('game', 'credit_line', String(v)), attr: 'data-setting', key: 'game.credit_line',
      note: T('As in 1991: the boss lends $ 200,000, haggled up to $ 300,000, from image 50 up to $ 600,000 -- without the staircase. The bug fix credit-repay is off with it, and locked.'),
      jump: () => showFix('credit-repay', toFixes.setOpen, toFixes.setGroup) }),
    row(T('Terror damage'), terrorHelp(), terrorDamage(terror, (v) => setSetting('game', 'terror_damage', String(v)))),
    ...SAMMY_KEYS.map((k, i) => row(sammyLabel(i), sammyHelp(i), sammyPrize(i, Number(sv('game', k) ?? 0), (v) => setSetting('game', k, String(v))))));
  if (classic) moneyCard.append(h('div', { class: 'cardnote' }, T('These amounts count in Classic too.')));
  const gameCard = setCard(T('Game'),
    row(T('Language'), T('The game\'s language; "ask" shows the game\'s own prompt.'), seg('game', 'language', [['english', T('English')], ['german', T('German')], ['ask', T('Ask')]])),
    row(T('Fourth speed'), T('A fourth button in the planner\'s Control-Center, twice as fast as the original\'s fastest. Chosen before the start (also with Classic); No: the original\'s three.'), yesno('game', 'fourth_speed')),
    row(T('Fast room changes'), T('Into a room and out again at once: no wipe, no wait while the room loads -- as in a network game (also with Classic). The game\'s clock stands still in a room change either way; No: the original\'s pace.'), yesno('game', 'fast_rooms')),
    row(T('Series topicality'), T('The series card shows how topical the series still is: a bar as on the film card, over all its episodes (also with Classic). No: the original\'s card.'), yesno('game', 'series_topicality')),
    row(T('AI improvement'), T('Computer players build up their transmitter network when you build up yours -- a while later (also with Classic). Off: the original.'), seg('game', 'ai_improve', [['off', T('Off')], ['normal', T('Normal')], ['strong', T('Strong')]])),
    row(T('Ad offers'), adOffersHelp(), adOffers(Number(sv('game', 'ad_offers') ?? 10), (v) => setSetting('game', 'ad_offers', String(v)))),
    row(T('Film offers'), filmOffersHelp(), filmOffers(Number(sv('game', 'film_offers') ?? 10), (v) => setSetting('game', 'film_offers', String(v)))),
    row(T('Role fit in the shop'), castFitHelp(), yesno('display', 'cast_fit')));
  if (classic) gameCard.append(h('div', { class: 'cardnote' }, T('Speed, room changes, the series topicality, the AI improvement, the offers and the role fit count in Classic too.')));
  const info = st.packs.length ? h('div', { class: 'packinfo', 'data-launch': 'info' }) : null;
  if (info) fillPackInfo(info, packSpec());
  const band = h('section', { class: 'playband', 'aria-label': T('Start') },
    h('div', { class: 'pbart' }, h('img', { src: '/api/room/104.png', alt: '' }), h('div', { class: 'scan' }), h('span', { class: 'onairsign' }, 'ON AIR'),
      h('div', { class: 'pbtitle' }, h('div', { class: 'overline' }, T('Play')), h('h1', {}, T("What's on today?")))),
    h('div', { class: 'pbctl' },
      h('div', { class: 'pbrow' },
        classicSwitch({ on: classic, set: (v) => setSetting('game', 'classic', v ? 'yes' : 'no'), sub: T('exactly the original of 1991, without fixes'), attrs: { 'data-setting': 'game.classic' } }),
        h('div', { class: 'gocol' },
          goButton({ cls: 'red', label: classic ? T('Play classic') : T('Play'), sub: n > 1 ? (saves ? T('saves and builds the packs first') : T('builds the packs first'))
            : n ? (saves ? T('saves and builds the pack first') : T('builds the pack first')) : T('the original game files in run/'),
            attrs: { id: 'launch', 'data-tip': n ? null : T('The original game files in run/ (your saves stay there).') }, onclick: () => launchGame() }))),
      h('code', { class: 'cmd', 'data-launch': 'cmd' }, `${playCmd()} ${spec ? '--mod ' + spec : ''}`.trim())));
  const log = h('div', { id: 'playlog', class: 'log', role: 'log' });
  const strip = h('div', { class: 'logstrip' }, h('span', { class: 'overline' }, T('Messages')), h('div', { class: 'logcol' }, log),
    h('button', { class: 'link', 'data-launch': 'hidelog', onclick: () => { st.log = []; renderEntry(); } }, T('hide')));
  fillLog(log);
  w.append(h('div', { class: 'page launchpage' },
    band,
    classic ? classicNote(T('Only the bug fixes are set (below); everything else stays as chosen.') + (n ? ' ' + T('With packs: the rules of 1991, the contents from the packs.') : '')) : null,
    st.dirty.size && st.mod && st.packs.includes(st.mod) ? h('div', { class: 'notice' }, h('span', {}, '●'), h('span', {}, T('"{title}" has unsaved changes: they are saved before the start.', { title: packTitle(st.mod) }))) : null,
    strip,
    h('section', { class: 'step', 'aria-labelledby': 'step1' },
      h('div', { class: 'steph' }, h('h2', { id: 'step1' }, T('1 · Contents')), h('span', { class: 'hint' }, saves ? T('The original game, or packs on it: several play together in their order. The editor shows the same.')
        : T('The original game, or packs on it: several play together in their order.')),
        h('span', { style: 'flex:1' }), st.packs.length ? h('button', { class: 'plate small', 'data-play': 'only-original', onclick: () => selectPacks([]) }, T('Only the original')) : null,
        saves ? h('button', { class: 'plate small', onclick: newModDialog }, icon('plus', 14), T('New pack')) : null),
      renderPackList(), info,
      n ? h('div', { class: 'hint' }, n > 1 ? T('Played in this order: where two packs change the same entry, the later one counts; each pack\'s new films, series and ads follow the ones before.')
        : T('The pack is saved and built first; games saved while playing it stay in its build folder.')) : null),
    h('section', { class: 'step', 'aria-labelledby': 'step2' },
      h('div', { class: 'steph' }, h('h2', { id: 'step2' }, T('2 · Game settings')), h('span', { class: 'hint' }, T('for every start; saved at once in {path}', { path: st.settings.path })),
        h('span', { style: 'flex:1' }), h('button', { class: 'link', 'data-launch': 'tosettings', onclick: () => { st.section = 'settings'; renderAll(); } }, T('Display, sound, mouse: Settings'))),
      h('div', { class: 'setcards' }, moneyCard, gameCard)),
    h('section', { class: 'step', 'aria-labelledby': 'step3' },
      h('div', { class: 'steph' }, h('h2', { id: 'step3' }, T('3 · Bug fixes')), h('span', { class: 'hint' }, T('what the game repairs in the original (docs/CHANGES.md)'))),
      fixesBlock({ fx: sv('game', 'fixes'), set: (v) => setSetting('game', 'fixes', v), lock: classic ? 'classic' : null,
        forced: corig ? creditForced(() => showCredit('game.credit_line')) : null,
        open: L.fixOpen, setOpen: (v) => { L.fixOpen = v; renderEntry(); }, group: L.fixGroup, setGroup: (i) => { L.fixGroup = i; renderEntry(); } }))));
}
async function quitLauncher() {
  try { await api('POST', '/api/quit', {}); } catch (e) { status(e.message, 'err'); return; }
  playLog('The launcher has ended: close this page. OpenMTV.cmd or ./play.sh starts it again.', 'ok');
  status(T('The launcher has ended: close this page. OpenMTV.cmd or ./play.sh starts it again.'), 'ok');
}
async function launchGame(dry) {
  const spec = st.packs.join('+');
  if (spec && !player()) playLog('Save the pack ...', '', true); else playLog('Start ...', '', true);
  if (spec && !player() && !(await save())) { playLog('Not saved: {why}', 'err', false, { why: $('#status').textContent }); return null; }
  delete st.packInfo[spec];
  try {
    const r = await api('POST', '/api/launch', { mod: spec || null, dry: !!dry });     /* (Classic: in the settings file) */
    if (r.built) playLog('Built {dir}', 'ok', false, { dir: r.built });
    if (dry) playLog('Would start: {cmd}', 'ok', false, { cmd: r.cmd }); else playLog('Started: {cmd}', 'ok', false, { cmd: r.cmd });
    status(T('Started: {cmd}', { cmd: r.cmd }), 'ok');
    return r;
  } catch (e) { playLog('Not started: {why}', 'err', false, { why: e.message }); status(T('Not started: {why}', { why: e.message }), 'err'); return null; }
}
/* build only, no start: the selection's game files in build/mods/A+B */
async function buildOnly() {
  const spec = st.packs.join('+');
  if (player()) playLog('Build ...', '', true);
  else {
    playLog('Save the pack ...', '', true);
    if (!(await save())) { playLog('Not saved: {why}', 'err', false, { why: $('#status').textContent }); return false; }
  }
  try {
    playLog('Build ...');
    const r = await api('POST', `/api/mods/${spec}/build`, {});
    playLog('Built {dir}: {files}', 'ok', false, { dir: r.built, files: r.files.join(', ') });
    playLog('Play it: {cmd}', 'ok', false, { cmd: r.cmd });
    status(T('Built "{mod}" in {dir}.', { mod: spec, dir: r.built }), 'ok');
    return true;
  } catch (e) { playLog('Not built: {why}', 'err', false, { why: e.message }); status(T('Not built: {why}', { why: e.message }), 'err'); return false; }
}
MODES.launcher = { page: true, get label() { return T('Play'); }, render: renderLauncher };

/* the page "Settings": what is the player's and not the game's -- the display, the AI upscaling,
   the sound, the mouse --, one choice for the single player and network games (launcher/server.go
   writes the display and the sound to both files, syncShared before and after a network game).
   The mouse is the single player's: a network game's is always System (the server refuses
   another for its file) */
function renderSettings(w) {
  if (!st.settings) {
    w.append(h('div', { class: 'hint' }, T('Reading the settings ...')));
    loadSettings().then(() => { if (st.section === 'settings') renderEntry(); }).catch((e) => status(e.message, 'err'));
    return;
  }
  const { seg } = settingControls(false), row = settingRow, col = 'display:flex;flex-direction:column;gap:12px';
  w.append(h('div', { class: 'page setpage' },
    h('section', { class: 'step', 'aria-labelledby': 'setshead' },
      h('div', { class: 'steph' }, h('h2', { id: 'setshead' }, T('Settings')),
        h('span', { class: 'hint', 'data-tip': st.settings.path }, T('yours, for the single player and network games alike; saved at once, they count from the next start'))),
      h('div', { class: 'setcards' },
        h('div', { style: col }, displayCard(), aiCard()),
        h('div', { style: col },
          setCard(T('Sound and input'),
            row(T('Sound'), T('The AdLib music and the sound effects.'), seg('sound', 'sound', [['on', T('On')], ['off', T('Off')]])),
            row(T('Mouse'), T('Captured: the game takes the mouse (Pause releases it). Free: the system pointer moves the game\'s cursor, with the system\'s speed, and can leave the window. System: as Free, and the system\'s own arrow is the game\'s (no lag) -- in a network game carried items too; the game\'s other cursors stay.'),
              seg('input', 'mouse', [['captured', T('Captured')], ['free', T('Free')], ['system', T('System')]])),
            h('div', { class: 'hint', 'data-setting-note': 'mouse', style: 'grid-column:1/-1' }, icon('lock', 12), ' ',
              T('In a network game the mouse is always System: every input goes to the server and back first, only the system\'s own arrow is at the mouse at once.'))))))));
}
MODES.settings = { page: true, get label() { return T('Settings'); }, render: renderSettings };

SELFTESTS.push(Object.assign(async function selftestLauncher(t) {
  const { ok, wait, go2 } = t;
  /* ---- the launcher: the settings (a test file of its own), the selection, the start (dry);
   * the player's kept selection is put back at the end */
  let keptBefore = null;
  try { keptBefore = localStorage.getItem(PACKS_KEY); } catch (e) { /* ignore */ }
  st.launch = { fixOpen: false, fixGroup: 0 };
  st.log = [];
  st.settings = null;
  /* the page "Settings": the display, the AI upscaling, the sound, the mouse -- the display and the
     sound written to the network games' file too, the mouse the single player's only */
  await go2('#settings');
  await wait(400);
  const spBefore = await api('GET', SETTINGS_URL());
  ok($('#rail [data-key="settings"]').classList.contains('active') && $$('#work [data-setting]').length === 7 && !$('#work [data-setting="display.cast_fit"]') && !$('#inspector').children.length,
    'settings: a page of its own on the board -- display, sound, mouse (seven settings; the role fit not here: a rule of the game)');
  const kiModels = (st.ki && st.ki.models) || [];
  if (st.ki && st.ki.error) ok($('#work #aicard') && !$$('#work #aicard .pill').length, 'settings: the AI upscaling card, the server without them (the former Python editor (editor.py)): no models');
  else ok(kiModels.length === 2 && $$('#work #aicard .pill').length === 2 && $$('#work #aicard a[href^="https://openmodeldb.info/"]').length === 2,
    'settings: the AI upscaling card -- both models, each with its state and its credit');
  $('#work [data-setting="display.filter"] [data-value="mmpx"]').click();
  await wait(400);
  const s3 = await api('GET', SETTINGS_URL()), m3 = await api('GET', SETTINGS_URL(true));
  ok(s3.values.display.filter === 'mmpx' && m3.values.display.filter === 'mmpx' && $('#work [data-setting="display.filter"] [data-value="mmpx"]').classList.contains('on'),
    'settings: the MMPX filter written -- for the single player and for network games');
  $('#work [data-setting="display.scale"]').value = '4';
  $('#work [data-setting="display.scale"]').dispatchEvent(new Event('change'));
  await wait(400);
  $('#work [data-setting="sound.sound"] [data-value="off"]').click();
  await wait(400);
  $('#work [data-setting="input.mouse"] [data-value="free"]').click();
  await wait(400);
  const s9 = await api('GET', SETTINGS_URL()), m9 = await api('GET', SETTINGS_URL(true));
  ok(s9.values.display.scale === '4' && m9.values.display.scale === '4' && s9.values.sound.sound === 'off' && m9.values.sound.sound === 'off',
    'settings: scale 4 and the sound off -- in both files');
  ok(s9.values.input.mouse === 'free' && m9.values.input.mouse === 'system' && $('#work [data-setting-note="mouse"]'),
    'settings: the mouse Free for the single player; network games keep System (the page says so)');
  await api('PUT', SETTINGS_URL(), { values: { display: { filter: 'sharp' }, sound: { sound: 'on' }, input: { mouse: spBefore.values.input.mouse } } });
  await go2('#launcher');
  await wait(400);
  const s0 = await api('GET', SETTINGS_URL());
  ok(/selftest-settings\.ini$/.test(s0.path) && $$('#work [data-setting]').length === 17 && (s0.fixes || []).length >= 12 && !$('#work [data-setting="display.filter"]') && !$('#work #aicard'),
    `launcher: seventeen game settings from the test file (the display and the sound on the page "Settings"), ${(s0.fixes || []).length} fixes from the game`);
  ok(!$('#inspector').children.length && $('#work .playband #launch') && $('#work .logstrip').classList.contains('empty') && !$('#work [data-fix]'),
    'launcher: no side column -- the start in the band on top, no messages yet, the fixes folded');
  $('#work [data-fixes="fold"]').click();
  await wait(200);
  ok($$('#work [data-fixgroup]').length >= 3 && $('#work [data-fix="intruder"]') && $('#work [data-fixes="fold"]').getAttribute('aria-expanded') === 'true', 'launcher: the fixes unfolded, in groups');
  $('#work [data-fixes="none"]').click();
  await wait(400);
  const s1 = await api('GET', SETTINGS_URL());
  ok(s1.values.game.fixes === 'none' && !$('#work [data-fix="intruder"]').classList.contains('on'), 'launcher: no fixes written to the file');
  $('#work [data-fixes="all"]').click();
  await wait(400);
  $('#work [data-fix="random0"]').click();
  await wait(400);
  const s2 = await api('GET', SETTINGS_URL());
  ok(!s2.values.game.fixes.split(',').includes('random0') && s2.values.game.fixes.split(',').includes('intruder') && $('#work .fixbar .hint').textContent.includes('random0'),
    'launcher: one fix left out, the folded line names it (' + s2.values.game.fixes + ')');
  /* Classic: the original's game, no fixes -- the choice of fixes kept for later */
  ok(s0.values.game.classic === 'no' && $('#work [data-setting="game.classic"]').getAttribute('aria-checked') === 'false', 'launcher: Classic off by default');
  $('#work [data-setting="game.classic"]').click();
  await wait(400);
  const s5 = await api('GET', SETTINGS_URL());
  ok(s5.values.game.classic === 'yes' && s5.values.game.fixes === s2.values.game.fixes && $('#work [data-classic="note"]') && $('#work [data-fix="intruder"]').disabled
    && $('#work #launch').textContent.includes(T('Play classic')), 'launcher: Classic on -- written, the fixes locked and their choice kept, the button says so');
  $('#work [data-setting="game.classic"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.classic === 'no' && !$('#work [data-fix="random0"]').disabled, 'launcher: Classic off again: the fixes as chosen');
  /* the start money: the default beside it, typed as the language writes numbers, refused out of range, back to the default */
  const mi = () => $('#work [data-setting="game.start_money"]'), mreset = () => $('#work [data-reset="game.start_money"]');
  ok(mi().value === numFmt(200000) && $('#work .moneyctl .was').textContent === T('Default {value}', { value: money(200000) }) && mreset().disabled,
    'launcher: start money at the default, the default beside it, nothing to reset');
  mi().value = numFmt(1500000);
  mi().dispatchEvent(new Event('change'));
  await wait(400);
  const s4 = await api('GET', SETTINGS_URL());
  ok(s4.values.game.start_money === '1500000' && mi().value === numFmt(1500000) && $('#work .moneyctl').classList.contains('changed') && !mreset().disabled,
    'launcher: start money ' + numFmt(1500000) + ' written (' + s4.values.game.start_money + ')');
  mi().value = '200000000';
  mi().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.start_money === '1500000' && mi().value === numFmt(1500000), 'launcher: start money above $100 million refused');
  mreset().click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.start_money === '200000' && mi().value === numFmt(200000) && mreset().disabled, 'launcher: start money back to the default');
  /* the credit line: 700 000 by default, written, refused off its steps of 100 000, back to the default */
  const ci = () => $('#work [data-setting="game.credit_line"]'), creset = () => $('#work [data-reset="game.credit_line"]');
  ok(s0.values.game.credit_line === '700000' && ci().value === numFmt(700000) && creset().disabled, 'launcher: the credit line at its default (' + numFmt(700000) + ')');
  ci().value = numFmt(1000000);
  ci().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.credit_line === '1000000' && ci().value === numFmt(1000000) && !creset().disabled, 'launcher: the credit line ' + numFmt(1000000) + ' written');
  ci().value = '750000';
  ci().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.credit_line === '1000000' && ci().value === numFmt(1000000), 'launcher: a credit line off the steps of 100 000 refused');
  creset().click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.credit_line === '700000' && creset().disabled, 'launcher: the credit line back to the default');
  /* the original's credit system (credit_line = original): the row keeps its place, the note under
     it slides open, the fix credit-repay off and locked -- the choice of fixes kept; back to the
     staircase: the line as before, credit-repay as chosen */
  const cseg = (v) => $(`#work [data-credit="game.credit_line"] [data-value="${v}"]`), cnote = () => $('#work [data-creditnote="game.credit_line"]');
  const cx = () => [cseg('line'), cseg('original'), ci()].map((e) => Math.round(e.getBoundingClientRect().left)).join(',');
  ci().value = numFmt(1000000);
  ci().dispatchEvent(new Event('change'));
  await wait(400);
  const cx0 = cx();
  ok(cseg('line').classList.contains('on') && !cnote().classList.contains('open') && cnote().inert, 'launcher: the staircase by default, the note under the credit line shut');
  cseg('original').click();
  await wait(700);
  const s6 = await api('GET', SETTINGS_URL());
  ok(s6.values.game.credit_line === 'original' && s6.values.game.fixes === s2.values.game.fixes && cseg('original').classList.contains('on') && ci().disabled && ci().value === numFmt(300000),
    'launcher: "Original 1991" written, the original\'s line shown, the choice of fixes kept');
  ok(cx() === cx0, 'launcher: the credit row keeps its place when it switches (' + cx0 + ' / ' + cx() + ')');
  ok(cnote().classList.contains('open') && !cnote().inert, 'launcher: the note under the credit line slides open');   /* (its height: a hidden page runs no transitions) */
  ok($('#work .fixbar .hint').textContent === T('off: {list}', { list: 'random0' }) + ' · ' + T('credit-repay off: the credit line is "Original 1991"'),
    'launcher: the folded line of the fixes says why credit-repay is off (' + $('#work .fixbar .hint').textContent + ')');
  $('#work [data-credit="tofix"]').click();
  await wait(500);
  const held = () => $('#work [data-fix="credit-repay"]');
  ok(held() && held().dataset.held === 'credit' && held().getAttribute('aria-checked') === 'false' && held().classList.contains('lk') && $('#work [data-fixgroup].fg.on').textContent.includes(T('Rules')),
    'launcher: "to the fix" shows credit-repay in its group: off, locked');
  $('#work [data-fixgroupset="none"]').click();
  await wait(400);
  const s7 = (await api('GET', SETTINGS_URL())).values.game.fixes.split(',');
  ok(s7.includes('credit-repay') && !s7.includes('tx-price'), 'launcher: the group\'s "none" leaves the held credit-repay as chosen');
  $('#work [data-fixgroupset="all"]').click();
  await wait(400);
  $('#work [data-fixes="back"]').click();
  await wait(300);
  ok(document.activeElement === cseg('original'), 'launcher: "change" goes back to the credit line');
  cseg('line').click();
  await wait(700);
  const s8 = await api('GET', SETTINGS_URL());
  ok(s8.values.game.credit_line === '1000000' && ci().value === numFmt(1000000) && !ci().disabled && cx() === cx0, 'launcher: back to the staircase: the line as before, in its place');
  ok(!cnote().classList.contains('open') && held().getAttribute('aria-checked') === 'true' && s8.values.game.fixes === s2.values.game.fixes, 'launcher: the note shut, credit-repay on again as chosen');
  creset().click();
  await wait(400);
  /* the terror damage: 50 000 by default, 0 written (no costs), back to the default */
  const ti = () => $('#work [data-setting="game.terror_damage"]'), treset = () => $('#work [data-reset="game.terror_damage"]');
  ok(s0.values.game.terror_damage === '50000' && ti().value === numFmt(50000) && treset().disabled, 'launcher: the terror damage at its default (' + numFmt(50000) + ')');
  ti().value = '0';
  ti().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.terror_damage === '0' && ti().value === numFmt(0) && !treset().disabled, 'launcher: the terror damage 0 written');
  treset().click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.terror_damage === '50000' && treset().disabled, 'launcher: the terror damage back to the default');
  /* the Sammy's prize money: none by default in its four categories, one of them set */
  ok(SAMMY_KEYS.every((k) => s0.values.game[k] === '0' && $(`#work [data-setting="game.${k}"]`).value === numFmt(0)), 'launcher: no Sammy prize money by default (four categories)');
  const sci = () => $('#work [data-setting="game.sammy_culture"]');
  sci().value = numFmt(250000);
  sci().dispatchEvent(new Event('change'));
  await wait(400);
  const sm = await api('GET', SETTINGS_URL());
  ok(sm.values.game.sammy_culture === '250000' && sm.values.game.sammy_news === '0' && sci().value === numFmt(250000) && !$('#work [data-reset="game.sammy_culture"]').disabled,
    'launcher: the Sammy prize for culture ' + numFmt(250000) + ' written, the others stay');
  /* the fourth speed: off by default, chosen here */
  ok(s0.values.game.fourth_speed === 'no' && $('#work [data-setting="game.fourth_speed"] [data-value="no"]').classList.contains('on'), 'launcher: the fourth speed off by default');
  $('#work [data-setting="game.fourth_speed"] [data-value="yes"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.fourth_speed === 'yes' && $('#work [data-setting="game.fourth_speed"] [data-value="yes"]').classList.contains('on'), 'launcher: the fourth speed chosen');
  /* fast room changes: off by default, chosen here */
  ok(s0.values.game.fast_rooms === 'no' && $('#work [data-setting="game.fast_rooms"] [data-value="no"]').classList.contains('on'), 'launcher: fast room changes off by default');
  $('#work [data-setting="game.fast_rooms"] [data-value="yes"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.fast_rooms === 'yes' && $('#work [data-setting="game.fast_rooms"] [data-value="yes"]').classList.contains('on'), 'launcher: fast room changes chosen');
  /* the series topicality: on by default, switched off here */
  ok(s0.values.game.series_topicality === 'yes' && $('#work [data-setting="game.series_topicality"] [data-value="yes"]').classList.contains('on'), 'launcher: the series topicality on by default');
  $('#work [data-setting="game.series_topicality"] [data-value="no"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.series_topicality === 'no' && $('#work [data-setting="game.series_topicality"] [data-value="no"]').classList.contains('on'), 'launcher: the series topicality switched off');
  /* the AI improvement: off by default, chosen here */
  ok(s0.values.game.ai_improve === 'off' && $('#work [data-setting="game.ai_improve"] [data-value="off"]').classList.contains('on'), 'launcher: the AI improvement off by default');
  $('#work [data-setting="game.ai_improve"] [data-value="strong"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.ai_improve === 'strong' && $('#work [data-setting="game.ai_improve"] [data-value="strong"]').classList.contains('on'), 'launcher: the AI improvement chosen');
  /* the ad offers: the original's 10 by default, 8 choices, 14 chosen here */
  const ao = () => $('#work [data-setting="game.ad_offers"]');
  ok(s0.values.game.ad_offers === '10' && ao().value === '10' && ao().options.length === 8, 'launcher: the ad offers at the original\'s 10 by default, 10 to 17 to choose');
  ao().value = '14';
  ao().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.ad_offers === '14' && ao().value === '14', 'launcher: 14 ad offers chosen');
  /* the film offers: the same, 13 chosen */
  const fo = () => $('#work [data-setting="game.film_offers"]');
  ok(s0.values.game.film_offers === '10' && fo().value === '10' && fo().options.length === 8, 'launcher: the film offers at the original\'s 10 by default, 10 to 17 to choose');
  fo().value = '13';
  fo().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.film_offers === '13' && fo().value === '13', 'launcher: 13 film offers chosen');
  /* the role fit in the shop: a rule of the game's start (the card "Game"), off by default */
  ok(s0.values.display.cast_fit === 'no' && $('#work [data-setting="display.cast_fit"] [data-value="no"]').classList.contains('on'), 'launcher: the role fit in the shop off by default');
  $('#work [data-setting="display.cast_fit"] [data-value="yes"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.display.cast_fit === 'yes' && $('#work [data-setting="display.cast_fit"] [data-value="yes"]').classList.contains('on'), 'launcher: the role fit in the shop switched on');
  await api('PUT', SETTINGS_URL(), { values: { display: { scale: '3', cast_fit: 'no' }, game: { classic: 'no', fixes: 'all', start_money: '200000', credit_line: '700000', terror_damage: '50000',
    sammy_news: '0', sammy_culture: '0', sammy_shows: '0', sammy_series: '0', fourth_speed: 'no', fast_rooms: 'no', series_topicality: 'yes', ai_improve: 'off', ad_offers: '10', film_offers: '10' } } });
  await loadSettings(true);
  renderEntry();
  /* the selection: the self-test's pack alone; the original stays its base */
  ok(same(st.packs, ['selftest']) && $('#work [data-play="original"]').classList.contains('on') && $('#work [data-play="original"]').disabled
    && $('#work [data-play="selftest"]').classList.contains('on'), 'launcher: the self-test pack on the original (the original locked)');
  ok($('#work [data-launch="cmd"]').textContent === `${playCmd()} --mod selftest` && !$('#work #buildonly'), 'launcher: the command (' + $('#work [data-launch="cmd"]').textContent + '), the start alone (no "Build only")');
  const r = await launchGame(true);
  ok(r && r.cmd === `${playCmd()} --mod selftest` && !r.pid && $('#playlog').textContent.includes(T('Would start: {cmd}', { cmd: r.cmd })) && !$('#work .logstrip').classList.contains('empty'),
    'launcher: the start, dry (nothing started); the messages under the band');
  const keptLog = st.log;
  $('#work [data-launch="hidelog"]').click();
  await wait(100);
  ok($('#work .logstrip').classList.contains('empty'), 'launcher: the messages hidden again');
  st.log = keptLog;
  renderEntry();
  ok(st.meta.window ? !$('#rail #quitlauncher') : $('#rail #quitlauncher') && $('#rail #quitlauncher').textContent === T('Quit the launcher'),
    st.meta.window ? 'launcher: no button to end it in its own window (closing the window does)' : 'launcher: a button at the foot of the board ends it (not pressed here)');
  /* several packs: in the order clicked, reordered, the report, the start (dry); the editor shows them together */
  const other = st.mods.find((m) => m.name !== 'selftest' && !m.complete && !m.error);
  if (other) {
    $(`#work [data-play="${other.name}"]`).click();
    await wait(900);
    const both = `selftest+${other.name}`;
    ok(same(st.packs, ['selftest', other.name]) && !st.mod && $('#work [data-launch="cmd"]').textContent === `${playCmd()} --mod ${both}`
      && $$('#work .packline').length === 2, `launcher: two packs together (${$('#work [data-launch="cmd"]').textContent}), nothing to save into until a change`);
    $(`#work [data-move="${other.name}-up"]`).click();
    await wait(900);
    const r2 = await launchGame(true);
    ok(r2 && r2.cmd === `${playCmd()} --mod ${other.name}+selftest` && $('#work [data-play="selftest"] .no').textContent === '2',
      'launcher: the order changed, the start of both, dry (' + (r2 && r2.cmd) + ')');
    ok(keptPacks().join('+') === `${other.name}+selftest`, 'launcher: the packs and their order kept for the next visit');
    $('#work [data-play="only-original"]').click();
    await wait(600);
    ok($('#work [data-launch="cmd"]').textContent === playCmd() && !keptPacks().length && $('#work [data-play="original"]').classList.contains('on'), 'launcher: back to the original game alone, kept');
  } else ok(true, 'launcher: several packs not checked (only the self-test pack is here)');
  await selectPacks(['selftest']);
  try { if (keptBefore === null) localStorage.removeItem(PACKS_KEY); else localStorage.setItem(PACKS_KEY, keptBefore); } catch (e) { /* ignore */ }
  st.launch = { fixOpen: false, fixGroup: 0 };
}, { player: true }));      /* (the player's launcher runs it too: selftest.js playerSelftest) */
