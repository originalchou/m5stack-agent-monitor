// server.ts — entrypoint. Wires the store, REST routes, WebSocket channels, the
// USB-serial device bridge, and the static admin frontend on :3050.

import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import express, { type ErrorRequestHandler } from 'express';
import { InMemorySessionStore } from './store';
import { createHooksRouter } from './routes/hooks';
import { createSessionsRouter } from './routes/sessions';
import { createDeviceRouter } from './routes/device';
import { Realtime } from './realtime';
import { DeviceSerial } from './serial';
import { lanIp } from './net-util';

const PORT = Number(process.env.PORT ?? 3050);
const __dirname = path.dirname(fileURLToPath(import.meta.url));

const store = new InMemorySessionStore();
const realtime = new Realtime(store);

const app = express();
app.use(express.json({ limit: '10mb' }));
app.use(express.static(path.join(__dirname, '../public'))); // admin frontend

// The device connects back over WiFi using the machine's LAN IP (not localhost).
const backendWsUrl = `ws://${lanIp()}:${PORT}/ws/device`;
const serial = new DeviceSerial({
  backendWsUrl,
  onEvent: (e) => realtime.broadcastAdmin(e),
});

app.get('/health', (_req, res) => {
  res.json({ ok: true, uptime: process.uptime(), device: serial.getStatus() });
});
app.use('/api/hooks', createHooksRouter(store, realtime));
app.use('/api/sessions', createSessionsRouter(store));
app.use('/api/device', createDeviceRouter(serial));

const onError: ErrorRequestHandler = (err, _req, res, _next) => {
  console.error('[backend] request error:', err instanceof Error ? err.message : err);
  if (!res.headersSent) res.status(400).json({ error: 'bad request' });
};
app.use(onError);

// Device -> backend: confirm a finished task, which removes it from tracking.
realtime.onDeviceMessage((msg) => {
  if (msg?.type === 'confirm_done' && typeof msg.sessionId === 'string') {
    store.removeSession('codex', msg.sessionId);
    realtime.broadcastSnapshot();
  }
});

// Admin frontend -> backend: mirror the HTTP controls over the WS for convenience.
realtime.onAdminMessage((msg, ws) => {
  if (msg?.type === 'get_status') {
    ws.send(JSON.stringify({ type: 'device_status', ...serial.getStatus() }));
  } else if (msg?.type === 'scan_wifi') {
    serial.scanWifi();
  } else if (msg?.type === 'set_wifi') {
    serial.setWifi(String(msg.ssid ?? ''), String(msg.password ?? ''));
  }
});

const server = http.createServer(app);
realtime.attach(server);

server.listen(PORT, () => {
  console.log(`\nagent-monitor backend on http://localhost:${PORT}  (device WS: ${backendWsUrl})`);
  console.log(`  POST /api/hooks/codex                 (Codex lifecycle hooks)`);
  console.log(`  GET  /api/sessions[?agentType=codex]  (list sessions)`);
  console.log(`  GET  /api/sessions/codex/:sessionId   (session detail, ?raw=1)`);
  console.log(`  GET  /api/device/status | POST /api/device/scan | POST /api/device/wifi`);
  console.log(`  WS   /ws/device  (CoreS3 realtime)    WS /ws/admin  (frontend)`);
  console.log(`  GET  /            (WiFi admin page)\n`);
  if (process.env.AGENT_MONITOR_NO_SERIAL) {
    console.log('  (serial device bridge disabled via AGENT_MONITOR_NO_SERIAL)\n');
  } else {
    serial.start();
  }
});
