# agent-monitor backend

Collects AI coding-agent status (starting with **Codex**) via lifecycle hooks and
serves it to the M5Stack agent monitor. Phase 1 goal: **see what the hooks actually
carry** — every event is pretty-printed to the console and folded into an in-memory
store you can inspect over HTTP.

## Run

```bash
npm install
npm run dev      # tsx watch, restarts on change
# or: npm start
```

Listens on `http://localhost:3050` (override with `PORT`).

## HTTP API

| Method | Path | Purpose |
| ------ | ---- | ------- |
| POST | `/api/hooks/codex` | Ingest one Codex hook event (logged + stored). |
| GET  | `/api/sessions[?agentType=codex]` | Compact list of sessions. |
| GET  | `/api/sessions/:agentType/:sessionId[?raw=1]` | Session detail; `raw=1` includes the full event stream. |
| GET  | `/health` | Liveness. |

## Data model (provisional)

Sessions are keyed `agentType → sessionId`. Each session keeps the **raw event
stream** plus a derived view: `turns` (by `turn_id`), each turn's `toolCalls` (by
`tool_use_id`, `running → completed`), a `pendingApproval` slot, and lifecycle
`status` (`active`/`ended`). The model is intentionally light — it will firm up once
we've observed real payloads. Storage sits behind the `SessionStore` interface
(`src/types.ts`) so a MongoDB implementation can drop in later without touching the
routes.

Layout: `src/server.ts` (entry), `src/routes/` (hooks + sessions), `src/store.ts`
(in-memory store), `src/logger.ts` (console output), `src/serialize.ts`, `src/types.ts`.

## Wiring up Codex hooks

Codex runs a command per lifecycle event and passes the payload as JSON on stdin.
`codex-hooks/forward-hook.sh` POSTs that payload to the backend, discards the
response, and prints `{}` — a safe no-op for every event that never alters Codex's
behaviour and **always exits 0**, so a stopped backend never blocks your agent.

Install (captures **all** Codex sessions on this machine):

```bash
mkdir -p ~/.codex/hooks
cp codex-hooks/forward-hook.sh ~/.codex/hooks/agent-monitor-forward.sh
chmod +x ~/.codex/hooks/agent-monitor-forward.sh

# If you have no ~/.codex/hooks.json yet, copy ours; otherwise merge the "hooks"
# block from codex-hooks/hooks.json into your existing file.
cp codex-hooks/hooks.json ~/.codex/hooks.json
```

Then, in Codex, run `/hooks` and **review + trust** the new hooks (Codex skips
untrusted command hooks). Start the backend, use Codex normally, and watch events
stream into the console.

Registered events: `SessionStart`, `SessionEnd`, `UserPromptSubmit`, `PreToolUse`,
`PostToolUse`, `PermissionRequest`, `Stop`. Point the hook at a different backend
with `AGENT_MONITOR_URL` (e.g. `http://localhost:3050`).

`UserPromptSubmit` fires whenever the user sends a prompt, so it doubles as the
"session is being worked right now" signal: it re-activates a session that had
ended (e.g. resuming an old session, or one that ended before the backend was
running) and records the prompt on its turn.

## What we learned about the hook payloads

- Every event carries `session_id`, `cwd`, `hook_event_name`, `transcript_path`;
  most also carry `model`, `permission_mode`, `turn_id`.
- Tool events add `tool_name` / `tool_input` (+ `tool_response` on Post) and
  `agent_id` / `agent_type` for subagents. `Stop` carries `last_assistant_message`.
- **No context-window usage or token counts**, and **no plan/task list field.**
  Plan/task data only appears indirectly as `update_plan` tool calls inside
  Pre/PostToolUse `tool_input`; context usage likely has to come from the
  transcript file. This is why phase 1 logs everything first.
