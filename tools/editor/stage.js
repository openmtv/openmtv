/* OpenMTV editor -- the card: the default view of films, series and ads.  The game's card in its
 * own look over the room it is offered in; the texts are typed on the card itself, in the
 * game's font at the card's scale, so wrap and cut-off show at once.  A row clicked shows the
 * values behind it below the card (schema.js cardEdit) -- a text: how it fits, the other
 * language.  Beside it what is not on the card, the TV, the checks, the changes. */
'use strict';

const CARD_SCALES = [2, 3, 4];
const isTextRow = (edit) => edit === 'title' || edit === 'desc';
function renderStage(sec, r, w, ins) {
  const ctx = { req: {} };
  $('#workhead').append(entryHead(sec, r, ctx));
  const lang = editLang(), other = LANGS.find((l) => l !== lang);
  const S = CARD_SCALES.includes(st.cardScale) ? st.cardScale : 3;
  const rowY = (i) => (1 + i * 8) * S;
  const canvas = h('canvas', { class: 'card' });
  const layer = h('div', { class: 'cardlayer' });
  const selMark = h('span', { class: 'rowsel' });
  const chgMarks = h('div');
  const pos = h('div', { class: 'cardpos big' }, canvas, layer, selMark, chgMarks);
  const zoom = h('div', { class: 'seg small', role: 'group', 'aria-label': T('Size of the card') }, ...CARD_SCALES.map((k) => h('button', { class: 'mono' + (k === S ? ' on' : ''), 'data-scale': k, 'aria-pressed': k === S ? 'true' : 'false',
    onclick: () => { st.cardScale = k; try { localStorage.setItem('madtv-editor-scale', k); } catch (e) { /* ignore */ } renderEntry(); } }, `${k}×`)));
  w.append(h('div', { class: 'stage' }, h('img', { class: 'stagebg', src: `/api/room/${sec.room}.png`, alt: '' }), h('div', { class: 'scan' }),
    h('div', { class: 'stagetop' }, h('span', { class: 'stagecap' }, icon(sec.icon, 14), T('As the game shows it: {place}, {lang}', { place: sec.plate, lang: LANG_NAME[lang] })), zoom),
    h('div', { class: 'stagecard' }, pos),
    h('div', { class: 'stagehint' }, T('Type on the card; click a line to change its values.'))));
  const tray = h('section', { class: 'box tray', 'aria-label': T('The values of a card row') });
  w.append(tray);
  /* the row shown below the card: a text being typed, or a row of values */
  const rowKeys = (edit) => sec.cardEdit && sec.cardEdit[edit];
  const valid = (edit) => edit && (isTextRow(edit) || rowKeys(edit)) && sec.card(r, lang).some((x) => x.edit === edit);
  if (!valid(st.stageRow[sec.key])) st.stageRow[sec.key] = (sec.card(r, lang).find((x) => rowKeys(x.edit)) || {}).edit || 'title';
  const pick = (edit) => { if (st.stageRow[sec.key] === edit) return; st.stageRow[sec.key] = edit; layout(); fillTray(); };
  /* the texts on the card: transparent fields in the game's font over the drawn rows */
  const textAt = (i, edit, multi) => {
    const el = h(multi ? 'textarea' : 'input', Object.assign({ class: 'ontext', spellcheck: 'false', 'data-text': `${i}-${lang}`, 'aria-label': `${(textFields(sec).find((x) => x.i === i) || {}).label || T('Text')}, ${LANG_NAME[lang]}` }, multi ? {} : { type: 'text' }));
    el.value = txt(r, i, lang);
    el.addEventListener('input', () => { setTxt(r, i, el.value, lang); st.undoKey = `${sec.key}:${r.id}:text:${i}:${lang}`; touched(); });
    el.addEventListener('focus', () => pick(edit));
    el.addEventListener('keydown', (e) => { if (e.key === 'Escape') el.blur(); });
    return el;
  };
  const title = textAt(0, 'title', false), desc = textAt(1, 'desc', true);
  const zones = h('div', { class: 'zones' });
  layer.append(zones, title, desc);
  const layout = () => {
    const rows = sec.card(r, lang);
    drawCard(canvas, rows, S);
    layer.style.width = canvas.style.width; layer.style.height = canvas.style.height;
    const di = rows.map((x, k) => (x.edit === 'desc' ? k : -1)).filter((k) => k >= 0);
    Object.assign(title.style, { left: TEXT_X * S + 'px', top: rowY(0) + 'px', width: (CARD_W - TEXT_X - 2) * S + 'px', height: 8 * S + 'px' });
    Object.assign(desc.style, { left: TEXT_X * S + 'px', top: rowY(di[0]) + 'px', width: TEXT_W * S + 'px', height: di.length * 8 * S + 'px' });
    const cur = st.stageRow[sec.key];
    zones.innerHTML = '';
    rows.forEach((row, k) => {
      const keys = rowKeys(row.edit);
      if (!keys || (k > 0 && rows[k - 1].edit === row.edit)) return;
      let n = 1;
      while (rows[k + n] && rows[k + n].edit === row.edit) n++;
      zones.append(h('button', { class: 'zone' + (row.edit === cur ? ' on' : ''), 'data-edit': row.edit, style: `top:${rowY(k)}px;height:${8 * S * n}px`,
        'aria-label': T('Change: {fields}', { fields: keys.map((x) => labelOf(fieldByKey(sec, x), r)).join(', ') }), 'data-tip': T('Change: {fields}', { fields: keys.map((x) => labelOf(fieldByKey(sec, x), r)).join(', ') }),
        onclick: () => pick(row.edit) }));
    });
    const at = rows.map((x, k) => (x.edit === cur ? k : -1)).filter((k) => k >= 0);
    selMark.style.display = at.length ? '' : 'none';
    if (at.length) Object.assign(selMark.style, { top: rowY(at[0]) + 'px', height: (at[at.length - 1] - at[0] + 1) * 8 * S + 'px' });
    /* the rows that differ from the original: a yellow dot on the right */
    chgMarks.innerHTML = '';
    const o = origById(sec.table, r.id);
    if (o) {
      const orows = sec.card(o, lang);
      rows.forEach((row, k) => { if (!orows[k] || rowText(row) !== rowText(orows[k])) chgMarks.append(h('span', { class: 'rowchg', style: `top:${rowY(k) + 4 * S - 4}px`, 'data-tip': T('Differs from the original') })); });
    }
    if (isTextRow(cur)) fillTextInfo();
  };
  /* below the card: the row's values -- the fields behind it, or how a text fits */
  let textInfo = null;
  const fillTextInfo = () => {
    if (!textInfo) return;
    const i = st.stageRow[sec.key] === 'title' ? 0 : 1;
    const t = textFields(sec).find((x) => x.i === i);
    const v = txt(r, i, lang), c = t && t.check && v ? t.check(v, lang) : null, bad = missingChars(v);
    const over = c && c.level === 'warn';
    let meter;
    if (i === 0) {
      meter = h('span', { class: 'meter' + (over ? ' over' : '') }, h('span', { class: 'mt' }, h('span', { style: `width:${Math.min(100, (textW(v) / TEXT_W) * 100)}%` })), h('span', {}, c ? c.msg : T('empty')));
    } else {
      const shown = sec.card(r, lang).filter((x) => x.edit === 'desc').length, k = v ? wrap(v, TEXT_W).length : 0;
      meter = h('span', { class: 'meter' + (over ? ' over' : '') }, h('span', { class: 'seglines', 'aria-hidden': 'true' }, ...[...Array(Math.max(shown, k)).keys()].map((n) => h('span', { class: n < k ? 'on' : '' }))), h('span', {}, c ? c.msg : T('empty')));
    }
    textInfo.innerHTML = '';
    textInfo.append(h('span', {}, i === 0 ? T('Width') : T('Lines')), meter,
      h('span', {}, LANG_NAME[other]), h('span', { class: 'other' }, txt(r, i, other) || T('(empty)')),
      h('span', {}, T('Characters')), bad.length ? h('span', { class: 'warn' }, T('not in the font: {chars}', { chars: bad.join(' ') })) : h('span', { class: 'ok' }, T("all in the game's font")));
  };
  const fillTray = () => {
    const cur = st.stageRow[sec.key];
    tray.innerHTML = '';
    textInfo = null;
    if (isTextRow(cur)) {
      const t = textFields(sec).find((x) => x.i === (cur === 'title' ? 0 : 1));
      textInfo = h('div', { class: 'textinfo' });
      tray.append(h('div', { class: 'boxtitle' }, h('span', { class: 'rowtag' }), `${t ? t.label : T('Text')}, ${LANG_NAME[lang]}`, h('span', { class: 'trayhint' }, T('typed on the card'))), textInfo);
      fillTextInfo();
      return;
    }
    const keys = rowKeys(cur) || [];
    const fields = keys.map((k) => fieldByKey(sec, k)).filter(Boolean);
    const group = sec.groups.find((g) => g.render && (g.fields || []).some((f) => keys.includes(f.key)));
    tray.append(h('div', { class: 'boxtitle' }, h('span', { class: 'rowtag' }), group ? (typeof group.title === 'function' ? group.title(r) : group.title) : fields.map((f) => labelOf(f, r)).join(', ')));
    if (group) tray.append(group.render(r, ctx, (o = {}) => touched(o)));
    else tray.append(fieldsGrid(sec, r, fields.filter((f) => !f.show || f.show(r)), ctx));
  };
  layout();
  fillTray();
  st.liveWork.push(layout);
  /* beside it: what is not on the card, the TV, the checks, the changes */
  const off = (sec.offCard || []).map((k) => fieldByKey(sec, k)).filter(Boolean);
  if (off.length) ins.append(panel(T('Not on the card'), fieldsGrid(sec, r, off, ctx)));
  for (const p of sec.panels) {
    if (p === 'card') continue;
    const el = PANELS[p]?.(sec, r);
    if (el) ins.append(el);
  }
}

MODES.game = { get label() { return T('Card'); }, render: renderStage };

SELFTESTS.push(async function selftestStage(t) {
  const { ok, wait, typeInto, go2, W } = t;
  /* ---- the card: the texts typed on the card itself, a row's values below it */
  await go2('#films/31');
  ok(st.mode === 'game' && W('.stage canvas.card') && $('#workhead .modetoggle [data-mode="game"]').classList.contains('on'), 'films open on their card');
  const on = $('#work .stage [data-text="1-en"]'), r31 = recById('films', 31);
  typeInto(on, 'Typed on the card, where the game wraps its lines like this one.');
  const rows = filmRows(r31, 'en');
  ok(r31.text.en[1].startsWith('Typed on the card') && rows[1].text === wrap(r31.text.en[1], TEXT_W)[0] && parseInt(on.style.width) === TEXT_W * st.cardScale, 'card: description typed on the card');
  on.dispatchEvent(new Event('focus'));
  ok(W('.tray .textinfo') && W('.tray .other').textContent === txt(r31, 1, 'de') && W('.zones .zone.on') === null, 'card: a text row shows how it fits and the other language');
  W('.zone[data-edit="genre"]').click();
  ok(W('.tray select[data-field="genre"]') && W('.tray [data-field="rating"]') && W('.zone[data-edit="genre"]').classList.contains('on'), 'card: a row shows its fields below the card (genre, rating)');
  ok(Array.isArray(st.liveWork) && $('#inspector').textContent.includes(T('Not on the card')) && $('#inspector [data-field="offered"]'), 'card: what is not on the card beside it');
  const hours = () => $$('#inspector .audcol .audbar').map((b) => b.style.height);
  const was = hours(), top31 = r31.topicality;
  W('.zone[data-edit="topicality"]').click();
  typeInto(W('.tray input[data-field="topicality"]'), String(Math.max(1, top31 - 30)));
  ok($$('#inspector .audcol').length === 7 && !same(hours(), was) && $('#inspector .audhead').textContent.includes(`${Math.max(...filmSolo(r31))} %`), 'card: the audience 18-24 beside it follows the topicality');
  typeInto(W('.tray input[data-field="topicality"]'), String(top31));
  /* the TV family under its toggles: the game's picture of the office, live */
  const famKey = () => ($('#inspector canvas.family') || {}).dataset?.key || '';
  const k0 = famKey(), bits0 = r31.flags & 0x1F;
  for (const b of [0, 1, 2, 3, 4]) if (!(r31.flags & (1 << b))) $(`#inspector [data-field="family"] [data-bit="${b}"]`).click();
  ok(k0 && (r31.flags & 0x1F) === 0x1F && famKey() !== k0 && $('#inspector .famnotes .warnline') && familyScene(0x1F, 50, 2, 1).shown.length === 3,
    'card: all five of the TV family chosen -- the picture redrawn, three on the sofa, two stay away');
  $('#inspector [data-famhour="7"]').click();
  ok(st.familyHour === 7 && $('#inspector .famnotes').textContent.includes(T('The child is in bed from 23:00.')), 'card: at 24:00 the child is in bed');
  $('#inspector [data-famhour="1"]').click();
  for (const b of [0, 1, 2, 3, 4]) if (!(bits0 & (1 << b))) $(`#inspector [data-field="family"] [data-bit="${b}"]`).click();
  ok((r31.flags & 0x1F) === bits0 && famKey() === k0, 'card: the family as before');
  W('[data-scale="2"]').click();
  ok(st.cardScale === 2 && W('.stage canvas.card').style.width === CARD_W * 2 + 'px', 'card: drawn at 2x');
  W('[data-scale="3"]').click();
  await go2('#series/2');
  W('.zone[data-edit="episode"]').click();
  ok(W('.tray table.episodes'), 'card: the episode row shows the episodes');
});
