/* OpenMTV editor -- the story tree (board E): the news stories as steps in columns (shortest
 * way from the story's start), the ways on as chips: forward, back or sideways (a loop),
 * the end; the selected step in the inspector. */
'use strict';

const storyTemplates = () => records('news_templates').filter((t) => t.start_node > 0);
/* steps of a story: shortest path from its start node -> Map(node -> step 1..) */
function storySteps(t) {
  const steps = new Map(), todo = [[t.start_node, 1]];
  while (todo.length) {
    const [n, s] = todo.shift();
    if (!n || n === NEWS_END || steps.has(n)) continue;
    const nd = recById('news_nodes', n);
    if (!nd) continue;
    steps.set(n, s);
    for (const x of nd.successors) todo.push([x, s + 1]);
  }
  return steps;
}
function unreachedNodes() {
  const seen = new Set();
  for (const t of storyTemplates()) for (const n of storySteps(t).keys()) seen.add(n);
  return records('news_nodes').filter((n) => !seen.has(n.id) && (txt(n, null, 'en') || txt(n, null, 'de') || n.successors.some((x) => x)));
}
function currentStory() {
  const id = st.sel.news;
  if (st.story && (st.story === 'none' || storyTemplates().some((t) => t.id === st.story && storySteps(t).has(id)))) return st.story;
  const t = id && newsTemplateOf(id);
  return (st.story = t ? t.id : id ? 'none' : (storyTemplates()[0] || {}).id);
}
function renderStoryShelf(sh) {
  const ts = storyTemplates(), cs = currentStory();
  sh.append(h('div', { class: 'shelfhead' }, h('h2', { class: 'stitle' }, T('Stories')), h('span', { class: 'scount' }, String(ts.length)), viewToggle(secByKey('news'))));
  const list = h('div', { id: 'list' });
  for (const t of ts) {
    const start = recById('news_nodes', t.start_node);
    list.append(h('button', { class: 'story' + (cs === t.id ? ' sel' : ''), 'data-story': t.id, onclick: () => { st.story = t.id; st.sel.news = t.start_node; renderShelf(); renderEntry(); } },
      h('span', { class: 'sn' }, TP(storySteps(t).size, 'Story {id}, {n} step, {category}', 'Story {id}, {n} steps, {category}', { id: t.id, category: NEWS_CATEGORY[t.category] || '' })), h('span', { class: 'sh' }, start ? txt(start, null) : '?')));
  }
  const un = unreachedNodes();
  list.append(h('button', { class: 'story' + (cs === 'none' ? ' sel' : ''), 'data-story': 'none', onclick: () => { st.story = 'none'; st.sel.news = (un[0] || {}).id; renderShelf(); renderEntry(); } },
    h('span', { class: 'sn' }, TP(un.length, 'Not in a story: {n} step', 'Not in a story: {n} steps')), h('span', { class: 'sh' }, T('game events and steps no story reaches'))));
  sh.append(list);
}
function renderTree(sec, w, ins) {
  const cs = currentStory();
  const t = cs !== 'none' && recById('news_templates', cs);
  const steps = t ? storySteps(t) : new Map(unreachedNodes().map((n) => [n.id, 1]));
  const head = h('div', { class: 'tbar' }, h('span', { class: 'ttl' }, t ? T('Story {id}, {category}', { id: t.id, category: newsCategory(t.category) }) : T('Steps outside the stories')), viewToggle(sec));
  if (t) t.parts.forEach((p, k) => { if (p) { const tp = recById('news_topics', p % 1000); head.append(h('button', { class: 'chip', onclick: () => go('topics', p % 1000) }, T('#{k} = topic {topic}: {phrases}', { k: k + 1, topic: p % 1000, phrases: tp ? (tp.text?.[st.lang] || []).filter(Boolean).slice(0, 2).join(', ') + ', ...' : '?' }))); } });
  w.append(head);
  const maxStep = Math.max(1, ...steps.values());
  const lanes = h('div', { class: 'lanes' });
  const sel = st.sel.news;
  for (let s = 1; s <= maxStep; s++) {
    const lane = h('div', { class: 'lane' }, h('div', { class: 'lanet' }, !t ? T('Steps') : s === 1 ? T('Step 1, start') : T('Step {n}', { n: s })));
    for (const [id, sn] of [...steps.entries()].sort((a, b) => a[0] - b[0])) {
      if (sn !== s) continue;
      const n = recById('news_nodes', id);
      const textEl = h('span', { class: 'nt' }, txt(n, null) || T('(empty)'));
      const chips = h('span', { class: 'nchips' }, ...n.successors.filter((x) => x).map((x) => {
        if (x === NEWS_END) return h('span', { class: 'nchip end' }, T('end'));
        const back = t && steps.has(x) && steps.get(x) <= s;
        return h('span', { class: 'nchip' + (back ? ' back' : ''), 'data-tip': back ? T('back or sideways: a loop') : T('goes on'), onclick: (e) => { e.stopPropagation(); st.sel.news = x; renderEntry(); } }, (back ? '<- ' : '-> ') + x);
      }));
      if (!n.successors.some((x) => x)) chips.append(h('span', { class: 'nchip dead', 'data-tip': T('No way on: the story stays here') }, T('stuck')));
      const card = h('button', { class: 'node' + (id === sel ? ' sel' : '') + (changed('news_nodes', n) ? ' chg' : ''), 'data-node': id, onclick: () => { st.sel.news = id; renderEntry(); } },
        h('span', { class: 'nh' }, h('b', {}, '#' + id), h('span', {}, T('value {v}, {h} h', { v: n.value, h: n.delay_hours }))), textEl, chips);
      if (id === sel) st.liveWork.push(() => { textEl.textContent = txt(n, null) || T('(empty)'); });
      lane.append(card);
    }
    lanes.append(lane);
  }
  w.append(h('div', { class: 'box lanewrap' }, lanes),
    h('div', { class: 'legend' }, h('span', { class: 'nchip' }, '-> 39'), ' ' + T('goes on') + '  ', h('span', { class: 'nchip back' }, '<- 31'), ' ' + T('back or sideways (a loop)') + '  ', h('span', { class: 'nchip end' }, T('end')), ' ' + T('the story ends')));
  /* the selected step */
  const r = sel && recById('news_nodes', sel);
  if (!r) { ins.append(panel(T('Step'), h('div', { class: 'hint pad' }, T('Click a step.')))); return; }
  const ctx = { req: {} };
  const g = sec.groups.find((x) => x.key === 'headline');
  const heads = h('div', { class: 'textblock' });
  for (const l of LANGS) heads.append(...textItem(sec, r, Object.assign({}, g.texts[0], { label: T('Headline, {lang}', { lang: LANG_NAME[l] }) }), l, ctx));
  ins.append(panel(T('Step #{id}', { id: r.id }), heads));
  ins.append(PANELS.headline(sec, r));
  /* the preview with other phrases of the story's topics (news_text picks one per part) */
  if (t && /#\d/.test(txt(r, null, editLang()))) {
    st.newsVariants = st.newsVariants || [];
    const tries = h('div', { class: 'used' });
    t.parts.forEach((p, k) => {
      const tp = p && recById('news_topics', p % 1000);
      if (!tp || !txt(r, null, editLang()).includes('#' + (k + 1))) return;
      const ph = (tp.text?.[editLang()] || []).map((x, v) => [v + 1, x]).filter((x) => x[1]);
      tries.append(h('div', { class: 'inline' }, `#${k + 1} =`, h('select', { 'data-variant': k + 1, onchange: (e) => { st.newsVariants[k] = +e.target.value; runLive(); } },
        ...ph.map(([v, x]) => h('option', { value: v, selected: (st.newsVariants[k] || 1) === v }, x)))));
    });
    ins.append(panel(T('Try other phrases'), tries));
  }
  const story = sec.groups.find((x) => x.key === 'story');
  const preds = records('news_nodes').filter((n) => n.successors.includes(r.id)).map((n) => n.id);
  ins.append(panel(T('Values and ways on'), fieldsGrid(sec, r, story.fields, ctx),
    h('div', { class: 'hint pad' }, preds.length ? h('span', { class: 'preds' }, T('Reached from'), ...preds.map((x) => h('button', { class: 'link', onclick: () => { st.sel.news = x; renderEntry(); } }, '#' + x))) : T('Nothing leads here.')),
    h('div', { class: 'pad' }, h('button', { class: 'plate small grey', onclick: () => go('news', r.id, { view: 'list' }) }, T('Open as a single step')))));
  ins.append(PANELS.checks(sec, r));
}

MODES.tree = { get label() { return T('Story tree'); }, view: true, sections: ['news'], shelf: renderStoryShelf, render: renderTree };

SELFTESTS.push(async function selftestTree(t) {
  const { ok, wait, typeInto, go2, W } = t;
  /* ---- E: the story tree */
  await go2('#news/38/tree');
  const t3 = recById('news_templates', 3), steps = storySteps(t3);
  ok(currentStory() === 3 && $$('#work .lane').length === Math.max(...steps.values()) && W('.node[data-node="38"].sel') && W('.nchip.back'), 'story tree: story 3 in steps, loops marked');
  const vsel = $('#inspector select[data-variant="1"]');
  typeInto(vsel, vsel.options[1].value);
  ok(newsHeadline(recById('news_nodes', 38), 'en', st.newsVariants).startsWith(vsel.options[1].textContent + ':'), 'story tree: the headline with another phrase of its topic');
  W('.node[data-node="39"]').click();
  ok(st.sel.news === 39 && $('#inspector').textContent.includes(T('Step #{id}', { id: 39 })), 'story tree: a step opens in the inspector');
});
