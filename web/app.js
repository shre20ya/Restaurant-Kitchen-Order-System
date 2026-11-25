/* Kitchen Queue - front end. Talks to the C server through /api/... */
const CURRENCY = '₹';          // change this symbol if you like

const $  = (s, el = document) => el.querySelector(s);
const $$ = (s, el = document) => [...el.querySelectorAll(s)];

let S = null;                  // last state from the server
let lastRaw = '';
let clockOffset = 0;           // serverTime - browserTime (seconds)
let cart = {};                 // menuId -> qty
let isVip = false;
let timerHandle, pollHandle;

/* ---------- helpers ---------- */
const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const money = n => CURRENCY + (Number.isInteger(+n) ? (+n).toFixed(0) : (+n).toFixed(2));
const nowSec = () => Date.now() / 1000 + clockOffset;
const mmss = sec => { sec = Math.max(0, Math.floor(sec)); return String(Math.floor(sec / 60)).padStart(2, '0') + ':' + String(sec % 60).padStart(2, '0'); };

const EMOJI = [['pizza','🍕'],['pasta','🍝'],['chicken','🍗'],['salad','🥗'],['fries','🍟'],['burger','🍔'],
  ['soup','🍲'],['coffee','☕'],['tea','🍵'],['cake','🍰'],['ice','🍨'],['sandwich','🥪'],['noodle','🍜'],
  ['rice','🍚'],['juice','🧃'],['shake','🥤'],['cola','🥤'],['fish','🐟'],['egg','🥚'],['paneer','🧀'],
  ['momo','🥟'],['wrap','🌯'],['roll','🌯'],['taco','🌮'],['biryani','🍛'],['curry','🍛'],['dosa','🥞'],['naan','🫓']];
function emoji(name) {
  const n = name.toLowerCase();
  for (const [k, e] of EMOJI) if (n.includes(k)) return e;
  return '🍽️';
}

function toast(text, kind = 'ok') {
  const t = document.createElement('div');
  t.className = 'toast' + (kind === 'err' ? ' err' : '');
  t.textContent = text;
  $('#toasts').appendChild(t);
  setTimeout(() => t.remove(), 3500);
}

async function post(path, data = {}) {
  try {
    const r = await fetch(path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams(data).toString()
    });
    const j = await r.json();
    toast(j.ok ? j.message : j.error, j.ok ? 'ok' : 'err');
    await refresh(true);
    return j;
  } catch (e) {
    toast('Server not reachable', 'err');
    setConn(false);
    return { ok: false };
  }
}

function setConn(on) {
  const c = $('#conn');
  c.classList.toggle('off', !on);
  $('b', c).textContent = on ? 'Live' : 'Offline';
}

/* ---------- polling ---------- */
async function refresh(force = false) {
  try {
    const r = await fetch('/api/state');
    const raw = await r.text();
    setConn(true);
    const j = JSON.parse(raw);
    clockOffset = j.now - Date.now() / 1000;
    if (force || raw.replace(/"now":\d+,/, '') !== lastRaw) {
      lastRaw = raw.replace(/"now":\d+,/, '');
      S = j;
      render();
    }
  } catch (e) {
    setConn(false);
  }
}

/* ---------- rendering ---------- */
function itemsHtml(o) {
  return '<ul class="items">' + o.items.map(i => `<li><span class="q">${i.qty}×</span>${esc(i.name)}</li>`).join('') + '</ul>';
}

function emptyHtml(icon, text) {
  return `<div class="empty"><span class="e">${icon}</span>${text}</div>`;
}

function pendingCard(o) {
  return `<article class="card pending ${o.vip ? 'vip' : ''}">
    <div class="card-top"><span class="oid">#${o.id}</span><span class="tbl">Table ${o.table}</span>
      ${o.vip ? '<span class="badge vip">★ VIP</span>' : ''}
      <span class="badge pos">#${o.pos} in queue</span></div>
    ${itemsHtml(o)}
    <div class="meta"><span>Est. wait ~${o.wait} min · waiting <span class="timer-inline" data-since="${o.created}">00:00</span></span><b>${money(o.total)}</b></div>
    <div class="card-actions"><button class="act red" data-act="cancel" data-id="${o.id}">✕ Cancel order</button></div>
  </article>`;
}

function cookingCard(o) {
  return `<article class="card cooking ${o.vip ? 'vip' : ''}">
    <div class="card-top"><span class="oid">#${o.id}</span><span class="tbl">Table ${o.table}</span>
      ${o.vip ? '<span class="badge vip">★ VIP</span>' : ''}
      <span class="timer" data-since="${o.started}">00:00</span></div>
    ${itemsHtml(o)}
    <div class="meta"><span>Target ${o.prepMin} min</span><b>${money(o.total)}</b></div>
    <div class="bar"><i data-since="${o.started}" data-prep="${o.prepMin}"></i></div>
    <div class="card-actions"><button class="act green" data-act="ready" data-id="${o.id}">✓ Mark READY</button></div>
  </article>`;
}

function readyCard(o) {
  return `<article class="card ready ${o.vip ? 'vip' : ''}">
    <div class="card-top"><span class="oid">#${o.id}</span><span class="tbl">Table ${o.table}</span>
      ${o.vip ? '<span class="badge vip">★ VIP</span>' : ''}
      <span class="timer" data-since="${o.finished}">00:00</span></div>
    ${itemsHtml(o)}
    <div class="meta"><span>Waiting to be served</span><b>${money(o.total)}</b></div>
    <div class="card-actions"><button class="act blue" data-act="serve" data-id="${o.id}">🍽 Serve to table ${o.table}</button></div>
  </article>`;
}

function histCard(o) {
  const items = o.items.map(i => i.qty + '× ' + esc(i.name)).join(', ');
  return `<div class="hist" title="${items}"><b>#${o.id}</b><span>T${o.table}</span>
    <span class="s ${o.status}">${o.status === 'SERVED' ? 'Served' : 'Cancelled'}</span>
    <span class="amt">${money(o.total)}</span></div>`;
}

function render() {
  if (!S) return;
  const st = S.stats;

  $('#chips').innerHTML = [
    ['🕒', st.pending, 'In queue'], ['🔥', st.cooking + ' / ' + S.chefs, 'Chefs busy'],
    ['🛎️', st.ready, 'Ready to serve'], ['✅', st.served, 'Served today'], ['💰', money(st.revenue), 'Revenue']
  ].map(c => `<div class="chip"><div class="ic">${c[0]}</div><div><b>${c[1]}</b><span>${c[2]}</span></div></div>`).join('');

  const fill = (id, list, fn, icon, text) => {
    $('#col-' + id).innerHTML = list.length ? list.map(fn).join('') : emptyHtml(icon, text);
    $('#n-' + id).textContent = list.length;
  };
  fill('pending', S.pending, pendingCard, '🧾', 'No orders waiting.<br>Place a new order to begin.');
  fill('cooking', S.cooking, cookingCard, '🍳', 'Nothing on the stove.');
  fill('ready',   S.ready,   readyCard,   '🛎️', 'No dishes waiting.');
  fill('done', [...S.done].reverse(), histCard, '📜', 'History will appear here.');

  renderMenu();
  renderReport();
  tick();
}

function renderMenu() {
  $('#menuGrid').innerHTML = S.menu.map(m => `<div class="mcard"><span class="em">${emoji(m.name)}</span>
    <h4>${esc(m.name)}</h4><div class="row"><span class="price">${money(m.price)}</span><span>⏱ ${m.prep} min</span></div></div>`).join('');
}

function renderReport() {
  const st = S.stats;
  const avg = st.avgPrepSec ? mmss(st.avgPrepSec) : '–';
  const cards = [
    ['Revenue', money(st.revenue), '', 'rgba(52,211,153,.14)'],
    ['Orders served', st.served, `${st.vipServed} VIP`, 'rgba(96,165,250,.14)'],
    ['Cancelled', st.cancelled, '', 'rgba(248,113,113,.14)'],
    ['Avg prep time', avg, 'mm:ss', 'rgba(255,122,69,.14)'],
    ['Active now', st.pending + st.cooking + st.ready, `${st.pending} queued · ${st.cooking} cooking · ${st.ready} ready`, 'rgba(251,191,36,.14)']
  ];
  $('#statGrid').innerHTML = cards.map(c => `<div class="stat" style="--glow:${c[3]}"><span>${c[0]}</span><b>${c[1]}</b><small>${c[2]}</small></div>`).join('');

  const sold = [...st.sold].sort((a, b) => b.qty - a.qty);
  const max = Math.max(1, ...sold.map(s => s.qty));
  $('#bars').innerHTML = sold.some(s => s.qty) ?
    sold.filter(s => s.qty).map(s => `<div class="brow"><div class="top"><span>${emoji(s.name)} ${esc(s.name)}</span><b>${s.qty}</b></div>
      <div class="track"><div class="fill" style="width:${s.qty / max * 100}%"></div></div></div>`).join('')
    : emptyHtml('📊', 'Serve some orders to see best sellers.');

  const rows = [...S.done].reverse().map(o => `<tr><td><b>#${o.id}</b></td><td>${o.table}</td>
    <td>${o.items.map(i => i.qty + '× ' + esc(i.name)).join(', ')}</td>
    <td><span class="s ${o.status} hist-s" style="font-size:11px;font-weight:700;padding:2px 8px;border-radius:99px">${o.status === 'SERVED' ? 'Served' : 'Cancelled'}</span></td>
    <td>${money(o.total)}</td></tr>`).join('');
  $('#histTable').innerHTML = '<tr><th>Order</th><th>Table</th><th>Items</th><th>Status</th><th>Total</th></tr>' +
    (rows || '<tr><td colspan="5" style="text-align:center;color:var(--muted);padding:30px">No completed orders yet.</td></tr>');
}

/* live timers + progress bars */
function tick() {
  const t = nowSec();
  $$('.timer, .timer-inline').forEach(el => { el.textContent = mmss(t - +el.dataset.since); });
  $$('.bar i').forEach(el => {
    const pct = Math.min(100, (t - +el.dataset.since) / (+el.dataset.prep * 60) * 100);
    el.style.width = pct + '%';
  });
}

/* ---------- navigation ---------- */
$('#tabs').addEventListener('click', e => {
  const b = e.target.closest('.tab');
  if (!b) return;
  $$('.tab').forEach(t => t.classList.toggle('active', t === b));
  $$('.view').forEach(v => v.classList.toggle('active', v.id === 'view-' + b.dataset.view));
});

/* ---------- order actions (cards) ---------- */
document.addEventListener('click', e => {
  const b = e.target.closest('[data-act]');
  if (!b) return;
  const id = b.dataset.id;
  if (b.dataset.act === 'cancel') post('/api/cancel', { id });
  if (b.dataset.act === 'ready')  post('/api/ready',  { id });
  if (b.dataset.act === 'serve')  post('/api/serve',  { id });
});
$('#btnStart').onclick = $('#btnStart2').onclick = () => post('/api/start');

/* ---------- new order dialog ---------- */
const dlgOrder = $('#dlgOrder');

function openOrder() {
  if (!S) return;
  cart = {}; isVip = false;
  $('#tblNum').value = 1;
  $$('#priSeg button').forEach(b => b.classList.toggle('on', b.dataset.vip === '0'));
  renderPick();
  renderCart();
  dlgOrder.showModal();
}
$('#btnNew').onclick = openOrder;

function renderPick() {
  $('#pickGrid').innerHTML = S.menu.map(m => `<div class="pitem" data-id="${m.id}">
    <span class="em">${emoji(m.name)}</span><h4>${esc(m.name)}</h4><small>${money(m.price)} · ${m.prep} min</small>
    <div class="stepper"><button type="button" data-d="-1">−</button><b>0</b><button type="button" data-d="1">＋</button></div></div>`).join('');
}

$('#pickGrid').addEventListener('click', e => {
  const b = e.target.closest('button[data-d]');
  if (!b) return;
  const card = b.closest('.pitem'), id = card.dataset.id;
  const q = Math.min(20, Math.max(0, (cart[id] || 0) + +b.dataset.d));
  if (q) cart[id] = q; else delete cart[id];
  $('b', card).textContent = q;
  card.classList.toggle('on', q > 0);
  renderCart();
});

function renderCart() {
  const ids = Object.keys(cart);
  let total = 0, prep = 0;
  $('#cartList').innerHTML = ids.length ? ids.map(id => {
    const m = S.menu.find(x => x.id == id);
    total += m.price * cart[id];
    prep = Math.max(prep, m.prep);
    return `<li><span>${cart[id]}× ${esc(m.name)}</span><b>${money(m.price * cart[id])}</b></li>`;
  }).join('') : '<li class="none">No items selected yet.</li>';
  $('#cartTotal').textContent = money(total);
  $('#cartPrep').textContent = prep ? prep + ' min' : '–';
  $('#btnPlace').disabled = !ids.length;
}

const clampTable = () => { const v = Math.min(50, Math.max(1, parseInt($('#tblNum').value) || 1)); $('#tblNum').value = v; return v; };
$('#tblMinus').onclick = () => { $('#tblNum').value = clampTable() - 1; clampTable(); };
$('#tblPlus').onclick  = () => { $('#tblNum').value = clampTable() + 1; clampTable(); };
$('#tblNum').onchange  = clampTable;

$('#priSeg').addEventListener('click', e => {
  const b = e.target.closest('button');
  if (!b) return;
  isVip = b.dataset.vip === '1';
  $$('#priSeg button').forEach(x => x.classList.toggle('on', x === b));
});

$('#btnPlace').onclick = async () => {
  const items = Object.entries(cart).map(([id, q]) => id + ':' + q).join(',');
  const r = await post('/api/order', { table: clampTable(), vip: isVip ? 1 : 0, items });
  if (r.ok) dlgOrder.close();
};

/* ---------- add menu item ---------- */
$('#addForm').addEventListener('submit', async e => {
  e.preventDefault();
  const r = await post('/api/menu', { name: $('#m-name').value.trim(), price: $('#m-price').value, prep: $('#m-prep').value });
  if (r.ok) e.target.reset();
});

/* ---------- exit ---------- */
$('#btnExit').onclick = () => $('#dlgExit').showModal();
$('#btnExitYes').onclick = async () => {
  try { await fetch('/api/exit', { method: 'POST' }); } catch (e) {}
  clearInterval(timerHandle); clearInterval(pollHandle);
  $('#dlgExit').close();
  document.body.innerHTML = `<div class="bye"><div><div class="logo">🍳</div><h1>Server stopped</h1>
    <p>All orders were saved. You can close this tab.</p><p>Run the program again to restart.</p></div></div>`;
};

/* generic dialog close buttons */
document.addEventListener('click', e => {
  if (e.target.matches('[data-close]')) e.target.closest('dialog').close();
  if (e.target.tagName === 'DIALOG') e.target.close();      // click on backdrop
});

/* ---------- keyboard shortcuts ---------- */
document.addEventListener('keydown', e => {
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (document.querySelector('dialog[open]') || /INPUT|TEXTAREA/.test(document.activeElement.tagName)) return;
  if (e.key === 'n' || e.key === 'N') { e.preventDefault(); openOrder(); }
  if (e.key === 's' || e.key === 'S') { e.preventDefault(); post('/api/start'); }
});

/* ---------- start ---------- */
refresh(true);
pollHandle  = setInterval(refresh, 2000);
timerHandle = setInterval(tick, 1000);
