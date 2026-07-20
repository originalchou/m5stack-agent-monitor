# Design: realtime data, transcript metrics, WiFi provisioning, live firmware

Date: 2026-07-20. Turns the hook-collection backend + UI mock into a working system.

## Data confirmed from real logs

- **`update_plan`** tool call carries `tool_input.plan = [{ step, status }]`,
  status ∈ `pending | in_progress | completed`. Subagent tool events carry an
  `agent_id`; main-agent events do not → ignore `update_plan` when `agent_id` present.
- **Transcript** (`transcript_path`, JSONL) has `event_msg` records with
  `payload.type === "token_count"`:
  - `info.model_context_window`, `info.last_token_usage.input_tokens` → context fill.
  - `rate_limits.primary` / `.secondary` = `{ used_percent, window_minutes, resets_at }`
    (`resets_at` is epoch seconds), plus `plan_type`.
  The **last** such record in the file = current values.

## Backend

### Session model additions
- `status`: `running | done` (replaces active/ended for device purposes).
  - `SessionStart` / `UserPromptSubmit` → `running` (re-activates a `done` session).
  - `Stop` → `done`, set `doneAt`, emit `task_done` notification.
  - `SessionEnd` → remove session.
  - Confirm from device → remove session entirely.
- `plan: { explanation, steps: [{ step, status }] }` — set from main-agent `update_plan`
  (Pre or PostToolUse, no `agent_id`).
- `subagents` — already tracked (agents list).
- `usage: { contextWindow, contextUsedTokens, contextLeftPercent, rateLimits: { primary, secondary }, planType }`
  refreshed from the transcript on **every** hook event (mtime-cached read of the last
  `token_count` line).
- `title` — derived task name: latest user prompt (first line, truncated), else cwd basename.
- `PermissionRequest` → emit `permission_request` notification only; no device action
  (user acts in Codex). Keep listening.

### Transcript parser
`transcript.ts`: `readLatestMetrics(path)` → `{ contextWindow, contextUsedTokens,
contextLeftPercent, rateLimits, planType }` by scanning the file's lines in reverse for
the last `token_count`. Cached by `path + size + mtimeMs`. Also a standalone CLI
`scripts/read-transcript.ts` printing the same for a given path.

### Realtime (WebSocket)
`ws` server on the HTTP server:
- `/ws/device` — the CoreS3. On connect: `hello` → backend replies `snapshot`
  (all sessions + per-agent usage). On any change: `session_update` / `snapshot`.
  Notifications: `{ type:'notification', kind:'task_done'|'permission_request', ... }`.
  Device → backend: `{ type:'confirm_done', sessionId }` → remove + rebroadcast.
- `/ws/admin` — the frontend. Pushes device USB status + wifi scan results live.

Device-facing session DTO: `{ id, agent, title, status, elapsedSeconds,
contextLeftPercent, plan:[{step,status}], agents:[{type,status}], activeAgents }`
and per-agent `usage`.

### Serial bridge (WiFi provisioning)
`serial.ts` using `serialport` + readline parser. Line-delimited JSON both ways.
- Auto-detect: list ports, open candidates, send `{type:'ping'}`, keep the one that
  answers `{type:'pong', device:'cores3', ...}`.
- backend→device: `ping`, `scan_wifi`, `set_wifi{ssid,password,backendWs}`.
- device→backend: `pong{wifi:{connected,ssid,ip}}`, `wifi_list{networks:[{ssid,rssi,secure}]}`,
  `wifi_result{ok,ip}`.
- HTTP for the frontend: `GET /api/device/status`, `POST /api/device/scan`,
  `POST /api/device/wifi {ssid,password}`; live updates over `/ws/admin`.

## Frontend (admin, `backend/public/`)
Single static page served by Express. Two steps:
1. **USB status** — "CoreS3 connected on <port>" / "waiting for device".
2. **WiFi setup** — Refresh (scan) → list SSIDs (signal + lock) → select + password →
   Confirm → shows result (connected + IP). Vanilla HTML/CSS/JS, same-origin.

## Firmware (`agent-monitor-main/`)
- `net_config.*` — Serial JSON protocol: `ping/pong`, `scan_wifi`→`WiFi.scanNetworks`→
  `wifi_list`, `set_wifi`→save creds to NVS (`Preferences`) + `WiFi.begin` → `wifi_result`.
  Stores the backend WS URL too.
- Boot: creds in NVS → connect; else show a **provisioning** screen ("Connect USB, open
  the admin page").
- `ws_client.*` — WebSocket client (**ArduinoHttpClient** `WebSocketClient`, polling API over
  WiFiClient) to the backend `/ws/device`; parses `snapshot`/`session_update`/`notification`
  (ArduinoJson), fills the live model; sends `confirm_done`.
- Replace `mock_data` with a live model populated from WS; keep existing screens.
- Notifications: on `task_done` / `permission_request`, show a banner + play a tone
  (`M5.Speaker.tone`); `task_done` shows a Confirm button → sends `confirm_done`.

## New dependencies
- Backend: `ws`, `serialport`, `@serialport/parser-readline` (+ `@types/ws`).
- Firmware: `ArduinoJson` (v7), `ArduinoHttpClient` (WebSocketClient) via Arduino Library Manager.

## Out of scope this pass
Spoken TTS (deferred; tone only), Claude Code integration, persistence/Mongo.
