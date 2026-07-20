// app.js — admin frontend for CoreS3 WiFi provisioning.
// Talks to the backend over /ws/admin (live device status + scan results) and the
// REST endpoints for triggering scan / sending credentials.

const $ = (id) => document.getElementById(id);
let selectedSsid = null;
let scanning = false;

// --- WebSocket to backend ---------------------------------------------------
function connectWs() {
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const ws = new WebSocket(`${proto}://${location.host}/ws/admin`);
  ws.onopen = () => ws.send(JSON.stringify({ type: 'get_status' }));
  ws.onmessage = (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    if (msg.type === 'device_status') renderDeviceStatus(msg);
    else if (msg.type === 'wifi_list') renderNetworks(msg.networks || []);
    else if (msg.type === 'wifi_result') renderResult(msg);
  };
  ws.onclose = () => setTimeout(connectWs, 2000); // auto-reconnect
}

// --- Step 1: USB status -----------------------------------------------------
function renderDeviceStatus(s) {
  const dot = $('usbDot');
  const card = $('wifiCard');
  if (s.connected) {
    dot.className = 'dot on';
    $('usbText').textContent = `CoreS3 connected${s.portPath ? ' on ' + s.portPath : ''}`;
    card.classList.remove('disabled');
    if (s.wifi && s.wifi.connected) {
      $('wifiState').textContent = `Connected to ${s.wifi.ssid || 'WiFi'}${s.wifi.ip ? ' · ' + s.wifi.ip : ''}`;
    }
  } else {
    dot.className = 'dot off';
    $('usbText').textContent = 'Waiting for device…';
    card.classList.add('disabled');
  }
}

// --- Step 2: WiFi -----------------------------------------------------------
function signalBars(rssi) {
  // rssi in dBm: -50 strong .. -90 weak → 1..4 bars
  const level = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
  let html = '<span class="bars">';
  for (let i = 1; i <= 4; i++) {
    const h = 4 + i * 2.5;
    const color = i <= level ? 'var(--accent)' : 'var(--muted)';
    html += `<i style="height:${h}px;background:${color}"></i>`;
  }
  return html + '</span>';
}

function renderNetworks(nets) {
  scanning = false;
  $('refreshBtn').textContent = 'Refresh';
  const ul = $('nets');
  if (!nets.length) {
    ul.innerHTML = '<li class="empty">No networks found. Try Refresh again.</li>';
    return;
  }
  // de-dupe by ssid, strongest first
  const seen = new Map();
  for (const n of nets) if (!seen.has(n.ssid) || n.rssi > seen.get(n.ssid).rssi) seen.set(n.ssid, n);
  const list = [...seen.values()].sort((a, b) => b.rssi - a.rssi);

  ul.innerHTML = '';
  for (const n of list) {
    const li = document.createElement('li');
    li.innerHTML = `${signalBars(n.rssi)}<span class="ssid">${escapeHtml(n.ssid)}</span>` +
      `${n.secure ? '<span class="lock">🔒</span>' : ''}`;
    li.onclick = () => selectNetwork(n, li);
    if (n.ssid === selectedSsid) li.classList.add('sel');
    ul.appendChild(li);
  }
}

function selectNetwork(n, li) {
  selectedSsid = n.ssid;
  document.querySelectorAll('ul.nets li').forEach((el) => el.classList.remove('sel'));
  li.classList.add('sel');
  $('selName').textContent = n.ssid;
  $('confirmBtn').disabled = false;
}

function renderResult(msg) {
  const el = $('result');
  if (msg.ok) {
    el.className = 'result good';
    el.textContent = `Connected${msg.ip ? ' · ' + msg.ip : ''}. The device will now stream data.`;
  } else {
    el.className = 'result bad';
    el.textContent = 'Could not connect. Check the password and try again.';
  }
  $('confirmBtn').disabled = false;
  $('confirmBtn').textContent = 'Confirm';
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

// --- Actions ----------------------------------------------------------------
$('refreshBtn').onclick = async () => {
  if (scanning) return;
  scanning = true;
  $('refreshBtn').textContent = 'Scanning…';
  $('nets').innerHTML = '<li class="empty">Scanning…</li>';
  await fetch('/api/device/scan', { method: 'POST' });
  // results arrive over the WS as wifi_list
};

$('confirmBtn').onclick = async () => {
  if (!selectedSsid) return;
  $('confirmBtn').disabled = true;
  $('confirmBtn').textContent = 'Connecting…';
  $('result').textContent = '';
  await fetch('/api/device/wifi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid: selectedSsid, password: $('password').value }),
  });
  // result arrives over the WS as wifi_result
};

// initial status (in case WS connects slowly)
fetch('/api/device/status').then((r) => r.json()).then(renderDeviceStatus).catch(() => {});
connectWs();
