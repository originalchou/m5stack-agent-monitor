// serialize.ts — convert the in-memory Session (which uses Map) into plain
// JSON-friendly objects for the inspection endpoints.

import type { Session } from './types';

export function serializeSession(s: Session, opts: { includeRaw?: boolean } = {}) {
  return {
    agentType: s.agentType,
    sessionId: s.sessionId,
    model: s.model,
    cwd: s.cwd,
    permissionMode: s.permissionMode,
    status: s.status,
    startedAt: s.startedAt,
    endedAt: s.endedAt,
    lastActivityAt: s.lastActivityAt,
    pendingApproval: s.pendingApproval,
    subagents: [...s.subagents.values()],
    turns: [...s.turns.values()].map((t) => ({
      turnId: t.turnId,
      startedAt: t.startedAt,
      lastActivityAt: t.lastActivityAt,
      prompt: t.prompt,
      lastAssistantMessage: t.lastAssistantMessage,
      toolCalls: [...t.toolCalls.values()],
    })),
    eventCount: s.events.length,
    ...(opts.includeRaw ? { events: s.events } : {}),
  };
}

/** Compact summary for the list endpoint (no turns / raw events). */
export function summarizeSession(s: Session) {
  return {
    agentType: s.agentType,
    sessionId: s.sessionId,
    model: s.model,
    cwd: s.cwd,
    status: s.status,
    startedAt: s.startedAt,
    lastActivityAt: s.lastActivityAt,
    turnCount: s.turns.size,
    subagentCount: s.subagents.size,
    activeSubagentCount: [...s.subagents.values()].filter((a) => a.status === 'running').length,
    eventCount: s.events.length,
    hasPendingApproval: s.pendingApproval !== null,
  };
}
