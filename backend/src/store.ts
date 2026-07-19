// store.ts — in-memory implementation of SessionStore.
//
// Sessions are keyed agentType -> sessionId. Each hook payload is appended to the
// session's raw event log and folded into a light derived model (turns, tool
// calls, pending approval, lifecycle status).

import type {
  AgentType,
  CodexHookPayload,
  Session,
  SessionStore,
  Subagent,
  ToolCall,
  Turn,
} from './types';

function now(): string {
  return new Date().toISOString();
}

export class InMemorySessionStore implements SessionStore {
  private sessions = new Map<AgentType, Map<string, Session>>();

  recordEvent(agentType: AgentType, payload: CodexHookPayload): Session {
    const session = this.ensureSession(agentType, payload);
    const ts = now();

    session.lastActivityAt = ts;
    session.events.push({ event: payload.hook_event_name, at: ts, payload });

    // Keep the latest known metadata on the session.
    if (payload.model) session.model = payload.model;
    if (payload.cwd) session.cwd = payload.cwd;
    if (payload.permission_mode) session.permissionMode = payload.permission_mode;

    switch (payload.hook_event_name) {
      case 'SessionStart':
        session.status = 'active';
        break;
      case 'SessionEnd':
        session.status = 'ended';
        session.endedAt = ts;
        break;
      case 'UserPromptSubmit':
        this.onUserPromptSubmit(session, payload, ts);
        break;
      case 'SubagentStart':
        this.onSubagentStart(session, payload, ts);
        break;
      case 'SubagentStop':
        this.onSubagentStop(session, payload, ts);
        break;
      case 'PreToolUse':
        this.onPreToolUse(session, payload, ts);
        break;
      case 'PostToolUse':
        this.onPostToolUse(session, payload, ts);
        break;
      case 'PermissionRequest':
        session.pendingApproval = {
          turnId: payload.turn_id,
          toolName: payload.tool_name,
          toolInput: payload.tool_input,
          requestedAt: ts,
        };
        break;
      case 'Stop':
        this.onStop(session, payload, ts);
        break;
      default:
        // Unhandled event kinds are still captured in the raw event log above.
        break;
    }

    return session;
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
    // Most recently active first.
    return out.sort((a, b) => b.lastActivityAt.localeCompare(a.lastActivityAt));
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
        status: 'active',
        startedAt: ts,
        lastActivityAt: ts,
        turns: new Map<string, Turn>(),
        subagents: new Map<string, Subagent>(),
        pendingApproval: null,
        events: [],
      };
      byId.set(payload.session_id, session);
    }
    return session;
  }

  private ensureTurn(session: Session, turnId: string, ts: string): Turn {
    let turn = session.turns.get(turnId);
    if (!turn) {
      turn = {
        turnId,
        startedAt: ts,
        lastActivityAt: ts,
        toolCalls: new Map<string, ToolCall>(),
      };
      session.turns.set(turnId, turn);
    }
    turn.lastActivityAt = ts;
    return turn;
  }

  private onUserPromptSubmit(session: Session, p: CodexHookPayload, ts: string): void {
    // A user prompt means the session is being worked right now. Re-activate it,
    // so resuming a previously ended session (or one that ended before the backend
    // was running) puts it back under active tracking.
    session.status = 'active';
    session.endedAt = undefined;
    if (p.turn_id) {
      const turn = this.ensureTurn(session, p.turn_id, ts);
      if (p.prompt) turn.prompt = p.prompt;
    }
  }

  private onSubagentStart(session: Session, p: CodexHookPayload, ts: string): void {
    if (!p.agent_id) return;
    const existing = session.subagents.get(p.agent_id);
    session.subagents.set(p.agent_id, {
      agentId: p.agent_id,
      agentType: p.agent_type ?? 'unknown',
      turnId: p.turn_id,
      status: 'running',
      startedAt: existing?.startedAt ?? ts,
    });
  }

  private onSubagentStop(session: Session, p: CodexHookPayload, ts: string): void {
    if (!p.agent_id) return;
    const existing = session.subagents.get(p.agent_id);
    session.subagents.set(p.agent_id, {
      agentId: p.agent_id,
      agentType: p.agent_type ?? existing?.agentType ?? 'unknown',
      turnId: p.turn_id ?? existing?.turnId,
      status: 'stopped',
      startedAt: existing?.startedAt ?? ts, // may have started before the backend was up
      stoppedAt: ts,
      lastAssistantMessage: p.last_assistant_message ?? null,
      transcriptPath: p.agent_transcript_path ?? null,
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
      // PostToolUse without a matching PreToolUse (e.g. server started mid-turn).
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
    // A turn finished; record the assistant's closing message and clear any
    // approval that was waiting on this turn.
    if (p.turn_id) {
      const turn = this.ensureTurn(session, p.turn_id, ts);
      turn.lastAssistantMessage = p.last_assistant_message ?? null;
    }
    if (session.pendingApproval && session.pendingApproval.turnId === p.turn_id) {
      session.pendingApproval = null;
    }
  }
}
