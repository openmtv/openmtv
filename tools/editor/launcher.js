/* OpenMTV editor -- the launcher ("Play"): the selection -- the original game and the packs laid
 * over it in their order (tools/modpacks.py: each pack's new entries appended after the ones
 * before), the same that the editor shows -- the game's settings (the player's settings.ini,
 * which ./play.sh and the game read; the former Python settings module (gamesettings.py)) and the start (the former Python launcher (play.py); the
 * command is shown as the player types it: ./play.sh, on Windows play.cmd).  The original is a
 * pack too: the base of the others, left out only when a complete pack is chosen (one the
 * editor made from a selection: it holds the original already).  A setting is written at
 * once, in place (the file's comments and order stay) -- "Classic" too ([game] classic: no
 * fixes, the fixes' choice kept for when it is off again).  The page has no side column: on
 * top the start -- the safe of the office with this selection's saved games (launcher/saves.go;
 * a tape picked starts with --load-slot), a new game beside it --, the messages under it once
 * there are any, then three tabs (gameTabs) that say shut what is set: the packs (and the safes
 * of other selections), the rules, the fixes (all, an own choice, Classic 1991).  The self-test
 * works on a settings file of its own (?test=1) and never starts the game (dry).  The tabs, the
 * rules' rows, the fixes, the money and the pack rows serve the lobby of the page "Multiplayer"
 * too (multiplayer.js: the network games' own settings file, mp).  What is the player's and
 * not the game's -- the display, the AI upscaling, the sound, the mouse -- is the page
 * "Settings" (renderSettings), for both. */
'use strict';
/* the settings file: the single player's, or (mp) the network games' own (multiplayer.ini) */
const SETTINGS_URL = (mp) => '/api/settings' + (new URLSearchParams(location.search).has('selftest') ? (mp ? '?test=1&mp=1' : '?test=1') : (mp ? '?mp=1' : ''));
const settingsKey = (mp) => (mp ? 'mpSettings' : 'settings');
st.launch = st.launch || { fixGroup: 0, slot: 0, onlyChanged: false };
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
  'trendy-studios': 'die Show „Im Trend“ (Film 138) beendet beim Senden die Produktionssperre jedes laufenden Studios, egal wer sie sendet: ein mitten im Dreh zurückgegebenes Studio wird sofort frei, die Produktion ist verloren, und später „beendet“ das Spiel Drehbuch 0 über das Nachrichtenbudget eines Computerspielers',
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
/* ---------------------------------------------------------------- the three tabs: Packs, Rules, Fixes
   Under the start of the page "Play" and in the lobby.  Each head says in one line what is set,
   so shut they are the summary and the page fits one screen; a click on a head opens its panel
   under the heads, a click on the open one -- or the chevron -- shuts it.  The panel slides: the
   drawer's height goes from what it showed to what it shows now (opening, shutting, from one tab
   to another), a panel coming in fades in.  The page is drawn anew on every change (renderEntry):
   the drawer starts at the height it had and is measured once it is in the page, and again while
   its content changes (a note sliding open).  Which tab is open is kept in this browser, by page. */
const TABS_KEY = 'madtv-tabs';
const TAB_IDS = ['packs', 'rules', 'fixes'];
const TAB_ICON = { packs: 'layers', rules: 'sliders', fixes: 'wrench' };
st.tabs = st.tabs || {};
function tabsOf(page) {
  if (!st.tabs[page]) {
    let v = null;
    try { v = (JSON.parse(localStorage.getItem(TABS_KEY) || '{}') || {})[page]; } catch (e) { /* ignore */ }
    st.tabs[page] = { open: v && TAB_IDS.includes(v.open) ? v.open : '', last: v && TAB_IDS.includes(v.last) ? v.last : 'packs' };
  }
  return st.tabs[page];
}
function setTab(page, open) {
  const t = tabsOf(page);
  t.open = open;
  if (open) t.last = open;
  try { const all = JSON.parse(localStorage.getItem(TABS_KEY) || '{}') || {}; all[page] = t; localStorage.setItem(TABS_KEY, JSON.stringify(all)); } catch (e) { /* ignore */ }
  renderEntry();
}
const tabsWas = {};          /* by page: the tab the drawer showed when last drawn, its height (px), open or not */
const tabsRO = {};
/* o: page, heads [{ id, label, sub, chg (the summary in yellow: something differs from the default) }],
   panel(id) -> the panel's element */
function gameTabs(o) {
  const t = tabsOf(o.page), open = t.open, was = tabsWas[o.page];
  const shown = open || (was && was.h > 0 ? was.tab : '');     /* (shutting: the panel stays while the drawer slides shut) */
  const panel = shown ? o.panel(shown) : null;
  if (panel && (!was || was.tab !== shown || !was.h)) panel.classList.add('fresh');   /* (a panel coming in fades in, not a change in the open one) */
  const inner = h('div', { class: 'gdrawer-in' }, panel);
  const drawer = h('div', { class: 'gdrawer', id: 'gdrawer-' + o.page, 'data-drawer': o.page }, inner);
  drawer.inert = !open;
  if (was) drawer.style.height = was.h + 'px';
  else { drawer.classList.add('instant'); drawer.style.height = open ? 'auto' : '0px'; }
  const size = () => {
    if (!drawer.isConnected) return;
    const px = open ? inner.offsetHeight : 0;
    drawer.style.height = px + 'px';
    tabsWas[o.page] = { tab: shown, h: px, open: !!open };
  };
  if (tabsRO[o.page]) tabsRO[o.page].disconnect();
  if (typeof ResizeObserver !== 'undefined') { tabsRO[o.page] = new ResizeObserver(size); tabsRO[o.page].observe(inner); }
  const fold = h('button', { class: 'gfold' + ((was ? was.open : open) ? ' up' : ''), 'data-tab': 'fold', 'aria-expanded': open ? 'true' : 'false', 'aria-controls': drawer.id,
    'aria-label': open ? T('Fold the tabs') : T('Unfold the tabs'), 'data-tip': open ? T('Fold the tabs') : T('Unfold the tabs'), onclick: () => setTab(o.page, open ? '' : t.last) }, icon('down', 16));
  /* (in the page by then; a timer, as a hidden page gets no frames) */
  setTimeout(() => { size(); fold.classList.toggle('up', !!open); setTimeout(() => drawer.classList.remove('instant'), 30); });
  tabsWas[o.page] = { tab: shown, h: was ? was.h : 0, open: was ? was.open : !!open };
  const heads = o.heads.map((x) => {
    const on = open === x.id;
    return h('button', { class: 'gtab' + (on ? ' on' : ''), 'data-tab': x.id, 'aria-expanded': on ? 'true' : 'false', 'aria-controls': drawer.id, onclick: () => setTab(o.page, on ? '' : x.id) },
      h('span', { class: 'gti' }, icon(TAB_ICON[x.id], 18)),
      h('span', { class: 'gtt' }, h('b', {}, x.label), h('span', { class: 'gts' + (x.chg ? ' chg' : ''), 'data-tabsum': x.id }, x.sub)));
  });
  return h('section', { class: 'gametabs', 'data-tabs': o.page, 'aria-label': T('Packs, rules, fixes') }, h('div', { class: 'gtabs' }, ...heads, fold), drawer);
}

/* the panel "Rules": the rules in groups by meaning, in two columns; a changed one marked (a dot,
   its control's border) with its way back to the default beside it -- the place stays empty for
   an unchanged one, so nothing moves.  o: cols [[[title, rows], ...], ...] (a row: { key, label,
   help, ctl, changed, def (the default, as text), reset() (none: shown only), tag, note }),
   only/setOnly (only the changed ones shown), resetAll() (none: nothing to reset here), lock (a
   line: why they cannot change) */
function rulesPanel(o) {
  const rows = o.cols.flat().flatMap((g) => g[1]), n = rows.filter((r) => r.changed).length, only = !!o.only && n > 0;
  const row = (r) => h('div', { class: 'rrow' + (r.changed ? ' chg' : '') + (only && !r.changed ? ' hide' : ''), 'data-rule': r.key },
    h('div', { class: 'rlab' }, h('span', { class: 'rdot' }), h('span', { class: 'rname' }, r.label), r.tag ? h('span', { class: 'rtag' }, r.tag) : null, help(r.help, r.label)),
    h('div', { class: 'rctl' }, r.ctl),
    h('button', { class: 'rreset', 'data-reset': r.key, disabled: !r.changed || !r.reset, tabindex: r.changed && r.reset ? null : '-1',
      'aria-label': T('Back to the default: {value}', { value: r.def }), 'data-tip': T('Back to the default: {value}', { value: r.def }), onclick: () => r.reset && r.reset() },
    icon('reset', 13), h('span', {}, r.def)),
    r.note || null);
  return h('div', { class: 'gpanel rulespanel' },
    h('div', { class: 'rhead' },
      h('span', { class: 'rcount' + (n ? ' chg' : ''), 'data-rules': 'count' }, n ? T('{n} of {total} changed', { n, total: rows.length }) : T('all {total} as by default', { total: rows.length })),
      o.lock ? h('span', { class: 'rlock' }, icon('lock', 12), o.lock) : null,
      h('span', { style: 'flex:1' }),
      h('div', { class: 'seg small', role: 'group', 'aria-label': T('Which rules') },
        h('button', { class: only ? '' : 'on', 'data-rules': 'all', 'aria-pressed': only ? 'false' : 'true', onclick: () => o.setOnly(false) }, T('All|rules')),
        h('button', { class: only ? 'on' : '', 'data-rules': 'changed', 'aria-pressed': only ? 'true' : 'false', disabled: !n, onclick: () => o.setOnly(true) }, T('Only the changed'))),
      h('button', { class: 'plate small', 'data-rules': 'reset', style: n && o.resetAll ? null : 'visibility:hidden', disabled: !n || !o.resetAll, onclick: () => o.resetAll() }, T('All to default'))),
    h('div', { class: 'rcols' }, ...o.cols.map((col) => h('div', { class: 'rcol' }, ...col.map(([title, rs]) =>
      h('div', { class: 'rgroup' + (only && !rs.some((r) => r.changed) ? ' hide' : '') }, h('div', { class: 'rgh' }, title), ...rs.map(row)))))));
}

/* a button's text in the width of the wider of two (the other one there, unseen) */
const twoLabels = (shown, other) => h('span', { class: 'twolabels' }, h('span', {}, shown), h('span', { class: 'other', 'aria-hidden': 'true' }, other));
/* the fixes in groups (the game's list, build/mtv-dec --fixes=list; one it adds later: "More") */
const fixGroups = () => [
  [T('Crashes and hangs'), ['intruder', 'random0']],
  [T('Computer players'), ['ai-studio', 'ai-fill-hour', 'ai-series-rule', 'ai-odd-range', 'ai-visit', 'ai-trendy', 'ai-rival', 'ai-arrived', 'ai-fill-genre', 'ai-lib-quality', 'ai-rating', 'ai-bids', 'ai-warning']],
  [T('Rules'), ['tx-price', 'catch-bounty', 'library-owner', 'case-owner', 'case-corner', 'credit-repay', 'studio-takeover', 'studio-reserved', 'trendy-studios', 'series-avg']],
  [T('Texts, data and display'), ['estate-text', 'news-cache', 'boss-music', 'film-200', 'live-aid', 'video-connection', 'title-cache']],
];
/* how the fixes are chosen: all of them (the default), an own choice, or Classic 1991 (none: the
   original with all its bugs).  The settings file has "fixes" (all, none or names) and Classic;
   the own choice is kept here (by page: the single player's, the network games') -- "All fixes"
   writes "all", "Own choice" brings the kept names back */
const FIXOWN_KEY = 'madtv-fixes-own';
st.fixOwn = st.fixOwn || {};
function fixOwn(which) {
  if (!st.fixOwn[which]) {
    let v = null;
    try { v = (JSON.parse(localStorage.getItem(FIXOWN_KEY) || '{}') || {})[which]; } catch (e) { /* ignore */ }
    st.fixOwn[which] = { mode: !!(v && v.mode), list: v && typeof v.list === 'string' ? v.list : '' };
  }
  return st.fixOwn[which];
}
function keepFixOwn(which, mode, list) {
  const o = fixOwn(which);
  o.mode = mode;
  if (list !== undefined) o.list = list;
  try { const all = JSON.parse(localStorage.getItem(FIXOWN_KEY) || '{}') || {}; all[which] = o; localStorage.setItem(FIXOWN_KEY, JSON.stringify(all)); } catch (e) { /* ignore */ }
}
/* the mode from the file's choice: Classic, else a list of names is an own choice, else "all"
   is all of them -- or an own choice that has all on */
const fixMode = (classic, fx, which) => (classic ? 'classic' : (fx || 'all') !== 'all' ? 'own' : fixOwn(which).mode ? 'own' : 'all');
/* the summary of the fixes for their tab's head: [text, differs] */
function fixesSummary(mode, fx, forced) {
  const all = (st.settings || st.mpSettings || {}).fixes || [], names = all.map((f) => f.name);
  if (mode === 'classic') return [T('Classic 1991: none on'), true];
  const chosen = new Set((fx || 'all') === 'all' ? names : fx === 'none' ? [] : fx.split(',').map((x) => x.trim()));
  if (forced) chosen.delete(forced.name);
  const on = names.filter((n) => chosen.has(n)).length;
  return on === names.length ? [T('all {n} on', { n: on }), false] : [T('{n} of {total} on', { n: on, total: names.length }), true];
}
/* the panel "Fixes": the choice -- a switch like every other setting, no tab: all fixes, an own
   choice, Classic 1991 --, under it the groups beside the fixes of the one picked, each with what
   the original does.  o: page, mode, pick(mode) (none: shown only), fx (all, none or names), set(v)
   (none: shown only), lock why it cannot change ('save': the savegame's, 'host': as the host sets
   it), mp (for every player), forced ({ name, why, back() }: a fix another setting holds off --
   off and locked, the choice for it kept: all on, all off and a group's all/none leave it as it
   was), group/setGroup; attr, key: the panel's data attribute */
const fixNoteWas = {};
function fixesPanel(o) {
  const all = (st.settings || st.mpSettings || {}).fixes || [];
  const fx = o.fx || 'all', names = all.map((f) => f.name), classic = o.mode === 'classic';
  const chosen = new Set(fx === 'all' ? names : fx === 'none' ? [] : fx.split(',').map((x) => x.trim()));
  const edit = !!o.set && !o.lock && !classic;
  const forced = !classic && o.forced && names.includes(o.forced.name) ? o.forced : null;
  const held = (n) => !!forced && forced.name === n;
  const isOn = (f) => !classic && chosen.has(f.name) && !held(f.name);
  const keep = (s) => { if (forced) { if (chosen.has(forced.name)) s.add(forced.name); else s.delete(forced.name); } return s; };
  const put = (s) => o.set((s = keep(s)).size === names.length ? 'all' : s.size ? names.filter((n) => s.has(n)).join(',') : 'none');
  const known = new Set(fixGroups().flatMap((g) => g[1]));
  const groups = fixGroups().map(([t, ns]) => [t, all.filter((f) => ns.includes(f.name))])
    .concat([[T('More'), all.filter((f) => !known.has(f.name))]]).filter((g) => g[1].length);
  const on = all.filter(isOn).length;
  const modes = [['all', T('All fixes')], ['own', T('Own choice')], ['classic', T('Classic 1991')]];
  const desc = { all: T('the game without its known bugs · recommended'), own: T('single fixes on or off, below'), classic: T('exactly the original of 1991, with all its bugs') }[o.mode];
  const choice = h('div', { class: 'seg lg', role: 'radiogroup', 'aria-label': T('Fixes|tab'), 'data-fixmode': o.mode },
    ...modes.map(([m, label]) => h('button', { class: o.mode === m ? 'on' : '', role: 'radio', 'aria-checked': o.mode === m ? 'true' : 'false', 'data-value': m,
      disabled: !o.pick || !!o.lock, onclick: () => { if (o.mode !== m) o.pick(m); } }, label)));
  /* Classic: what it does, what it keeps -- sliding open and shut */
  const was = o.page in fixNoteWas ? fixNoteWas[o.page] : classic;
  fixNoteWas[o.page] = classic;
  const keptN = (() => { const k = fixOwn(o.mp ? 'mp' : 'sp').list || (fx !== 'all' && fx !== 'none' ? fx : ''); return k === 'none' ? 0 : k && k !== 'all' ? k.split(',').length : names.length; })();
  const note = kiSlide(h('div', { class: 'slide' + (was ? ' open' : ''), 'data-fixes': 'classicnote' },
    h('div', {}, h('div', { class: 'classicnote' }, icon('lock', 16), h('div', {},
      h('b', {}, T('Classic 1991: no fix -- the rules, bugs and timing of the original.')),
      h('span', {}, [o.mp ? T('For every player.') : '', o.pick ? T('Your own choice ({n} of {total} on) is kept: "Own choice" brings it back. Packs and rules stay as set.', { n: keptN, total: names.length }) : ''].filter(Boolean).join(' ')))))), classic, was);
  const g = Math.min(o.group || 0, Math.max(groups.length - 1, 0));
  const panel = h('div', { class: 'gpanel fixespanel', [o.attr || 'data-setting']: o.key || 'game.fixes' },
    h('div', { class: 'kmode' }, choice, h('span', { class: 'kdesc' }, desc), h('span', { class: 'kcount', 'data-fixes': 'count' }, T('{n} of {total} on', { n: on, total: all.length }))),
    note,
    o.lock ? h('div', { class: 'fixlockline' }, icon('lock', 12), o.lock === 'save' ? T('The saved evening sets them; a new game takes your choice again.') : T('As the host sets them.')) : null);
  if (!groups.length) return panel;
  const [gt, gf] = groups[g];
  panel.append(h('div', { class: 'fixbody' + (classic ? ' k' : '') },
    h('div', { class: 'fixgroups', role: 'tablist', 'aria-label': T('Groups') }, ...groups.map(([t, fs], i) => h('button', {
      class: 'fg' + (i === g ? ' on' : ''), role: 'tab', 'aria-selected': i === g ? 'true' : 'false', 'data-fixgroup': String(i), onclick: () => o.setGroup(i) },
    h('span', {}, t), h('span', { class: 'gc' }, `${fs.filter(isOn).length} / ${fs.length}`)))),
    h('div', { class: 'fixpanel', role: 'tabpanel' },
      h('div', { class: 'fph' }, h('b', {}, gt), h('span', { class: 'hint' }, classic ? T('none · Classic 1991') : T('{n} of {total} on', { n: gf.filter(isOn).length, total: gf.length })),
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
  panel.append(h('div', { class: 'hint fixfoot' }, T('Every difference from the original: docs/CHANGES.md')));
  return panel;
}
/* the big button: its symbol over two lines -- what it does, how it stands.  cls: red (play),
   cont (go on from a saved game), ready (green: go), todo (orange: your turn), wait (orange,
   muted: nothing to do yet) */
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
/* the packs played: the original first (the base, left out only for a complete pack), then the
   chosen ones in their order -- numbered, moved by their grip (drag it, or the arrow keys) --,
   then the others, to switch on (each to the end of the row).  o: packs and select(packs) (the
   editor's selection by default); manage: delete buttons and the unsaved mark (the launcher);
   attr, moveAttr: the switches' and grips' data attributes (the lobby's own); disabled.
   Returns the rows: [the active ones' head, list, the others' head, list] */
let packFocus = null;          /* the grip moved by the keyboard: focused again once the page is drawn anew */
function packRows(o = {}) {
  const packs = o.packs || st.packs, select = o.select || selectPacks, manage = o.manage !== false && !player();
  const attr = o.attr || 'data-play', moveAttr = o.moveAttr || 'data-move';
  const held = heldBy(packs), complete = holdsOriginal(packs), holder = packs.find((n) => modInfo(n)?.complete);
  const sw = (on, name, disabled, onclick, label) => h('button', { class: 'psw' + (on ? ' on' : ''), role: 'switch', 'aria-checked': on ? 'true' : 'false', [attr]: name,
    disabled: !!disabled, 'aria-label': label, onclick });
  const tags = (m) => h('span', { class: 'tags' },
    m.protected && manage ? h('span', { class: 'pill dark', 'data-tip': T('Never changed by the editor: a change makes a new pack (unless unlocked in the overview).') }, icon('lock', 12), T('protected')) : null,
    m.complete ? h('span', { class: 'pill blue', 'data-tip': T('Holds the original and the packs it was made from: {list}', { list: [T('Original')].concat((m.from || []).map(packTitle)).join(' + ') }) }, T('complete')) : null,
    manage && st.mod === m.name && st.dirty.size ? h('span', { class: 'pill yellow' }, T('not saved')) : null);
  /* the line under a pack's title: parts in a row that wraps (a pack's choice of titles joins it later) */
  const sub = (...parts) => h('span', { class: 'psub' }, ...parts.filter(Boolean).map((x) => h('span', {}, x)));
  const del = (m) => (manage && !m.protected ? h('button', { class: 'pdel', 'data-delete': m.name, 'aria-label': T('Delete "{title}" ...', { title: m.title || m.name }),
    'data-tip': T('Delete "{title}" ...', { title: m.title || m.name }), onclick: () => deleteDialog(m.name) }, icon('remove', 14)) : h('span', {}));
  const move = (from, to) => {
    if (to === from || to < 0 || to >= packs.length) return;
    const p = packs.slice(), [x] = p.splice(from, 1);
    p.splice(to, 0, x);
    select(p);
  };
  const active = h('div', { class: 'plist', 'data-packs': 'active' });
  active.append(h('div', { class: 'prow on base' + (complete ? ' off' : ''), [attr]: 'original', 'aria-pressed': complete ? 'false' : 'true' },
    h('span', { class: 'pnum' }, complete ? '' : '1'), h('span', { class: 'pmark' }, icon('lock', 14)),
    h('span', { class: 'pt' }, h('b', {}, T('The original game')), sub(complete ? T('held in "{pack}" already', { pack: packTitle(holder) }) : T('the base of every pack; left out only for a complete pack'))),
    h('span', { class: 'pill dark' }, T('base')), h('span', {}), h('span', {})));
  packs.forEach((name, k) => {
    const m = modInfo(name) || { name, title: name };
    const grip = h('button', { class: 'grip', [moveAttr]: name, disabled: !!o.disabled || packs.length < 2,
      'aria-label': T('Move "{title}": drag it, or the arrow keys', { title: m.title || name }), 'data-tip': packs.length > 1 ? T('Drag to move it; or the arrow keys') : null,
      onkeydown: (e) => {
        const d = e.key === 'ArrowUp' ? -1 : e.key === 'ArrowDown' ? 1 : 0;
        if (!d || k + d < 0 || k + d >= packs.length) return;
        e.preventDefault();
        packFocus = moveAttr + '=' + name;
        move(k, k + d);
      },
      onpointerdown: (e) => packDrag(e, k, active, (to) => move(k, to)) }, icon('grip', 16));
    active.append(h('div', { class: 'prow on', 'data-packrow': name },
      h('span', { class: 'pnum no' }, String(k + (complete ? 1 : 2))), grip,
      h('span', { class: 'pt' }, h('b', {}, m.title || name), m.error ? h('span', { class: 'err', 'data-packerror': name }, m.error) : sub(`mods/${name}`, packSummary(m))),
      tags(m), del(m),
      sw(true, name, o.disabled, () => select(packs.filter((n) => n !== name)), T('"{title}" is played: off', { title: m.title || name }))));
  });
  const more = h('div', { class: 'plist', 'data-packs': 'more' });
  for (const m of st.mods) {
    if (packs.includes(m.name)) continue;
    const inside = held[m.name];
    more.append(h('div', { class: 'prow' + (inside ? ' off' : ''), 'data-packrow': m.name },
      h('span', { class: 'pnum' }), h('span', {}),
      h('span', { class: 'pt' }, h('b', {}, m.title || m.name), m.error ? h('span', { class: 'err', 'data-packerror': m.name }, m.error)
        : inside ? sub(T('held in "{pack}" already', { pack: packTitle(inside) })) : sub(`mods/${m.name}`, packSummary(m))),
      tags(m), del(m),
      sw(false, m.name, !!inside || !!o.disabled || !!m.error, () => select(packs.concat([m.name])), T('"{title}": play it too', { title: m.title || m.name }))));
  }
  if (packFocus) {
    const want = packFocus;
    packFocus = null;
    setTimeout(() => { const [a, v] = want.split('='); const el = $(`#work [${a}="${v}"]`); if (el) el.focus(); });
  }
  return [h('div', { class: 'h3' }, T('Active packs'), h('span', { class: 'hint' }, T('played together in this order; where two change the same entry, the later one counts'))), active,
    h('div', { class: 'h3' }, T('More packs'), h('span', { class: 'hint' }, more.children.length ? T('switched on: to the end of the row') : T('none here: "New pack" makes one'))), more];
}
/* a pack dragged by its grip: the others make room as it passes them; let go, the order is taken */
function packDrag(e, from, list, done) {
  if (e.button !== 0 || e.currentTarget.disabled) return;
  e.preventDefault();
  const grip = e.currentTarget, rows = [...list.querySelectorAll('[data-packrow]')], row = rows[from];
  if (!row || rows.length < 2) return;
  const mid = rows.map((r) => { const b = r.getBoundingClientRect(); return b.top + b.height / 2; });
  const step = rows.length > 1 ? Math.abs(mid[1] - mid[0]) : row.offsetHeight;
  const y0 = e.clientY;
  let to = from;
  grip.setPointerCapture(e.pointerId);
  row.classList.add('dragging');
  list.classList.add('sorting');
  const onmove = (ev) => {
    const dy = ev.clientY - y0, y = mid[from] + dy;
    row.style.transform = `translateY(${dy}px)`;
    to = rows.reduce((n, r, i) => n + (i !== from && mid[i] < y ? 1 : 0), 0);
    rows.forEach((r, i) => {
      if (i === from) return;
      const shift = i > from && i <= to ? -step : i < from && i >= to ? step : 0;
      r.style.transform = shift ? `translateY(${shift}px)` : '';
    });
  };
  const onup = () => {
    grip.removeEventListener('pointermove', onmove);
    grip.removeEventListener('pointerup', onup);
    grip.removeEventListener('pointercancel', onup);
    rows.forEach((r) => { r.style.transform = ''; });
    row.classList.remove('dragging');
    list.classList.remove('sorting');
    if (to !== from) done(to);
  };
  grip.addEventListener('pointermove', onmove);
  grip.addEventListener('pointerup', onup);
  grip.addEventListener('pointercancel', onup);
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
/* an amount of dollars (the start money, the credit line): as the interface language writes them;
   the rule's row (rulesPanel) shows the default and the way back to it.  o: cur the value,
   set(dollars) to change it (none: shown only), skey the settings key (its schema: default,
   lowest, highest), label, bad() the message for a refused value, step (the value a multiple of
   it); attr, key: the control's data attribute and its value */
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
      onchange: commit, onkeydown: (e) => { if (e.key === 'Enter') e.target.blur(); } }));
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
   note (its text), jump() to the fix.  Returns { ctl, note } for the rule's row */
const CREDIT_ORIGINAL = 'original';
const creditWas = {};    /* by row: the system it showed last -- its note slides from there */
const creditKept = {};   /* by row: the line before "Original 1991", back to it */
function creditParts(o) {
  const sch = ((st.settings || st.mpSettings || {}).schema || []).find((x) => x.section === 'game' && x.key === 'credit_line');
  const def = sch ? sch.opts[0] : 700000;
  const pick = (orig) => () => {
    if (orig === o.orig) return;
    if (orig) creditKept[o.key] = o.cur;
    o.set(orig ? CREDIT_ORIGINAL : creditKept[o.key] || def);
  };
  const seg = h('div', { class: 'seg', role: 'group', 'aria-label': T('Credit system'), 'data-credit': o.key },
    h('button', { class: o.orig ? '' : 'on', 'data-value': 'line', 'aria-pressed': o.orig ? 'false' : 'true', disabled: !o.set, onclick: pick(false) }, T('Staircase')),
    h('button', { class: o.orig ? 'on' : '', 'data-value': CREDIT_ORIGINAL, 'aria-pressed': o.orig ? 'true' : 'false', 'aria-label': T('Original 1991'), 'data-tip': T('Original 1991'),
      disabled: !o.set, onclick: pick(true) }, T('1991|credit')));
  const amount = !o.orig ? creditLine(o.cur, o.set, o.attr, o.key)
    : h('div', { class: 'moneyctl orig' }, h('span', { class: 'unit' }, '$'),
      h('input', { type: 'text', [o.attr]: o.key, 'aria-label': T('Credit line'), value: numFmt(300000), disabled: true }));
  const was = o.key in creditWas ? creditWas[o.key] : o.orig;
  creditWas[o.key] = o.orig;
  const note = h('div', { class: 'slide' + (was ? ' open' : ''), 'data-creditnote': o.key },
    h('div', {}, h('div', { class: 'inlinenote' }, icon('lock', 14),
      h('span', {}, o.note, o.jump ? ' ' : '', o.jump ? h('button', { class: 'link', 'data-credit': 'tofix', onclick: o.jump }, T('to the fix')) : null))));
  note.inert = !o.orig;
  if (was !== o.orig) setTimeout(() => { void note.offsetHeight; note.classList.toggle('open', o.orig); });   /* (in the page by then; a timer, as a hidden page gets no frames) */
  return { ctl: h('div', { class: 'creditctl' }, seg, amount), note };
}
/* the fix held off by the original's credit (fixesPanel's forced): its row, why, and back to the credit */
const creditForced = (back) => ({ name: 'credit-repay', why: T('off: the credit line is set to "Original 1991"'), short: T('credit-repay off: the credit line is "Original 1991"'), back });
/* to a fix: the tab "Fixes" opened at its group, the row shown.  setGroup(i): the page's group */
function showFix(page, name, setGroup) {
  const all = (st.settings || st.mpSettings || {}).fixes || [];
  const known = new Set(fixGroups().flatMap((g) => g[1]));
  const groups = fixGroups().map(([, ns]) => all.filter((f) => ns.includes(f.name))).concat([all.filter((f) => !known.has(f.name))]).filter((g) => g.length);
  setGroup(Math.max(groups.findIndex((fs) => fs.some((f) => f.name === name)), 0));
  setTab(page, 'fixes');
  setTimeout(() => {
    const r = $(`#work [data-fix="${name}"]`);
    if (!r) return;
    r.scrollIntoView({ behavior: 'smooth', block: 'center' });
    r.classList.add('flash');
  }, 350);
}
/* back to a credit row: the tab "Rules" opened, its choice focused */
function showCredit(page, key) {
  setTab(page, 'rules');
  setTimeout(() => {
    const s = $(`#work [data-credit="${key}"]`);
    if (!s) return;
    s.scrollIntoView({ behavior: 'smooth', block: 'center' });
    (s.querySelector('button.on') || s.querySelector('button')).focus({ preventScroll: true });
  }, 350);
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
/* the Sammy's categories, short (the rule's four amounts in one row) */
const sammyShort = (i) => [T('News|sammy'), T('Culture|sammy'), T('Shows|sammy'), T('Series|sammy')][i];
/* a selection of packs in a few words: "Original + New films" (a complete pack: its own title) */
function selTitle(spec) {
  const names = spec ? spec.split('+') : [];
  if (!names.length) return T('Original game');
  const titles = names.map(packTitle).join(' + ');
  return holdsOriginal(names) ? titles : T('Original game') + ' + ' + titles;
}

/* ---------------------------------------------------------------- the page "Play"
   On top the start: the safe of the office as the game draws it (room 58) -- its 10 tapes the
   saved games of this selection of packs (launcher/saves.go: each selection plays from game files
   of its own, so each has its own safe); a click picks one, a double click starts it -- beside it
   the picked tape and the two starts: on from it (--load-slot), or a new game.  Under it the
   messages, then the three tabs (gameTabs): the packs (and the safes of other selections), the
   rules, the fixes.  The saves are read again when the page comes back into view (a game played
   meanwhile may have saved). */
const SAFE_X = (col) => 2 * (0x40 + 0x76 * col), SAFE_Y = (row) => 2 * (0x11 * row + 8);
function loadSafe(spec, force) {
  if (!force && st.safeLoading === spec) return;
  st.safeLoading = spec;
  api('GET', '/api/saves' + (spec ? '?mod=' + encodeURIComponent(spec) : '')).then((d) => {
    if (st.safeLoading !== spec) return;
    const before = st.safe;
    st.safeLoading = null;
    if (before && JSON.stringify(before) === JSON.stringify(d)) return;   /* (nothing saved meanwhile: the page as it is -- a field being typed in keeps its text) */
    st.safe = d;
    const kept = d.slots[(st.launch.slot || 0) - 1];
    if (!before || before.spec !== d.spec || !(kept && kept.saved)) st.launch.slot = d.newest || 0;
    if (st.section === 'launcher' && layoutOf() === 'launcher') renderEntry();
  }).catch((e) => {
    if (st.safeLoading !== spec) return;
    st.safeLoading = null;
    st.safe = { spec, error: e.message, slots: [], safes: [], newest: 0 };
    if (st.section === 'launcher' && layoutOf() === 'launcher') renderEntry();
  });
}
window.addEventListener('focus', () => { if (st.safe && st.section === 'launcher' && !new URLSearchParams(location.search).has('selftest')) loadSafe(st.safe.spec, true); });
/* the tape picked: the safe's labels marked in place (a double click needs the same element twice),
   the column beside it drawn anew */
function pickTape(n) {
  st.launch.slot = n;
  for (const b of $$('#work .safepic .safetape')) {
    const on = Number(b.dataset.slot) === n;
    b.classList.toggle('on', on);
    b.setAttribute('aria-pressed', on ? 'true' : 'false');
  }
  const col = $('#work .tapecol');
  if (col) col.replaceWith(tapeCol());
}
function tapeCol() {
  const sf = st.safe && st.safe.spec === st.packs.join('+') ? st.safe : null, n = st.packs.length;
  const pick = st.launch.slot || 0, s = sf && sf.slots[pick - 1] && sf.slots[pick - 1].saved ? sf.slots[pick - 1] : null;
  const classic = sv('game', 'classic') === 'yes';
  const what = n ? TP(n, 'with the pack {list}', 'with the packs {list}', { list: st.packs.map(packTitle).join(' + ') }) : T('the original game');
  return h('div', { class: 'tapecol' },
    h('div', { class: 'tapehead' }, h('span', { class: 'overline' }, s ? T('Tape {n}', { n: s.slot }) : T('No tape picked')),
      h('span', { class: 'pill dark', style: s && sf.newest === s.slot ? null : 'visibility:hidden' }, T('saved last'))),
    h('div', { class: 'biglabel' + (s ? '' : ' blank') }, h('span', { 'data-gametext': '' }, s ? s.name : '')),
    h('dl', { class: 'facts' },
      h('dt', {}, T('Station')), h('dd', {}, s ? [h('span', { class: 'sdot s' + s.station }), STATIONS[s.station] || '?', s.manager ? h('span', { class: 'mgr', 'data-gametext': '' }, ' · ' + s.manager) : null] : '–'),
      h('dt', {}, T('Game day')), h('dd', {}, s ? T('Day {day}, {time}', { day: s.day, time: s.clock }) : '–'),
      h('dt', {}, T('Money')), h('dd', { class: 'mono' }, s ? money(s.money) : '–')),
    h('div', { class: 'tapego' },
      goButton({ cls: 'cont', icon: 'tape', label: T('Continue'), sub: s ? T('tape {n} · day {day}', { n: s.slot, day: s.day }) : T('pick a tape in the safe'), disabled: !s,
        attrs: { id: 'continue', 'data-launch': 'continue', 'data-tip': s ? T('Starts the game straight into this saved game.') : null }, onclick: () => s && launchGame(false, s.slot) }),
      goButton({ cls: 'red', label: classic ? T('New game, classic') : T('New game'), sub: what, attrs: { id: 'launch', 'data-tip': n ? T('The packs are built first; games saved while playing them stay in their own safe.') : T('The original game files in run/ (your saves stay there).') },
        onclick: () => launchGame() })));
}
function startBlock() {
  const spec = st.packs.join('+'), sf = st.safe && st.safe.spec === spec ? st.safe : null;
  if (!sf) loadSafe(spec);
  const slots = sf ? sf.slots : [], pick = st.launch.slot || 0, saved = slots.filter((s) => s.saved && s.name.trim()).length;   /* (the original's own blank-named save in slot 1 does not count) */
  const tapes = [];
  for (let i = 0; i < 10; i++) {
    const s = slots[i] || { slot: i + 1, name: '', saved: false };
    tapes.push(h('button', { class: 'safetape' + (s.slot === pick ? ' on' : ''), 'data-slot': String(s.slot), disabled: !s.saved,
      style: `left:${SAFE_X(i < 5 ? 0 : 1)}px;top:${SAFE_Y(i % 5 + 1)}px`, 'aria-pressed': s.slot === pick ? 'true' : 'false',
      'aria-label': s.saved ? T('Tape {n}: "{name}"', { n: s.slot, name: s.name.trim() || T('no name') }) : T('Tape {n} is empty', { n: s.slot }),
      onclick: () => pickTape(s.slot), ondblclick: () => { pickTape(s.slot); launchGame(false, s.slot); } },
    h('span', { class: 'tname', 'data-gametext': '' }, s.name)));
  }
  const hint = !sf ? T('Reading the saved games ...') : sf.error ? '' : saved
    ? TP(saved, 'one of 10 tapes saved', '{n} of 10 tapes saved') + ' · ' + T('a click picks one, a double click starts it')
    : T('no saved game yet: in the game, the safe in your office saves');
  return h('section', { class: 'startblock', 'aria-label': T('Start') },
    h('div', { class: 'safecol' },
      h('div', { class: 'steph' }, h('h2', {}, T('Safe')), h('span', { class: 'hint', 'data-safe': 'hint' }, hint)),
      h('div', { class: 'safebox' }, h('div', { class: 'safepic' }, h('img', { src: '/api/room/116.png', alt: T('The safe in the office, as in the game'), draggable: 'false' }), ...tapes)),
      sf && sf.error ? h('div', { class: 'hint err' }, sf.error) : null),
    tapeCol());
}
/* the panel "Packs": the packs played, and beside them the safes -- every selection with saved
   games of its own; "Switch" sets its packs, the safe above shows their saves */
function packsPanelPlay() {
  const n = st.packs.length, saves = !player(), spec = st.packs.join('+');
  const info = n ? h('div', { class: 'packinfo', 'data-launch': 'info' }) : null;
  if (info) fillPackInfo(info, packSpec());
  const sf = st.safe && st.safe.spec === spec ? st.safe : null;
  const here = sf ? sf.slots.filter((s) => s.saved && s.name.trim()) : [];
  const list = [{ spec, count: here.length, newest: sf && sf.newest ? sf.slots[sf.newest - 1].name : '' }].concat(((sf && sf.safes) || []).filter((s) => s.spec !== spec));
  const have = (s) => !s || s.split('+').every((x) => modInfo(x));
  return h('div', { class: 'gpanel packspanel' },
    h('div', { class: 'pcol' }, ...packRows(),
      h('div', { class: 'pbtns' },
        n ? h('button', { class: 'plate small', 'data-play': 'only-original', onclick: () => selectPacks([]) }, T('Only the original')) : null,
        saves ? h('button', { class: 'plate small', onclick: newModDialog }, icon('plus', 14), T('New pack')) : null),
      info),
    h('div', { class: 'scol' },
      h('div', { class: 'h3' }, T('Safes'), h('span', { class: 'hint' }, T('every selection of packs has its own'))),
      ...list.map((s) => h('div', { class: 'vault' + (s.spec === spec ? ' now' : ''), 'data-safe': s.spec || 'original' },
        h('span', { class: 'pt' }, h('b', {}, selTitle(s.spec)),
          h('span', {}, s.count ? TP(s.count, 'one saved game', '{n} saved games') + (s.newest ? ' · ' + T('last: "{name}"', { name: s.newest }) : '') : T('no saved game yet'))),
        s.spec === spec ? h('span', { class: 'pill yellow' }, T('shown above'))
          : h('button', { class: 'plate small', 'data-safeswitch': s.spec || 'original', disabled: !have(s.spec),
            'data-tip': have(s.spec) ? T('Sets these packs: the safe above shows their saved games.') : T('A pack of it is not here any more.'),
            onclick: () => selectPacks(s.spec ? s.spec.split('+') : []) }, T('Switch')))),
      h('div', { class: 'hint' }, T('A game saved with packs loads only with the same packs in the same order: each selection keeps its games in its own files (build/mods/).'))));
}
/* the rules of the single player's game: [[title, rows], ...] in two columns (rulesPanel) */
function playRules() {
  const { seg, yesno } = settingControls(false);
  const sch = (sec, key) => ((st.settings.schema || []).find((x) => x.section === sec && x.key === key) || {}).opts || [];
  const def = (sec, key) => sch(sec, key)[0];
  const optRow = (sec, key, label, help, opts, extra = {}) => {
    const d = String(def(sec, key)), v = String(sv(sec, key) ?? d);
    return Object.assign({ key: `${sec}.${key}`, label, help, ctl: opts ? seg(sec, key, opts) : yesno(sec, key), changed: v !== d,
      def: opts ? (opts.find((x) => x[0] === d) || [d, d])[1] : d === 'yes' ? T('Yes') : T('No'), rv: { [sec]: { [key]: d } } }, extra);
  };
  const num = (key) => Number(sv('game', key) ?? def('game', key));
  const amountRow = (key, label, help, ctl, extra = {}) => Object.assign({ key: 'game.' + key, label, help, ctl, changed: num(key) !== def('game', key),
    def: numFmt(def('game', key)), rv: { game: { [key]: String(def('game', key)) } } }, extra);
  const offersRow = (key, label, help, ctl) => amountRow(key, label, help, ctl, { def: String(def('game', key)) });
  const corig = sv('game', 'credit_line') === CREDIT_ORIGINAL, credit = corig ? 700000 : num('credit_line');
  const cp = creditParts({ orig: corig, cur: credit, set: (v) => setSetting('game', 'credit_line', String(v)), attr: 'data-setting', key: 'game.credit_line',
    note: T('As in 1991: the boss lends $ 200,000, haggled up to $ 300,000, from image 50 up to $ 600,000 -- without the staircase. The bug fix credit-repay is off with it, and locked.'),
    jump: () => showFix('launcher', 'credit-repay', (i) => { st.launch.fixGroup = i; }) });
  const sammy = SAMMY_KEYS.map((k) => num(k));
  const cols = [
    [[T('General'), [optRow('game', 'language', T('Language'), T('The game\'s language; "ask" shows the game\'s own prompt.'), [['english', T('English')], ['german', T('German')], ['ask', T('Ask')]])]],
      [T('Start'), [
        amountRow('start_money', T('Start money'), T('Your cash when a new game starts (also with Classic); a saved game keeps its own. The credit stays $ 200,000.'),
          startMoney(num('start_money'), (v) => setSetting('game', 'start_money', String(v))), { tag: T('new game only') }),
        { key: 'game.credit_line', label: T('Credit line'), help: creditHelp(), ctl: cp.ctl, note: cp.note, changed: corig || credit !== def('game', 'credit_line'),
          def: T('Staircase') + ' ' + numFmt(def('game', 'credit_line')), rv: { game: { credit_line: String(def('game', 'credit_line')) } } }]],
      [T('Pace'), [
        optRow('game', 'fourth_speed', T('Fourth speed'), T('A fourth button in the planner\'s Control-Center, twice as fast as the original\'s fastest. Chosen before the start (also with Classic); No: the original\'s three.')),
        optRow('game', 'fast_rooms', T('Fast room changes'), T('Into a room and out again at once: no wipe, no wait while the room loads -- as in a network game (also with Classic). The game\'s clock stands still in a room change either way; No: the original\'s pace.'))]],
      [T('Opponents'), [optRow('game', 'ai_improve', T('AI improvement'), T('Computer players build up their transmitter network when you build up yours -- a while later (also with Classic). Off: the original.'), [['off', T('Off')], ['normal', T('Normal')], ['strong', T('Strong')]])]]],
    [[T('Shops'), [
      offersRow('ad_offers', T('Ad offers'), adOffersHelp(), adOffers(num('ad_offers'), (v) => setSetting('game', 'ad_offers', String(v)))),
      offersRow('film_offers', T('Film offers'), filmOffersHelp(), filmOffers(num('film_offers'), (v) => setSetting('game', 'film_offers', String(v)))),
      optRow('display', 'cast_fit', T('Role fit in the shop'), castFitHelp()),
      optRow('game', 'series_topicality', T('Series topicality'), T('The series card shows how topical the series still is: a bar as on the film card, over all its episodes (also with Classic). No: the original\'s card.'))]],
    [T('Events and prizes'), [
      amountRow('terror_damage', T('Terror damage'), terrorHelp(), terrorDamage(num('terror_damage'), (v) => setSetting('game', 'terror_damage', String(v)))),
      { key: 'game.sammy', label: T('Sammy prizes'), help: T('Prize money for the winner of the Sammy, by its category, you or a computer player (also with Classic). The original pays none: the Sammy brings image only.'),
        ctl: h('div', { class: 'sammyctl' }, ...SAMMY_KEYS.flatMap((k, i) => [h('span', {}, sammyShort(i)), sammyPrize(i, sammy[i], (v) => setSetting('game', k, String(v)))])),
        changed: sammy.some((x, i) => x !== def('game', SAMMY_KEYS[i])), def: numFmt(0),
        rv: { game: Object.fromEntries(SAMMY_KEYS.map((k) => [k, String(def('game', k))])) } }]]]];
  for (const r of cols.flat().flatMap((g) => g[1])) r.reset = () => putSettings(r.rv);
  return cols;
}
/* several settings at once: { section: { key: value } } */
async function putSettings(values, mp) {
  const k = settingsKey(mp);
  try {
    st[k] = await api('PUT', SETTINGS_URL(mp), { values });
    status(T('Saved in {path}.', { path: st[k].path }), 'ok');
  } catch (e) { status(e.message, 'err'); }
  renderEntry();
}
const setSettings = (game, mp) => putSettings({ game }, mp);
/* all the rows' defaults in one (rulesPanel's "All to default") */
function rulesDefaults(cols) {
  const out = {};
  for (const r of cols.flat().flatMap((g) => g[1])) for (const [sec, kv] of Object.entries(r.rv || {})) out[sec] = Object.assign(out[sec] || {}, kv);
  return out;
}
/* the fixes' choice (fixesPanel): all, own, Classic -- the own choice kept for its way back */
function pickFixModePlay(m) {
  const fx = sv('game', 'fixes') || 'all';
  if (m === 'classic') return setSettings({ classic: 'yes' });
  if (m === 'all') {
    keepFixOwn('sp', false, fx !== 'all' && fx !== 'none' ? fx : undefined);
    return setSettings({ classic: 'no', fixes: 'all' });
  }
  const own = fx !== 'all' ? fx : fixOwn('sp').list || 'all';
  keepFixOwn('sp', true, own);
  return setSettings({ classic: 'no', fixes: own });
}
function renderLauncher(w) {
  if (!st.settings) {
    w.append(h('div', { class: 'hint' }, T('Reading the settings ...')));
    loadSettings().then(() => { if (st.section === 'launcher') renderEntry(); }).catch((e) => status(e.message, 'err'));
    return;
  }
  const L = st.launch, spec = st.packs.join('+');
  const classic = sv('game', 'classic') === 'yes', fx = sv('game', 'fixes') || 'all';
  const corig = sv('game', 'credit_line') === CREDIT_ORIGINAL;
  const mode = fixMode(classic, fx, 'sp');
  const forced = corig ? creditForced(() => showCredit('launcher', 'game.credit_line')) : null;
  const rules = playRules(), changed = rules.flat().flatMap((g) => g[1]).filter((r) => r.changed);
  const log = h('div', { id: 'playlog', class: 'log', role: 'log' });
  const strip = h('div', { class: 'logstrip' }, h('span', { class: 'overline' }, T('Messages')), h('div', { class: 'logcol' }, log),
    h('button', { class: 'link', 'data-launch': 'hidelog', onclick: () => { st.log = []; renderEntry(); } }, T('hide')));
  fillLog(log);
  const fixSum = fixesSummary(mode, fx, forced);
  const tabs = gameTabs({ page: 'launcher', heads: [
    { id: 'packs', label: T('Packs'), sub: selTitle(spec) },
    { id: 'rules', label: T('Rules'), sub: changed.length ? T('{n} changed: {list}', { n: changed.length, list: changed.map((r) => r.label).join(', ') }) : T('as by default'), chg: changed.length > 0 },
    { id: 'fixes', label: T('Fixes|tab'), sub: fixSum[0], chg: fixSum[1] }],
  panel: (id) => (id === 'packs' ? packsPanelPlay()
    : id === 'rules' ? rulesPanel({ cols: rules, only: L.onlyChanged, setOnly: (v) => { L.onlyChanged = v; renderEntry(); },
      resetAll: () => putSettings(rulesDefaults(rules)) })
      : fixesPanel({ page: 'launcher', mode, pick: pickFixModePlay, fx, forced, group: L.fixGroup, setGroup: (i) => { L.fixGroup = i; renderEntry(); },
        set: (v) => { keepFixOwn('sp', true, v); setSettings({ classic: 'no', fixes: v }); } })) });
  w.append(h('div', { class: 'page launchpage' },
    h('h1', { class: 'sr' }, T('Play')),
    st.dirty.size && st.mod && st.packs.includes(st.mod) ? h('div', { class: 'notice' }, h('span', {}, '●'), h('span', {}, T('"{title}" has unsaved changes: they are saved before the start.', { title: packTitle(st.mod) }))) : null,
    startBlock(),
    strip,
    tabs,
    h('div', { class: 'playfoot' },
      h('span', {}, T('Display, sound, mouse:') + ' ', h('button', { class: 'link', 'data-launch': 'tosettings', onclick: () => { st.section = 'settings'; renderAll(); } }, T('Settings'))),
      h('span', {}, T('The start as a command:') + ' ', h('code', { class: 'cmd', 'data-launch': 'cmd' }, `${playCmd()} ${spec ? '--mod ' + spec : ''}`.trim())))));
}
async function quitLauncher() {
  try { await api('POST', '/api/quit', {}); } catch (e) { status(e.message, 'err'); return; }
  playLog('The launcher has ended: close this page. OpenMTV.cmd or ./play.sh starts it again.', 'ok');
  status(T('The launcher has ended: close this page. OpenMTV.cmd or ./play.sh starts it again.'), 'ok');
}
/* the start: a new game (the setup), or SLOT's saved game of the safe (--load-slot); dry: only the command */
async function launchGame(dry, slot) {
  const spec = st.packs.join('+');
  if (spec && !player()) playLog('Save the pack ...', '', true); else playLog('Start ...', '', true);
  if (spec && !player() && !(await save())) { playLog('Not saved: {why}', 'err', false, { why: $('#status').textContent }); return null; }
  delete st.packInfo[spec];
  try {
    const r = await api('POST', '/api/launch', { mod: spec || null, dry: !!dry, slot: slot || null });     /* (Classic: in the settings file) */
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
  const until = async (f, ms = 5000) => { const end = Date.now() + ms; while (Date.now() < end) { if (f()) return true; await wait(100); } return false; };
  /* ---- the launcher: the settings (a test file of its own), the selection, the start (dry);
   * the player's kept selection is put back at the end */
  let keptBefore = null;
  try { keptBefore = localStorage.getItem(PACKS_KEY); } catch (e) { /* ignore */ }
  st.launch = { fixGroup: 0, slot: 0, onlyChanged: false };
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
  /* the page "Play": the start on top (the safe, the tape picked, the two starts), the three tabs shut */
  st.tabs = {};
  for (const k of Object.keys(tabsWas)) delete tabsWas[k];
  st.fixOwn = {};
  let keptTabsAll = null, keptOwn = null;
  try { keptTabsAll = localStorage.getItem(TABS_KEY); keptOwn = localStorage.getItem(FIXOWN_KEY); localStorage.removeItem(TABS_KEY); localStorage.removeItem(FIXOWN_KEY); } catch (e) { /* ignore */ }
  const tab = async (id) => { setTab('launcher', id); await wait(150); };
  await go2('#launcher');
  await wait(400);
  const s0 = await api('GET', SETTINGS_URL());
  ok(/selftest-settings\.ini$/.test(s0.path) && (s0.fixes || []).length >= 12 && !$('#work [data-setting="display.filter"]') && !$('#work #aicard'),
    `launcher: the test's settings file, ${(s0.fixes || []).length} fixes from the game (the display and the sound on the page "Settings")`);
  ok(!$('#inspector').children.length && $('#work .startblock #launch') && $('#work .startblock #continue') && $('#work .logstrip').classList.contains('empty')
    && $$('#work [data-tab]').length === 4 && $$('#work .gtab.on').length === 0 && $('#work [data-drawer="launcher"]').inert && !$('#work [data-fix]'),
    'launcher: no side column -- the start on top, no messages yet, the three tabs shut');
  /* the safe: the selection's own (launcher/saves.go), its 10 tapes drawn where the game draws them */
  await until(() => st.safe && st.safe.spec === st.packs.join('+'));
  const sf = st.safe, tapes = $$('#work .safepic .safetape');
  ok(sf && sf.slots.length === 10 && tapes.length === 10 && tapes.every((b, i) => b.disabled === !sf.slots[i].saved)
    && tapes[0].style.left === '128px' && tapes[0].style.top === '50px' && tapes[9].style.left === '364px' && tapes[9].style.top === '186px'
    && tapes.every((b) => Math.round(b.getBoundingClientRect().height) === 16 && Math.round(b.getBoundingClientRect().width) === 158),
    `launcher: the safe of "${sf && (sf.dir || '?')}": 10 tapes where the game draws them, the plate's size (158 x 16), ${sf ? sf.slots.filter((x) => x.saved).length : 0} saved`);
  const saved = sf ? sf.slots.find((x) => x.saved) : null;
  if (saved) {
    $(`#work .safepic .safetape[data-slot="${saved.slot}"]`).click();
    await wait(100);
    ok($(`#work .safepic .safetape[data-slot="${saved.slot}"]`).classList.contains('on') && !$('#work #continue').disabled
      && $('#work .tapecol').textContent.includes(T('Day {day}, {time}', { day: saved.day, time: saved.clock })), `launcher: tape ${saved.slot} picked -- its day and clock beside the safe, "Continue" ready`);
    const rs = await launchGame(true, saved.slot);
    ok(rs && rs.cmd.endsWith(`--load-slot ${saved.slot}`), 'launcher: "Continue", dry: the game straight into the tape (' + (rs && rs.cmd) + ')');
    st.log = [];
    renderEntry();
  } else ok($('#work #continue').disabled, 'launcher: no saved game in this safe: "Continue" off');
  /* the tabs: a head opens its panel, again shuts it; the chevron opens the last one; kept */
  await tab('rules');
  ok($('#work [data-tab="rules"]').getAttribute('aria-expanded') === 'true' && $('#work .gtab.on[data-tab="rules"]') && !$('#work [data-drawer="launcher"]').inert
    && $$('#work [data-setting]').length === 15, 'launcher: the tab "Rules" open -- fifteen game settings in their groups');
  $('#work [data-tab="rules"]').click();
  await wait(150);
  ok($('#work [data-tab="rules"]').getAttribute('aria-expanded') === 'false' && $('#work [data-drawer="launcher"]').inert && tabsOf('launcher').last === 'rules', 'launcher: a click on the open head shuts it');
  $('#work [data-tab="fold"]').click();
  await wait(150);
  let kt = null;
  try { kt = JSON.parse(localStorage.getItem(TABS_KEY) || '{}').launcher; } catch (e) { /* ignore */ }
  ok($('#work [data-tab="rules"]').getAttribute('aria-expanded') === 'true' && (!kt || kt.open === 'rules'), 'launcher: the chevron opens the last tab again, kept in this browser');
  /* the fixes: all of them, an own choice (kept), Classic 1991 -- a switch like every setting */
  await tab('fixes');
  const fm = (v) => $(`#work [data-fixmode] [data-value="${v}"]`);
  ok($$('#work [data-fixgroup]').length >= 3 && $('#work [data-fix="intruder"]') && fm('all').classList.contains('on') && $('#work [data-tabsum="fixes"]').textContent === T('all {n} on', { n: s0.fixes.length }),
    'launcher: the tab "Fixes": all on, in groups; its head says so');
  fm('own').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.fixes === 'all' && fm('own').classList.contains('on'), 'launcher: "Own choice": still all on, the choice kept here');
  $('#work [data-fix="random0"]').click();
  await wait(400);
  const s2 = await api('GET', SETTINGS_URL());
  ok(!s2.values.game.fixes.split(',').includes('random0') && s2.values.game.fixes.split(',').includes('intruder') && $('#work [data-tabsum="fixes"]').classList.contains('chg'),
    'launcher: one fix left out, the head in yellow (' + s2.values.game.fixes + ')');
  fm('all').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.fixes === 'all' && $('#work [data-fix="random0"]').getAttribute('aria-checked') === 'true', 'launcher: "All fixes": all on again');
  fm('own').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.fixes === s2.values.game.fixes && $('#work [data-fix="random0"]').getAttribute('aria-checked') === 'false', 'launcher: "Own choice" brings the own choice back');
  /* Classic: the original's game, no fixes -- the choice of fixes kept for later */
  ok(s0.values.game.classic === 'no', 'launcher: Classic off by default');
  fm('classic').click();
  await wait(400);
  const s5 = await api('GET', SETTINGS_URL());
  ok(s5.values.game.classic === 'yes' && s5.values.game.fixes === s2.values.game.fixes && $('#work [data-fixes="classicnote"]').classList.contains('open') && $('#work [data-fix="intruder"]').disabled
    && $('#work #launch').textContent.includes(T('New game, classic')) && $('#work [data-tabsum="fixes"]').textContent === T('Classic 1991: none on'),
  'launcher: Classic 1991 -- written, the fixes locked and their choice kept, the note open, the button says so');
  fm('own').click();
  await wait(400);
  const s5b = (await api('GET', SETTINGS_URL())).values.game;
  ok(s5b.classic === 'no' && s5b.fixes === s2.values.game.fixes && !$('#work [data-fix="random0"]').disabled, 'launcher: Classic off again: the own choice as it was');
  /* the rules: the start money -- changed, marked, back to the default */
  await tab('rules');
  const mi = () => $('#work [data-setting="game.start_money"]'), mreset = () => $('#work [data-reset="game.start_money"]'), mrow = () => $('#work [data-rule="game.start_money"]');
  ok(mi().value === numFmt(200000) && mreset().disabled && !mrow().classList.contains('chg'), 'launcher: start money at the default, nothing to reset');
  mi().value = numFmt(1500000);
  mi().dispatchEvent(new Event('change'));
  await wait(400);
  const s4 = await api('GET', SETTINGS_URL());
  ok(s4.values.game.start_money === '1500000' && mi().value === numFmt(1500000) && mrow().classList.contains('chg') && !mreset().disabled && mreset().textContent.includes(numFmt(200000))
    && $('#work [data-tabsum="rules"]').textContent.includes(T('Start money')), 'launcher: start money ' + numFmt(1500000) + ' written, marked, the default beside it, the head names it');
  mi().value = '200000000';
  mi().dispatchEvent(new Event('change'));
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.game.start_money === '1500000' && mi().value === numFmt(1500000), 'launcher: start money above $100 million refused');
  $('#work [data-rules="changed"]').click();
  await wait(150);
  ok(!mrow().classList.contains('hide') && $('#work [data-rule="game.fourth_speed"]').classList.contains('hide'), 'launcher: "Only the changed" -- the start money, not the fourth speed');
  $('#work [data-rules="all"]').click();
  await wait(150);
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
  ok($('#work [data-tabsum="fixes"]').textContent === T('{n} of {total} on', { n: s0.fixes.length - 2, total: s0.fixes.length }),
    'launcher: the head of the fixes counts credit-repay off too (' + $('#work [data-tabsum="fixes"]').textContent + ')');
  $('#work [data-credit="tofix"]').click();
  await wait(500);
  const held = () => $('#work [data-fix="credit-repay"]');
  ok($('#work [data-tab="fixes"]').getAttribute('aria-expanded') === 'true' && held() && held().dataset.held === 'credit' && held().getAttribute('aria-checked') === 'false'
    && held().classList.contains('lk') && $('#work [data-fixgroup].fg.on').textContent.includes(T('Rules')), 'launcher: "to the fix" opens the tab "Fixes" at credit-repay: off, locked');
  $('#work [data-fixgroupset="none"]').click();
  await wait(400);
  const s7 = (await api('GET', SETTINGS_URL())).values.game.fixes.split(',');
  ok(s7.includes('credit-repay') && !s7.includes('tx-price'), 'launcher: the group\'s "none" leaves the held credit-repay as chosen');
  $('#work [data-fixgroupset="all"]').click();
  await wait(400);
  $('#work [data-fixes="back"]').click();
  await wait(500);
  ok($('#work [data-tab="rules"]').getAttribute('aria-expanded') === 'true' && document.activeElement === cseg('original'), 'launcher: "change" goes back to the credit line, in the tab "Rules"');
  cseg('line').click();
  await wait(700);
  const s8 = await api('GET', SETTINGS_URL());
  ok(s8.values.game.credit_line === '1000000' && ci().value === numFmt(1000000) && !ci().disabled && cx() === cx0, 'launcher: back to the staircase: the line as before, in its place');
  await tab('fixes');
  ok(held().getAttribute('aria-checked') === 'true' && s8.values.game.fixes === s2.values.game.fixes, 'launcher: credit-repay on again as chosen');
  await tab('rules');
  ok(!cnote().classList.contains('open'), 'launcher: the note under the credit line shut again');
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
  ok(SAMMY_KEYS.every((k) => s0.values.game[k] === '0' && $(`#work [data-setting="game.${k}"]`).value === numFmt(0)), 'launcher: no Sammy prize money by default (four categories in one row)');
  const sci = () => $('#work [data-setting="game.sammy_culture"]');
  sci().value = numFmt(250000);
  sci().dispatchEvent(new Event('change'));
  await wait(400);
  const sm = await api('GET', SETTINGS_URL());
  ok(sm.values.game.sammy_culture === '250000' && sm.values.game.sammy_news === '0' && sci().value === numFmt(250000) && !$('#work [data-reset="game.sammy"]').disabled,
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
  /* the role fit in the shop: a rule of the game's start, off by default */
  ok(s0.values.display.cast_fit === 'no' && $('#work [data-setting="display.cast_fit"] [data-value="no"]').classList.contains('on'), 'launcher: the role fit in the shop off by default');
  $('#work [data-setting="display.cast_fit"] [data-value="yes"]').click();
  await wait(400);
  ok((await api('GET', SETTINGS_URL())).values.display.cast_fit === 'yes' && $('#work [data-setting="display.cast_fit"] [data-value="yes"]').classList.contains('on'), 'launcher: the role fit in the shop switched on');
  const nchg = $$('#work .rrow.chg').length;
  $('#work [data-rules="reset"]').click();
  await wait(500);
  const sr = await api('GET', SETTINGS_URL());
  ok(nchg === 8 && !$$('#work .rrow.chg').length && sr.values.game.film_offers === '10' && sr.values.display.cast_fit === 'no' && sr.values.game.sammy_culture === '0' && sr.values.game.fixes === s2.values.game.fixes,
    `launcher: "All to default" -- the ${nchg} changed rules back, the fixes untouched`);
  await api('PUT', SETTINGS_URL(), { values: { display: { scale: '3', cast_fit: 'no' }, game: { classic: 'no', fixes: 'all', start_money: '200000', credit_line: '700000', terror_damage: '50000',
    sammy_news: '0', sammy_culture: '0', sammy_shows: '0', sammy_series: '0', fourth_speed: 'no', fast_rooms: 'no', series_topicality: 'yes', ai_improve: 'off', ad_offers: '10', film_offers: '10' } } });
  await loadSettings(true);
  /* the selection: the self-test's pack alone; the original stays its base */
  await tab('packs');
  ok(same(st.packs, ['selftest']) && $('#work [data-play="original"]').getAttribute('aria-pressed') === 'true' && $('#work [data-play="selftest"]').classList.contains('on')
    && $('#work [data-tabsum="packs"]').textContent === selTitle('selftest'), 'launcher: the self-test pack on the original (the original the base); the head says so');
  ok($('#work [data-launch="cmd"]').textContent === `${playCmd()} --mod selftest` && !$('#work #buildonly') && $('#work [data-safe="selftest"].now'),
    'launcher: the command (' + $('#work [data-launch="cmd"]').textContent + '), the start alone (no "Build only"); its safe among the safes');
  const r = await launchGame(true);
  ok(r && r.cmd === `${playCmd()} --mod selftest` && !r.pid && $('#playlog').textContent.includes(T('Would start: {cmd}', { cmd: r.cmd })) && !$('#work .logstrip').classList.contains('empty'),
    'launcher: a new game, dry (nothing started); the messages under the start');
  const keptLog = st.log;
  $('#work [data-launch="hidelog"]').click();
  await wait(100);
  ok($('#work .logstrip').classList.contains('empty'), 'launcher: the messages hidden again');
  st.log = keptLog;
  renderEntry();
  ok(st.meta.window ? !$('#rail #quitlauncher') : $('#rail #quitlauncher') && $('#rail #quitlauncher').textContent === T('Quit the launcher'),
    st.meta.window ? 'launcher: no button to end it in its own window (closing the window does)' : 'launcher: a button at the foot of the board ends it (not pressed here)');
  /* several packs: in the order switched on, moved by the grip (the keyboard), the report, the start (dry) */
  const other = st.mods.find((m) => m.name !== 'selftest' && !m.complete && !m.error);
  if (other) {
    $(`#work [data-play="${other.name}"]`).click();
    await wait(900);
    const both = `selftest+${other.name}`;
    ok(same(st.packs, ['selftest', other.name]) && !st.mod && $('#work [data-launch="cmd"]').textContent === `${playCmd()} --mod ${both}`
      && $$('#work .packline').length === 2, `launcher: two packs together (${$('#work [data-launch="cmd"]').textContent}), nothing to save into until a change`);
    $(`#work [data-move="${other.name}"]`).dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
    await wait(900);
    const r2 = await launchGame(true);
    ok(r2 && r2.cmd === `${playCmd()} --mod ${other.name}+selftest` && $('#work [data-packrow="selftest"] .no').textContent === '3' && document.activeElement === $(`#work [data-move="${other.name}"]`),
      'launcher: moved up by the arrow key (its grip keeps the focus), the start of both, dry (' + (r2 && r2.cmd) + ')');
    ok(keptPacks().join('+') === `${other.name}+selftest`, 'launcher: the packs and their order kept for the next visit');
    $('#work [data-play="only-original"]').click();
    await wait(600);
    ok($('#work [data-launch="cmd"]').textContent === playCmd() && !keptPacks().length && $('#work [data-play="original"]').getAttribute('aria-pressed') === 'true', 'launcher: back to the original game alone, kept');
  } else ok(true, 'launcher: several packs not checked (only the self-test pack is here)');
  await selectPacks(['selftest']);
  try {
    if (keptBefore === null) localStorage.removeItem(PACKS_KEY); else localStorage.setItem(PACKS_KEY, keptBefore);
    if (keptTabsAll === null) localStorage.removeItem(TABS_KEY); else localStorage.setItem(TABS_KEY, keptTabsAll);
    if (keptOwn === null) localStorage.removeItem(FIXOWN_KEY); else localStorage.setItem(FIXOWN_KEY, keptOwn);
  } catch (e) { /* ignore */ }
  st.tabs = {};
  st.fixOwn = {};
  st.launch = { fixGroup: 0, slot: 0, onlyChanged: false };
}, { player: true }));      /* (the player's launcher runs it too: selftest.js playerSelftest) */
