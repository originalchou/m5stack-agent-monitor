// store.ts — in-memory implementation of SessionStore.
//
// Sessions are keyed agentType -> sessionId. Each hook payload is appended to the
// raw event log and folded into a derived model: lifecycle status, title, plan
// (main-agent update_plan), subagents, and turns. Context-window / rate-limit usage
// is refreshed from the transcript by refreshUsage() (async, called by the route).

import { basename } from 'node:path';
import type {
  AgentType,
  CodexHookPayload,
  PlanStep,
  Session,
  SessionStore,
  Subagent,
  ToolCall,
  Turn,
} from './types';
import { readLatestMetrics, type TranscriptMetrics } from './transcript';

function now(): string {
  return new Date().toISOString();
}

function firstLine(s: string, max = 60): string {
  const line = s.split('\n')[0]?.trim() ?? '';
  return line.length > max ? line.slice(0, max - 1) + '…' : line;
}

/** Codex hook tool_response is a JSON string; parse it (objects pass through). */
function parseToolResponse(r: unknown): any {
  if (typeof r === 'string') {
    try {
      return JSON.parse(r);
    } catch {
      return null;
    }
  }
  return r ?? null;
}

/** "/root/mock_task_1" -> "mock_task_1". */
function shortAgentName(name: string): string {
  if (name.startsWith('/root/')) return name.slice('/root/'.length);
  return name.replace(/^\//, '');
}

export class InMemorySessionStore implements SessionStore {
  private sessions = new Map<AgentType, Map<string, Session>>();
  private usageByAgent = new Map<AgentType, TranscriptMetrics>();

  recordEvent(agentType: AgentType, payload: CodexHookPayload): Session {
    const session = this.ensureSession(agentType, payload);
    const ts = now();

    session.lastActivityAt = ts;
    session.events.push({ event: payload.hook_event_name, at: ts, payload });

    if (payload.model) session.model = payload.model;
    if (payload.cwd) session.cwd = payload.cwd;
    if (payload.permission_mode) session.permissionMode = payload.permission_mode;
    // Remember the main-agent transcript (subagent events point at their own file).
    if (!payload.agent_id && payload.transcript_path) {
      session.transcriptPath = payload.transcript_path;
    }
    if (!session.title && session.cwd) session.title = basename(session.cwd);

    switch (payload.hook_event_name) {
      case 'SessionStart':
        session.status = 'running';
        break;
      case 'SessionEnd':
        // handled by the route (removes the session); nothing to fold here.
        break;
      case 'UserPromptSubmit':
        session.status = 'running';
        session.doneAt = undefined;
        if (payload.prompt) {
          session.title = firstLine(payload.prompt);
          if (payload.turn_id) this.ensureTurn(session, payload.turn_id, ts).prompt = payload.prompt;
        }
        break;
      case 'PreToolUse':
        this.maybeUpdatePlan(session, payload, ts);
        this.onPreToolUse(session, payload, ts);
        break;
      case 'PostToolUse':
        this.maybeUpdatePlan(session, payload, ts);
        this.maybeUpdateSubagents(session, payload, ts);
        this.onPostToolUse(session, payload, ts);
        break;
      // SubagentStart/SubagentStop carry a UUID agent_id that can't be joined to the
      // spawn/wait task names, so they're kept only in the raw event log (default case).
      case 'Stop':
        this.onStop(session, payload, ts);
        break;
      case 'PermissionRequest':
        // Device-notification only (handled by the route); no state change.
        break;
      default:
        break;
    }

    return session;
  }

  /** Refresh usage (context window + rate limits) from the session transcript. */
  async refreshUsage(session: Session): Promise<void> {
    const metrics = await readLatestMetrics(session.transcriptPath);
    if (!metrics) return;
    session.usage = metrics;
    // Rate limits are account-global; keep the freshest per agent for usage screens.
    this.usageByAgent.set(session.agentType, metrics);
  }

  getSession(agentType: AgentType, sessionId: string): Session | undefined {
    return this.sessions.get(agentType)?.get(sessionId);
  }

  listSessions(agentType?: AgentType): Session[] {
    const out: Session[] = [];
    for (const [type, byId] of this.sessions) {
      if (agentType && type !== agentType) continue;
      out.push(...byId.values());
    }
    return out.sort((a, b) => b.lastActivityAt.localeCompare(a.lastActivityAt));
  }

  removeSession(agentType: AgentType, sessionId: string): boolean {
    return this.sessions.get(agentType)?.delete(sessionId) ?? false;
  }

  latestUsage(agentType: AgentType): TranscriptMetrics | null {
    return this.usageByAgent.get(agentType) ?? null;
  }

  // --- helpers --------------------------------------------------------------

  private ensureSession(agentType: AgentType, payload: CodexHookPayload): Session {
    let byId = this.sessions.get(agentType);
    if (!byId) {
      byId = new Map<string, Session>();
      this.sessions.set(agentType, byId);
    }
    let session = byId.get(payload.session_id);
    if (!session) {
      const ts = now();
      session = {
        agentType,
        sessionId: payload.session_id,
        status: 'running',
        startedAt: ts,
        lastActivityAt: ts,
        plan: null,
        usage: null,
        turns: new Map<string, Turn>(),
        subagents: new Map<string, Subagent>(),
        events: [],
      };
      byId.set(payload.session_id, session);
    }
    return session;
  }

  private ensureTurn(session: Session, turnId: string, ts: string): Turn {
    let turn = session.turns.get(turnId);
    if (!turn) {
      turn = { turnId, startedAt: ts, lastActivityAt: ts, toolCalls: new Map<string, ToolCall>() };
      session.turns.set(turnId, turn);
    }
    turn.lastActivityAt = ts;
    return turn;
  }

  /** Main-agent update_plan → session plan. Subagent plans (with agent_id) ignored. */
  private maybeUpdatePlan(session: Session, p: CodexHookPayload, ts: string): void {
    if (p.tool_name !== 'update_plan' || p.agent_id) return;
    const input = p.tool_input as { explanation?: string; plan?: unknown } | undefined;
    if (!input || !Array.isArray(input.plan)) return;
    const steps: PlanStep[] = input.plan
      .filter((s): s is { step: string; status: string } => !!s && typeof s === 'object')
      .map((s) => ({
        step: String((s as any).step ?? ''),
        status:
          (s as any).status === 'in_progress' || (s as any).status === 'completed'
            ? (s as any).status
            : 'pending',
      }));
    session.plan = { explanation: input.explanation, steps, updatedAt: ts };
  }

  /**
   * Subagents are modelled from the MAIN agent's collaboration tool calls, keyed by
   * agent_name path. collaborationspawn_agent creates one (running); its response
   * gives the "/root/<task>" name. collaborationwait_agent returns the full roster
   * with each agent's status, which we sync. Subagent-originated events (agent_id
   * present) are ignored here.
   */
  private maybeUpdateSubagents(session: Session, p: CodexHookPayload, ts: string): void {
    if (p.agent_id) return;
    if (p.tool_name === 'collaborationspawn_agent') this.onSpawnAgent(session, p, ts);
    else if (p.tool_name === 'collaborationwait_agent') this.onWaitAgent(session, p, ts);
  }

  private onSpawnAgent(session: Session, p: CodexHookPayload, ts: string): void {
    const resp = parseToolResponse(p.tool_response);
    // Prefer the canonical "/root/<task>" path from the response; fall back to the
    // short task_name from the input.
    let name = typeof resp?.task_name === 'string' ? resp.task_name : undefined;
    if (!name) {
      const input = p.tool_input as { task_name?: string } | undefined;
      if (input?.task_name) name = '/root/' + input.task_name;
    }
    if (!name || name === '/root') return;
    this.upsertSubagent(session, name, 'running', ts, null);
  }

  private onWaitAgent(session: Session, p: CodexHookPayload, ts: string): void {
    const resp = parseToolResponse(p.tool_response);
    const agents = resp?.agents;
    if (!Array.isArray(agents)) return; // e.g. { timed_out: true }
    for (const a of agents) {
      const name = a?.agent_name;
      if (typeof name !== 'string' || name === '/root') continue; // /root = main agent
      const st = a.agent_status;
      const running = st === 'running';
      const lastMessage =
        !running && st && typeof st === 'object' && typeof st.completed === 'string'
          ? st.completed
          : null;
      this.upsertSubagent(session, name, running ? 'running' : 'stopped', ts, lastMessage);
    }
  }

  private upsertSubagent(
    session: Session,
    name: string,
    status: 'running' | 'stopped',
    ts: string,
    lastMessage: string | null,
  ): void {
    const existing = session.subagents.get(name);
    session.subagents.set(name, {
      name,
      displayName: shortAgentName(name),
      status,
      startedAt: existing?.startedAt ?? ts,
      updatedAt: ts,
      lastMessage: lastMessage ?? existing?.lastMessage ?? null,
    });
  }

  private onPreToolUse(session: Session, p: CodexHookPayload, ts: string): void {
    if (!p.turn_id || !p.tool_use_id) return;
    const turn = this.ensureTurn(session, p.turn_id, ts);
    turn.toolCalls.set(p.tool_use_id, {
      toolUseId: p.tool_use_id,
      toolName: p.tool_name ?? 'unknown',
      agentId: p.agent_id,
      agentType: p.agent_type,
      input: p.tool_input,
      status: 'running',
      startedAt: ts,
    });
  }

  private onPostToolUse(session: Session, p: CodexHookPayload, ts: string): void {
    if (!p.turn_id || !p.tool_use_id) return;
    const turn = this.ensureTurn(session, p.turn_id, ts);
    const call = turn.toolCalls.get(p.tool_use_id);
    if (call) {
      call.response = p.tool_response;
      call.status = 'completed';
      call.completedAt = ts;
    } else {
      turn.toolCalls.set(p.tool_use_id, {
        toolUseId: p.tool_use_id,
        toolName: p.tool_name ?? 'unknown',
        agentId: p.agent_id,
        agentType: p.agent_type,
        input: p.tool_input,
        response: p.tool_response,
        status: 'completed',
        startedAt: ts,
        completedAt: ts,
      });
    }
  }

  private onStop(session: Session, p: CodexHookPayload, ts: string): void {
    session.status = 'done';
    session.doneAt = ts;
    if (p.turn_id) {
      this.ensureTurn(session, p.turn_id, ts).lastAssistantMessage = p.last_assistant_message ?? null;
    }
  }
}
