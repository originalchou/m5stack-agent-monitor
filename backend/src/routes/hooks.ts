// routes/hooks.ts — ingest lifecycle-hook payloads from coding agents.
//
// POST /api/hooks/codex — one Codex hook event. We log it, fold it into the store,
// refresh usage from the transcript, emit any device notification, and broadcast the
// updated snapshot to connected devices. The forwarder ignores our response body.

import { Router } from 'express';
import type { CodexHookPayload, SessionStore } from '../types';
import type { Realtime } from '../realtime';
import { logHook } from '../logger';

export function createHooksRouter(store: SessionStore, realtime: Realtime): Router {
  const router = Router();

  router.post('/codex', async (req, res) => {
    const payload = req.body as CodexHookPayload;
    if (!payload || typeof payload !== 'object' || !payload.hook_event_name) {
      res.status(400).json({ error: 'expected a Codex hook payload with hook_event_name' });
      return;
    }
    logHook('codex', payload);
    const session = store.recordEvent('codex', payload);
    await store.refreshUsage(session);

    switch (payload.hook_event_name) {
      case 'Stop':
        realtime.notifyDevice({ kind: 'task_done', sessionId: session.sessionId, title: session.title });
        break;
      case 'PermissionRequest': {
        const ti = payload.tool_input as { command?: string; description?: string } | undefined;
        realtime.notifyDevice({
          kind: 'permission_request',
          sessionId: session.sessionId,
          title: session.title,
          tool: payload.tool_name,
          command: ti?.command,
          description: ti?.description,
        });
        break;
      }
      case 'SessionEnd':
        store.removeSession('codex', payload.session_id);
        break;
      default:
        break;
    }

    realtime.broadcastSnapshot();
    res.status(204).end();
  });

  return router;
}
