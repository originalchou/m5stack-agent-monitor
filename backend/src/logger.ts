// logger.ts — pretty console output for incoming hook payloads.
//
// The whole point of phase 1 is to *see* what the hooks actually carry, so this
// prints a one-line summary plus the full JSON payload for every event.

import type { CodexHookPayload } from './types';

const DIM = '\x1b[2m';
const BOLD = '\x1b[1m';
const RESET = '\x1b[0m';
const CYAN = '\x1b[36m';
const YELLOW = '\x1b[33m';

/** Short descriptor of what an event touched, for the summary line. */
function subject(p: CodexHookPayload): string {
  const bits: string[] = [];
  if (p.tool_name) bits.push(`tool=${p.tool_name}`);
  if (p.source) bits.push(`source=${p.source}`);
  if (p.turn_id) bits.push(`turn=${p.turn_id}`);
  return bits.length ? `  ${DIM}${bits.join(' ')}${RESET}` : '';
}

export function logHook(agentType: string, p: CodexHookPayload): void {
  const time = new Date().toLocaleTimeString();
  const event = p.hook_event_name ?? 'Unknown';
  const session = p.session_id ? p.session_id.slice(0, 8) : '????????';

  console.log(
    `\n${DIM}${time}${RESET} ${CYAN}${BOLD}${agentType}${RESET} ` +
      `${YELLOW}${event}${RESET} ${DIM}session=${session}${RESET}${subject(p)}`,
  );
  console.log(JSON.stringify(p, null, 2));
}
