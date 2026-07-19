// routes/sessions.ts — read-only inspection of collected sessions.

import { Router } from 'express';
import type { AgentType, SessionStore } from '../types';
import { serializeSession, summarizeSession } from '../serialize';

export function createSessionsRouter(store: SessionStore): Router {
  const router = Router();

  // GET /api/sessions?agentType=codex  — compact list of all sessions.
  router.get('/', (req, res) => {
    const agentType = req.query.agentType as AgentType | undefined;
    res.json(store.listSessions(agentType).map(summarizeSession));
  });

  // GET /api/sessions/codex/:sessionId?raw=1  — full detail for one session.
  router.get('/:agentType/:sessionId', (req, res) => {
    const agentType = req.params.agentType as AgentType;
    const session = store.getSession(agentType, req.params.sessionId);
    if (!session) {
      res.status(404).json({ error: 'session not found' });
      return;
    }
    res.json(serializeSession(session, { includeRaw: req.query.raw === '1' }));
  });

  return router;
}
