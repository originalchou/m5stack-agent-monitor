# m5stack-agent-monitor

A physical **agent monitor** for AI coding agents (Codex today; Claude Code planned), built on
an **M5Stack CoreS3** (ESP32-S3). It shows live running tasks (name, agent, running time), each
task's plan/subtasks, subagents, and context-window remaining, plus each agent's usage limit and
reset time. It alerts you when a task **finishes** (confirm on-device to clear it) or **needs
approval** (you act in the agent) — with a **tone** on the speaker.

## How it works

```
Frontend (browser) ──HTTP/WS──> Backend ──USB serial──> CoreS3   (WiFi provisioning)
                                   │
Codex hooks ──REST──> Backend ──WebSocket──> CoreS3               (realtime data + alerts)
                         └── reads transcript_path for context / rate-limits
```

- **Codex hooks** POST lifecycle events to the backend (see `backend/codex-hooks/`).
- The **backend** folds them into live sessions (status, title, plan, subagents), reads the
  session transcript for context-window + usage limits, and pushes snapshots/notifications to
  the device over WebSocket.
- The **CoreS3** renders the dashboard and reacts to notifications. It gets onto WiFi via the
  **admin page**, which provisions it over USB serial.

## Components

- **Firmware** (`agent-monitor-main/`) — CoreS3 app: WiFi provisioning, WebSocket client, touch
  UI, tone alerts.
- **Backend** (`backend/`) — Node/TypeScript on `localhost:3050`: hook ingest, transcript
  metrics, realtime WS, USB-serial bridge. Serves the admin page. See `backend/README.md`.

## Hardware

M5Stack CoreS3 — ESP32-S3, 320×240 IPS capacitive touch screen, camera, dual mic, speaker,
WiFi/BLE, 16 MB flash, 8 MB PSRAM.

## Running it

**Backend + hooks + admin page:** see [`backend/README.md`](./backend/README.md). In short:
`cd backend && npm install && npm run dev`, install the Codex hooks, open `http://localhost:3050`.

**Firmware (Arduino IDE):**
1. Install the M5Stack ESP32 board package; select board **M5CoreS3**.
2. Install libraries: **M5Unified** + **M5GFX** (already present), **ArduinoJson** (v7), and
   **WebSockets** by Markus Sattler (arduinoWebSockets) — both via the Library Manager.
3. Open `agent-monitor-main/agent-monitor-main.ino`, build, and flash over USB-C.
4. With the device on USB, open the admin page (`http://localhost:3050`) and set up WiFi. Once
   connected, the device streams live data from the backend.

## Docs

Design notes in [`docs/design/`](./docs/design). Architecture + conventions in
[`AGENTS.md`](./AGENTS.md).
