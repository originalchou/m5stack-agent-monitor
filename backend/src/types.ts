// types.ts — the data model for collected agent status.
//
// NOTE: this model is deliberately provisional. We're still discovering what the
// Codex hooks actually carry (see logger output), so every session also keeps the
// raw event stream. The derived shape (turns / tool calls / approvals) will firm
// up once we've observed real payloads.

export type AgentType = 'codex' | 'claude';

/**
 * Raw Codex hook payload. Only the fields we currently read are typed; the index
 * signature keeps the rest so nothing is dropped. See codex-hook-ref for the full
 * per-event wire format.
 */
export interface CodexHookPayload {
  hook_event_name: string;
  session_id: string;
  cwd?: string;
  model?: string;
  permission_mode?: string;
  transcript_path?: string | null;
  turn_id?: string;
  tool_name?: string;
  tool_use_id?: string;
  tool_input?: unknown;
  tool_response?: unknown;
  agent_id?: string;
  agent_type?: string;
  source?: string;
  reason?: string;
  last_assistant_message?: string | null;
  stop_hook_active?: boolean;
  [key: string]: unknown;
}

export interface ToolCall {
  toolUseId: string;
  toolName: string;
  agentId?: string;
  agentType?: string;
  input?: unknown;
  response?: unknown;
  status: 'running' | 'completed';
  startedAt: string;
  completedAt?: string;
}

export interface Turn {
  turnId: string;
  startedAt: string;
  lastActivityAt: string;
  toolCalls: Map<string, ToolCall>; // keyed by toolUseId
  lastAssistantMessage?: string | null;
}

export interface PendingApproval {
  turnId?: string;
  toolName?: string;
  toolInput?: unknown;
  requestedAt: string;
}

export interface RawEvent {
  event: string; // hook_event_name
  at: string; // ISO timestamp the backend received it
  payload: CodexHookPayload;
}

export interface Session {
  agentType: AgentType;
  sessionId: string;
  model?: string;
  cwd?: string;
  permissionMode?: string;
  status: 'active' | 'ended';
  startedAt: string;
  endedAt?: string;
  lastActivityAt: string;
  turns: Map<string, Turn>; // keyed by turnId
  pendingApproval: PendingApproval | null;
  events: RawEvent[]; // full raw history, newest last
}

/**
 * Storage interface. The current implementation keeps everything in memory; a
 * MongoDB-backed implementation can drop in behind this same interface later.
 */
export interface SessionStore {
  /** Fold one hook payload into the store, returning the updated session. */
  recordEvent(agentType: AgentType, payload: CodexHookPayload): Session;
  getSession(agentType: AgentType, sessionId: string): Session | undefined;
  listSessions(agentType?: AgentType): Session[];
}
