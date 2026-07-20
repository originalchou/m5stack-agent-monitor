// serialize.ts — convert the in-memory model (which uses Map) into JSON-friendly
// shapes: full detail for the inspection API, and compact DTOs for the device.

import type { AgentType, Session } from './types';
import type { TranscriptMetrics } from './transcript';

function elapsedSeconds(startedAt: string): number {
  return Math.max(0, Math.floor((Date.now() - Date.parse(startedAt)) / 1000));
}

// --- Inspection API -------------------------------------------------------

export function serializeSession(s: Session, opts: { includeRaw?: boolean } = {}) {
  return {
    agentType: s.agentType,
    sessionId: s.sessionId,
    title: s.title,
    model: s.model,
    cwd: s.cwd,
    permissionMode: s.permissionMode,
    status: s.status,
    startedAt: s.startedAt,
    doneAt: s.doneAt,
    lastActivityAt: s.lastActivityAt,
    plan: s.plan,
    usage: s.usage,
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

export function summarizeSession(s: Session) {
  return {
    agentType: s.agentType,
    sessionId: s.sessionId,
    title: s.title,
    status: s.status,
    startedAt: s.startedAt,
    lastActivityAt: s.lastActivityAt,
    contextLeftPercent: s.usage?.contextLeftPercent ?? null,
    planSteps: s.plan?.steps.length ?? 0,
    subagentCount: s.subagents.size,
    activeSubagentCount: [...s.subagents.values()].filter((a) => a.status === 'running').length,
    eventCount: s.events.length,
  };
}

// --- Device DTOs (sent over the WebSocket) --------------------------------

export function toDeviceSession(s: Session) {
  const agents = [...s.subagents.values()].map((a) => ({
    type: a.agentType,
    status: a.status, // 'running' | 'stopped'
  }));
  return {
    id: s.sessionId,
    agent: s.agentType,
    title: s.title ?? s.sessionId.slice(0, 8),
    status: s.status, // 'running' | 'done'
    elapsedSeconds: elapsedSeconds(s.startedAt),
    contextLeftPercent: s.usage?.contextLeftPercent ?? null,
    plan: s.plan?.steps ?? [],
    agents,
    activeAgents: agents.filter((a) => a.status === 'running').length,
  };
}

export function toDeviceUsage(agent: AgentType, m: TranscriptMetrics | null) {
  const primary = m?.rateLimits.primary ?? null;
  const secondary = m?.rateLimits.secondary ?? null;
  return {
    agent,
    usedPercent: primary?.usedPercent ?? null,
    resetsAt: primary?.resetsAt ?? null,
    windowMinutes: primary?.windowMinutes ?? null,
    planType: m?.planType ?? null,
    secondary: secondary
      ? { usedPercent: secondary.usedPercent, resetsAt: secondary.resetsAt, windowMinutes: secondary.windowMinutes }
      : null,
  };
}
