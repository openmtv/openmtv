/* OpenMTV editor -- self-test: /?selftest
 * edits through the page's own inputs in a pack "selftest", saves, reads the stored tables
 * back, then tries the modes (monitor, table, the card, overview, story tree, launcher,
 * multiplayer: a lobby of its own with a second player of the server's, dry), a
 * new pack made by the first change of a selection (then deleted again), then
 * the German interface (selftestGerman); the result goes into the page title (headless
 * Chrome: chrome --headless --dump-dom 'http://127.0.0.1:8641/?selftest#editor').  It never starts
 * the game (the launcher only dry); it builds the pack once (build/mods/selftest).  The
 * player's chosen packs are put back at the end.  It runs in
 * English unless ?ui=de is given (then all of it in German); the checks name the page's
 * texts through T(). */
'use strict';

async function selftest() {
  if (player()) return playerSelftest();
  const log = [];
  const ok = (cond, what) => log.push((cond ? 'ok ' : 'FAIL ') + what);
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const typeInto = (el, v) => { el.value = v; el.dispatchEvent(new Event(el.tagName === 'SELECT' ? 'change' : 'input')); };
  const go2 = async (hash) => { location.hash = hash; fromHash(); renderAll(); await wait(50); };
  const W = (s) => $('#work ' + s);
  const confirmDialog = async (label) => {                   /* press a button of the open dialog */
    const b = [...document.querySelectorAll('#overlay button')].find((x) => x.textContent === label);
    if (b) b.click();
    await wait(300);
    return !!b;
  };
  let keptSel = null;
  try { keptSel = localStorage.getItem(PACKS_KEY); } catch (e) { /* ignore */ }
  try {
    if (st.startedAt) {                   /* (the player's launcher: it starts as this page, at "Play") */
      ok(st.section === st.startedAt.slice(1) && location.hash === st.startedAt && !document.body.classList.contains('player') && !!$('#rail .nameplate[data-key="films"]')
        && !$('#rail [data-key="editor"]') && $('#subtitle').hidden && !!$('#save').offsetParent,
        `the start: the editor's board at ${st.startedAt}, the contents on it, no "Editor" to press`);
      ok(!!$('#rail #quitlauncher') === !st.meta.window, st.meta.window ? 'the start: in its own window no button to end it' : 'the start: in the browser a button to end it');
    }
    const films = secByKey('films');
    if (!st.mods.find((m) => m.name === 'selftest')) {
      await api('POST', '/api/mods', { name: 'selftest', title: 'self-test' });
      st.mods = await api('GET', '/api/mods');
    }
    await loadMod('selftest');
    st.lang = 'en'; st.plang = null;
    ok(st.mod === 'selftest', 'an own pack chosen alone is changed in place');
    await go2('#films/31/form');
    ok(W('[data-text="0-en"]') && W('[data-text="0-de"]') && W('[data-text="1-en"]') && W('[data-text="1-de"]'), 'texts in both languages side by side');
    ok(!/null|undefined/.test($('#shelf').textContent + $('#workhead').textContent + $('#work').textContent + $('#inspector').textContent), 'no stray null in the page');
    typeInto(W('[data-text="0-en"]'), 'Licence revoked');
    typeInto(W('[data-text="1-en"]'), 'Dalton\'s Bond, now with a longer description that wraps over several lines of the card.');
    typeInto(W('input[data-field="quality"]'), '80');
    typeInto(W('select[data-field="c13"]'), '4');                          /* country: France */
    const d31 = diffs(films, recById('films', 31)).map((x) => x.label).join(', ');
    ok(d31 === [T('Title') + ' (EN)', T('Description') + ' (EN)', T('Quality'), T('Country')].join(', ') && $$('#inspector .diff').length === 4, 'the inspector lists the changed fields (' + d31 + ')');
    await go2('#series/2/form');
    st.epText = true; renderEntry();
    typeInto(W('[data-text="12-en"]'), 'A new episode text');              /* episode 1 text */
    await go2('#ads/2/form');
    typeInto(W('select[data-rule="sign"]'), 'no');                         /* none of these ... */
    typeInto(W('select[data-rule="0"]'), '7');                             /* ... live broadcasts */
    await go2('#news/2');
    typeInto(W('[data-text="h-en"]'), '#1: Government falls.');
    ok(st.dirty.size === 4, 'four tables changed (' + [...st.dirty].join(' ') + ')');
    /* the preview follows the language being edited */
    await go2('#films/31/form');
    W('[data-text="0-de"]').dispatchEvent(new Event('focus'));
    const f31 = recById('films', 31);
    ok(editLang() === 'de' && filmRows(f31, editLang())[0].text === f31.text.de[0], 'German preview (focus on a German field)');
    $('.lang[data-lang="en"]').click();
    ok(editLang() === 'en' && $('.lang[data-lang="en"]').classList.contains('on') && $('#inspector .phead').textContent.endsWith('EN'), 'the game texts\' switch sets the preview back');
    /* remove a film, copy another into the free slot; a screenplay into an unused slot */
    await go2('#films/100');
    const blocked = usage(films, recById('films', 29)).keep.length > 0 && usage(films, recById('films', 52)).keep.length > 0;
    ok(blocked, 'special and produced films cannot be removed');
    removeAsk(films, recById('films', 100));
    ok(usage(films, recById('films', 100)).free, 'film 100 removed (free slot)');
    copyEntry(films, recById('films', 31));
    ok(await confirmDialog(T('Slot #{id}', { id: 100 })), 'copy asks first');
    ok(st.sel.films === 100 && recById('films', 100).text.en[0] === 'Licence revoked 2' && (recById('films', 100).flags & 0x20), 'film 31 copied into slot 100');
    await go2('#screenplays/2');
    copyScreenplay(recById('screenplays', 2));
    await confirmDialog(T('Copy'));
    const sp = recById('screenplays', st.sel.screenplays), tf = spTarget(sp);
    ok(sp.id === 25 && (sp.flags & 2) && tf && tf.id === 139 && tf.text.en[0] === recById('films', 52).text.en[0] + ' 2' && !(tf.flags & 0x20), 'screenplay 2 copied into unused slot 25 (film 139)');
    /* films beyond the original's 200: copies as new films, an added one deleted again */
    await go2('#films/31/form');
    copyEntry(films, recById('films', 31));
    ok(await confirmDialog(films.newOne(201)), 'copy offers a new film');
    copyEntry(films, recById('films', 31));
    await confirmDialog(films.newOne(202));
    ok(recById('films', 201) && st.sel.films === 202 && (recById('films', 202).flags & 0x20) && recById('films', 202).text.de[0] === origById('films', 31).text.de[0] + ' 2', 'films 201 and 202 added');
    removeAsk(films, recById('films', 202));
    deleteAdded(films, recById('films', 202));
    await confirmDialog(T('Delete'));
    ok(!recById('films', 202) && nextId('films') === 202, 'added film 202 deleted');
    /* a new, empty film: its required fields are marked until they are filled */
    const newBtn = $('#newentry');
    ok(newBtn && newBtn.getAttribute('aria-label') === films.newOne(202), 'the list offers "New film #202"');
    newBtn.click();
    await wait(200);
    const nf = recById('films', st.sel.films);
    ok(nf && nf.id === 202 && (nf.flags & 0x20) && missing(films, nf).length === 5 && document.querySelectorAll('#work .missing').length === 5,
      'new film 202: required fields marked (' + (nf ? missing(films, nf).map((x) => x.key) : '-') + ')');
    ok(missing(films, recById('films', 164)).length === 0, 'the original\'s own gaps do not count (film 164)');
    typeInto(W('[data-text="0-en"]'), 'Selftest Film');
    typeInto(W('[data-text="1-en"]'), 'A film made by the self-test.');
    typeInto(W('[data-text="0-de"]'), 'Selbsttest-Film');
    typeInto(W('[data-text="1-de"]'), 'Ein Film aus dem Selbsttest.');
    ok(incomplete().length === 1 && incomplete()[0].endsWith(': ' + T('price')), 'only the price missing (' + incomplete() + ')');
    renderStatusBar();
    ok(allProblems().some((p) => p.level === 'err' && p.id === 202 && p.msg.includes(T('price'))) && $('#rail-problems .count').textContent === String(allProblems().length) && $('#rail-problems .count').classList.contains('err')
      && $('#reset').textContent === T('Not saved'), 'the rail counts the problems, the top bar says "not saved" (' + $('#rail-problems .count').textContent + ')');
    $('#rail-problems').click();
    ok($$('#overlay .check').length === allProblems().length && /#202/.test($('#overlay').textContent), 'the problems dialog lists them');
    await confirmDialog(T('Close'));
    typeInto(W('input[data-field="price"]'), '120000');
    await wait(200);
    ok(!missing(films, nf).length && !document.querySelectorAll('#work .missing').length && !document.querySelector('#list .tape.incomplete'), 'new film complete');
    /* one series in the starting pool is enough (the original pool fills up); not an unfinished one */
    await go2('#series/5/form');
    W('[data-field="pool"]').click();
    ok(!gameChecks().length && seriesPool().length === 1 && seriesPool()[0].id === 5, 'one series in the starting pool');
    ok(poolBlock('series', recById('series', 40)) !== '', 'an unfinished series cannot join it');
    /* new series and ads (the engine takes up to #999 of each): their required fields too; deleted again */
    for (const key of ['series', 'ads']) {
      const sec = secByKey(key);
      if (!canGrow(sec)) { ok(!$('#shelffoot') || key !== st.section, `no New ${sec.what} while the engine takes ${st.meta.tables[key].max - 1}`); continue; }
      await go2('#' + key + '/1');
      newEntry(sec);
      await wait(200);
      const r = recById(sec.table, st.sel[key]), miss = r ? missing(sec, r) : [];
      ok(r && r.id === 51 && miss.some((x) => x.key === 'title-en') && document.querySelectorAll('#work .missing').length > 0 && incomplete().some((x) => x.includes('#51')),
        `new ${sec.what} #51: required fields marked (${miss.map((x) => x.key)})`);
      deleteAdded(sec, r);
      await confirmDialog(T('Delete'));
      ok(!recById(sec.table, 51), `new ${sec.what} #51 deleted again`);
    }
    await selftestScreenplays({ ok, wait, typeInto, go2, W, confirmDialog });
    ok(await save(), 'saved');
    ok((await api('GET', '/api/mods/selftest/films')).records.some((x) => x.id === 201), 'film 201 stored');
    const f = (await api('GET', '/api/mods/selftest/films')).records.find((x) => x.id === 31);
    ok(f.text.en[0] === 'Licence revoked' && f.quality === 80 && unkGet(f, 0x13) === 4, 'film 31 stored');
    ok(f.text.de[0] === origById('films', 31).text.de[0], 'German title untouched');
    const s = (await api('GET', '/api/mods/selftest/series')).records.find((x) => x.id === 2);
    ok(s.text.en[12] === 'A new episode text', 'series 2 episode text stored');
    const a = (await api('GET', '/api/mods/selftest/ads')).records.find((x) => x.id === 2);
    ok(a.genre_rule[0] === -7, 'ad 2 refuses live broadcasts (' + a.genre_rule + ')');
    const n = (await api('GET', '/api/mods/selftest/news_nodes')).records.find((x) => x.id === 2);
    ok(n.text.en === '#1: Government falls.', 'news 2 stored');
    const m = (await api('GET', '/api/mods')).find((x) => x.name === 'selftest');
    ok(m && m.changed.films === 5 && m.changed.series === 2 && m.changed.ads === 1 && m.changed.news_nodes === 1 && m.changed.screenplays === 1, 'mod counts its changes (' + JSON.stringify(m && m.changed) + ')');
    ok(recById('films', 200).text.en[0] === 'Prince of Darkness' && recById('films', 200).text.de[0] === 'Die Fürsten der Dunkelheit', 'film 200 repaired (data fix)');

    /* ---- the year from 2000 on (byte +0x14 = year - 1900; the game prints 20nn from 100) */
    await go2('#films/31/form');
    typeInto(W('input[data-field="year"]'), '2005');
    const origin = (r) => filmRows(r, 'en').find((x) => x.edit === 'origin').segs.map((x) => x.s || '').join('');
    ok(unkGet(f31, 0x14) === 105 && origin(f31).endsWith('2005'), 'year 2005 on the card (' + origin(f31) + ')');
    typeInto(W('input[data-field="year"]'), '1989');
    ok(unkGet(f31, 0x14) === 89 && origin(f31).endsWith('1989'), 'year 1989 on the card');
    /* ---- a changed field shows the original value and resets alone */
    const qrow = W('input[data-field="quality"]').closest('.ctl').nextElementSibling;
    ok(qrow.textContent.includes(T('was {value}', { value: 65 })), 'changed field shows its original value (' + qrow.textContent + ')');
    qrow.querySelector('.reset').click();
    ok(f31.quality === 65 && f31.text.en[0] === 'Licence revoked' && !W('input[data-field="quality"]').closest('.ctl').nextElementSibling.textContent, 'one field back to the original, the others stay');
    /* ---- the market: one choice */
    const choose = (v) => W(`[data-field="offered"] button[data-v="${v}"]`).click();
    choose(0x20);
    ok((f31.flags & 0x60) === 0x20 && W('[data-field="offered"] button.on').dataset.v === '32', 'market choice: film dealer');
    choose(0x60);
    ok((f31.flags & 0x60) === 0x60, 'market choice: auction');
    await go2('#films/29/form');
    choose(0);
    ok((recById('films', 29).flags & 0x20) && $('#status').classList.contains('err'), 'market choice: the live football stays in the game');
    /* ---- global search */
    typeInto($('#gsearch'), 'Goldfinger');
    $('#gresults .gres').click();
    ok(st.section === 'films' && st.sel.films === 33, 'search: Goldfinger -> film 33');

    /* ---- the modes, each with its own checks (monitor.js, overview.js, table.js, stage.js, tree.js) */
    const modeScripts = $$('script[src]').map((x) => x.getAttribute('src')).filter((x) => !/\/(i18n|game|schema|editor|selftest)\.js$/.test(x) && !x.startsWith('/wails/'));     /* (the launcher's own window adds its runtime's) */
    ok(SELFTESTS.length === modeScripts.length && !PAGE_ERRORS.length, `every mode loaded: ${modeScripts.join(' ')}` + (PAGE_ERRORS.length ? ' -- ' + PAGE_ERRORS.join('; ') : ''));
    for (const fn of SELFTESTS) await fn({ ok, wait, typeInto, go2, W, confirmDialog });
    await selftestPacks({ ok, wait, typeInto, go2, W, confirmDialog });
    await selftestGerman({ ok, wait, typeInto, go2, W, confirmDialog });

    /* "back to the original" is not saved at once; "Reset" in the header brings the saved mod back */
    const m1 = (await api('GET', '/api/mods')).find((x) => x.name === 'selftest');
    resetDialog();
    await confirmDialog(T('Everything'));
    await wait(300);
    const m1b = (await api('GET', '/api/mods')).find((x) => x.name === 'selftest');
    ok(m1 && Object.keys(m1.changed).length && JSON.stringify(m1b.changed) === JSON.stringify(m1.changed) && st.dirty.size,
      'reset: back to the original, not saved yet (' + JSON.stringify(m1.changed) + ' still in the file)');
    $('#reset').click();
    await confirmDialog(T('Discard'));
    await wait(500);
    const back = await Promise.all(Object.keys(m1.changed).map(async (t) => JSON.stringify((await api('GET', `/api/mods/selftest/${t}`)).records) === JSON.stringify(st.data[t].records)));
    ok(!st.dirty.size && back.length && back.every(Boolean), 'reset: the header\'s Reset brings the saved mod back');
    resetDialog();
    await confirmDialog(T('Everything'));
    await save();
    await wait(500);
    const m2 = (await api('GET', '/api/mods')).find((x) => x.name === 'selftest');
    ok(m2 && !Object.keys(m2.changed).length, 'reset: everything original again after Save (' + JSON.stringify(m2 && m2.changed) + ')');
  } catch (e) { log.push('FAIL ' + e.message); console.error(e); }
  try { if (keptSel === null) localStorage.removeItem(PACKS_KEY); else localStorage.setItem(PACKS_KEY, keptSel); } catch (e) { /* ignore */ }
  document.title = 'SELFTEST ' + (log.some((x) => x.startsWith('FAIL')) ? 'FAIL' : 'OK') + ': ' + log.join('; ');
}

/* ---- screenplays: an unfinished one offered is incomplete until its production is filled in
   through the form (roles, lists), the bytes where the game reads them; a series' lists follow
   its episodes; the entries are the original again at the end (the pack stays complete) */
async function selftestScreenplays({ ok, wait, typeInto, go2, W }) {
  const sec = secByKey('screenplays'), keys = (r) => missing(sec, r).flatMap((x) => x.keys || [x.key]);
  const back = (table, id) => { const recs = st.data[table].records, r = recById(table, id); recs[recs.indexOf(r)] = clone(origById(table, id)); markDirty([table]); };
  ok(!missing(sec, recById('screenplays', 20)).length && missing(sec, recById('screenplays', 20), true).length === 2, 'screenplay 20: its roles without requirements are the original\'s own gap');
  ok(records('screenplays').filter((x) => x.flags & 2 || x.id === 24).every((x) => !missing(sec, x).length), 'every screenplay the game offers is complete');
  /* a show never offered: 26 */
  await go2('#screenplays/26/form');
  const r = recById('screenplays', 26);
  W('[data-field="offered"] button').click();
  await wait(50);
  ok(['price', 'desc-en', 'desc-de', 'ep1-quality', 'ep1-list'].every((k) => keys(r).includes(k)) && W('[data-ep-q="1"]').classList.contains('missing') && W('[data-ep-list="1"]').classList.contains('missing')
    && $('#work .tab[data-tab="lists"] .tmark').classList.contains('miss') && incomplete().some((x) => x.includes('#26')), 'screenplay 26 offered: incomplete, its list and base quality marked (' + keys(r) + ')');
  typeInto(W('input[data-field="price"]'), '60000');
  typeInto(W('[data-text="0-en"]'), 'Contestants take on their next of kin.');
  typeInto(W('[data-text="0-de"]'), 'Kandidaten treten gegen ihre Verwandten an.');
  ok(W('tr[data-role="8"]') && !W('tr[data-role="9"]') && W('select[data-role-cat="1"]').value === '5', 'its eight roles of the original (categories only), the first a show master');
  W('[data-item-add="1"]').click();
  await wait(50);
  typeInto(W('input[data-ep-q="1"]'), '40');
  ok(spList(r, 1).length === 1 && spList(r, 1)[0].item === 1 && same(keys(r), ['role1-req']) && W('[data-role-req="1"]').classList.contains('missing'), 'an entry: role 1, weight 5; now its requirements are missing (' + keys(r) + ')');
  ok(W('[data-role-del="1"]').disabled && !W('[data-role-del="8"]').disabled, 'a role in a list cannot be removed, one in no list can');
  typeInto(W('input[data-req="1-0"]'), '12');
  typeInto(W('input[data-req="1-3"]'), '25');
  ok(!missing(sec, r).length && !document.querySelectorAll('#work .missing').length && !incomplete().some((x) => x.includes('#26')), 'screenplay 26 complete');
  ok(unkGet(r, 0x08) === 40 && unkGet(r, 0x0C) === 1 && unkGet(r, 0x0D) === 5 && unkGet(r, 0x0F) === 0 && unkGet(r, 0x168) === 12 && unkGet(r, 0x16B) === 20 && unkGet(r, 0x1A4) === 5
    && Object.keys(r).join() === 'id,price,state,flags,target,duration_hours,unknown,text' && Object.keys(r.unknown).join() === '0x08,0x168', 'the bytes where the game reads them, the record in the export\'s order');
  /* four sets in the first list: no studio takes it */
  for (const item of [17, 18, 19, 20]) { W('[data-item-add="1"]').click(); await wait(20); typeInto(W(`select[data-item="1-${spList(r, 1).length}"]`), String(item)); }
  ok(keys(r).includes('ep1-sets') && W('.spep[data-ep="1"] .spephead .hint.err'), 'four sets in the first list: refused (a studio holds 3)');
  typeInto(W('select[data-item="1-5"]'), '17');
  ok(entryChecks(sec, r).some((c) => c.level === 'warn' && c.msg.includes(propName(17))) && W('select[data-item="1-2"]').classList.contains('warned')
    && W('select[data-item="1-5"]').classList.contains('warned') && !W('select[data-item="1-3"]').classList.contains('warned'), 'the same set twice: a warning, both entries marked');
  W('[data-item-del="1-5"]').click();
  await wait(20);
  ok(!missing(sec, r).length && spList(r, 1).map((x) => x.item).join() === '1,17,18,19', 'one set out again: complete');
  const d26 = diffs(sec, r).map((x) => x.key);
  ok(['offered', 'price', 'role1', 'ep1'].every((k) => d26.includes(k)) && !d26.includes('unknown'), 'the changes name the role and the list, no "other bytes" (' + d26 + ')');
  back('screenplays', 26);
  ok(!changed('screenplays', recById('screenplays', 26)), 'screenplay 26 the original again');
  /* a weight changed and set back: the original again (same JSON) */
  await go2('#screenplays/8/form');
  st.tab.screenplays = 'lists'; renderEntry();
  typeInto(W('input[data-weight="1-1"]'), '3');
  ok(changed('screenplays', recById('screenplays', 8)), 'screenplay 8: a weight changed');
  typeInto(W('input[data-weight="1-1"]'), '10');
  ok(!changed('screenplays', recById('screenplays', 8)), 'screenplay 8: set back, the original again');
  await go2('#screenplays/11/form');
  st.tab.screenplays = 'lists'; renderEntry();
  const w11 = spList(origById('screenplays', 11), 4)[0].weight;
  typeInto(W('input[data-weight="4-1"]'), String(w11 === 1 ? 2 : 1));
  typeInto(W('input[data-weight="4-1"]'), String(w11));
  ok(!changed('screenplays', recById('screenplays', 11)), 'screenplay 11: the bytes past the end of its fourth list stay as they were');
  const n11 = spList(recById('screenplays', 11), 4).length;
  W('[data-item-add="4"]').click();
  await wait(20);
  W(`[data-item-del="4-${n11 + 1}"]`).click();
  await wait(20);
  ok(spList(recById('screenplays', 11), 4).length === n11 && !changed('screenplays', recById('screenplays', 11)), 'screenplay 11: an entry added and taken out again -- the original again');
  st.tab.screenplays = 'all';
  /* a series never offered: 38 -- a list for each of the 10 episodes of series 38 */
  await go2('#screenplays/38/form');
  st.tab.screenplays = 'all'; renderEntry();
  const s = recById('screenplays', 38);
  W('[data-field="offered"] button').click();
  const m38 = missing(sec, s).map((x) => x.msg);
  ok(m38.includes(T('episode length of series {id}', { id: 38 })) && m38.includes(TP(10, 'base quality (episode {list})', 'base quality (episodes {list})', { list: '1-10' })) && $$('#work .spep').length === 10,
    'screenplay 38 offered: ten lists, its series without a length (' + m38.join(', ') + ')');
  W('#addrole').click();
  await wait(50);
  ok(W('select[data-role-cat="1"]').value === '1' && unkGet(s, SP_REQ(1)) === 10, 'a new role: an actor, requirements 10');
  W('[data-item-add="1"]').click();
  await wait(50);
  typeInto(W('input[data-ep-q="1"]'), '50');
  W('[data-ep-copy="1"]').click();
  await wait(50);
  const prod = () => keys(s).filter((k) => /^(ep|role)/.test(k));
  ok([1, 5, 10].every((e) => unkGet(s, SP_EP(e)) === 50 && spList(s, e).length === 1) && !prod().length, 'its list copied to the episodes without one, the production complete (' + keys(s) + ')');
  recById('series', 38).episodes = 4; markDirty(['series']); renderEntry();
  ok($$('#work .spep').length === 4 && !prod().length && keys(s).includes('target-length'), 'four episodes in its series: four lists');
  spPutList(s, 7, [{ item: 2, weight: 5, got: 0 }]); renderEntry();
  ok(spRoleCount(s) === 1 && !W('tr[data-role="2"]'), 'a role only in a list past its series\' episodes is not shown (the lists the game uses)');
  back('screenplays', 38); back('series', 38);
  ok(!changed('screenplays', recById('screenplays', 38)) && !changed('series', recById('series', 38)), 'screenplay and series 38 the original again');
}

/* ---- packs: the original alone, and a protected pack -- the first change makes a new, complete
   pack with all of the selection's content; the packs chosen stay as they were; a protected
   pack changed directly only when unlocked; the packs made here are deleted again */
async function selftestPacks({ ok, wait, typeInto, go2, W, confirmDialog }) {
  if (st.dirty.size) await save();
  const before = (await api('GET', '/api/mods')).map((m) => m.name);
  const made = [];
  const firstChange = async () => {
    await go2('#films/31');
    W('.zone[data-edit="topicality"]').click();
    typeInto(W('.tray input[data-field="quality"]'), '71');
    await (st.forking || Promise.resolve());
    await wait(100);
    if (st.mod) made.push(st.mod);
    return st.mod;
  };
  await selectPacks([]);
  ok(!st.mod && !st.packs.length, 'the original alone: nothing to change in place');
  const a = await firstChange(), ma = modInfo(a) || {};
  ok(a && /^pack-\d+$/.test(a) && !before.includes(a) && ma.complete && !(ma.from || []).length && same(st.packs, [a]) && st.dirty.has('films'),
    'the first change on the original made a new complete pack (' + a + ')');
  ok(await save() && (await api('GET', `/api/mods/${a}/films`)).records.find((r) => r.id === 31).quality === 71, 'the change saved into the new pack');
  await go2('#launcher');
  setTab('launcher', 'packs');   /* (the packs: in their tab) */
  await wait(300);
  ok($('#work [data-play="original"]').getAttribute('aria-pressed') === 'false' && $('#work [data-play="original"]').textContent.includes(T('held in "{pack}" already', { pack: packTitle(a) })), 'launcher: the original held in the complete pack');
  ok(await selectPacks(['selftest', a]) && !st.mod && records('films').find((r) => r.id === 31).quality === 71, 'a complete pack with another pack: shown combined, nothing to change in place');
  const twice = await api('GET', `/api/packs/${a}+${a}`).then((r) => r.error || '', (e) => e.message);
  ok(twice.includes(a), 'server: a pack twice refused (' + twice + ')');
  const prot = st.mods.find((m) => m.protected);
  if (prot) {
    const refused = await api('PUT', `/api/mods/${prot.name}/films`, { records: st.orig.films.records }).then(() => '', (e) => e.message);
    ok(refused.includes(prot.name), 'server: a protected pack takes no change without unlock (' + refused + ')');
    await selectPacks([prot.name]);
    ok(!st.mod, 'a protected pack alone: nothing to change in place');
    const b = await firstChange(), mb = modInfo(b) || {};
    ok(b && b !== prot.name && mb.complete && same(mb.from, [prot.name]), 'the first change on a protected pack made a new pack holding it (' + b + ')');
    await save();
    const held = await api('GET', `/api/packs/${b}+${prot.name}`).then((r) => r.error || '', (e) => e.message);
    ok(held.includes(prot.name) && held.includes(b), 'server: a pack not played with a complete pack that holds it (' + held + ')');
    await go2('#launcher');
    setTab('launcher', 'packs');
    await wait(300);
    ok($(`#work [data-play="${prot.name}"]`).disabled, 'launcher: a pack held in the chosen one cannot be added');
    await selectPacks([prot.name]);
    await go2('#overview');
    $('#unlock').click();
    await confirmDialog(T('Unlock'));
    ok(st.mod === prot.name, 'unlocked: the protected pack is changed directly');
    st.unlocked.delete(prot.name);
  } else ok(true, 'no protected pack here: its checks left out');
  await selectPacks([made[made.length - 1]]);
  await go2('#overview');
  ok(!$(`#modlist [data-delete="${(st.mods.find((m) => m.protected) || {}).name}"]`) && $(`#modlist [data-delete="${made[0]}"]`), 'overview: own packs have a delete button, protected ones none');
  $('#deletepack').click();
  await confirmDialog(T('Delete'));
  await wait(300);
  ok(!modInfo(made[made.length - 1]) && !st.packs.length, 'overview: the pack being changed deleted, out of the selection');
  await go2('#launcher');
  setTab('launcher', 'packs');   /* (the packs: in their tab) */
  await wait(300);
  for (const n of made.slice(0, -1)) {
    $(`#work [data-delete="${n}"]`).click();
    await confirmDialog(T('Delete'));
    await wait(300);
  }
  ok(made.length && made.every((n) => !modInfo(n)) && !st.packs.length, 'launcher: the other packs made here deleted from the list (' + made.join(' ') + ')');
  await selectPacks(['selftest']);
  ok(st.mod === 'selftest', 'back to the self-test pack');
}

/* ---- the interface in German (i18n.js): the German table against the sources, then every
   section, form tab, mode and dialog drawn in German -- T() meets no key without a German
   text (UI.missing), and no text, tooltip or placeholder on the page is still an English key
   (forgotten T) -- the server's messages in German, an unsaved edit kept across the switch;
   at the end back to the language the test started in */
async function sourceKeys() {                     /* the keys of the sources: T, TP, playLog and mpLog calls; index.html [data-i18n] */
  const STR = String.raw`(?:'((?:\\.|[^'\\])*)'|"((?:\\.|[^"\\])*)")`;
  const unq = (a, b) => (a ?? b).replace(/\\(.)/g, '$1');
  const keys = new Set();
  for (const f of $$('script[src]').map((x) => x.getAttribute('src'))) {
    let src = await (await fetch(f)).text();
    if (/\/i18n\.js$/.test(f)) src = src.slice(0, src.indexOf('const DE = {'));
    for (const m of src.matchAll(new RegExp(String.raw`\bT\(\s*` + STR, 'g'))) keys.add(unq(m[1], m[2]));
    for (const m of src.matchAll(new RegExp(String.raw`\bTP\([^,()]+(?:\([^()]*\))?[^,()]*,\s*` + STR + String.raw`\s*,\s*` + STR, 'g'))) { keys.add(unq(m[1], m[2])); keys.add(unq(m[3], m[4])); }
    for (const m of src.matchAll(new RegExp(String.raw`\b(?:playLog|mpLog)\(\s*` + STR, 'g'))) keys.add(unq(m[1], m[2]));
  }
  const page = new DOMParser().parseFromString(await (await fetch('/')).text(), 'text/html');
  for (const el of page.querySelectorAll('[data-i18n]')) {
    if (!el.children.length && el.dataset.i18n !== 'attrs' && el.textContent.trim()) keys.add(el.textContent.trim());
    for (const a of STATIC_ATTRS) if (el.hasAttribute(a)) keys.add(el.getAttribute(a));
  }
  return [...keys];
}
/* English left on the German page: texts and attributes that are an English key of DE (one
   German differs from), or match one with placeholders; the mods' own titles and the game's own words ([data-gametext]) are content */
function englishLeftovers() {
  if (!englishLeftovers.re) {
    const words = new Set(), pats = [];
    const esc = (x) => x.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    for (const [k, d] of Object.entries(DE)) {
      const en = k.split('|')[0];
      if (en === d) continue;
      if (!/\{\w+\}/.test(en)) words.add(en);
      else if ((en.replace(/\{\w+\}/g, '').match(/[A-Za-z]/g) || []).length >= 3) pats.push(en.split(/\{\w+\}/).map(esc).join('.+?'));
    }
    Object.assign(englishLeftovers, { words, re: new RegExp('^(?:' + pats.join('|') + ')$', 's') });
  }
  const { words, re } = englishLeftovers, content = new Set(st.mods.flatMap((m) => [m.title, m.name]));
  const out = new Set();
  const test = (s, where) => { s = (s || '').replace(/\s+/g, ' ').trim(); if (s && !content.has(s) && (words.has(s) || re.test(s))) out.add(`${where} "${s}"`); };
  const walk = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  while (walk.nextNode()) { const n = walk.currentNode; if (!n.parentElement.closest('script, style, #tip, [data-gametext]')) test(n.nodeValue, 'text'); }      /* (the game's own words: content) */
  const ATTRS = ['data-tip', 'data-tiptitle', 'placeholder', 'aria-label', 'alt', 'title', 'data-short'];
  for (const el of document.querySelectorAll(ATTRS.map((a) => `[${a}]`).join(','))) for (const a of ATTRS) if (el.hasAttribute(a)) test(el.getAttribute(a), a);
  test(document.title, 'title');
  return [...out];
}
async function selftestGerman({ ok, wait, typeInto, go2, W, confirmDialog }) {
  const startLang = UI.lang, startTexts = st.lang;
  /* the table against the sources */
  const keys = await sourceKeys();
  const noDe = keys.filter((k) => !(k in DE));
  ok(!noDe.length, `German table: all ${keys.length} keys of the sources translated` + (noDe.length ? ' -- missing: ' + noDe.join(' | ') : ''));
  const ph = (x) => new Set([...x.split('|')[0].matchAll(/\{(\w+)\}/g)].map((m) => m[1]));
  const badPh = Object.keys(DE).filter((k) => [...ph(DE[k])].some((p) => !ph(k).has(p)));
  const unused = Object.keys(DE).filter((k) => !keys.includes(k));
  ok(!badPh.length && !unused.length, `German table: ${Object.keys(DE).length} texts, placeholders agree, none unused` + (badPh.length ? ' -- placeholders: ' + badPh.join(' | ') : '') + (unused.length ? ' -- unused: ' + unused.join(' | ') : ''));
  const fd = st.meta.fields_de || {}, allF = Object.entries(st.meta.fields).flatMap(([k, fs]) => fs.map((f) => k + ':' + f[2]));
  const noHelp = allF.filter((x) => !(fd[x.split(':')[0]] || {})[x.split(':')[1]]);
  const used = SECTIONS.flatMap((s) => s.groups.flatMap((g) => g.fields || [])).map((f) => f.help).filter((x) => /^\w+:\w+$/.test(x || ''));
  ok(!noHelp.length && used.every((x) => !noHelp.includes(x)), `field help: all ${allF.length} catalog fields in German (the form uses ${new Set(used).size})` + (noHelp.length ? ' -- missing: ' + noHelp.join(' ') : ''));

  /* the first choice of the interface language, and a switch is kept */
  const cases = [['?ui=de', 'en', 'en-US', 'de'], ['?ui=en', 'de', 'de-DE', 'en'], ['?selftest', 'de', 'de-DE', 'en'], ['?selftest&ui=de', null, 'en-US', 'de'],
    ['', 'de', 'en-US', 'de'], ['', 'en', 'de-DE', 'en'], ['', null, 'de-AT', 'de'], ['', 'xx', 'de', 'de'], ['', null, 'fr-FR', 'en'], ['', null, undefined, 'en']];
  const wrong = cases.filter(([q, k, b, want]) => pickUiLang(q, k, b) !== want);
  const kept = keptUiLang();
  setUiLang(startLang === 'de' ? 'en' : 'de');
  const stored = keptUiLang();
  setUiLang(startLang, { keep: false });
  try { if (kept === null) localStorage.removeItem(UI_KEY); else localStorage.setItem(UI_KEY, kept); } catch (e) { /* ignore */ }
  ok(!wrong.length && stored === (startLang === 'de' ? 'en' : 'de') && keptUiLang() === kept, 'interface language: ?ui=, else the kept choice, else the browser\'s; a switch is kept' + (wrong.length ? ' -- ' + JSON.stringify(wrong) : ''));

  /* the switch keeps an unsaved edit */
  await go2('#films/31/form');
  typeInto(W('[data-text="0-en"]'), 'Typed before the switch');
  const dirty = [...st.dirty].join(), undos = st.undo.length;
  UI.missing.clear();
  setUiLang('de', { keep: false });
  st.lang = 'de'; renderAll();                    /* the game texts in German too, as a German player edits them */
  ok(UI.lang === 'de' && document.documentElement.lang === 'de' && recById('films', 31).text.en[0] === 'Typed before the switch' && W('[data-text="0-en"]').value === 'Typed before the switch'
    && [...st.dirty].join() === dirty && st.undo.length === undos && $('#save').classList.contains('dirty'), 'German: the switch keeps the unsaved edit, its tables and undo');
  ok($('#reset').textContent === 'Nicht gespeichert' && $('#save').textContent.trim() === 'Speichern'
    && $('#uilang .label').textContent === 'Oberfläche' && $('#textlang .label').textContent === 'Spieltexte' && $$('#rail .boardtitle').map((x) => x.textContent).join(',') === 'Spiel,Inhalte,Auswahl' && $('#rail .nameplate.active .lbl').textContent === 'Filme'
    && $('#rail .playcard b').textContent === 'Spielen' && $('#gsearch').placeholder.startsWith('Suchen') && $('#undo').dataset.tip === 'Rückgängig (Strg+Z)' && document.title === 'OpenMTV-Editor' && $('#status').textContent === 'Oberfläche: Deutsch',
  'German: top bar, rail, title and message (Nicht gespeichert, Speichern, Oberfläche / Spieltexte, Spiel / Inhalte / Auswahl, Spielen)');
  ok($$('#work .tab').map((x) => x.textContent).join(',') === 'Alle,Texte,Programm,Karte,Markt,Monitor' && $('#workhead .overline').textContent.startsWith('Film #31')
    && $$('#filters .chip').map((x) => x.firstChild.textContent).join(',') === 'Geändert,Agentur,Versteigerung,Startpool,Frei,Unvollständig' && $('#work label[data-tip]').dataset.tip === 'Pflicht für einen Eintrag im Spiel'
    && $('#work .help').dataset.tip.startsWith('1 Lovestory, 2 Actionfilm'), 'German: form tabs, filter chips, required mark, field help from the catalog (Lovestory, Actionfilm ...)');
  ok(W('select[data-field="genre"]').options[0].textContent === st.meta.film_genres.de[0], 'German: the genres follow the game texts (' + st.meta.film_genres.de[0] + ')');
  st.lang = 'en'; renderEntry();
  ok(W('select[data-field="genre"]').options[0].textContent === st.meta.film_genres.en[0] && $('#work .tab').textContent === 'Alle', 'German interface, English game texts: the two switches are apart');
  st.lang = 'de'; renderAll();

  /* every section, entry kinds, form tab */
  const left = new Set();
  const scan = (where) => { for (const x of englishLeftovers()) left.add(where + ': ' + x); };
  const films = secByKey('films'), recs = (t) => records(t);
  const pick = (t, f) => (recs(t).find(f) || {}).id;
  removeAsk(films, recById('films', 100));                                       /* a free slot */
  ok($('#status').textContent.startsWith('#100 "') && $('#status').textContent.includes('ist aus dem Spiel; sein Platz ist frei'), 'German: an entry taken out of the game');
  const visits = {
    films: [31, 29, 52, pick('films', (r) => r.genre === 6), pick('films', (r) => r.genre === 7 && !SPECIAL.films[r.id]), pick('films', (r) => r.genre === 8), 100, 201],
    series: [2, 18, pick('series', (r) => r.jingle)],
    ads: [2, 3],
    screenplays: [2, 24, 25, 26, 38, pick('screenplays', (r) => spIsSeries(r))],
    news: [2, 38, 166, (unreachedNodes()[0] || {}).id],
    topics: [1],
  };
  st.epText = true;
  for (const sec of SECTIONS) {
    for (const id of new Set(visits[sec.key].filter(Boolean))) {
      await go2(`#${sec.key}/${id}`);
      scan(`#${sec.key}/${id}`);
      st.mode = 'form';
      for (const g of sec.groups) { st.tab[sec.key] = g.key; renderEntry(); scan(`#${sec.key}/${id}/${g.key}`); }
      st.tab[sec.key] = 'all';
    }
  }
  /* a start library (the tables hold no owner; the start savegame does): for a moment */
  for (const [t, id, owner] of [['films', 33, 1], ['series', 3, 2]]) {
    const r = recById(t, id), was = r.owner;
    r.owner = owner; await go2(`#${t}/${id}`); scan(`#${t}/${id}, owner ${owner}`);
    ok($('#workhead .einfo').textContent.includes(`im Startarchiv von ${STATIONS[owner]}`) && $(`#list .tape[data-id="${id}"] .badge`).dataset.tip === `im Startarchiv von ${STATIONS[owner]}`, `German: ${t} ${id} in a start library`);
    r.owner = was; renderAll();
  }
  const nVisits = new Set(Object.entries(visits).flatMap(([k, ids]) => ids.filter(Boolean).map((i) => k + i))).size;
  ok(nVisits >= 20 && $$('#rail .nameplate:not(.quitplate)').length === SECTIONS.length + 2 && $('#rail .playcard'), `German: every section visited, its card and every form tab of ${nVisits} entries`);
  /* the modes */
  for (const key of ['films', 'series', 'ads', 'screenplays', 'news', 'topics']) {
    await go2(`#${key}/table`);
    $$('#work tbody .selbox').slice(0, 2).forEach((b) => b.click());
    $('.views .plate')?.click();
    scan(`#${key}/table`);
  }
  ok($('#work .tbar .ttl').textContent === 'Themen: Tabelle' && $$('#work .tbar .seg button').map((b) => b.getAttribute('aria-label')).join(',') === 'Liste,Tabelle', 'German: the table mode in every section');
  await go2('#films/table');
  W('.selbox').click();
  ok(/ausgewählt/.test($('#work .bulk').textContent) && $('#work .bulk [data-bulk-pool]').textContent === 'rein' && $('#inspector').textContent.includes('Alle Filme nach Genre'), 'German: bulk bar and the genre table (ausgewählt, rein, Alle Filme nach Genre)');
  for (const h of ['#films/31/game', '#series/2/game', '#ads/2/game']) {
    await go2(h);
    for (const z of $$('#work .zone')) { z.click(); scan(h + ' ' + z.dataset.edit); }
    W('[data-text="0-de"]')?.dispatchEvent(new Event('focus'));
    scan(h + ' title');
  }
  ok(W('.tray .textinfo') && W('.stagehint').textContent.startsWith('Auf der Karte tippen') && W('.stagecap').textContent.startsWith('So zeigt es das Spiel'), 'German: the card, its rows\' values and a text\'s fit');
  for (const h of ['#films/31/monitor', '#series/2/monitor']) { await go2(h); scan(h); }
  ok($('#inspector .plan') && $('#inspector').textContent.includes('Monitorplan, ganze Auswahl') && W('[data-choice="genre"] b').textContent === 'Folgt dem Genre', 'German: the monitor tab and its plan');
  await go2('#news/38/tree');
  scan('tree');
  ok($$('#work .lanet')[0].textContent === 'Schritt 1, Start' && $('#inspector').textContent.includes('Schritt #38') && $('#shelf .stitle').textContent === 'Geschichten', 'German: the story tree');
  $('#list [data-story="none"]').click();
  scan('tree, outside the stories');
  await go2('#launcher');
  await wait(400);
  scan('launcher');
  const deTabs = {};
  for (const t of ['packs', 'rules', 'fixes']) {
    setTab('launcher', t);
    await wait(200);
    scan('launcher, ' + t);
    deTabs[t] = $('#work .gdrawer').textContent;
  }
  setTab('launcher', '');
  ok($('#work').textContent.includes('Tresor') && $('#work').textContent.includes('Neues Spiel') && $('#work').textContent.includes('Korrekturen') && deTabs.packs.includes('Das Originalspiel')
    && deTabs.rules.includes('Allgemein') && deTabs.fixes.includes('Klassisch 1991') && deTabs.fixes.includes('Alle Korrekturen'), 'German: the launcher, its safe, the packs, the rules, Classic and the fixes');
  await go2('#multiplayer');
  await wait(400);
  scan('multiplayer');
  ok($('#work').textContent.includes('Gemeinsam spielen') && $('#work').textContent.includes('Spiel eröffnen') && $('#rail .playcard.mpcard b').textContent === 'Mehrspieler', 'German: the multiplayer page');
  await go2('#overview');
  scan('overview');
  ok($('#rail .nameplate.active .lbl').textContent === 'Übersicht' && $('#work').textContent.includes('Änderungen gegenüber dem Original') && $('#inspector').textContent.includes('Startet ein neues Spiel?')
    && (st.log || []).some((x) => T(x.key, x.params) === 'Speichere den Pack ...'), 'German: the overview, its checks and the log of the English build, now in German');
  typeInto($('#gsearch'), 'zzqqxx');
  scan('search');
  ok($('#gresults').textContent === 'Nichts gefunden.', 'German: the search');
  $('#gsearch').value = ''; hideResults();

  /* the dialogs */
  const dlg = async (open, where, button) => { open(); await wait(100); scan(where); const t = $('#overlay .boxtitle')?.textContent; await confirmDialog(button); return t; };
  await go2('#films/31');
  const tCopy = await dlg(() => copyEntry(films, recById('films', 31)), 'copy', 'Abbrechen');
  const tSp = await dlg(() => copyScreenplay(recById('screenplays', 2)), 'copy screenplay', 'Abbrechen');
  const tReset = await dlg(() => resetDialog(), 'reset', 'Abbrechen');
  const tProb = await dlg(() => $('#rail-problems').click(), 'problems', 'Schließen');
  const tNew = await dlg(() => newModDialog(), 'new pack', 'Abbrechen');
  const tUns = await dlg(() => askUnsaved(), 'unsaved', 'Abbrechen');
  await go2('#series/2/form');
  const tSame = await dlg(() => $$('#work .plate').find((b) => b.textContent === 'Gleiche Qualität für alle ...').click(), 'same quality', 'Abbrechen');
  await go2('#films/table');
  const tView = await dlg(() => $('.views .dashed').click(), 'save the view', 'Abbrechen');
  ok([tCopy, tSp, tReset, tProb, tNew, tUns, tSame, tView].join(',') === `Kopieren,Kopieren,Zurück zum Original,Probleme (${allProblems().length}),Neuer Pack,Ungespeicherte Änderungen,Gleiche Qualität für alle,Ansicht speichern`, 'German: the dialogs (' + [tCopy, tSp, tReset, tProb, tNew, tUns, tSame, tView].join(', ') + ')');
  /* a new film, incomplete: the form, saved as a draft, refused by the server's build -- in German; then deleted */
  await go2('#films/31');
  newEntry(films);
  await wait(100);
  const nf = recById('films', st.sel.films);
  scan('new film');
  ok(nf && nf.id === 203 && $('#status').textContent.startsWith('Neuer Film #203: die rot markierten Felder ausfüllen') && $('#workhead .reqbox').textContent.startsWith('Fehlt, bevor es auf Sendung kann: englischer Titel'), 'German: a new film and its required fields (' + $('#workhead .reqbox').textContent + ')');
  await save();
  ok($('#status').textContent.startsWith('"selftest" gespeichert -- noch nicht spielbar: Film #203'), 'German: saved as a draft (' + $('#status').textContent.slice(0, 60) + ' ...)');
  await buildOnly();
  ok($('#status').textContent.startsWith('Nicht gebaut: noch nicht spielbar -- Film 203: englischer Titel, englische Beschreibung'), 'server, German: the build refuses the incomplete film (' + $('#status').textContent.slice(0, 80) + ' ...)');
  const tDel = await dlg(() => deleteAdded(films, nf), 'delete', 'Löschen');
  await save();
  ok(tDel === 'Löschen' && !recById('films', 203) && $('#status').textContent === '"selftest" gespeichert.', 'German: the delete dialog, saved again');
  /* the server's other messages */
  const err = async (...a) => { try { await api(...a); return ''; } catch (e) { return e.message; } };
  const msgs = [await err('POST', '/api/mods', { name: 'bad name!' }), await err('POST', '/api/mods', { name: 'selftest' }),
    await err('PUT', '/api/mods/selftest/films', { records: st.data.films.records.concat([{ id: 1000 }]) }),
    await err('PUT', '/api/mods/selftest/films', { records: st.data.films.records.map((r) => (r.id === 31 ? Object.assign(clone(r), { text: { en: ['10 €', 'x'], de: r.text.de } }) : r)) })];
  /* (the Go editor names the entry and its field; the Python one it replaced, the former Python editor (editor.py), did not) */
  ok(['ein Mod-Name besteht aus 1-40 Buchstaben, Ziffern, - oder _|den Mod "selftest" gibt es schon|Film #1000: die Nummer ist zu hoch (höchstens 999)|Film #31, englischer Titel: "€" lässt sich im Zeichensatz des Spiels (Codepage 437) nicht schreiben',
    'ein Mod-Name besteht aus 1-40 Buchstaben, Ziffern, - oder _|den Mod "selftest" gibt es schon|Filme: Nummer 1000 außerhalb des Bereichs (höchstens 999)|"€" lässt sich im Zeichensatz des Spiels (Codepage 437) nicht schreiben'].includes(msgs.join('|')),
    'server, German: mod name, existing mod, id range, character set (' + msgs.join(' | ') + ')');
  /* a status message and a refused removal */
  removeAsk(films, recById('films', 29));
  ok($('#status').textContent.startsWith('#29 bleibt im Spiel: die wiederkehrende Fußball-Live-Übertragung'), 'German: why an entry stays in the game');
  scan('status');

  /* what T() missed, what was left English */
  ok(!UI.missing.size, 'German: T() met no key without a German text' + (UI.missing.size ? ' -- ' + [...UI.missing].join(' | ') : ''));
  ok(!left.size, 'German: no English left in texts, tooltips, placeholders, labels' + (left.size ? ' -- ' + [...left].slice(0, 30).join(' | ') : ''));

  /* back */
  st.epText = false;
  setUiLang(startLang, { keep: false });
  st.lang = startTexts;
  await go2('#films/31/form');
  ok(UI.lang === startLang && document.documentElement.lang === startLang && $$('#rail .boardtitle').map((x) => x.textContent).join(',') === [T('Game'), T('Contents'), T('Selection')].join(',')
    && (startLang !== 'en' || ($('#save').textContent.trim() === 'Save' && $$('#work .tab')[1].textContent === 'Texts')) && recById('films', 31).text.en[0] === 'Typed before the switch',
  'back to ' + (startLang === 'en' ? 'English' : 'German') + ', the edit still there');
}

/* the player's launcher (build/openmtv, launcher/server.go): the page "Play" alone, served by
 * the Go program -- what it shows and loads, then the launcher's own checks (selftestLauncher,
 * marked player) on the pack "selftest", which the test tool makes (tools/launchertest.py);
 * the game is never started (dry).  The result in the page title, as selftest() */
async function playerSelftest() {
  const log = [];
  const ok = (cond, what) => log.push((cond ? 'ok ' : 'FAIL ') + what);
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const typeInto = (el, v) => { el.value = v; el.dispatchEvent(new Event(el.tagName === 'SELECT' ? 'change' : 'input')); };
  const go2 = async (hash) => { location.hash = hash; fromHash(); renderAll(); await wait(50); };
  const W = (s) => $('#work ' + s);
  const loaded = (img) => (img.complete ? Promise.resolve() : new Promise((r) => { img.onload = r; img.onerror = r; }));
  let keptSel = null;
  try { keptSel = localStorage.getItem(PACKS_KEY); } catch (e) { /* ignore */ }
  try {
    ok(document.body.classList.contains('player') && st.section === 'launcher' && $('#body').dataset.layout === 'launcher', 'player: the launcher page alone');
    ok(['launcher', 'multiplayer', 'settings', 'editor'].every((k) => $(`#rail [data-key="${k}"]`)) && !$('#rail .nameplate[data-key="films"]') && !$('#rail-problems') && !!$('#rail #quitlauncher') === !st.meta.window,
      'player: the board holds play, multiplayer, the settings, the editor and the end -- no contents, no checks');
    ok(['#save', '#gsearchbox', '#undo', '#textlang'].every((s) => !$(s).offsetParent) && $('#menu').offsetParent, 'player: no editor controls in the header, the interface language stays');
    ok($('#rail .railver').textContent === `${T('Version')} ${shortVersion(st.meta.version)}` && $('#rail .railver').dataset.tip === T('Version {version}', { version: st.meta.version }),
      'player: the version at the foot of the board, as the release page writes it (' + $('#rail .railver').textContent + ')');
    ok($('#logo').textContent === 'OpenMTV' && $('#logo').offsetWidth > 0, `player: the name in the top bar (${$('#logo').offsetWidth}px)`);
    if (!st.settings) { await loadSettings(); renderEntry(); }
    const art = W('.safepic img');
    if (art) await loaded(art);
    ok(art && art.naturalWidth === 320 && art.naturalHeight === 109, `player: the safe of the office as the game draws it (${art && art.naturalWidth}x${art && art.naturalHeight})`);
    ok(!!modInfo('selftest'), 'player: the pack "selftest" is there (the test tool makes it)');
    setTab('launcher', 'packs');
    await wait(150);
    const broken = st.mods.filter((m) => m.error);
    ok(broken.every((m) => W(`[data-packerror="${m.name}"]`)?.textContent === m.error && W(`[data-play="${m.name}"]`).disabled),
      `player: ${broken.length} packs that cannot be read: in the list with why, not to choose` + (broken[0] ? ` (${broken[0].error})` : ''));
    await selectPacks(['selftest']);
    ok(same(st.packs, ['selftest']) && !st.mod && !W('[data-delete]') && ![...$$('#work .plate')].some((b) => b.textContent.includes(T('New pack'))),
      'player: the pack chosen, nothing to save into, no "New pack" and no delete');
    for (const fn of SELFTESTS.filter((f) => f.player)) await fn({ ok, wait, typeInto, go2, W });
    const err = async (...a) => { try { await api(...a); return ''; } catch (e) { return e.message; } };
    const refused = await err('PUT', SETTINGS_URL(), { values: { game: { start_money: 'abc' } } });
    ok(refused === (UI.lang === 'de' ? 'game.start_money: "abc" nimmt das Spiel nicht' : 'game.start_money: "abc" is not a value the game takes'),
      'player: the server refuses a value, in the interface language (' + refused + ')');
    /* the launcher's "Editor": this page becomes the editor (last: it stays one) */
    $('#rail [data-key="editor"]').click();
    let edited = false;
    for (let i = 0; i < 300 && !edited; i++) { await wait(100); edited = !player() && !document.body.classList.contains('player') && !!st.orig.films && !!st.data.films && !!$('#rail .nameplate[data-key="films"]'); }
    ok(edited && st.section === 'films' && location.hash.startsWith('#films') && $('#subtitle').textContent === T('Editor') && same(st.packs, ['selftest']) && $('#save').offsetParent,
      'player: "Editor" makes this page the content editor (its data, the selection\'s tables, the films)');
    ok($('#rail .railver') && $('#rail .railver').textContent === `${T('Version')} ${shortVersion(st.meta.version)}`, 'player: the editor\'s board shows the version too (' + ($('#rail .railver') || {}).textContent + ')');
    ok(!PAGE_ERRORS.length, 'player: no script errors' + (PAGE_ERRORS.length ? ' -- ' + PAGE_ERRORS.join('; ') : ''));
    if (UI.lang === 'de') ok(!UI.missing.size, 'player, German: T() met no key without a German text' + (UI.missing.size ? ' -- ' + [...UI.missing].join(' | ') : ''));
  } catch (e) { log.push('FAIL ' + e.message); console.error(e); }
  try { if (keptSel === null) localStorage.removeItem(PACKS_KEY); else localStorage.setItem(PACKS_KEY, keptSel); } catch (e) { /* ignore */ }
  document.title = 'SELFTEST ' + (log.some((x) => x.startsWith('FAIL')) ? 'FAIL' : 'OK') + ': ' + log.join('; ');
}
