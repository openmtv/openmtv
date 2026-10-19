/* OpenMTV editor -- the Monitor tab (board A): what the office TV shows while a film or
 * series airs, in the game's own monitor (game.js drawMonitor): the genre picture or one of
 * its own, the hour on the TV, the monitor plan of the whole mod. */
'use strict';

const PIC_NAMES = {};
onUiLang(() => Object.assign(PIC_NAMES, { 220: [T('Dark screen'), T('before 18:00')], 236: [T('News|picture'), ':00 - :05'], 237: [T('Ads|picture'), T(':55, an ad booked')], 238: [T('Test card'), T('nothing on / no ad')] }));
function picLabel(p) {
  if (p >= FILM_PIC + 1 && p <= FILM_PIC + 10) return st.meta.film_genres[st.lang][p - FILM_PIC - 1];
  if (p >= SERIES_PIC + 1 && p <= SERIES_PIC + 5) return st.meta.series_genres[st.lang][p - SERIES_PIC - 1];
  return (PIC_NAMES[p] || [T('picture {n}', { n: p })])[0];
}
/* who shows a picture of their own: -> ['film 3', 'series 18', ...] */
function ownPictureUsers(p) {
  const out = [];
  for (const [table, what] of [['films', T('film')], ['series', T('series')]]) for (const r of records(table)) if (r.jingle === p) out.push({ table, what, id: r.id });
  return out;
}
function monitorView(r, base, done) {
  const sec = cur();
  const genrePic = base + (r.genre || 1), pic = r.jingle || genrePic;
  const genreName = (sec.genres().find((x) => x[0] === r.genre) || [0, '?'])[1];
  const big = h('canvas', { class: 'monitor big' });
  drawMonitor(big, pic, 3);
  const hour = h('div', { class: 'hourbar' },
    h('span', { class: 'fixed', 'data-tip': T('News: picture 236, fixed in the game') }, ':00'),
    h('span', { class: 'prog' }, T(':05 - :55 picture {n}', { n: pic })),
    h('span', { class: 'fixed', 'data-tip': T('The ad block: picture 237, or 238 when no ad is booked; fixed in the game') }, ':55'));
  const choice = h('div', { class: 'picchoice', role: 'radiogroup' },
    h('button', { role: 'radio', class: !r.jingle ? 'on' : '', 'aria-checked': !r.jingle ? 'true' : 'false', 'data-choice': 'genre', onclick: () => { r.jingle = 0; done({ rerender: true }); } },
      h('b', {}, T('Follows the genre')), h('span', {}, T('{genre} -> picture {n}. A new genre brings its picture along.', { genre: genreName, n: genrePic }))),
    h('button', { role: 'radio', class: r.jingle ? 'on' : '', 'aria-checked': r.jingle ? 'true' : 'false', 'data-choice': 'own', onclick: () => { if (!r.jingle) { r.jingle = genrePic; done({ rerender: true }); } } },
      h('b', {}, T('Own picture')), h('span', {}, T('Stays when the genre changes, like series 18, 19 and 20 do with 226.'))));
  const tile = (p) => {
    const users = ownPictureUsers(p).filter((u) => !(u.table === sec.table && u.id === r.id));
    const isCur = p === pic;
    const note = isCur ? (r.jingle ? T('now, own picture') : T('now, genre')) : (PIC_NAMES[p] ? PIC_NAMES[p][1] : users.length ? T('also {list}', { list: users.slice(0, 3).map((u) => `${u.what} ${u.id}`).join(', ') }) : '');
    return h('button', { class: 'pic' + (isCur ? ' cur' : ''), 'data-pic': p, 'aria-pressed': isCur ? 'true' : 'false', 'data-tip': PIC_NAMES[p] && p !== 220 ? T('The game shows this one itself at its times; a programme may use it too.') : null,
      onclick: () => { if (p === pic && (r.jingle || p === genrePic)) return; r.jingle = p; done({ rerender: true }); } },
    h('img', { src: `/api/picture/${p}.png`, alt: '' }), h('span', { class: 'pl' }, `${p} ${picLabel(p)}`), h('span', { class: 'pn' + (isCur ? ' cur' : '') }, note || ' '));
  };
  const group = (title, ps) => h('div', { class: 'picgroup' }, h('div', { class: 'pgt' }, title), h('div', { class: 'picgrid' }, ...ps.map(tile)));
  const range = (a, n) => [...Array(n).keys()].map((k) => a + k);
  return h('div', { class: 'monitorview' },
    h('div', { class: 'mleft' }, h('div', { class: 'tvstage' }, big),
      h('div', { class: 'hint' }, T('Every hour while it airs')), hour,
      h('div', { class: 'hint' }, T("The game's own monitor (bottom panel); the picture sits where the game puts it."))),
    h('div', { class: 'mright' }, choice,
      group(T('Film genres'), range(FILM_PIC + 1, 10)), group(T('Series genres'), range(SERIES_PIC + 1, 5)), group(T('The game uses these itself'), [220, 236, 237, 238])));
}
function monitorInspector(sec, r, ins) {
  const rows = [
    [T('Before 18:00'), '220', T('dark screen, fixed'), true], [T('News, :00 - :05'), '236', T('every hour, fixed'), true], [T('Ad block, :55'), '237 / 238', T('238 when no ad is booked, fixed'), true],
    [T('Films'), '221 - 230', T("genre picture, or the film's own"), false], [T('Series'), '231 - 235', T("genre picture, or the series' own"), false], [T('Nothing scheduled'), '238', T('test card, channel digit drawn on it, fixed'), true]];
  ins.append(panel(T('Monitor plan, the whole selection'), h('div', { class: 'plan' }, ...rows.map(([when, p, note, fixed]) => h('div', { class: 'planrow' + (fixed ? ' fixed' : '') },
    h('span', { class: 'pw' }, fixed ? icon('lock', 12) : null, when), h('span', { class: 'pp' }, p), h('span', { class: 'pnote' }, note)))),
  h('div', { class: 'hint pad' }, T("From the game's code (tv_update_picture 0054:3738). The fixed rows cannot be set per entry."))));
  const own = h('div', { class: 'used' });
  for (const [table, what] of [['films', T('Films')], ['series', T('Series')]]) {
    const us = records(table).filter((x) => x.jingle);
    own.append(h('div', { class: 'ownrow' }, h('b', {}, what + ': '), us.length ? '' : T('none, all follow their genre')));
    const byPic = {};
    for (const x of us) (byPic[x.jingle] = byPic[x.jingle] || []).push(x);
    for (const [p, xs] of Object.entries(byPic)) own.append(h('div', { class: 'preds' }, ...xs.map((x) => h('button', { class: 'link', onclick: () => go(table, x.id, { tab: 'monitor' }) }, `${x.id} ${txt(x, 0)}`)), `-> ${p}`));
  }
  ins.append(panel(T('Own pictures in this selection'), own));
  ins.append(h('div', { class: 'later' }, h('b', {}, T('Later, for the decompiled build')),
    h('div', {}, T('A picture per ad contract and per news story, like films have: a new field and a small change in tv_update_picture. This picker would serve them as is.'))));
}

SELFTESTS.push(async function selftestMonitor(t) {
  const { ok, wait, typeInto, go2, W } = t;
  const f31 = recById('films', 31);
  /* ---- the monitor: the game's own TV with the chosen picture */
  await go2('#films/31/monitor');
  const big = W('canvas.monitor.big');
  ok(big && +big.dataset.pic === 225 && $('#inspector .plan') && $$('#inspector .planrow.fixed').length === 4, 'monitor tab: genre picture 225, the monitor plan with the fixed pictures');
  W('.pic[data-pic="229"]').click();
  await wait(400);
  const big2 = W('canvas.monitor.big');
  const pic = await loadImg('/api/picture/229.png'), tv = await loadImg('/api/sprite/9/0.png');
  const px = (src, x, y) => { const c = document.createElement('canvas'); c.width = src.width; c.height = src.height; const g = c.getContext('2d'); g.drawImage(src, 0, 0); return [...g.getImageData(x, y, 1, 1).data].join(); };
  ok(f31.jingle === 229 && +big2.dataset.pic === 229 && px(big2, 8 + 50, 2 + 40) === px(pic, 50, 40) && px(big2, 100, 70) === px(tv, 100, 70) && W('[data-choice="own"]').classList.contains('on'),
    'own picture 229 drawn into the monitor sprite at +8,+2');
  W('[data-choice="genre"]').click();
  ok(f31.jingle === 0 && +W('canvas.monitor.big').dataset.pic === 225, 'back to the genre picture');
});
