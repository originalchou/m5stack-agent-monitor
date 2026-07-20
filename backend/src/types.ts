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

export interface Subagent {
  agentId: string;
  agentType: string;
  turnId?: string;
  status: 'running' | 'stopped';
  startedAt: string;
  stoppedAt?: string;
  lastAssistantMessage?: string | null;
  transcriptPath?: string | null;
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
  subagents: Map<string, Subagent>;
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
