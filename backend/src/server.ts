// server.ts — entrypoint. Wires the store, routes, and HTTP server on :3050.

import express, { type ErrorRequestHandler } from 'express';
import { InMemorySessionStore } from './store';
import { createHooksRouter } from './routes/hooks';
import { createSessionsRouter } from './routes/sessions';

const PORT = Number(process.env.PORT ?? 3050);

const store = new InMemorySessionStore();
const app = express();

// tool_response payloads can be sizable; allow a generous body.
app.use(express.json({ limit: '10mb' }));

app.get('/health', (_req, res) => {
  res.json({ ok: true, uptime: process.uptime() });
});

app.use('/api/hooks', createHooksRouter(store));
app.use('/api/sessions', createSessionsRouter(store));

// Never let a malformed body (bad JSON) crash the process — the forwarder
// ignores our response anyway, so just record and move on.
const onError: ErrorRequestHandler = (err, _req, res, _next) => {
  console.error('[backend] request error:', err instanceof Error ? err.message : err);
  if (!res.headersSent) res.status(400).json({ error: 'bad request' });
};
app.use(onError);

app.listen(PORT, () => {
  console.log(`\nagent-monitor backend listening on http://localhost:${PORT}`);
  console.log(`  POST /api/hooks/codex                 (Codex lifecycle hooks)`);
  console.log(`  GET  /api/sessions[?agentType=codex]  (list sessions)`);
  console.log(`  GET  /api/sessions/codex/:sessionId   (session detail, ?raw=1 for events)`);
  console.log(`  GET  /health\n`);
});
