// routes/hooks.ts — ingest lifecycle-hook payloads from coding agents.
//
// POST /api/hooks/codex  — one Codex hook event (SessionStart/End, Pre/PostToolUse,
// PermissionRequest, Stop). We log it and fold it into the store, then reply with
// no content; the forwarder script ignores the response body entirely.

import { Router } from 'express';
import type { CodexHookPayload, SessionStore } from '../types';
import { logHook } from '../logger';

export function createHooksRouter(store: SessionStore): Router {
  const router = Router();

  router.post('/codex', (req, res) => {
    const payload = req.body as CodexHookPayload;
    if (!payload || typeof payload !== 'object' || !payload.hook_event_name) {
      res.status(400).json({ error: 'expected a Codex hook payload with hook_event_name' });
      return;
    }
    logHook('codex', payload);
    store.recordEvent('codex', payload);
    res.status(204).end();
  });

  return router;
}
