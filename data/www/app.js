/* LED controller web UI - vanilla JS, talks to the /api endpoints */
const $ = s => document.querySelector(s);
const $$ = s => [...document.querySelectorAll(s)];
const COLORS = ['#f6c344', '#4fc3f7', '#ff7b7b', '#7ddc8a', '#c792ea', '#ffab70', '#64ffda', '#f48fb1'];
const TZ = [
  ['Eastern (New York / Montreal)', 'EST5EDT,M3.2.0,M11.1.0'],
  ['Central (Chicago / Winnipeg)', 'CST6CDT,M3.2.0,M11.1.0'],
  ['Mountain (Denver / Edmonton)', 'MST7MDT,M3.2.0,M11.1.0'],
  ['Arizona (no DST)', 'MST7'],
  ['Pacific (Los Angeles / Vancouver)', 'PST8PDT,M3.2.0,M11.1.0'],
  ['Alaska', 'AKST9AKDT,M3.2.0,M11.1.0'],
  ['Hawaii', 'HST10'],
  ['Atlantic (Halifax)', 'AST4ADT,M3.2.0,M11.1.0'],
  ['Newfoundland', 'NST3:30NDT,M3.2.0,M11.1.0'],
  ['UTC', 'UTC0'],
  ['UK / Ireland / Portugal', 'GMT0BST,M3.5.0/1,M10.5.0'],
  ['Central Europe', 'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Eastern Europe', 'EET-2EEST,M3.5.0/3,M10.5.0/4'],
  ['India', 'IST-5:30'],
  ['China / Singapore', 'CST-8'],
  ['Japan / Korea', 'JST-9'],
  ['Australia East (Sydney)', 'AEST-10AEDT,M10.1.0,M4.1.0/3'],
  ['Australia East (Brisbane, no DST)', 'AEST-10'],
  ['New Zealand', 'NZST-12NZDT,M9.5.0,M4.1.0/3'],
];

let cfg = null, prof = null, st = null, selProf = 0, selCh = 0, manualTimer = null;

/* ---------------- helpers ---------------- */
async function api(path, body, method) {
  const o = { method: method || (body ? 'POST' : 'GET'), headers: {} };
  if (body) { o.headers['Content-Type'] = 'application/json'; o.body = JSON.stringify(body); }
  const r = await fetch('/api/' + path, o);
  let j = null; try { j = await r.json(); } catch (e) { }
  if (!r.ok) throw new Error((j && j.error) || r.statusText);
  return j;
}
function toast(msg, err) {
  const t = $('#toast'); t.textContent = msg; t.className = 'show' + (err ? ' err' : '');
  clearTimeout(t._t); t._t = setTimeout(() => t.className = '', 2600);
}
const fail = e => toast(e.message || String(e), true);
function hhmm(m) { m = ((Math.round(m) % 1440) + 1440) % 1440; return String(Math.floor(m / 60)).padStart(2, '0') + ':' + String(m % 60).padStart(2, '0'); }
function toMin(s) { const [h, m] = (s || '0:0').split(':').map(Number); return (h * 60 + m) % 1440; }
function fmt(v, d = 0) { return (v == null || isNaN(v)) ? '–' : Number(v).toFixed(d); }
function getPath(o, p) { return p.split('.').reduce((a, k) => a == null ? a : a[k], o); }
function setPath(o, p, v) { const ks = p.split('.'); let a = o; for (let i = 0; i < ks.length - 1; i++) { if (a[ks[i]] == null) a[ks[i]] = {}; a = a[ks[i]]; } a[ks[ks.length - 1]] = v; }
function bindForm(root, obj) {
  root.querySelectorAll('[data-k]').forEach(el => {
    const v = getPath(obj, el.dataset.k);
    if (el.type === 'checkbox') el.checked = !!v; else el.value = v == null ? '' : v;
  });
}
function readForm(root, obj) {
  root.querySelectorAll('[data-k]').forEach(el => {
    let v;
    if (el.type === 'checkbox') v = el.checked;
    else if (el.type === 'number') v = el.value === '' ? getPath(obj, el.dataset.k) : Number(el.value);
    else v = el.value;
    setPath(obj, el.dataset.k, v);
  });
}

/* ---------------- tabs ---------------- */
$$('nav button').forEach(b => b.onclick = () => {
  $$('nav button').forEach(x => x.classList.toggle('active', x === b));
  $$('.tab').forEach(x => x.classList.toggle('active', x.id === 'tab-' + b.dataset.tab));
  if (b.dataset.tab === 'sched') drawGraph();
});

/* ---------------- dashboard ---------------- */
function renderStatus() {
  const e = st.engine;
  $('#devName').textContent = st.name;
  $('#clock').textContent = st.time.now + (st.time.synced ? '' : st.time.approx ? ' (approx.)' : ' (clock not set)');
  const pm = $('#pillMode');
  pm.textContent = e.mode.toUpperCase() + (e.modeTimeoutS > 0 ? ' · ' + Math.ceil(e.modeTimeoutS / 60) + ' min' : '');
  pm.className = 'pill m-' + e.mode;
  $('#pillWifi').textContent = st.wifi.connected ? st.wifi.ssid + ' ' + st.wifi.rssi + ' dBm' : st.wifi.ap ? 'AP ' + st.wifi.apSsid : 'offline';
  $('#pillTemp').textContent = st.sensors.temp == null ? 'no temp sensor' : fmt(st.sensors.temp, 1) + ' °C' + (st.sensors.hum != null ? ' · ' + fmt(st.sensors.hum) + ' %RH' : '');
  $$('#modeBtns button').forEach(b => b.classList.toggle('active', b.dataset.mode === e.mode));
  if (document.activeElement !== $('#master')) $('#master').value = st.master;
  $('#masterVal').textContent = fmt(st.master) + ' %';

  const list = $('#chList');
  if (list.children.length !== e.channels.length) {
    list.innerHTML = e.channels.map((c, i) => `<div class="ch">
      <div class="row"><span class="dot" style="background:${COLORS[i]}"></span><b class="nm"></b><span class="pct"></span></div>
      <div class="bar"><div class="fill" style="background:${COLORS[i]}"></div><div class="tgt"></div></div>
      <input type="range" min="0" max="100" step="1" class="man" data-i="${i}">
      <div class="sub muted small"></div></div>`).join('');
    list.querySelectorAll('.man').forEach(r => r.oninput = sendManual);
  }
  e.channels.forEach((c, i) => {
    const d = list.children[i];
    d.querySelector('.nm').textContent = c.name;
    d.querySelector('.pct').textContent = fmt(c.output) + ' %';
    d.querySelector('.fill').style.width = c.output + '%';
    d.querySelector('.tgt').style.left = c.target + '%';
    d.querySelector('.sub').textContent = 'driver ' + fmt(c.driver, 1) + ' %' + (c.enabled ? '' : ' · disabled');
    const r = d.querySelector('.man');
    r.style.display = e.mode === 'manual' ? '' : 'none';
    if (document.activeElement !== r) r.value = c.manual;
  });

  $('#nowProfile').textContent = e.profile;
  $('#nowMoon').textContent = fmt(e.moon * 100) + ' % illuminated';
  $('#nowCloud').textContent = e.cloud < 0.999 ? 'dimmed to ' + fmt(e.cloud * 100) + ' %' : 'clear';
  $('#nowThermal').textContent = e.shutdown ? 'SHUTDOWN (over temperature)' : e.thermal < 0.999 ? 'dimmed to ' + fmt(e.thermal * 100) + ' %' : 'ok';
  $('#nowAccl').textContent = e.acclimation < 0.999 ? fmt(e.acclimation * 100) + ' %' : 'off / complete';
  $('#nowRelay').textContent = e.relay ? 'on' : 'off';
  $('#btnPreview').textContent = e.mode === 'preview' ? 'Stop preview (' + hhmm(e.simMinute || 0) + ')' : 'Preview day (60 s)';
  $('#btnStorm').textContent = e.stormS > 0 ? 'Stop storm (' + e.stormS + ' s)' : 'Storm (60 s)';
  if ($('#tab-sched').classList.contains('active')) drawGraph();
  if ($('#tab-sys').classList.contains('active')) renderSys();
}
function sendManual() {
  clearTimeout(manualTimer);
  manualTimer = setTimeout(() => {
    const levels = $$('#chList .man').map(r => Number(r.value));
    api('mode', { mode: 'manual', levels }).catch(fail);
  }, 120);
}
$$('#modeBtns button').forEach(b => b.onclick = () => {
  const body = { mode: b.dataset.mode };
  if (b.dataset.mode === 'manual') body.levels = st.engine.channels.map(c => Math.round(c.output));
  api('mode', body).then(poll).catch(fail);
});
$('#pause').onchange = e => {
  const s = Number(e.target.value); e.target.value = '';
  if (s) api('mode', { mode: 'off', timeoutS: s }).then(poll).catch(fail);
};
$('#master').onchange = e => api('master', { master: Number(e.target.value) }).then(poll).catch(fail);
$('#master').oninput = e => $('#masterVal').textContent = e.target.value + ' %';
$('#btnPreview').onclick = () => api('preview', st.engine.mode === 'preview' ? { stop: true } : { seconds: 60 }).then(poll).catch(fail);
$('#btnStorm').onclick = () => {
  if (st.engine.stormS > 0) return api('storm', { stop: true }).then(poll).catch(fail);
  if (!cfg.effects.lightning.enabled) return toast('Enable lightning in Effects first', true);
  api('storm', { seconds: 60 }).then(poll).catch(fail);
};

/* ---------------- schedule ---------------- */
function levelAt(pts, t, smooth) {
  const n = pts.length; if (!n) return 0; if (n === 1) return pts[0][1];
  t = ((t % 1440) + 1440) % 1440;
  let a = null, b = null, ta = 0, tb = 0;
  for (let i = 0; i + 1 < n; i++) if (t >= pts[i][0] && t < pts[i + 1][0]) { a = pts[i]; b = pts[i + 1]; ta = a[0]; tb = b[0]; break; }
  if (!a) { a = pts[n - 1]; b = pts[0]; if (t >= a[0]) { ta = a[0]; tb = b[0] + 1440; } else { ta = a[0] - 1440; tb = b[0]; } }
  const span = tb - ta; if (span <= 0) return b[1];
  let f = Math.min(1, Math.max(0, (t - ta) / span));
  if (smooth) f = 0.5 - 0.5 * Math.cos(f * Math.PI);
  return a[1] + (b[1] - a[1]) * f;
}
const curP = () => prof.profiles[selProf];
function ensureProfileShape(p) {
  p.channels = p.channels || [];
  while (p.channels.length < cfg.maxChannels) p.channels.push([]);
  p.channels.forEach(pts => pts.sort((x, y) => x[0] - y[0]));
}
function renderSched() {
  prof.profiles.forEach(ensureProfileShape);
  if (selProf >= prof.profiles.length) selProf = 0;
  if (selCh >= cfg.numChannels) selCh = 0;
  const sel = $('#profSel');
  sel.innerHTML = prof.profiles.map((p, i) => `<option value="${i}">${p.name}${i === prof.active ? ' ★' : ''}</option>`).join('');
  sel.value = selProf;
  const p = curP();
  $('#profName').value = p.name; $('#profSmooth').checked = !!p.smooth;
  $('#btnActive').disabled = selProf === prof.active;
  $('#btnAddProf').disabled = prof.profiles.length >= cfg.maxProfiles;
  $('#btnDelProf').disabled = prof.profiles.length <= 1;
  $('#chTabs').innerHTML = cfg.channels.slice(0, cfg.numChannels).map((c, i) =>
    `<button type="button" data-i="${i}" class="${i === selCh ? 'active' : ''}" style="color:${COLORS[i]}">${c.name}</button>`).join('');
  $$('#chTabs button').forEach(b => b.onclick = () => { selCh = Number(b.dataset.i); renderSched(); });
  const tb = $('#points tbody');
  tb.innerHTML = p.channels[selCh].map((pt, i) => `<tr>
    <td><input type="time" value="${hhmm(pt[0])}" data-i="${i}" class="pt-t"></td>
    <td><input type="number" min="0" max="100" step="1" value="${Math.round(pt[1] * 10) / 10}" data-i="${i}" class="pt-l"></td>
    <td><button type="button" class="secondary pt-x" data-i="${i}">✕</button></td></tr>`).join('');
  tb.querySelectorAll('.pt-t').forEach(el => el.onchange = () => { p.channels[selCh][el.dataset.i][0] = toMin(el.value); renderSched(); });
  tb.querySelectorAll('.pt-l').forEach(el => el.onchange = () => { p.channels[selCh][el.dataset.i][1] = Math.min(100, Math.max(0, Number(el.value))); drawGraph(); });
  tb.querySelectorAll('.pt-x').forEach(el => el.onclick = () => { p.channels[selCh].splice(el.dataset.i, 1); renderSched(); });
  $('#btnAddPt').disabled = p.channels[selCh].length >= cfg.maxPoints;
  drawGraph();
}
function drawGraph() {
  if (!prof || !cfg) return;
  const cv = $('#graph'), dpr = window.devicePixelRatio || 1;
  const W = cv.clientWidth || 600, H = 260;
  if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
  const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, W, H);
  const L = 34, R = 10, T = 10, B = 22, w = W - L - R, h = H - T - B;
  const X = m => L + m / 1440 * w, Y = l => T + h - l / 100 * h;
  g.strokeStyle = '#2a3441'; g.fillStyle = '#8b98a5'; g.font = '11px system-ui'; g.lineWidth = 1;
  for (let hr = 0; hr <= 24; hr += 3) { g.beginPath(); g.moveTo(X(hr * 60), T); g.lineTo(X(hr * 60), T + h); g.stroke(); g.textAlign = 'center'; g.fillText(String(hr).padStart(2, '0'), X(hr * 60), H - 6); }
  for (let l = 0; l <= 100; l += 25) { g.beginPath(); g.moveTo(L, Y(l)); g.lineTo(L + w, Y(l)); g.stroke(); g.textAlign = 'right'; g.fillText(l, L - 5, Y(l) + 4); }
  const p = curP();
  for (let c = 0; c < cfg.numChannels; c++) {
    if (!cfg.channels[c].enabled) continue;
    const pts = p.channels[c]; if (!pts.length) continue;
    g.strokeStyle = COLORS[c]; g.lineWidth = c === selCh ? 2.5 : 1.2; g.globalAlpha = c === selCh ? 1 : 0.55;
    g.beginPath();
    for (let x = 0; x <= w; x += 2) { const m = x / w * 1440, y = Y(levelAt(pts, m, p.smooth)); x === 0 ? g.moveTo(L + x, y) : g.lineTo(L + x, y); }
    g.stroke();
    if (c === selCh) { g.fillStyle = COLORS[c]; pts.forEach(pt => { g.beginPath(); g.arc(X(pt[0]), Y(pt[1]), 4.5, 0, 7); g.fill(); }); }
  }
  g.globalAlpha = 1;
  if (st && st.time && (st.time.known || st.engine.mode === 'preview')) {
    const m = st.engine.mode === 'preview' ? st.engine.simMinute : st.time.minute;
    g.strokeStyle = '#ff6b6b'; g.lineWidth = 1.5; g.setLineDash([4, 3]);
    g.beginPath(); g.moveTo(X(m), T); g.lineTo(X(m), T + h); g.stroke(); g.setLineDash([]);
  }
}
$('#graph').onclick = ev => {
  const cv = $('#graph'), r = cv.getBoundingClientRect();
  const L = 34, R = 10, T = 10, B = 22, w = r.width - L - R, h = r.height - T - B;
  const m = Math.round(Math.min(1439, Math.max(0, (ev.clientX - r.left - L) / w * 1440)) / 5) * 5;
  const l = Math.round(Math.min(100, Math.max(0, (T + h - (ev.clientY - r.top)) / h * 100)));
  const pts = curP().channels[selCh];
  if (pts.length >= cfg.maxPoints) return toast('Max ' + cfg.maxPoints + ' points', true);
  pts.push([m, l]); renderSched();
};
$('#profSel').onchange = e => { selProf = Number(e.target.value); renderSched(); };
$('#profName').onchange = e => { curP().name = e.target.value.slice(0, 19) || 'Profile'; renderSched(); };
$('#profSmooth').onchange = e => { curP().smooth = e.target.checked; drawGraph(); };
$('#btnAddPt').onclick = () => { const pts = curP().channels[selCh]; const last = pts.length ? pts[pts.length - 1] : [600, 50]; pts.push([(last[0] + 60) % 1440, last[1]]); renderSched(); };
$('#btnCopyAll').onclick = () => { const p = curP(), src = p.channels[selCh]; for (let c = 0; c < cfg.maxChannels; c++) if (c !== selCh) p.channels[c] = src.map(pt => [...pt]); renderSched(); toast('Curve copied to all channels'); };
function wizardPts() {
  const rs = toMin($('#wzRise').value), ramp = Math.max(1, Number($('#wzRamp').value)), se = toMin($('#wzSet').value), pk = Math.min(100, Math.max(1, Number($('#wzPeak').value)));
  return [[rs, 0], [(rs + ramp) % 1440, pk], [((se - ramp) % 1440 + 1440) % 1440, pk], [se, 0]];
}
$('#btnWzCh').onclick = () => { curP().channels[selCh] = wizardPts(); renderSched(); };
$('#btnWzAll').onclick = () => { const p = curP(); for (let c = 0; c < cfg.maxChannels; c++) p.channels[c] = wizardPts(); renderSched(); };
$('#btnAddProf').onclick = () => { prof.profiles.push({ name: 'Profile ' + (prof.profiles.length + 1), smooth: true, channels: [] }); selProf = prof.profiles.length - 1; renderSched(); };
$('#btnDelProf').onclick = () => {
  if (!confirm('Delete profile "' + curP().name + '"?')) return;
  prof.profiles.splice(selProf, 1);
  if (prof.active >= prof.profiles.length) prof.active = prof.profiles.length - 1;
  else if (prof.active > selProf) prof.active--;
  selProf = Math.min(selProf, prof.profiles.length - 1); renderSched();
};
async function saveProfiles(msg) {
  try { await api('profiles', prof); toast(msg || 'Schedules saved'); await loadProfiles(); poll(); } catch (e) { fail(e); }
}
$('#btnSaveProf').onclick = () => saveProfiles();
$('#btnActive').onclick = () => { prof.active = selProf; saveProfiles('"' + curP().name + '" is now active'); };

/* ---------------- effects ---------------- */
function renderFx() {
  bindForm($('#fxForm'), cfg);
  const a = cfg.acclimation;
  if (a.enabled && a.startEpoch) {
    const days = (Date.now() / 1000 - a.startEpoch) / 86400;
    $('#acclInfo').textContent = days >= a.days ? 'completed' : 'day ' + Math.floor(days + 1) + ' of ' + a.days + ' (' + fmt(st ? st.engine.acclimation * 100 : 0) + ' %)';
  } else $('#acclInfo').textContent = '';
}
$('#btnSaveFx').onclick = async () => { readForm($('#fxForm'), cfg); try { await api('config', cfg); toast('Effects saved'); await loadConfig(); } catch (e) { fail(e); } };
$('#btnAcclNow').onclick = async () => {
  readForm($('#fxForm'), cfg);
  try { await api('acclimation/start', { start: cfg.acclimation.start, days: cfg.acclimation.days }); toast('Acclimation started'); await loadConfig(); } catch (e) { fail(e); }
};

/* ---------------- settings ---------------- */
function renderSet() {
  const tz = $('#tzSel');
  if (!tz.options.length) {
    tz.innerHTML = '<option value="">custom…</option>' + TZ.map(t => `<option value="${t[1]}">${t[0]}</option>`).join('');
    tz.onchange = () => { if (tz.value) $('[data-k="time.tz"]').value = tz.value; };
  }
  bindForm($('#setForm'), cfg);
  tz.value = TZ.some(t => t[1] === cfg.time.tz) ? cfg.time.tz : '';
  $('#numCh').max = cfg.maxChannels;
  $('#wfSsid').value = $('#wfSsid').value || cfg.wifi.ssid;
  $('#chTable tbody').innerHTML = cfg.channels.slice(0, cfg.maxChannels).map((c, i) => `<tr>
    <td>${i + 1}</td>
    <td><input type="checkbox" data-k="channels.${i}.enabled"></td>
    <td><input data-k="channels.${i}.name" maxlength="15" style="min-width:110px"></td>
    <td><input type="number" min="-1" max="48" data-k="channels.${i}.pin"></td>
    <td><input type="checkbox" data-k="channels.${i}.invert"></td>
    <td><input type="number" min="0" max="100" step="0.5" data-k="channels.${i}.min"></td>
    <td><input type="number" min="0" max="100" step="0.5" data-k="channels.${i}.max"></td>
    <td><input type="number" min="0.2" max="4" step="0.1" data-k="channels.${i}.gamma"></td>
    <td><input type="checkbox" data-k="channels.${i}.moon"></td>
    <td><input type="checkbox" data-k="channels.${i}.effects"></td></tr>`).join('');
  bindForm($('#chTable'), cfg);
  if (st) {
    $('#wifiState').textContent = st.wifi.connected ? 'connected to ' + st.wifi.ssid + ' (' + st.wifi.ip + ')' : st.wifi.ap ? 'access point mode, not connected' : 'not connected';
    $('#timeState').textContent = st.time.synced ? 'NTP synced' : st.time.approx ? 'approximate (restored / manual)' : 'clock not set';
  }
}
$('#btnSaveSet').onclick = async () => {
  readForm($('#setForm'), cfg);
  try { await api('config', cfg); toast('Settings saved'); await loadConfig(); } catch (e) { fail(e); }
};
$('#btnScan').onclick = async () => {
  $('#wifiState').textContent = 'scanning…';
  try {
    let nets = (await api('wifi/scan')).networks;
    if (!nets.length) { await new Promise(r => setTimeout(r, 2500)); nets = (await api('wifi/scan')).networks; }
    nets.sort((a, b) => b.rssi - a.rssi);
    const seen = new Set();
    $('#scanList').innerHTML = nets.filter(n => n.ssid && !seen.has(n.ssid) && seen.add(n.ssid))
      .map(n => `<div data-ssid="${n.ssid.replace(/"/g, '&quot;')}"><span>${n.secure ? '🔒 ' : ''}${n.ssid}</span><span class="muted">${n.rssi} dBm</span></div>`).join('') || '<div class="muted">nothing found, try again</div>';
    $$('#scanList div[data-ssid]').forEach(d => d.onclick = () => { $('#wfSsid').value = d.dataset.ssid; $('#wfPass').focus(); });
    $('#wifiState').textContent = nets.length + ' networks';
  } catch (e) { fail(e); }
};
$('#btnConnect').onclick = async () => {
  const ssid = $('#wfSsid').value.trim(); if (!ssid) return toast('SSID required', true);
  try {
    await api('wifi', { ssid, pass: $('#wfPass').value });
    toast('Connecting to ' + ssid + '… if you are on the access point, rejoin your network and open http://' + cfg.device.hostname + '.local');
  } catch (e) { fail(e); }
};
$('#btnBrowserTime').onclick = () => api('time', { epoch: Math.floor(Date.now() / 1000) }).then(() => { toast('Clock set'); poll(); }).catch(fail);

/* ---------------- system ---------------- */
function renderSys() {
  const up = st.uptimeS, d = Math.floor(up / 86400), hh = Math.floor(up % 86400 / 3600), mm = Math.floor(up % 3600 / 60);
  const rows = [['Firmware', st.fw], ['Uptime', (d ? d + 'd ' : '') + hh + 'h ' + mm + 'm'], ['Free heap', Math.round(st.heap / 1024) + ' kB'],
    ['Hostname', st.wifi.hostname + '.local'], ['IP', st.wifi.ip || st.wifi.apIp || '–'], ['MAC', st.wifi.mac], ['Time', st.time.now + ' (' + st.time.tz + ')'],
    ['Local temp sensor', st.sensors.local ? 'DS18B20 found' : 'none']];
  $('#sysInfo').innerHTML = rows.map(r => `<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join('');
}
$('#btnReboot').onclick = () => confirm('Reboot the controller?') && api('reboot').then(() => toast('Rebooting…')).catch(fail);
$('#btnAp').onclick = () => api('wifi/ap').then(() => toast('Access point started')).catch(fail);
$('#btnFactory').onclick = () => confirm('Erase all settings and schedules? The device restarts in access-point mode.') && api('factory').then(() => toast('Resetting…')).catch(fail);
$('#btnOta').onclick = () => {
  const f = $('#otaFile').files[0]; if (!f) return toast('Choose a .bin file first', true);
  const type = $('#otaType').value;
  if (!confirm('Upload ' + f.name + ' as ' + (type === 'fs' ? 'filesystem' : 'firmware') + '? The device reboots afterwards.')) return;
  const fd = new FormData(); fd.append('update', f, f.name);
  const xhr = new XMLHttpRequest(), prog = $('#otaProg');
  prog.style.display = ''; prog.value = 0; $('#btnOta').disabled = true;
  xhr.upload.onprogress = ev => { if (ev.lengthComputable) prog.value = ev.loaded / ev.total * 100; };
  xhr.onload = () => { $('#btnOta').disabled = false; prog.style.display = 'none'; xhr.status === 200 ? toast('Update done, rebooting…') : toast('Update failed: ' + xhr.responseText, true); };
  xhr.onerror = () => { $('#btnOta').disabled = false; prog.style.display = 'none'; toast('Upload error', true); };
  xhr.open('POST', '/api/update?type=' + type); xhr.send(fd);
};
$('#restoreFile').onchange = async e => {
  const f = e.target.files[0]; if (!f) return;
  try {
    const j = JSON.parse(await f.text());
    if (!j.config && !j.profiles) throw new Error('not a backup file');
    if (!confirm('Restore settings' + (j.profiles ? ' and schedules' : '') + ' from ' + f.name + '?')) return;
    await api('restore', j); toast('Restored'); await loadConfig(); await loadProfiles(); poll();
  } catch (err) { fail(err); }
  e.target.value = '';
};

/* ---------------- loading / polling ---------------- */
async function loadConfig() { cfg = await api('config'); renderFx(); renderSet(); if (prof) renderSched(); }
async function loadProfiles() { prof = await api('profiles'); if (cfg) renderSched(); }
let pollTimer = null, failures = 0;
async function poll() {
  clearTimeout(pollTimer);
  try { st = await api('status'); failures = 0; renderStatus(); }
  catch (e) { if (++failures === 3) toast('Lost connection to the controller', true); }
  pollTimer = setTimeout(poll, st && st.engine.mode === 'preview' ? 700 : 2000);
}
(async () => {
  try { await loadConfig(); await loadProfiles(); await poll(); }
  catch (e) { fail(e); setTimeout(() => location.reload(), 4000); }
})();
window.addEventListener('resize', drawGraph);
