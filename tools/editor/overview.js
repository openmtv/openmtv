/* OpenMTV editor -- the overview: the pack being changed (its title, where it came from) or the
 * selection that is shown, what it changes against the original per section and per entry,
 * does a new game start; beside it every pack.  A protected pack is changed in place only
 * after "Change it directly" here (for this visit of the page). */
'use strict';

function renderOverview(w, ins) {
  const m = st.mod ? modInfo(st.mod) || { name: st.mod } : null;
  const one = st.packs.length === 1 ? modInfo(st.packs[0]) : null;
  const head = h('div', { class: 'pagehead' });
  const ph = h('div', { class: 'ph' });
  const actions = h('div', { class: 'actions' });
  if (m) {
    ph.append(h('div', { class: 'overline' }, T('Pack · mods/{name}', { name: m.name })), h('h1', {}, m.title || m.name),
      h('div', { class: 'sub' }, m.complete ? T('Holds {list}, made in the editor.', { list: [T('Original')].concat((m.from || []).map(packTitle)).join(' + ') }) : m.from?.length ? T('Started from {list}.', { list: m.from.map(packTitle).join(', ') }) : T('Played on the original game (film 200 repaired).')));
  } else {
    ph.append(h('div', { class: 'overline' }, T('Selection')), h('h1', {}, st.packs.length ? selectionList() : T('The original game')),
      h('div', { class: 'sub' }, one && one.protected ? T('"{title}" is protected: the first change makes a new pack with all of it; this one stays as it is.', { title: packTitle(one.name) })
        : T('The first change makes a new pack with all of it; the packs chosen stay as they are.')));
    if (one && one.protected) actions.append(h('button', { class: 'plate small', id: 'unlock', onclick: unlockDialog }, icon('unlock', 14), T('Change it directly ...')));
  }
  if (st.dirty.size) actions.append(h('button', { class: 'plate small', onclick: discardDialog }, T('Discard unsaved')));
  if (m) actions.append(h('button', { class: 'plate small', onclick: () => resetDialog(null) }, icon('reset', 14), T('Back to the original ...')));
  if (m && !m.protected) actions.append(h('button', { class: 'plate small', id: 'deletepack', onclick: () => deleteDialog(st.mod) }, icon('remove', 14), T('Delete pack ...')));
  actions.append(h('button', { class: 'plate small', onclick: () => { st.section = 'launcher'; renderAll(); } }, T('Choose packs')));
  head.append(ph, actions);
  const page = h('div', { class: 'page' }, head);
  if (m) {
    const title = h('input', { type: 'text', id: 'modtitle', value: m.title || m.name });
    const desc = h('textarea', { id: 'moddesc', rows: 2, placeholder: T('What this pack changes, for whoever plays it') });
    desc.value = m.description || '';
    const saveInfo = async () => {
      try {
        const info = await api('POST', `/api/mods/${st.mod}/info`, { title: title.value.trim() || st.mod, description: desc.value });
        st.mods = st.mods.map((x) => (x.name === st.mod ? info : x));
        status(T('Pack title and description saved.'), 'ok');
      } catch (e) { status(e.message, 'err'); }
    };
    title.addEventListener('change', saveInfo); desc.addEventListener('change', saveInfo);
    page.append(h('div', { class: 'fieldsform' }, h('label', { for: 'modtitle' }, T('Title')), title, h('label', { for: 'moddesc' }, T('Description')), desc));
  }
  /* per section, then per entry */
  const tiles = h('div', { class: 'tiles', role: 'list' });
  const per = h('div', { class: 'entrydiffs' });
  for (const sec of SECTIONS) {
    const ch = records(sec.table).filter((r) => changed(sec.table, r));
    const added = ch.filter((r) => !origById(sec.table, r.id)).length;
    tiles.append(h('button', { class: 'tile secrow' + (ch.length ? ' chg' : ''), role: 'listitem', 'data-sec': sec.key, onclick: () => { filt(sec).chips.changed = !!ch.length; go(sec.key, ch[0]?.id, { view: 'list' }); } },
      h('span', { class: 'tl1' }, h('span', {}, sec.label), st.dirty.has(sec.table) ? h('span', { class: 'pill yellow' }, T('not saved')) : null),
      ch.length ? h('span', { class: 'big' }, String(ch.length), ' ', h('small', {}, T('of {n} changed', { n: records(sec.table).length }))) : h('span', { class: 'sm' }, T('as the original · {n}', { n: records(sec.table).length })),
      added ? h('span', { class: 'sm' }, T('{n} added', { n: added })) : null));
    if (!ch.length) continue;
    per.append(h('div', { class: 'pgt' }, sec.label));
    for (const r of ch.slice(0, 200)) {
      const o = origById(sec.table, r.id);
      const d = o ? diffs(sec, r) : [];
      per.append(h('div', { class: 'erow' }, h('button', { class: 'link', onclick: () => go(sec.key, r.id, { view: 'list' }) }, `#${r.id}`), h('span', { class: 'et' }, titleOf(sec, r)),
        h('span', { class: 'ed' }, o ? d.slice(0, 4).map((x) => `${x.label} ${x.from} -> ${x.to}`).join('; ') + (d.length > 4 ? '; ' + T('{n} more', { n: d.length - 4 }) : '') : T('added in this pack')),
        o ? h('button', { class: 'plate small white', 'data-revert': `${sec.key}-${r.id}`, onclick: () => { const recs = st.data[sec.table].records; recs[recs.indexOf(r)] = clone(o); markDirty([sec.table]); renderEntry(); status(T('#{id} is the original again.', { id: r.id })); } }, T('Revert')) : h('span')));
    }
  }
  page.append(tiles, h('section', { class: 'box' }, h('div', { class: 'boxtitle' }, T('Changes against the original')),
    per.childNodes.length ? per : h('div', { class: 'hint pad' }, T('Nothing changed yet.'))));
  w.append(page);
  /* beside it: does a new game start, every pack */
  const c = startCounts();
  const checks = [
    { level: c.films >= 9 ? 'ok' : 'err', msg: T('Films for the stations: {n} free, 9 needed', { n: c.films }) },
    { level: c.series >= 3 ? 'ok' : 'err', msg: c.series === 3 ? T('Series for the stations: {n} free, 3 needed (removing one more hangs a new game)', { n: c.series }) : T('Series for the stations: {n} free, 3 needed', { n: c.series }) }];
  const probs = allProblems();
  const font = probs.filter((p) => p.kind === 'font'), wide = probs.filter((p) => p.kind === 'card');
  const inc = probs.filter((p) => p.level === 'err' && p.sec);
  checks.push(font.length ? { level: 'warn', msg: TP(font.length, "{n} changed text with characters the game's font lacks", "{n} changed texts with characters the game's font lacks") } : { level: 'ok', msg: T("All changed texts fit the game's character set") });
  checks.push(wide.length ? { level: 'warn', msg: TP(wide.length, '{n} changed text wider or longer than the card', '{n} changed texts wider or longer than the card') } : { level: 'ok', msg: T('All changed texts fit their cards') });
  checks.push(inc.length ? { level: 'err', msg: TP(inc.length, '{n} incomplete entry: not on air until filled in', '{n} incomplete entries: not on air until filled in') } : { level: 'ok', msg: T('Every entry in the game is complete') });
  ins.append(panel(T('Will a new game start?'), h('div', { class: 'checks-list' }, ...checks.map((x) => checkRow(x)),
    ...probs.filter((p) => p.sec).slice(0, 30).map((p) => checkRow(p, () => go(p.sec, p.id, { view: 'list' }))))));
  const list = h('div', { class: 'used', id: 'modlist' });
  for (const x of st.mods) {
    const n = Object.values(x.changed || {}).reduce((a, b) => a + b, 0);
    list.append(h('div', { class: 'modrow2' }, h('button', { class: 'modc' + (st.packs.includes(x.name) ? ' sel' : ''), 'data-mod': x.name, disabled: !!x.error, onclick: async () => { if (await selectPacks([x.name])) { st.section = 'overview'; renderAll(); } } },
      h('span', { class: 'mi' }, icon(x.protected ? 'lock' : 'folder', 16)),
      h('span', { class: 'mt' }, h('b', {}, x.title || x.name), x.error ? h('span', { class: 'err' }, x.error) : h('span', {}, `mods/${x.name}`)),
      h('span', { class: 'mn' }, n ? T('{n} changed', { n }) : '')),
      x.protected ? null : h('button', { class: 'reset packdel', 'data-delete': x.name, 'aria-label': T('Delete "{title}" ...', { title: x.title || x.name }), 'data-tip': T('Delete "{title}" ...', { title: x.title || x.name }), onclick: () => deleteDialog(x.name) }, icon('remove', 14))));
  }
  list.append(h('button', { class: 'plate small', style: 'align-self:flex-start;margin-top:6px', onclick: newModDialog }, icon('plus', 14), T('New pack')));
  ins.append(panel(T('All packs'), list));
}
/* a pack of the player's own deleted (any of the lists: launcher, overview): its folder goes, its
   build folder (saved games) stays; taken out of the selection if it is in it */
function deleteDialog(name = st.mod) {
  const m = modInfo(name);
  if (!m || m.protected) return;
  const unsaved = st.mod === name && st.dirty.size;
  dialog(T('Delete pack'), h('div', {}, T('Delete "{title}"?', { title: packTitle(name) }), h('br'),
    h('span', { class: 'hint' }, unsaved ? T('Its folder mods/{name} goes, unsaved changes too; build/mods/{name} with the games saved in it stays.', { name })
      : T('Its folder mods/{name} goes; build/mods/{name} with the games saved in it stays.', { name }))),
  [[T('Cancel'), 'grey'], [T('Delete'), 'red', async () => {
    try {
      await api('DELETE', `/api/mods/${name}`);
      st.mods = await api('GET', '/api/mods');
      if (st.packs.includes(name)) { st.dirty.clear(); await loadSelection(st.packs.filter((n) => n !== name), { quiet: true }); }
      else renderAll();
      status(T('"{title}" deleted.', { title: m.title || name }), 'ok');
    } catch (e) { status(e.message, 'err'); return false; }
  }]]);
}
/* a protected pack changed in place: on purpose, for this visit of the page */
function unlockDialog() {
  const name = st.packs[0];
  dialog(T('Change it directly'), h('div', {}, T('Change "{title}" itself instead of making a new pack?', { title: packTitle(name) }), h('br'),
    h('span', { class: 'hint' }, T('For packs that are kept up to date as they are (the content packs). It stays unlocked until the page is loaded again.'))),
  [[T('Cancel'), 'grey'], [T('Unlock'), 'red', () => {
    st.unlocked.add(name); st.mod = editTarget(); renderAll();
    status(T('"{title}" is unlocked: changes go into it.', { title: packTitle(name) }), 'ok');
  }]]);
}

MODES.overview = { page: true, get label() { return T('Overview'); }, render: renderOverview };

SELFTESTS.push(async function selftestOverview(t) {
  const { ok, go2 } = t;
  /* ---- the overview: changes per section, every pack, build only (no start) */
  await go2('#overview');
  ok($$('#work .tile').length === SECTIONS.length && $$('#work .tile')[0].textContent.includes(T('of {n} changed', { n: records('films').length })) && $('#modlist .modc.sel')?.dataset.mod === 'selftest', 'overview: changes per section, the packs');
  ok($$('#work .erow').length >= 10, 'overview: every changed entry with its fields (' + $$('#work .erow').length + ')');
  ok(await buildOnly() && /build\/mods\/selftest/.test($('#playlog') ? $('#playlog').textContent : st.log.map((x) => T(x.key, x.params)).join(' ')), 'build only: the log names build/mods/selftest');
});
