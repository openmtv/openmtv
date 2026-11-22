/* OpenMTV editor -- the studios: the twelve studios of the building (launcher/studios.go).
 * Three are the stations' when a game starts; the other nine numbers belong to the doors
 * of public rooms, which become free studios after a bomb -- a pack may make some of them
 * free from the start (not the consulates: the terrorists come from them), and set the
 * size (the sets a studio holds, 1-3) and the base rent of every studio; the three start
 * studios together (no station starts better off).
 *   studioPlan(items, o)     the estate agent's plan (/api/plan.png, the game's picture) with
 *                            a ring and a number per studio; o.pick(id) makes them buttons
 *   studioOverview(views)    the plan and a list: the launcher's packs, the lobby
 * The editor's section: schema.js (key 'studios'); its picture group and panel here. */
'use strict';

const STUDIO_STARTS = [6, 8, 10];               /* (the original's: launcher/studios.go TestStudioDoors) */
const STUDIO_COLORS = ['#888', '#de2424', '#40a140', '#556aa1'];
const PLAN_W = 320, PLAN_H = 120;
const studioDoor = (id) => (st.meta?.studio_doors || []).find((d) => d.studio === id);
const consulateRoom = (room) => room === 0x12 || room === 0x13;
/* what a studio is: 'station' (a start studio), 'locked' (a consulate), else 'public' */
function studioKind(id, renter0, room) {
  if (renter0 >= 1 && renter0 <= 3) return 'station';
  return consulateRoom(room) ? 'locked' : 'public';
}
const studioKindOf = (r) => studioKind(r.id, (origById('studios', r.id) || r).renter, studioDoor(r.id)?.room);
/* the estate agent: 1.5 x the base rent a day (studio_take_over 229E:1080), three days of it to move in */
const studioDaily = (rent) => rent + Math.trunc(rent / 2);
const studioMoveIn = (rent) => 3 * studioDaily(rent);
function studioTitle(r, lang = st.lang) {
  const o = origById('studios', r.id) || r;
  if (o.renter >= 1 && o.renter <= 3) return T("{station}'s start studio", { station: STATIONS[o.renter] });
  const d = studioDoor(r.id);
  return (d && gameStr(d.sign, lang)) || T('Studio {n}', { n: r.id });
}
/* the offered screenplays by the sets of their first list (script_count_scenes 1DA8:2293):
   a studio of size N takes those with N sets at most */
function screenplaySets() {
  const n = [0, 0, 0, 0];
  for (const sp of records('screenplays')) {
    if (!(sp.flags & 2)) continue;
    let k = 0;
    for (let i = 1; i <= SP_ITEMS; i++) if (spIsSet(unkGet(sp, SP_EP(1) + 1 + 3 * i))) k++;
    n[Math.min(k, 3)]++;
  }
  return n;
}

/* the plan: the game's picture, a ring per studio (free from the start: yellow; the selected
   one: thick), a number beside it; locked ones a lock */
function studioPlan(items, o = {}) {
  const fig = h('div', { class: 'studioplan', 'data-studioplan': '' },
    h('img', { class: 'pix', src: '/api/plan.png', alt: T("The estate agent's plan of the building, as in the game"), draggable: 'false' }));
  const pct = (v, of) => `${(v * 100) / of}%`;
  for (const it of items) {
    const ring = { class: 'sring ' + it.kind + (it.sel ? ' sel' : ''), style: `left:${pct(it.x - 2, PLAN_W)};top:${pct(it.y - 2, PLAN_H)};width:${pct(14, PLAN_W)};height:${pct(15, PLAN_H)}`,
      'data-studio': it.id, 'data-tip': it.tip || null, 'aria-label': it.tip || String(it.id) };
    fig.append(o.pick ? h('button', Object.assign(ring, { onclick: () => o.pick(it.id) })) : h('span', ring),
      h('span', { class: 'snum ' + it.kind + (it.sel ? ' sel' : ''), style: `left:${pct(it.x + 12, PLAN_W)};top:${pct(it.y + 1, PLAN_H)}` },
        it.kind === 'locked' ? icon('lock', 9) : null, String(it.id)));
  }
  return fig;
}

/* the editor: the plan of the current selection, the studio of this entry chosen */
function studioPlanGroup(r) {
  const items = (st.meta.studio_doors || []).map((d) => {
    const rec = recById('studios', d.studio) || {};
    const kind = rec.renter === 255 ? 'free' : studioKindOf(rec.id ? rec : { id: d.studio });
    return { id: d.studio, x: d.x, y: d.y, kind, sel: d.studio === r.id, tip: studioTitle(rec.id ? rec : { id: d.studio }) };
  });
  const pick = (id) => { select(id); setTimeout(() => $(`#work .sring[data-studio="${id}"]`)?.focus()); };
  return h('div', { class: 'pad' }, studioPlan(items, { pick }),
    h('div', { class: 'planlegend' },
      h('span', {}, T('Click a room to choose it.')),
      h('span', {}, h('i', { class: 'lg free' }), T('free from the start')),
      h('span', {}, icon('lock', 11), T('locked')),
      h('span', {}, T('hatched: a room with a studio behind its door'))));
}

/* a size or rent of a start studio is the three start studios' (no station starts better off) */
function studioSet(key) {
  return (r, v) => {                             /* (a copy -- the changes list's scratch -- alone) */
    const coupled = recById('studios', r.id) === r && studioKindOf(r) === 'station';
    for (const t of coupled ? STUDIO_STARTS.map((id) => recById('studios', id)).filter(Boolean) : [r]) t[key] = v;
    if (coupled) setTimeout(() => { if (cur()?.key === 'studios') renderList(); });     /* (their rows' marks) */
  };
}

function studioFields() {
  return [
    { key: 'renter', label: T('At the start'), help: T("Free from the start: the door says \"Free studio\" and the estate agent offers it, as after a bomb in the room. Public: the room's own door; a bomb there makes it a free studio. The computer players rent only public rooms (fix ai-studio), never a free studio."),
      type: 'choice', opts: () => [[0, T('Public room')], [255, T('Free studio')]], show: (r) => studioKindOf(r) === 'public', rerender: true,
      note: (r) => (r.renter === 255 ? T('-> the estate agent offers it on day 1') : null) },
    { key: 'start', label: T('At the start'), type: 'info', show: (r) => studioKindOf(r) !== 'public',
      help: T('A station\'s start studio stays its station\'s. The consulates stay public rooms: the terrorists come from them -- free from the start, there would be no bombs, and no sabotage at the information board.'),
      text: (r) => (studioKindOf(r) === 'station' ? T("{station}'s from day 1", { station: STATIONS[(origById('studios', r.id) || r).renter] })
        : T('Public room -- locked: the terrorists come from the consulates. After a bomb it is a free studio with this size and rent.')) },
    { key: 'size', label: T('Size'), help: T("The sets a studio holds (script_count_scenes 1DA8:2293): a screenplay whose shopping list has more sets does not go into it."),
      type: 'choice', opts: () => [[1, '1'], [2, '2'], [3, '3']], set: studioSet('size'), rerender: true,
      note: (r) => { const n = screenplaySets(), fit = n[0] + n.slice(1, r.size + 1).reduce((a, b) => a + b, 0), all = n.reduce((a, b) => a + b, 0);
        return [TP(r.size, 'holds {n} set:', 'holds {n} sets:'), h('span', { class: 'value' }, T('{fit} of {all} offered screenplays fit', { fit, all }))]; } },
    { key: 'rent', label: T('Base rent'), help: T('A day, while it is rented. The estate agent asks 1.5 times as much a day (the rent rises to that when it is taken) and three days of it to move in.'),
      type: 'money', max: 1000000, step: 500, set: studioSet('rent'),
      note: (r) => (studioKindOf(r) === 'station' ? [T('-> each station pays'), h('span', { class: 'value' }, T('{day} a day', { day: money(r.rent) }))]
        : [T('-> the estate agent asks'), h('span', { class: 'value' }, T('{day} a day, {moveIn} to move in', { day: money(studioDaily(r.rent)), moveIn: money(studioMoveIn(r.rent)) }))]) },
  ];
}

/* the entry's own checks (entryChecks, schema.js) */
function studioChecks(r) {
  const out = [], kind = studioKindOf(r);
  if (kind === 'station') {
    const s = STUDIO_STARTS.map((id) => recById('studios', id)).filter(Boolean);
    if (s.some((x) => x.size !== s[0].size || x.rent !== s[0].rent)) out.push({ level: 'err', msg: T('The three start studios differ: they take one size and one rent') });
  }
  const o = origById('studios', r.id) || r;
  if (kind === 'station' && r.renter !== o.renter) out.push({ level: 'err', msg: T("A station's start studio stays its station's") });
  if (kind === 'locked' && r.renter !== 0) out.push({ level: 'err', msg: T('A consulate stays a public room') });
  if (kind === 'public' && r.renter !== 0 && r.renter !== 255) out.push({ level: 'err', msg: T('At the start: public (0) or free (255)') });
  if (!(r.size >= 1 && r.size <= 3)) out.push({ level: 'err', msg: T('Size 1 to 3') });
  if (!(r.rent >= 0 && r.rent <= 1000000)) out.push({ level: 'err', msg: T('Base rent 0 to 1,000,000') });
  if (!out.length) out.push({ level: 'ok', msg: kind === 'station' ? T('Each station pays {day} a day', { day: money(r.rent) })
    : kind === 'public' && r.renter !== 255 ? T('Public room: a studio after a bomb') : T('The estate agent asks {day} a day', { day: money(studioDaily(r.rent)) }) });
  return out;
}

/* the inspector: what the room is in the game, the screenplays by size, who rents it */
function studioPanel(sec, r) {
  const kind = studioKindOf(r), n = screenplaySets(), all = n.reduce((a, b) => a + b, 0);
  const bar = h('div', { class: 'setbar', role: 'img', 'aria-label': T('{a} screenplays need 1 set, {b} need 2, {c} need 3', { a: n[0] + n[1], b: n[2], c: n[3] }) },
    ...[n[0] + n[1], n[2], n[3]].map((v, k) => h('span', { class: k < r.size ? 'on' : '', style: `flex:${v || 0.001}` })));
  return panel(T('The studio'),
    h('div', { class: 'pad studioinfo' },
      h('div', { class: 'h4' }, T('In the original')),
      h('p', {}, kind === 'station' ? T("{station}'s studio from the first day; the stations pay its rent every day.", { station: STATIONS[(origById('studios', r.id) || r).renter] })
        : kind === 'locked' ? T('A consulate: the terrorists come from here. A terrorist from the other one can bomb it -- then it is a free studio.')
          : T('A public room, a door and a sign. It becomes a free studio when a terrorist bombs it (a sign swapped at the information board).')),
      h('div', { class: 'h4' }, T('Screenplays by size')), bar,
      h('div', { class: 'setrows' },
        h('span', {}, T('1 set')), h('span', {}, String(n[0] + n[1])),
        h('span', {}, T('2 sets')), h('span', {}, String(n[2])),
        h('span', {}, T('3 sets: only size 3')), h('span', {}, String(n[3]))),
      h('div', { class: 'hint' }, TP(all, 'of {n} screenplay the agency offers', 'of {n} screenplays the agency offers')),
      h('div', { class: 'h4' }, T('Who rents it')),
      h('p', {}, T('Humans, at the estate agent -- in a network game whoever is there first. The computer players rent only public rooms.')),
      h('div', { class: 'h4' }, T('Saved games')),
      h('p', {}, T('A new game starts with it; a saved game keeps its building.'))));
}

/* the overview: the studios as a game with these packs starts (the launcher's report, the
   lobby's packs) -- the plan with numbers, the list: the free ones, the start studios, public
   rooms the packs change, then the rest in a line.  The price: what its renter pays a day (the
   estate agent's 1.5 times the base rent -- a public room after a bomb too --, a start studio's
   station the base rent).  o.head: false in a dialog (its title says it) */
function studioOverview(views, o = {}) {
  const name = (v) => (v.renter >= 1 && v.renter <= 3 ? T('Start studio') : v.name?.[UI.lang] || v.name?.en || T('Studio {n}', { n: v.studio }));   /* (the reader's language: the launcher has no game-texts switch) */
  const kind = (v) => (v.renter === 255 ? 'free' : v.renter >= 1 && v.renter <= 3 ? 'station' : consulateRoom(v.room) ? 'locked' : 'public');
  const items = views.map((v) => ({ id: v.studio, x: v.x, y: v.y, kind: kind(v), tip: name(v) }));
  const free = views.filter((v) => kind(v) === 'free'), starts = views.filter((v) => kind(v) === 'station');
  const changedPublic = views.filter((v) => ['public', 'locked'].includes(kind(v)) && v.changed), rest = views.filter((v) => ['public', 'locked'].includes(kind(v)) && !v.changed);
  const atStart = (v) => ({ free: T('free|studio'), station: STATIONS[v.renter], public: T('public'), locked: T('public') })[kind(v)];
  const row = (v, cls) => [h('span', { class: 'snum inline ' + kind(v) }, String(v.studio)), h('span', { class: cls }, name(v)), h('span', { class: 'mono dim' }, String(v.floor)),
    h('span', { class: 'mono' }, String(v.size)), h('span', { class: cls }, atStart(v)),
    h('span', { class: 'mono num' }, numFmt(kind(v) === 'station' ? v.rent : studioDaily(v.rent)))];
  const groups = [free.flatMap((v) => row(v, 'yel')), starts.flatMap((v) => row(v, 'st' + v.renter)), changedPublic.flatMap((v) => row(v, ''))].filter((g) => g.length);
  const sum = h('span', { class: 'svsum' }, T('{free} free · {starts} start studios · {rest} public', { free: free.length, starts: starts.length, rest: changedPublic.length + rest.length }));
  return h('section', { class: 'studioview' + (o.head === false ? ' bare' : ''), 'data-studios': '' },
    o.head === false ? h('div', { class: 'svhead' }, sum)
      : h('div', { class: 'h3' }, T('Studios in the building'), h('span', { class: 'hint' }, T('with these packs, when a game starts')), sum),
    h('div', { class: 'svbody' },
      h('div', { class: 'svplan' }, studioPlan(items), h('div', { class: 'hint' }, T("The estate agent's plan, as in the game; the numbers are the list's."))),
      h('div', { class: 'svlist' },
        h('span', { class: 'th' }, T('No.')), h('span', { class: 'th' }, T('Room')), h('span', { class: 'th' }, T('Fl.')), h('span', { class: 'th' }, T('Size')),
        h('span', { class: 'th' }, T('At the start')), h('span', { class: 'th num', 'data-tip': T("What its renter pays a day: the estate agent's price (1.5 times the base rent), for a start studio its station the base rent") }, T('$ a day')),
        ...groups.flatMap((g, k) => (k ? [h('span', { class: 'sep' }), ...g] : g)),
        rest.length ? h('span', { class: 'rest' }, T('Public, as in the original: {list}. They become studios only after a bomb.', { list: rest.map((v) => `${v.studio} ${name(v)}`).join(', ') })) : null)));
}
