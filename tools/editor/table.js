/* OpenMTV editor -- the table mode (board B, Mischpult): compare and balance many entries,
 * change values in place or in bulk (prices, quality, market, starting pool).  Columns come
 * from the section's schema (schema.js columns). */
'use strict';

const tableState = (sec) => (st.tables = st.tables || {})[sec.key] || (st.tables[sec.key] = { q: '', sort: null, sel: new Set(), chips: {}, genre: 0, last: null });
function colDef(sec, c) {
  const f = c.field ? fieldByKey(sec, c.field) : null;
  return {
    key: c.key, label: c.label, f, text: c.text, render: c.render,
    get: (r) => (c.text ? titleOf(sec, r) : c.get ? c.get(r) : fieldGet(f, r)),
    fmt: (v, r) => (c.fmt ? c.fmt(v, r) : f ? (f.type === 'money' ? numFmt(v) : f.type === 'number' || f.type === 'bar' ? String(v) : fmtVal(f, v, r)) : String(v)),
    num: !c.text, right: !c.text && !(f && ['select', 'choice'].includes(f.type)) && (!c.fmt || c.num), plain: c.plain, tip: c.tip,
    editable: !!f && ['number', 'money', 'bar', 'select', 'choice'].includes(f.type),
    numeric: !!f && ['number', 'money', 'bar'].includes(f.type),
  };
}
function tableRows(sec, TS, cols) {
  const toks = TS.q.trim().toLowerCase().split(/\s+/).filter(Boolean);
  const chips = chipsOf(sec).filter((c) => TS.chips[c.key]);
  const preset = TS.preset != null ? (sec.presets || [])[TS.preset] : null;
  let rows = records(sec.table).filter((r) => {
    if (TS.genre && r.genre !== TS.genre) return false;
    if (!chips.every((c) => c.test(r))) return false;
    if (preset && preset.test && !preset.test(r)) return false;
    for (const t of toks) {
      const m = /^([a-zäöüß$.]+)(<=|>=|!=|<|>|=)(-?\d+)$/.exec(t);
      if (m) {
        const c = cols.find((x) => x.key.toLowerCase() === m[1] || x.label.toLowerCase().startsWith(m[1]));
        if (!c) continue;
        const v = +c.get(r), n = +m[3];
        const ok = { '<': v < n, '>': v > n, '<=': v <= n, '>=': v >= n, '=': v === n, '!=': v !== n }[m[2]];
        if (!ok) return false;
      } else if (!titleOf(sec, r).toLowerCase().includes(t.replace(/^#/, '')) && String(r.id) !== t.replace(/^#/, '')) return false;
    }
    return true;
  });
  if (TS.sort) {
    const [k, dir] = TS.sort, c = cols.find((x) => x.key === k) || { get: (r) => r.id, num: true };
    rows = rows.slice().sort((a, b) => { const x = c.get(a), y = c.get(b); return (c.num ? x - y : String(x).localeCompare(String(y))) * dir || a.id - b.id; });
  }
  return rows;
}
/* the audience by hour, 18..24, as seven small bars (soloShare, game.js; full height = 80 %, the
   best films reach 76), a gap before 21:00; X rated: 18..20 hatched -- the moral guardian takes it
   away before 21:00 */
function soloStrip(vals, xrated) {
  const hours = ['18', '19', '20', '21', '22', '23', '24'];
  const words = vals.map((v, k) => `${hours[k]}:00 ${v} %` + (xrated && k < 3 ? ' ' + T('(moral guardian)') : '')).join(', ');
  return h('span', { class: 'strip', role: 'img', 'aria-label': words, 'data-tip': words, 'data-tiptitle': T('Alone on air, by hour'), 'data-max': Math.max(...vals) },
    ...vals.map((v, k) => h('i', { class: v === 0 ? 'zero' : xrated && k < 3 ? 'risk' : '', style: `height:${Math.max(1, Math.min(26, Math.round((v * 26) / 80)))}px` })));
}
function renderTable(sec, w, ins) {
  const TS = tableState(sec);
  const cols = sec.columns.map((c) => colDef(sec, c));
  const rows = tableRows(sec, TS, cols);
  const total = records(sec.table).length;
  TS.sel = new Set([...TS.sel].filter((id) => recById(sec.table, id)));
  /* toolbar */
  const q = h('input', { type: 'text', id: 'tq', placeholder: T('Filter: title, #id, quality>40 ...'), value: TS.q });
  q.addEventListener('input', () => { TS.q = q.value; clearTimeout(TS.qt); TS.qt = setTimeout(() => { renderEntry(); const n = $('#tq'); n.focus(); n.setSelectionRange(n.value.length, n.value.length); }, 250); });
  /* the genre, as in the list (the inspector's genre table sets it too) */
  const genre = sec.genres ? genreSelect(sec, TS.genre, (g) => { TS.genre = g; renderEntry(); }, { class: 'tgenre' + (TS.genre ? ' on' : ''), 'data-tgenre': '' }) : null;
  const bar = h('div', { class: 'box tbar' }, h('span', { class: 'ttl' }, T('{label}: table', { label: sec.label })), viewToggle(sec), q, genre,
    ...chipsOf(sec).map((c) => h('button', { class: 'chip' + (TS.chips[c.key] ? ' on' : ''), 'data-chip': c.key, onclick: () => { TS.chips[c.key] = !TS.chips[c.key]; renderEntry(); } }, c.label)),
    TS.preset != null && sec.presets?.[TS.preset] ? h('button', { class: 'chip on', onclick: () => { TS.preset = null; renderEntry(); } }, `${sec.presets[TS.preset].label} x`) : null,
    h('span', { class: 'tcount' }, T('{n} of {total}', { n: rows.length, total })));
  w.append(bar);
  if (TS.sel.size) w.append(bulkBar(sec, TS, cols));
  /* the table */
  const allOn = rows.length && rows.every((r) => TS.sel.has(r.id));
  const thead = h('tr', {}, h('th', { class: 'selc' }, h('button', { class: 'selbox' + (allOn ? ' on' : ''), 'aria-label': T('Select all'), 'data-tip': T('Select all shown'),
    onclick: () => { if (allOn) rows.forEach((r) => TS.sel.delete(r.id)); else rows.forEach((r) => TS.sel.add(r.id)); renderEntry(); } })),
  h('th', { class: 'num', onclick: () => sortBy(TS, 'id') }, '#' + sortMark(TS, 'id')),
  ...cols.map((c) => h('th', { class: c.right ? 'num' : '', 'data-col': c.key, 'aria-sort': TS.sort && TS.sort[0] === c.key ? (TS.sort[1] > 0 ? 'ascending' : 'descending') : null, onclick: () => sortBy(TS, c.key),
    'data-tip': c.tip || (c.f ? helpText(c.f) || null : null) }, c.label + sortMark(TS, c.key))));
  const tb = h('tbody');
  for (const r of rows) tb.append(tableRow(sec, TS, cols, r, rows));
  w.append(h('div', { class: 'box tablewrap' }, h('table', { class: 'mix' }, h('thead', {}, thead), tb)));
  w.append(h('div', { class: 'hint light' }, T("Click a cell to change it in place (Enter keeps it, Esc not); shift-click selects a range; click a title to open the entry. Dealer asks and $ per quality follow the game's own formulas.")));
  tableInspector(sec, TS, cols, rows, ins);
}
function sortMark(TS, key) { return TS.sort && TS.sort[0] === key ? (TS.sort[1] > 0 ? ' ^' : ' v') : ''; }
function sortBy(TS, key) { TS.sort = TS.sort && TS.sort[0] === key ? (TS.sort[1] > 0 ? [key, -1] : null) : [key, 1]; renderEntry(); }
function tableRow(sec, TS, cols, r, rows) {
  const on = TS.sel.has(r.id), ch = changed(sec.table, r), o = origById(sec.table, r.id);
  const tr = h('tr', { class: (on ? 'on' : '') + (ch ? ' chg' : ''), 'data-id': r.id },
    h('td', { class: 'selc' }, h('button', { class: 'selbox' + (on ? ' on' : ''), 'aria-label': T('Select #{id}', { id: r.id }), onclick: (e) => {
      if (e.shiftKey && TS.last) {
        const ids = rows.map((x) => x.id), a = ids.indexOf(TS.last), b = ids.indexOf(r.id);
        if (a >= 0 && b >= 0) for (const id of ids.slice(Math.min(a, b), Math.max(a, b) + 1)) TS.sel.add(id);
      } else if (on) TS.sel.delete(r.id); else TS.sel.add(r.id);
      TS.last = r.id; renderEntry();
    } })),
    h('td', { class: 'num id' }, r.id));
  for (const c of cols) {
    const v = c.get(r);
    const was = o && c.f && !c.text && !same(fieldGet(c.f, o), fieldGet(c.f, r));
    let content;
    if (c.render) content = c.render(r);
    else if (c.f && c.f.type === 'bar' && !c.plain) content = h('span', { class: 'mbar' }, h('span', { class: 'mtrack' }, h('span', { class: 'mfill', style: `width:${Math.min(100, (v / c.f.max) * 100)}%` })), c.fmt(v, r));
    else content = c.fmt(v, r);
    const td = h('td', { class: (c.right ? 'num' : '') + (c.editable ? ' ed' : '') + (was ? ' was' : '') + (c.text ? ' ttl' : ''), 'data-col': c.key,
      'data-tip': was ? T('was {value}', { value: c.fmt(fieldGet(c.f, o), o) }) : null }, content);
    if (c.text) td.addEventListener('click', () => go(sec.key, r.id, { view: 'list' }));
    else if (c.editable) td.addEventListener('click', () => cellEdit(sec, r, c, td));
    tr.append(td);
  }
  return tr;
}
function cellEdit(sec, r, c, td) {
  if (td.querySelector('input, select')) return;
  const f = c.f;
  let el;
  if (f.type === 'select' || f.type === 'choice') {
    const opts = typeof f.opts === 'function' ? f.opts(r) : f.opts;
    el = h('select', {}, ...opts.map(([v, n]) => h('option', { value: v, selected: fieldGet(f, r) === v }, n)));
  } else el = h('input', { type: 'number', value: fieldGet(f, r), min: f.min ?? 0, max: f.max ?? (f.type === 'money' ? 99999999 : 999), step: f.type === 'money' ? 1000 : 1 });
  td.innerHTML = ''; td.append(el); el.focus(); el.select?.();
  let doneEdit = false;
  const commit = (keep) => {
    if (doneEdit) return; doneEdit = true;
    if (keep) {
      let v = Math.trunc(+el.value || 0);
      if (f.type !== 'select' && f.type !== 'choice') v = Math.max(f.min ?? 0, Math.min(f.max ?? (f.type === 'money' ? 99999999 : 99999), v));
      if (!setGuarded(sec, r, f, v)) { renderEntry(); return; }
      markDirty([sec.table]);
    }
    renderEntry();
  };
  el.addEventListener('keydown', (e) => { if (e.key === 'Enter') commit(true); if (e.key === 'Escape') { e.stopPropagation(); commit(false); } });
  el.addEventListener('blur', () => commit(true));
  if (el.tagName === 'SELECT') el.addEventListener('change', () => commit(true));
}
/* a value through the field's guard: -> true, or false with the reason in the status line */
function setGuarded(sec, r, f, v) {
  const before = clone(r);
  fieldSet(f, r, v);
  if (f.guard) {
    const err = GUARDS[f.guard](sec, r, before);
    if (err) { restore(r, before); status(err, 'err'); return false; }
  }
  return true;
}
function bulkBar(sec, TS, cols) {
  const sel = [...TS.sel].map((id) => recById(sec.table, id)).filter(Boolean);
  const numCols = cols.filter((c) => c.numeric);
  TS.bulk = TS.bulk || { col: (numCols.find((c) => c.key === 'price') || numCols[0] || {}).key, op: 'x', val: '0.9' };
  const B = TS.bulk;
  const preview = h('span', { class: 'bprev' });
  const calc = (c, r) => { const v = +fieldGet(c.f, r), x = +B.val; const n = B.op === 'x' ? Math.round(v * x) : B.op === '+' ? v + x : x; return Math.max(c.f.min ?? 0, Math.min(c.f.max ?? (c.f.type === 'money' ? 99999999 : 99999), Math.trunc(n))); };
  const roundMoney = (c, v) => (c.f.type === 'money' ? roundTo(v, 100) : v);
  const showPrev = () => {
    const c = numCols.find((x) => x.key === B.col);
    preview.textContent = c && B.val !== '' ? T('Preview: {list}', { list: sel.slice(0, 3).map((r) => `#${r.id} ${c.fmt(fieldGet(c.f, r), r)} -> ${c.fmt(roundMoney(c, calc(c, r)), r)}`).join(', ') + (sel.length > 3 ? ', ...' : '') }) : '';
  };
  const colSel = h('select', { 'data-bulk': 'col', onchange: (e) => { B.col = e.target.value; showPrev(); } }, ...numCols.map((c) => h('option', { value: c.key, selected: B.col === c.key }, c.label)));
  const opSel = h('select', { 'data-bulk': 'op', onchange: (e) => { B.op = e.target.value; showPrev(); } }, h('option', { value: 'x', selected: B.op === 'x' }, 'x'), h('option', { value: '+', selected: B.op === '+' }, '+'), h('option', { value: '=', selected: B.op === '=' }, '='));
  const val = h('input', { type: 'text', 'data-bulk': 'val', value: B.val, oninput: (e) => { B.val = e.target.value; showPrev(); } });
  const applyNum = () => {
    const c = numCols.find((x) => x.key === B.col);
    if (!c || B.val === '' || isNaN(+B.val)) { status(T('A number, please.'), 'err'); return; }
    let n = 0;
    for (const r of sel) { if (setGuarded(sec, r, c.f, roundMoney(c, calc(c, r)))) n++; }
    markDirty([sec.table]); renderEntry();
    const op = B.op === 'x' ? T('times') : B.op === '+' ? T('plus') : T('set to');
    status(TP(n, '{label} of {n} entry: {op} {val}.', '{label} of {n} entries: {op} {val}.', { label: c.label, op, val: B.val }), 'ok');
  };
  const setAll = (fkey, v, what) => {
    const f = fieldByKey(sec, fkey);
    let n = 0; const refused = [];
    for (const r of sel) { if (setGuarded(sec, r, f, v)) n++; else refused.push(r.id); }
    markDirty([sec.table]); renderEntry();
    status(refused.length ? TP(n, '{what}: {n} entry; kept in the game: {list} (see why on the entry)', '{what}: {n} entries; kept in the game: {list} (see why on the entry)', { what, list: refused.map((x) => '#' + x).join(' ') })
      : TP(n, '{what}: {n} entry.', '{what}: {n} entries.', { what }), refused.length ? 'err' : 'ok');
  };
  const market = fieldByKey(sec, 'offered') && fieldByKey(sec, 'offered').type === 'choice';
  const pool = fieldByKey(sec, 'pool');
  showPrev();
  return h('div', { class: 'box bulk' }, h('b', {}, T('{n} selected', { n: sel.length })),
    numCols.length ? h('span', { class: 'inline' }, colSel, opSel, val, h('button', { class: 'plate small green', 'data-bulk': 'apply', onclick: applyNum }, T('Apply'))) : null,
    market ? h('span', { class: 'inline' }, T('Offered:'), ...MARKET.map(([v, n]) => h('button', { class: 'plate small white', 'data-bulk-market': v, onclick: () => setAll('offered', v, T('Offered: {what}', { what: n })) }, n))) : null,
    pool ? h('span', { class: 'inline' }, T('Start pool:'), h('button', { class: 'plate small white', 'data-bulk-pool': 1, onclick: () => {
      const ok = sel.filter((r) => !poolBlock(sec.table, r));
      ok.forEach((r) => { r.flags2 |= POOL; });
      markDirty([sec.table]); renderEntry();
      status(sel.length > ok.length ? T('Start pool: {n} marked, {m} cannot join it.', { n: ok.length, m: sel.length - ok.length }) : T('Start pool: {n} marked.', { n: ok.length }), 'ok');
    } }, T('in|pool')), h('button', { class: 'plate small white', onclick: () => setAll('pool', 0, T('Out of the start pool')) }, T('out|pool'))) : null,
    h('button', { class: 'plate small grey', onclick: () => { TS.sel.clear(); renderEntry(); } }, T('Clear')), preview);
}
function tableInspector(sec, TS, cols, rows, ins) {
  if (sec.genres) {
    const gs = sec.genres();
    const priceOf = sec.key === 'films' ? (r) => r.price : (r) => seriesPrice(r.episodes_qtf, r.episode_price);
    const qOf = sec.key === 'films' ? (r) => r.quality : (r) => r.critics * 10;
    const tb = h('tbody');
    for (const [g, name] of gs) {
      const rs = records(sec.table).filter((r) => r.genre === g && !usage(sec, r).free);
      const avg = (fn) => (rs.length ? Math.round(rs.reduce((a, r) => a + fn(r), 0) / rs.length) : 0);
      tb.append(h('tr', { class: TS.genre === g ? 'on' : '', onclick: () => { TS.genre = TS.genre === g ? 0 : g; renderEntry(); } },
        h('td', {}, h('span', { class: 'gsq', style: `background:${sec.spine({ genre: g })}` }), name), h('td', { class: 'num' }, rs.length), h('td', { class: 'num' }, avg(qOf)), h('td', { class: 'num' }, numFmt(avg(priceOf)))));
    }
    ins.append(panel(T('All {many} by genre', { many: sec.many }), h('table', { class: 'gtable' }, h('thead', {}, h('tr', {}, h('th', {}, T('Genre')), h('th', { class: 'num' }, T('In game')), h('th', { class: 'num' }, sec.key === 'films' ? T('Q') : T('Critics')), h('th', { class: 'num' }, T('Price')))), tb),
      h('div', { class: 'hint pad' }, T('Averages of the entries in the game. Click a genre to filter the table.'))));
  } else {
    const nums = cols.filter((c) => c.numeric);
    const sum = (c) => rows.reduce((a, r) => a + (+c.get(r) || 0), 0);
    ins.append(panel(T('Shown entries'), h('div', { class: 'used' }, h('div', {}, TP(rows.length, '{n} entry', '{n} entries')),
      ...nums.map((c) => h('div', {}, T('{label}: total {sum}, average {avg}', { label: c.label, sum: numFmt(sum(c)), avg: rows.length ? numFmt(Math.round(sum(c) / rows.length)) : 0 }))))));
  }
  const saved = savedViews(sec);
  const box = h('div', { class: 'views' });
  (sec.presets || []).forEach((p, i) => box.append(h('button', { class: 'plate white wide' + (TS.preset === i ? ' on' : ''), onclick: () => { TS.preset = p.test ? i : null; TS.q = p.q || ''; if (p.sort) TS.sort = p.sort; renderEntry(); } }, p.label)));
  for (const v of saved) box.append(h('div', { class: 'inline' }, h('button', { class: 'plate white wide', onclick: () => { Object.assign(TS, { q: v.q, sort: v.sort, chips: Object.assign({}, v.chips), genre: v.genre || 0 }); renderEntry(); } }, v.name),
    h('button', { class: 'reset', 'data-tip': T('Forget this view'), onclick: () => { saveViews(sec, saved.filter((x) => x !== v)); renderEntry(); } }, icon('err', 12))));
  box.append(h('button', { class: 'plate grey wide dashed', onclick: () => {
    const name = h('input', { type: 'text', value: TS.q || T('My view') });
    dialog(T('Save the view'), h('div', { class: 'fields two' }, h('label', {}, T('Name')), name, h('div', { class: 'wide hint' }, (st.meta.window ? T('Filter, sorting and chips; kept on this computer.') : T('Filter, sorting and chips; kept in this browser.')))),
      [[T('Cancel'), 'grey'], [T('Save'), 'green', () => { saveViews(sec, saved.concat([{ name: name.value || T('view'), q: TS.q, sort: TS.sort, chips: Object.assign({}, TS.chips), genre: TS.genre }])); renderEntry(); }]]);
  } }, T('+ Save current view')));
  ins.append(panel(T('Views'), box));
}
function savedViews(sec) { try { return JSON.parse(localStorage.getItem('madtv-editor-views-' + sec.key) || '[]'); } catch (e) { return []; } }
function saveViews(sec, v) { try { localStorage.setItem('madtv-editor-views-' + sec.key, JSON.stringify(v)); } catch (e) { /* ignore */ } }

MODES.table = { get label() { return T('Table'); }, view: true, render: renderTable };

SELFTESTS.push(async function selftestTable(t) {
  const { ok, wait, typeInto, go2, W } = t;
  /* ---- B: the table, bulk change, undo */
  await go2('#films/table');
  ok($$('#work table.mix tbody tr').length === records('films').length, 'table: every film a row');
  typeInto($('#tq'), 'quality>60');
  await wait(400);
  const hi = records('films').filter((r) => r.quality > 60);
  ok($$('#work table.mix tbody tr').length === hi.length && hi.length > 0, `table filter quality>60 (${hi.length})`);
  const g2 = hi.filter((r) => r.genre === 2).length;   /* the genre box beside the filter, with it */
  typeInto(W('select[data-tgenre]'), '2');
  await wait(100);
  ok($$('#work table.mix tbody tr').length === g2 && W('select[data-tgenre]').value === '2', `table: the genre box (quality>60, genre 2: ${g2})`);
  $$('#inspector .gtable tbody tr')[2].click();             /* the inspector's genre table: genre 3, the box follows */
  await wait(100);
  ok(W('select[data-tgenre]').value === '3' && $$('#work table.mix tbody tr').length === hi.filter((r) => r.genre === 3).length, 'table: a genre clicked beside it sets the box');
  typeInto(W('select[data-tgenre]'), '0');
  await wait(100);
  const ids = $$('#work table.mix tbody tr').slice(0, 2).map((tr) => +tr.dataset.id);
  const before = ids.map((id) => recById('films', id).price);
  $(`#work tr[data-id="${ids[0]}"] .selbox`).click();
  $(`#work tr[data-id="${ids[1]}"] .selbox`).click();
  typeInto(W('select[data-bulk="col"]'), 'price'); typeInto(W('select[data-bulk="op"]'), 'x'); typeInto(W('input[data-bulk="val"]'), '0.5');
  W('[data-bulk="apply"]').click();
  ok(ids.every((id, k) => recById('films', id).price === roundTo(Math.round(before[k] * 0.5), 100)), 'bulk: two prices halved');
  undo();
  ok(ids.every((id, k) => recById('films', id).price === before[k]), 'undo: the prices back');
  redo(); undo();
  st.view.films = 'table'; renderAll();
  const td = W(`tr[data-id="${ids[0]}"] td[data-col="blocks"]`);
  td.click();
  const ed = td.querySelector('input');
  ed.value = '3'; ed.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' }));
  ok(recById('films', ids[0]).blocks === 3, 'table: a cell edited in place');
  undo();
  /* the audience by hour: computed from the record as it is now (any pack, unsaved changes) */
  const x = records('films').find((r) => r.flags2 & 8 && r.topicality > 0);
  typeInto($('#tq'), '#' + x.id);
  await wait(400);
  const cell = W(`tr[data-id="${x.id}"] td[data-col="solo"]`), bars = cell ? $$('.strip i', cell) : [];
  ok(bars.length === 7 && bars.slice(0, 3).every((b) => b.classList.contains('risk') || b.classList.contains('zero')) && !bars[4].classList.contains('risk')
    && +cell.querySelector('.strip').dataset.max === Math.max(...filmSolo(x)), `table: audience 18-24 of film ${x.id}, X rated: 18-20 hatched`);
  const topWas = x.topicality;
  x.topicality = 1; markDirty(['films']); renderEntry();
  ok(+W(`tr[data-id="${x.id}"] td[data-col="solo"] .strip`).dataset.max === Math.max(...filmSolo(x)) && Math.max(...filmSolo(x)) < Math.max(...soloShare(topWas, x.genre)), 'table: the strip follows a change');
  undo();
  typeInto($('#tq'), '');
  await wait(400);
});
