// types.ts — the data model for collected agent status.
//
// Derived from real Codex hook payloads + transcripts (see docs/design). Every
// session still keeps the raw event stream so nothing is lost.

import type { TranscriptMetrics } from './transcript';

export type AgentType = 'codex' | 'claude';

/**
 * Raw Codex hook payload. Only the fields we read are typed; the index signature
 * keeps the rest. See codex-hook-ref for the full per-event wire format.
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
  agent_transcript_path?: string | null;
  source?: string;
  reason?: string;
  prompt?: string;
  last_assistant_message?: string | null;
  stop_hook_active?: boolean;
  [key: string]: unknown;
}

export type PlanStepStatus = 'pending' | 'in_progress' | 'completed';

export interface PlanStep {
  step: string;
  status: PlanStepStatus;
}

export interface SessionPlan {
  explanation?: string;
  steps: PlanStep[];
  updatedAt: string;
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
  prompt?: string;
  toolCalls: Map<string, ToolCall>;
  lastAssistantMessage?: string | null;
}

// A subagent as reported by the main agent's collaboration tool calls
// (collaborationspawn_agent / collaborationwait_agent). Keyed by its agent_name
// path (e.g. "/root/mock_task_1"); displayName is the short task name.
// NOTE: this is intentionally NOT keyed by the SubagentStart hook's UUID agent_id —
// that UUID never appears in the spawn/wait payloads, so the two can't be joined.
export interface Subagent {
  name: string; // agent_name path, e.g. "/root/mock_task_1"
  displayName: string; // short name, e.g. "mock_task_1"
  status: 'running' | 'stopped';
  startedAt: string;
  updatedAt: string;
  lastMessage?: string | null; // completion message, when finished
}

export interface RawEvent {
  event: string; // hook_event_name
  at: string; // ISO timestamp the backend received it
  payload: CodexHookPayload;
}

export interface Session {
  agentType: AgentType;
  sessionId: string;
  title?: string; // derived task name (latest prompt / cwd basename)
  model?: string;
  cwd?: string;
  permissionMode?: string;
  status: 'running' | 'done';
  startedAt: string;
  doneAt?: string; // when Stop fired
  lastActivityAt: string;
  transcriptPath?: string | null; // main-agent transcript, for usage refresh
  plan: SessionPlan | null; // main-agent update_plan
  usage: TranscriptMetrics | null; // context + rate limits from the transcript
  turns: Map<string, Turn>;
  subagents: Map<string, Subagent>; // keyed by agent_name
  // Internal correlation state (not serialized): pair each spawn's agent_name with the
  // SubagentStart agent_id that immediately follows it, so SubagentStop can flip the
  // right subagent to stopped mid-turn.
  pendingSpawns: string[]; // agent_names awaiting a SubagentStart binding (FIFO)
  agentIdToName: Map<string, string>; // SubagentStart agent_id -> agent_name
  events: RawEvent[]; // full raw history, newest last
}

/**
 * Storage interface. In-memory today; a MongoDB implementation can drop in behind
 * the same interface later.
 */
export interface SessionStore {
  recordEvent(agentType: AgentType, payload: CodexHookPayload): Session;
  /** Refresh context-window + rate-limit usage from the session transcript. */
  refreshUsage(session: Session): Promise<void>;
  getSession(agentType: AgentType, sessionId: string): Session | undefined;
  listSessions(agentType?: AgentType): Session[];
  removeSession(agentType: AgentType, sessionId: string): boolean;
  /** Latest known usage per agent, for the device's usage screens. */
  latestUsage(agentType: AgentType): TranscriptMetrics | null;
}
