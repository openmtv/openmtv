/* OpenMTV editor -- the sections as schemas.  The layout (editor.js) never knows what a film
 * is: a section brings
 *   list:    spine colour, badge, filter chips          (the list column)
 *   groups:  field groups -> the form, its tabs, the per-field original value and reset,
 *            the "changed against the original" list    (the form column)
 *   columns: the table mode                             (modes.js, Mischpult)
 *   card:    the game card's rows, cardEdit: which fields a card row opens (in-game mode)
 *   panels:  the inspector's panels; checks: the entry's own checks
 * A field: { key, label, help ('kind:key' of catalog.py or a text), type, get(r), set(r, v)
 *   (default r[key]), opts, min, max, req (required key, game.js REQUIRED), note(r), fmt(v),
 *   show(r), rerender (form again after a change), render(r, ctx, done) for type 'custom' }.
 * Types: select, number, money, bar, year, choice, toggles, info, custom.
 * A texts group: { texts: [{ i, label, rows, req, check(v, lang) }] } -- both languages side
 * by side.  New areas need a schema here, not a new layout.
 * Labels, titles, helps, notes are T() texts (i18n.js): defineSections() runs again after a
 * switch of the interface language. */
'use strict';

const LANGS = ['en', 'de'];
const LANG_NAME = {};
const filmGenres = () => st.meta.film_genres[st.lang].map((n, i) => [i + 1, n]);
const seriesGenres = () => st.meta.series_genres[st.lang].map((n, i) => [i + 1, n]);
const byte = (off, base = 0x13, len = 4) => ({ get: (r) => unkGet(r, off), set: (r, v) => unkSet(r, base, len, off, v) });
const bit = (key, b) => ({ get: (r) => (r[key] >> b) & 1, set: (r, v) => { r[key] = v ? r[key] | (1 << b) : r[key] & ~(1 << b); } });

/* text checks: the card's width and lines, measured against the game */
const widthCheck = (v) => (textW(v) > TEXT_W ? { level: 'warn', msg: T('{w} of {max} px: wider than the card, the game cuts it off', { w: textW(v), max: TEXT_W }) }
  : { level: 'ok', msg: T('{w} of {max} px, fits the card', { w: textW(v), max: TEXT_W }) });
const linesCheck = (n) => (v) => { const k = v ? wrap(v, TEXT_W).length : 0; return k > n ? { level: 'warn', msg: T('{k} lines: the card shows {n}, the rest is cut off', { k, n }) } : { level: 'ok', msg: T('{k} of {n} card lines', { k, n }) }; };
const linesInfo = (v) => ({ level: 'ok', msg: TP(v ? wrap(v, TEXT_W).length : 0, '{n} card line', '{n} card lines') });

/* The texts below are in the interface language: the sections are defined again after a switch
   (onUiLang); what holds them by key (st.sel, st.filter, st.tables) stays. */
/* the market: one choice for the two flag bits (5 on the market, 6 through the auction) */
const MARKET = [];
const NEWS_CATEGORY = [];
const SECTIONS = [];
const marketField = (kind) => ({ key: 'offered', label: T('Offered'), help: kind + ':flags', type: 'choice', opts: MARKET,
  get: (r) => (r.flags & 0x20 ? r.flags & 0x60 : 0), set: (r, v) => { r.flags = (r.flags & ~0x60) | v; }, guard: 'market' });
const startField = () => ({ key: 'start', label: T('At the start'), type: 'info', help: T("Who holds it when a game starts. A start library is stored in the start savegame together with the stations' schedules, so it is shown here, not changed."),
  text: (r) => (r.owner >= 1 && r.owner <= 3 ? T("in {station}'s library", { station: STATIONS[r.owner] }) : T('on the market (no station holds it)')) });
const poolField = (table) => ({ key: 'pool', label: T('Starting pool'), type: 'custom', help: T('Mark films and series a new game may give to the stations, instead of the original pools. Needs the decompiled build (play.sh); the original machine code (--lifted) keeps the original pools.'),
  get: (r) => (r.flags2 & POOL ? 1 : 0), set: (r, v) => { r.flags2 = v ? r.flags2 | POOL : r.flags2 & ~POOL; }, fmt: (v) => (v ? T('in the pool') : T('not in it')),
  render: (r, ctx, done) => poolControl(table, r, done) });
const monitorGroup = (base) => ({ key: 'monitor', title: T('Monitor'), tabOnly: true, render: (r, ctx, done) => monitorView(r, base, done),
  fields: [{ key: 'jingle', label: T('TV picture'), type: 'custom', fmt: (v) => (v ? T('picture {n}', { n: v }) : T('genre picture')) }] });
/* the TV family's members by flag bit (tv_family_update 0054:39CF, game.js familyScene) */
const familyNames = () => [T('Father'), T('Grandma'), T('Grandpa'), T('Child'), T('Son')];
const familyBits = () => familyNames().map((n, b) => [b, `${b + 1} ${n}`]);
const familyField = (table) => ({ key: 'family', label: T('TV family watches'), help: T('Who of the TV family in the office (bottom right) watches it: bits 0-4 of its flags. Three sit on the sofa at most, the first in the game\'s order (grandma, father, grandpa, son, child); the child goes to bed at 23:00. Their faces follow the programme: its topicality and genre.'), type: 'toggles', bits: familyBits(),
  get: (r) => r.flags & 0x1F, set: (r, v) => { r.flags = (r.flags & ~0x1F) | v; }, note: (r) => T('{n} of 5', { n: [0, 1, 2, 3, 4].filter((b) => r.flags & (1 << b)).length }), extra: (r) => familyPicture(table, r) });
const offeredFmt = (v) => ({ 0: '-', 32: T('dealer'), 96: T('auction') })[v] ?? v;
/* a screenplay's role and list as one value each (the changes against the original); the form
   edits them in rolesTable and listsView (editor.js) */
const spRoleField = (t) => ({ key: 'role' + t, label: T('Role {n}', { n: t }), type: 'custom',
  get: (r) => [unkGet(r, SP_CAT(t))].concat([0, 1, 2, 3].map((i) => unkGet(r, SP_REQ(t) + i))),
  set: (r, v) => { spSet(r, SP_CAT(t), v[0]); v.slice(1).forEach((x, i) => spSet(r, SP_REQ(t) + i, x)); spTidy(r); },
  fmt: (v) => (v.some((x) => x) ? T('{category}: humor {h}, appearance {a}, skill {s}', { category: actorCat(v[0]), h: v[1], a: v[3], s: v[4] }) : '-') });
const spListField = (e) => ({ key: 'ep' + e, label: (r) => (spIsSeries(r) ? T('Episode {e}: shopping list', { e }) : e === 1 ? T('Shopping list') : T('List {e}', { e })), type: 'custom',
  get: (r) => [...Array(0x23).keys()].map((i) => unkGet(r, SP_EP(e) + i)),
  set: (r, v) => { v.forEach((x, i) => spSet(r, SP_EP(e) + i, x)); spTidy(r); },
  fmt: (v, r) => {
    const items = [];
    for (let k = 1; k <= SP_ITEMS && v[1 + 3 * k]; k++) items.push(`${spItemName(r, v[1 + 3 * k])} x${v[2 + 3 * k]}`);
    return v[0] || items.length ? T('quality {q}: {items}', { q: v[0], items: items.join(', ') || '-' }) : '-';
  } });

function defineSections() {
  return [
  { key: 'films', table: 'films', plate: T('Movie agency'), room: 106, label: T('Films'), what: T('film'), many: T('films'), newOne: (id) => T('New film #{id}', { id }), kind: 'film', icon: 'film',
    spine: (r) => GENRE_COLORS[r.genre] || '#888', genres: filmGenres, tvBase: FILM_PIC,
    card: filmRows, panels: ['card', 'audience', 'tv', 'checks', 'changes', 'used'],
    blank: (id) => ({ id, genre: 1, quality: 50, wear_rate: 3, topicality: 50, blocks: 2, owner: 0, flags: 0x20 | 0x07, flags2: 0,
      price: 0, jingle: 0, live_day: 0, live_slot: 0, unknown: { '0x13': '005b0505' }, text: { en: ['', ''], de: ['', ''] } }),
    groups: [
      { key: 'texts', title: T('Texts'), texts: [
        { i: 0, label: T('Title'), req: 'title', check: widthCheck },
        { i: 1, label: T('Description'), rows: 3, req: 'desc', check: linesCheck(FILM_LINES) }] },
      { key: 'programme', title: T('Programme'), fields: [
        { key: 'genre', label: T('Genre'), help: 'film:genre', type: 'select', opts: filmGenres, req: 'genre', rerender: true },
        { key: 'blocks', label: T('Length'), help: 'film:blocks', type: 'number', min: 1, max: 7, req: 'blocks', note: () => T('hours (1-7, ends by 1:00)') },
        { key: 'quality', label: T('Quality'), help: 'film:quality', type: 'bar', max: 100, req: 'quality' },
        { key: 'topicality', label: T('Topicality'), help: 'film:topicality', type: 'bar', max: 100, req: 'topicality' },
        { key: 'wear_rate', label: T('Wear per airing'), help: 'film:wear_rate', type: 'number', min: 0, max: 100,
          note: (r) => [T('-> each airing costs'), h('span', { class: 'value' }, T('-{n} topicality', { n: tdiv(i8(r.quality) * i8(r.wear_rate), 5) }))] },
        { key: 'price', label: T('Base price'), help: 'film:price', type: 'money', req: 'price',
          note: (r) => [T('-> the dealer asks'), h('span', { class: 'value', 'data-tip': T('movie_price (1DA8:3F5B): half the price fixed, half scaled by topicality / quality, rounded to $1 000.') }, money(moviePrice(r.price, r.quality, r.topicality)))] }] },
      { key: 'card', title: T('Card'), fields: [
        Object.assign({ key: 'c13', label: T('Country'), help: 'film:c13', type: 'select', opts: () => st.meta.countries[st.lang].map((n, i) => [i, n]), show: (r) => r.genre !== 8 }, byte(0x13)),
        Object.assign({ key: 'year', label: T('Year'), help: T('Production year on the card, 1900-2099 (byte +0x14 counts from 1900; the game prints 20nn from 100 on).'), type: 'number', min: 1900, max: 2099, show: (r) => r.genre !== 8 },
          { get: (r) => 1900 + unkGet(r, 0x14), set: (r, v) => unkSet(r, 0x13, 4, 0x14, v - 1900) }),
        Object.assign({ key: 'betty', label: T('Betty'), help: T('Shows: Betty bar 0-10 (byte +0x13).'), type: 'bar', max: 10, show: (r) => r.genre === 8 }, byte(0x13)),
        Object.assign({ key: 'critics8', label: T('Critics'), help: T('Shows: critics bar 0-10 (byte +0x14).'), type: 'bar', max: 10, show: (r) => r.genre === 8 }, byte(0x14)),
        Object.assign({ key: 'c15', label: (r) => (r.genre === CULTURE ? T('Betty') : T('Critics')), help: 'film:c15', type: 'bar', max: 10, show: (r) => r.genre !== 8 }, byte(0x15)),
        Object.assign({ key: 'c16', label: (r) => (r.genre === CULTURE ? T('Critics') : T('Box office')), help: 'film:c16', type: 'bar', max: 10, show: (r) => r.genre !== 8 }, byte(0x16)),
        { key: 'rating', label: T('Rating'), help: 'film:flags2', type: 'toggles', bits: [[3, T('X rated'), T('The moral guardian comes before 21:00')], [2, T('One airing only')]], get: (r) => r.flags2 & 0x0C, set: (r, v) => { r.flags2 = (r.flags2 & ~0x0C) | v; } },
        { key: 'family', label: T('TV family watches'), help: 'film:flags', type: 'toggles', bits: familyBits(), extra: (r) => familyPicture('films', r),
          get: (r) => r.flags & 0x1F, set: (r, v) => { r.flags = (r.flags & ~0x1F) | v; }, note: (r) => T('{n} of 5', { n: [0, 1, 2, 3, 4].filter((b) => r.flags & (1 << b)).length }) }] },
      { key: 'market', title: T('Market'), fields: [
        marketField('film'), startField(),
        { key: 'live_day', label: T('Live day'), help: 'film:live_day', type: 'number', min: 0, max: 999, show: (r) => r.genre === 7 },
        { key: 'live_slot', label: T('Live hour'), help: 'film:live_slot', type: 'select', opts: [0, 1, 2, 3, 4, 5, 6, 7].map((s) => [s, s ? `${s + 17}:00` : '-']), show: (r) => r.genre === 7 },
        poolField('films')] },
      monitorGroup(FILM_PIC)],
    cardEdit: { genre: ['genre', 'rating'], origin: ['c13', 'year'], c15: ['c15'], c16: ['c16'], c13: ['betty'], c14: ['critics8'], live: ['live_day', 'live_slot'],
      price: ['blocks', 'price'], topicality: ['quality', 'topicality', 'wear_rate'], rating: ['rating'] },
    offCard: ['family', 'offered', 'start', 'pool'],
    columns: [
      { key: 'title', label: T('Title'), text: true },
      { key: 'genre', label: T('Genre'), field: 'genre' },
      { key: 'blocks', label: T('h'), field: 'blocks' },
      { key: 'quality', label: T('Quality'), field: 'quality' },
      { key: 'topicality', label: T('Top.'), field: 'topicality', plain: true },
      { key: 'solo', label: T('Audience 18-24'), tip: T('Share by hour, 18:00 to 24:00, alone on air: its topicality and the audience of its genre at that hour; no rival, no news lead-in, no trend (broadcast_phase_eval 24B8:0C4A). Hatched: X rated before 21:00, when the moral guardian comes. Sorted by the best hour.'),
        get: (r) => Math.max(...filmSolo(r)), render: (r) => soloStrip(filmSolo(r), r.flags2 & 8), num: true },
      { key: 'price', label: T('Base price'), field: 'price' },
      { key: 'asks', label: T('Dealer asks'), get: (r) => moviePrice(r.price, r.quality, r.topicality), fmt: numFmt, num: true },
      { key: 'ppq', label: T('$/Q'), tip: T('Base price per quality point'), get: (r) => (r.quality ? Math.round(r.price / r.quality) : 0), fmt: numFmt },
      { key: 'offered', label: T('Offered'), field: 'offered', fmt: offeredFmt },
      { key: 'pool', label: T('Pool'), get: (r) => (r.flags2 & POOL ? 1 : 0), fmt: (v) => (v ? T('pool') : '') },
      { key: 'x', label: T('X|column'), tip: T('X rated'), get: (r) => (r.flags2 & 8 ? 1 : 0), fmt: (v) => (v ? T('X|cell') : '') },
      { key: 'wear', label: T('Wear'), field: 'wear_rate' }],
    presets: [
      { label: T('Price check: $ per quality'), sort: ['ppq', -1], q: '' },
      { label: T('Start pool candidates'), q: 'free', test: (r) => !poolBlock('films', r) },
      { label: T('Missing German texts'), test: (r) => !txt(r, 0, 'de').trim() || !txt(r, 1, 'de').trim() }],
  },
  { key: 'series', table: 'series', plate: T('Series archive'), room: 101, label: T('Series'), what: T('series'), many: T('series|many'), newOne: (id) => T('New series #{id}', { id }), kind: 'series', icon: 'series',
    spine: (r) => SERIES_COLORS[r.genre] || '#888', genres: seriesGenres, tvBase: SERIES_PIC,
    card: (r, lang) => seriesRows(r, lang), panels: ['card', 'audience', 'tv', 'checks', 'changes', 'used'],
    blank: (id) => ({ id, episodes_qtf: [0, 0, 0, 50, 50, 1].concat(Array(27).fill(0)), genre: 1, episodes: 1, blocks: 1, owner: 0, flags: 0x20 | 0x07, flags2: 0,
      price: 0, episode_price: 0, jingle: 0, next_episode: 0, wear_rate: 10, critics: 5, press: 5, text: { en: Array(22).fill(''), de: Array(22).fill('') } }),
    groups: [
      { key: 'texts', title: T('Texts'), texts: [
        { i: 0, label: T('Title'), req: 'title', check: widthCheck },
        { i: 1, label: T('Description'), rows: 3, req: 'desc', check: linesCheck(SERIES_LINES) }] },
      { key: 'series', title: T('Series|group'), fields: [
        { key: 'genre', label: T('Genre'), help: 'series:genre', type: 'select', opts: seriesGenres, req: 'genre', rerender: true },
        { key: 'episodes', label: T('Episodes'), help: 'series:episodes', type: 'number', min: 1, max: 10, req: 'episodes' },
        { key: 'blocks', label: T('Episode length'), help: 'series:blocks', type: 'number', min: 1, max: 7, req: 'blocks', note: () => T('hours') },
        { key: 'episode_price', label: T('Episode price'), help: 'series:episode_price', type: 'money', req: 'episode_price',
          note: (r) => [T('-> the dealer asks'), h('span', { class: 'value', 'data-tip': T('series_price (1DA8:4035): produced episodes x episode price, half fixed, half scaled by topicality / quality.') }, money(seriesPrice(r.episodes_qtf, r.episode_price)))] },
        { key: 'wear_rate', label: T('Wear rate'), help: 'series:wear_rate', type: 'number', min: 0, max: 100 },
        { key: 'critics', label: T('Critics'), help: 'series:critics', type: 'bar', max: 10 },
        { key: 'press', label: T('Press'), help: 'series:press', type: 'bar', max: 10 }] },
      { key: 'episodes', title: (r) => T('Episodes {n}/10', { n: producedEpisodes(r) }), render: (r, ctx, done) => episodesTable(r, ctx, done),
        fields: [{ key: 'episodes_qtf', label: T('Episode values'), type: 'custom', fmt: (v) => [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].filter((e) => v[e * 3 + 2] & 1).map((e) => `${e}: ${v[e * 3]}/${v[e * 3 + 1]}`).join(' ') }] },
      { key: 'market', title: T('Market'), fields: [marketField('series'), startField(), poolField('series'), familyField('series')] },
      monitorGroup(SERIES_PIC)],
    cardEdit: { genre: ['genre'], critics: ['critics'], press: ['press'], price: ['episodes', 'blocks', 'episode_price'], episode: ['episodes_qtf'] },
    offCard: ['family', 'wear_rate', 'offered', 'start', 'pool'],
    columns: [
      { key: 'title', label: T('Title'), text: true },
      { key: 'genre', label: T('Genre'), field: 'genre' },
      { key: 'episodes', label: T('Ep.'), field: 'episodes' },
      { key: 'produced', label: T('Prod.'), get: (r) => producedEpisodes(r) },
      { key: 'blocks', label: T('h'), field: 'blocks' },
      { key: 'solo', label: T('Audience 18-24'), tip: T('Share by hour, 18:00 to 24:00, of the first episode alone on air: its topicality and the audience of the series genre at that hour; no rival, no news lead-in, no trend (broadcast_phase_eval 24B8:0C4A). Sorted by the best hour.'),
        get: (r) => Math.max(...seriesSolo(r)), render: (r) => soloStrip(seriesSolo(r), false), num: true },
      { key: 'episode_price', label: T('Episode price'), field: 'episode_price' },
      { key: 'asks', label: T('Dealer asks'), get: (r) => seriesPrice(r.episodes_qtf, r.episode_price), fmt: numFmt, num: true },
      { key: 'offered', label: T('Offered'), field: 'offered', fmt: offeredFmt },
      { key: 'pool', label: T('Pool'), get: (r) => (r.flags2 & POOL ? 1 : 0), fmt: (v) => (v ? T('pool') : '') },
      { key: 'critics', label: T('Critics'), field: 'critics' },
      { key: 'press', label: T('Press'), field: 'press' },
      { key: 'wear', label: T('Wear'), field: 'wear_rate' }],
    presets: [
      { label: T('Missing German texts'), test: (r) => !txt(r, 0, 'de').trim() || !txt(r, 1, 'de').trim() },
      { label: T('On the market'), test: (r) => !!(r.flags & 0x20) }],
  },
  { key: 'ads', table: 'ads', plate: T('Advertising agency'), room: 111, label: T('Ads'), what: T('ad'), many: T('ads'), newOne: (id) => T('New ad #{id}', { id }), kind: 'ad', icon: 'ad',
    spine: () => '#f0d020', card: adRows, panels: ['card', 'checks', 'changes', 'used'],
    blank: (id) => ({ id, days: 5, spots: 3, min_audience_class: 3, target_group: 0, genre_rule: [0, 0, 0], holder: 0, fee: 50000, penalty: 10000, text: { en: ['', ''], de: ['', ''] } }),
    groups: [
      { key: 'texts', title: T('Texts'), texts: [
        { i: 0, label: T('Advertiser'), req: 'title', check: widthCheck },
        { i: 1, label: T('Slogan'), rows: 3, req: 'desc', check: linesInfo }] },
      { key: 'contract', title: T('Contract'), fields: [
        { key: 'spots', label: T('Spots'), help: 'ad:spots', type: 'number', min: 1, max: 99, req: 'spots' },
        { key: 'days', label: T('Days'), help: 'ad:days', type: 'number', min: 1, max: 99, req: 'days' },
        { key: 'min_audience_class', label: T('Minimum audience'), help: 'ad:min_audience_class', type: 'select', req: 'min_audience_class', opts: () => st.meta.audience_classes.map((m, i) => [i + 1, T('{k}: {m} million viewers', { k: i + 1, m: numFmt(m) })]) },
        { key: 'fee', label: T('Bonus'), help: 'ad:fee', type: 'money', note: (r) => (r.fee ? '' : T('0: the agency never offers it')) },
        { key: 'penalty', label: T('Penalty'), help: 'ad:penalty', type: 'money' }] },
      { key: 'condition', title: T('Condition'), fields: [
        { key: 'genre_rule', label: T('Programme condition'), help: 'ad:genre_rule', type: 'custom', render: (r, ctx, done) => ruleControl(r, done),
          fmt: (v) => { const list = v.filter((c) => c).map((c) => Math.abs(c)).join(', '); return v.some((c) => c) ? (v.some((c) => c < 0) ? T('none of {list}', { list }) : T('one of {list}', { list })) : T('none'); } }] },
      { key: 'agency', title: T('Agency'), fields: [
        { key: 'holder', label: T('Holder'), help: 'ad:holder', type: 'select', opts: [[0, T('free')], [1, 'Mad-TV'], [2, 'Fun-TV'], [3, 'Sun-TV'], [4, T('on offer at the agency')]] }] }],
    cardEdit: { fee: ['fee'], penalty: ['penalty'], audience: ['min_audience_class'], spots: ['spots', 'days'], rule: ['genre_rule'] },
    offCard: ['holder'],
    columns: [
      { key: 'title', label: T('Advertiser'), text: true },
      { key: 'spots', label: T('Spots'), field: 'spots' },
      { key: 'days', label: T('Days'), field: 'days' },
      { key: 'aud', label: T('Audience'), field: 'min_audience_class', fmt: (v) => String(v) },
      { key: 'fee', label: T('Bonus'), field: 'fee' },
      { key: 'penalty', label: T('Penalty'), field: 'penalty' },
      { key: 'perspot', label: T('Per spot'), get: (r) => (r.spots ? Math.round(r.fee / r.spots) : 0), fmt: numFmt, num: true },
      { key: 'rule', label: T('Condition'), get: (r) => (r.genre_rule || []).filter((c) => c).length, fmt: (v, r) => adRows(r, st.lang).filter((x) => x.edit === 'rule').map((x) => x.text).join(' ') }],
    presets: [{ label: T('Best bonus per spot'), sort: ['perspot', -1] }, { label: T('Not offered (no bonus)'), test: (r) => !r.fee }],
  },
  { key: 'screenplays', table: 'screenplays', plate: T('Screenplay agency'), room: 103, label: T('Screenplays'), what: T('screenplay'), many: T('screenplays'), kind: 'screenplay', icon: 'script',
    spine: () => '#e8e8e8', panels: ['target', 'checks', 'changes', 'used'],
    groups: [
      { key: 'screenplay', title: T('Screenplay'), fields: [
        Object.assign({ key: 'offered', label: T('Offered'), help: T('The screenplay agency offers it (flag bit 1). The original never offers 25-33 and 37-50: nothing in the code sets the flag.'), type: 'toggles', bits: [[1, T('at the screenplay agency')]] },
          { get: (r) => r.flags & 2, set: (r, v) => { r.flags = (r.flags & ~2) | v; } },
          { note: (r) => (!(origById('screenplays', r.id).flags & 2) ? h('span', { class: 'warn' }, T('Never offered in the original game: its production was never played, so it may not work as intended.')) : '') }),
        { key: 'kind', label: T('Kind'), type: 'info', help: T('A film production or a series production (flags bit 0); a series production has a shopping list for each episode of its series.'), text: (r) => (spIsSeries(r) ? T('series production') : T('film production')) },
        { key: 'target', label: T('Becomes'), type: 'custom', help: T('The film (or series) record the production turns into; it stays off the market until produced.'), rerender: true, req: 'target', render: (r, ctx, done) => targetControl(r, done),
          fmt: (v) => '#' + v },
        { key: 'repeat', label: T('Productions'), help: T('Flag bit 2: the studio keeps the screenplay after a production -- up to 10 productions, all with the one shopping list, and each one airs only once. Every show of the original has it.'), type: 'toggles', bits: [[2, T('up to 10, each airs once')]], show: (r) => !spIsSeries(r),
          get: (r) => r.flags & 4, set: (r, v) => { r.flags = (r.flags & ~4) | v; }, rerender: true },
        { key: 'price', label: T('Price'), help: T('What the screenplay agency asks.'), type: 'money', req: 'price' },
        { key: 'duration_hours', label: T('Shooting time'), help: T('Hours of production in the studio, for each episode or production; 0 counts as 10 (studio_start_production 229E:0B98).'), type: 'number', min: 0, max: 999,
          note: (r) => (r.duration_hours ? T('hours') : T('hours -- 0: the game takes 10')) }] },
      { key: 'texts', title: T('Texts'), texts: [{ i: 0, label: T('Description'), rows: 3, req: 'desc' }].concat([...Array(SP_ROLES).keys()].map((k) => ({ i: k + 1, label: T('Role {n} name', { n: k + 1 }), hidden: true }))) },
      { key: 'roles', title: (r) => T('Roles {n}', { n: spRoleCount(r) }), render: (r, ctx, done) => rolesTable(r, ctx, done),
        reqKeys: [...Array(SP_ROLES).keys()].flatMap((k) => [`role${k + 1}-category`, `role${k + 1}-req`]),
        fields: [...Array(SP_ROLES).keys()].map((k) => spRoleField(k + 1)) },
      { key: 'lists', title: T('Shopping lists'), render: (r, ctx, done) => listsView(r, ctx, done),
        reqKeys: [...Array(10).keys()].flatMap((k) => [`ep${k + 1}-quality`, `ep${k + 1}-list`]).concat(['ep1-sets']),
        fields: [...Array(10).keys()].map((k) => spListField(k + 1)) }],
    columns: [
      { key: 'title', label: T('Becomes'), text: true },
      { key: 'kind', label: T('Kind'), get: (r) => (spIsSeries(r) ? 1 : 0), fmt: (v) => (v ? T('series') : T('film')) },
      { key: 'price', label: T('Price'), field: 'price' },
      { key: 'duration_hours', label: T('Hours'), field: 'duration_hours' },
      { key: 'offered', label: T('Offered'), get: (r) => (r.flags & 2 ? 1 : 0), fmt: (v) => (v ? T('agency') : '') }],
  },
  { key: 'news', table: 'news_nodes', plate: T('News studio'), room: 110, label: T('News stories'), what: T('news'), many: T('news stories'), kind: 'news', icon: 'news',
    spine: () => '#6dbade', panels: ['headline', 'story', 'checks', 'changes'],
    groups: [
      { key: 'headline', title: T('Headline'), texts: [{ i: null, label: T('Headline'), rows: 2 }],
        hint: T("#1 .. #5: a phrase from the topic of that part of the story (News topics); the preview takes each topic's first phrase.") },
      { key: 'story', title: T('Story'), fields: [
        { key: 'value', label: T('Value'), help: T('Base value of the news item; with the phrases it decides its price class.'), type: 'number', min: 0, max: 255 },
        { key: 'delay_hours', label: T('Next step after'), help: T('Hours until a following story can appear.'), type: 'number', min: 0, max: 255, note: () => T('hours') },
        { key: 'successors', label: T('Continues with'), help: T('Up to four steps that can follow this one, one of them at random; "end" ends the story (9999).'), type: 'custom',
          render: (r, ctx, done) => successorsControl(r, done), fmt: (v) => v.filter((x) => x).map((x) => (x === NEWS_END ? T('end') : '#' + x)).join(' ') || T('none') }] }],
    columns: [
      { key: 'title', label: T('Headline'), text: true },
      { key: 'value', label: T('Value'), field: 'value' },
      { key: 'delay_hours', label: T('Hours'), field: 'delay_hours' },
      { key: 'succ', label: T('Continues with'), get: (r) => r.successors.filter((x) => x).length, fmt: (v, r) => r.successors.filter((x) => x).map((x) => (x === NEWS_END ? T('end') : x)).join(' ') },
      { key: 'story', label: T('Story'), get: (r) => newsTemplateOf(r.id)?.id || 0, fmt: (v) => (v ? T('story {n}', { n: v }) : '-') }],
  },
  { key: 'topics', table: 'news_topics', plate: T('News topics'), room: 110, label: T('Topics'), what: T('topic'), many: T('topics'), kind: 'topic', icon: 'topic',
    spine: () => '#6dbade', panels: ['checks', 'changes', 'stories'],
    groups: [
      { key: 'phrases', title: T('Phrases'), render: (r, ctx, done) => phrasesTable(r, ctx, done),
        fields: [{ key: 'phrase_value', label: T('Phrase values'), type: 'custom', fmt: (v) => v.join(' ') }],
        texts: [...Array(10).keys()].map((k) => ({ i: k, label: T('Phrase {n}', { n: k + 1 }), hidden: true })) }],
    columns: [
      { key: 'title', label: T('Phrases'), text: true },
      { key: 'n', label: T('Count'), get: (r) => (r.text?.en || []).filter(Boolean).length },
      { key: 'sum', label: T('Values'), get: (r) => r.phrase_value.reduce((a, b) => a + b, 0) }],
  },
  ];
}
onUiLang(() => {
  Object.assign(LANG_NAME, { en: T('English'), de: T('Deutsch') });
  MARKET.splice(0, MARKET.length, [0, T('Not in the game|market'), T('Neither the dealer nor the auction offers it')], [0x20, T('Dealer|market'), T('The film dealer offers it (flags bit 5)')], [0x60, T('Auction|market'), T('The auction offers it (flags bits 5 and 6)')]);
  NEWS_CATEGORY.splice(0, NEWS_CATEGORY.length, '', T('politics'), T('show business'), T('sundry'));
  SECTIONS.splice(0, SECTIONS.length, ...defineSections());
});

function titleOf(sec, rec, lang = st.lang) {
  switch (sec.key) {
    case 'films': case 'series': return txt(rec, 0, lang) || T('(no title)');
    case 'ads': return txt(rec, 0, lang) || T('(no name)');
    case 'screenplays': return targetTitle(rec, lang) || (txt(rec, 0, lang).slice(0, 40) || T('(empty)'));
    case 'news': return typeof rec.text?.[lang] === 'string' ? rec.text[lang] || T('(empty)') : T('(empty)');
    case 'topics': return (rec.text?.[lang] || []).filter(Boolean).slice(0, 3).join(', ') || T('(empty)');
  }
  return '';
}
const producedEpisodes = (r) => [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].filter((e) => r.episodes_qtf[e * 3 + 2] & 1).length;

/* where an entry is, as a short badge for the list */
function badgeOf(sec, r) {
  const u = usage(sec, r);
  if (u.free) return { text: T('free'), cls: 'free' };
  if (sec.key === 'films' || sec.key === 'series') {
    if (r.owner >= 1 && r.owner <= 3) return { text: ['', 'Mad', 'Fun', 'Sun'][r.owner], cls: 'lib', tip: T("in {station}'s start library", { station: STATIONS[r.owner] }) };
    if (producedBy(sec.table, r.id).length) return { text: T('studio'), cls: 'studio', tip: T('a screenplay produces it') };
    if (r.flags & 0x20) return { text: r.flags & 0x40 ? T('auction|badge') : T('dealer'), cls: r.flags & 0x40 ? 'auction' : 'dealer' };
    if (r.flags2 & POOL) return { text: T('pool'), cls: 'pool' };
  }
  if (sec.key === 'ads' && r.fee) return { text: T('agency'), cls: 'dealer' };
  if (sec.key === 'screenplays' && r.flags & 2) return { text: T('agency'), cls: 'dealer' };
  return null;
}
/* filter chips of the list (and the table) */
function chipsOf(sec) {
  const out = [{ key: 'changed', label: T('Changed'), test: (r) => changed(sec.table, r), count: true }];
  if (sec.key === 'films' || sec.key === 'series') {
    out.push({ key: 'dealer', label: T('Dealer'), test: (r) => (r.flags & 0x60) === 0x20 },
      { key: 'auction', label: T('Auction'), test: (r) => (r.flags & 0x60) === 0x60 },
      { key: 'pool', label: T('Start pool'), test: (r) => !!(r.flags2 & POOL) });
  }
  if (['films', 'series', 'ads', 'screenplays'].includes(sec.key)) out.push({ key: 'free', label: T('Free'), test: (r) => usage(sec, r).free });
  if (REQUIRED[sec.key]) out.push({ key: 'incomplete', label: T('Incomplete'), test: (r) => missing(sec, r).length > 0 });
  return out;
}

/* the entry's own checks: [{ level: ok|warn|err, msg }] */
function entryChecks(sec, r) {
  const out = [];
  const textItems = textFields(sec);
  for (const t of textItems) {
    for (const l of LANGS) {
      const v = txt(r, t.i, l);
      const bad = missingChars(v);
      if (bad.length) out.push({ level: 'warn', msg: T('{label} ({lang}): not in the game\'s font, shows as ".": {chars}', { label: t.label, lang: l.toUpperCase(), chars: bad.join(' ') }) });
      const c = t.check && v ? t.check(v, l) : null;
      if (c && c.level === 'warn') out.push({ level: 'warn', msg: `${t.label} (${l.toUpperCase()}): ${c.msg}` });
    }
  }
  if (!out.length && textItems.length) out.push({ level: 'ok', msg: T("All texts fit the card and the game's font") });
  const m = missing(sec, r);
  if (m.length) out.unshift({ level: 'err', msg: T('Required before it can go on air: {missing}', { missing: m.map((x) => x.msg).join(', ') }) });
  if (sec.key === 'series') {
    for (let e = 1; e <= 10; e++) {
      for (const l of LANGS) {
        const t = txt(r, 1 + e, l);
        if ((r.episodes_qtf[e * 3 + 2] & 1) && textW(`${e}.${t}`) > TEXT_W) out.push({ level: 'warn', msg: T('Episode {e} ({lang}): with "{e}." in front the title runs past the card line', { e, lang: l.toUpperCase() }) });
      }
      if ((r.episodes_qtf[e * 3 + 2] & 1) && (!txt(r, 1 + e, 'en').trim() || !txt(r, 1 + e, 'de').trim())) out.push({ level: 'warn', msg: T('Episode {e} is produced but has no title in both languages', { e }) });
    }
    const n = producedEpisodes(r);
    if (n !== r.episodes) out.push({ level: 'warn', msg: TP(n, '{n} episode produced, the card says {m}', '{n} episodes produced, the card says {m}', { m: r.episodes }) });
  }
  if (sec.key === 'screenplays' && !(origById('screenplays', r.id).flags & 2) && (r.flags & 2)) out.push({ level: 'warn', msg: T('Never offered in the original: its production was never played') });
  if (sec.key === 'screenplays') {
    for (const x of spDupes(r)) {
      const o = { where: spIsSeries(r) ? T('Episode {e}', { e: x.e }) : T('The list'), item: propName(x.item), n: x.n };
      out.push({ level: 'warn', msg: spIsSet(x.item) ? T('{where}: the set "{item}" {n} times -- bought and paid {n} times, it takes {n} of the studio\'s places', o)
        : T('{where}: "{item}" {n} times -- bought and paid {n} times', o) });
    }
  }
  if (sec.key === 'news') {
    const t = newsTemplateOf(r.id);
    for (const l of LANGS) {
      for (const m2 of (txt(r, null, l).match(/#\d/g) || [])) {
        const k = +m2[1];
        if (t && !(t.parts[k - 1] > 0)) out.push({ level: 'warn', msg: T('Headline ({lang}): {code} has no topic in story {id}', { lang: l.toUpperCase(), code: m2, id: t.id }) });
      }
    }
    if (!t && !SPECIAL.news_nodes[r.id]) out.push({ level: 'warn', msg: T('No story reaches this step') });
  }
  if (['films', 'series'].includes(sec.key)) {
    const g = gameChecks();
    out.push(g.length ? { level: 'err', msg: g.join(' ') } : (() => { const c = startCounts(); return { level: 'ok', msg: T('A new game starts: {films} films free (9 needed), {series} series (3 needed)', c) }; })());
  }
  return out;
}
/* the text items of a section (both languages each) */
const textFields = (sec) => sec.groups.flatMap((g) => g.texts || []);
