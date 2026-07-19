# AGENTS.md — Agent Monitor

Guidance for AI coding agents (Claude Code, Codex, etc.) working in this repository.

## What this project is

An **agent monitor**: an embedded appliance that watches the status of AI coding agents
(Claude Code, Codex, …) running on the developer's computer and surfaces them on a physical
device — an **M5Stack CoreS3** (ESP32-S3 dev board with Wi-Fi, camera, 2" touch screen, dual
mic, and speaker).

The device shows, per agent:
- how many tasks are running, their names, and elapsed running time;
- **approval requests** — when a task needs approval, the device notifies the user and shows
  **Approve / Reject** buttons; the choice is sent back to the agent;
- **task completion** notifications;
- **usage-limit** status for each agent.

Notifications are **spoken aloud** through the device speaker (e.g. "Claude Code needs your
approval", "task finished") in addition to the on-screen prompt — see "Spoken notifications".

## Repository layout

This repo **is** the project. Application code lives here:

```
m5stack-agent-monitor/           ← this git repo
├── AGENTS.md                    ← this file
├── README.md
├── agent-monitor-main/          ← Arduino sketch (CoreS3 firmware entrypoint)
│   └── agent-monitor-main.ino
├── firmware/                    ← (to add) additional firmware modules
├── backend/                     ← (to add) status-collection server
└── frontend/                    ← (optional, to add) admin/config web page
```

### Reference material (NOT in this repo)

The following live **outside** this repo, in the parent workspace / the developer's local
Arduino install. They are read-only references — do not modify or vendor them in:

- **M5Unified** — M5Stack unified hardware library (already installed in Arduino).
- **M5GFX** — M5Stack graphics library, a dependency of M5Unified (already installed).
- **xiaozhi-esp32** — a mature ESP32 voice-agent project; reference for the ESP32↔backend
  protocol and audio streaming.

## System architecture (two, maybe three parts)

1. **Embedded app** (`agent-monitor-main/`, `firmware/` — Arduino IDE / ESP32).
   Runs on the CoreS3. Connects over Wi-Fi to the backend, renders task/agent status on the
   touch screen, handles approval button taps, and speaks/show notifications.

2. **Backend server** (`backend/`).
   Runs on the developer's computer. Collects live agent status by integrating with each
   agent's SDK / hooks (Claude Code, Codex, …). Aggregates tasks, running times, approval
   requests, completion events, and usage limits, then pushes them to the device and relays
   the device's approve/reject decisions back to the agents.

3. **(Optional) Admin frontend** (`frontend/`) — a web page for configuration / inspecting
   agent status. Add only if it earns its keep.

### Device ↔ backend communication

Use the **xiaozhi-esp32** project as the reference for the ESP32↔backend protocol
(its `docs/websocket.md`, `mqtt-udp.md`, `mcp-protocol.md`). Its pattern: a **WebSocket**
connection with a JSON **`hello` handshake**, then a bidirectional stream of **JSON control
messages** dispatched by a `type` field (plus optional binary frames for audio).

For this project, model the traffic as JSON control messages, e.g.:
- backend → device: `task_update`, `approval_request`, `task_done`, `usage_limit`, `hello`;
- device → backend: `hello`, `approval_response` (approve/reject + task id), `ack`.

Prefer WebSocket for the low-latency, push-based flow this app needs. Keep the message schema
in one shared place and document it so firmware and backend stay in sync.

### Spoken notifications (core feature)

Key events (approval requests, task completion, usage-limit warnings) are announced **out loud**
through the CoreS3 speaker, not just shown on screen. Decide the TTS approach and record it here:

- **Backend-side TTS (recommended default):** the backend synthesizes speech and streams audio
  to the device, which just plays it. This keeps voices/quality flexible and the firmware
  simple. Reuse xiaozhi's **Opus-over-WebSocket** audio path (`M5.Speaker` for playback) — see
  its `main/protocols/` and `docs/websocket.md` for the binary-frame handling.
- **Device-side cues:** for lightweight alerts, the device can also play pre-baked audio clips
  or tones from flash/SD without contacting the backend.

Carry a spoken-text/audio hint on the relevant control messages (e.g. an `announce` string on
`approval_request` / `task_done`, or a reference to an audio stream) so the device knows what to
say. The mic is available for future voice control but is not required for the core flow.

## Target hardware — M5Stack CoreS3

- **MCU:** ESP32-S3 (dual-core Xtensa LX7 @ 240 MHz), Wi-Fi + BLE, 16 MB flash, 8 MB PSRAM.
- **Display:** 2.0" IPS, **320×240**, capacitive **touch**.
- **Also on board:** GC0308 camera, dual PDM mic, speaker (AW88298 amp), AXP2101 PMU,
  AW9523 GPIO expander, BMI270 IMU + BMM150 mag, SD-card slot, USB-C.

The xiaozhi project has a reference board config at `main/boards/m5stack-core-s3/`.

## Embedded development (Arduino IDE)

- **Board:** select **M5CoreS3** (M5Stack board package for ESP32-S3) in Arduino IDE.
- **Libraries:** `M5Unified` and its dependency `M5GFX` are **already installed** in Arduino —
  `#include <M5Unified.h>` directly. Do not vendor copies of them into the app.
- **Core idioms** (see the M5Unified `examples/Basic/` folder):
  - `M5.begin(cfg)` in `setup()`, `M5.update()` first thing each `loop()`.
  - Display via `M5.Display` (an M5GFX canvas); touch via `M5.Touch`; buttons via `M5.BtnA/B/C`;
    mic via `M5.Mic`; speaker via `M5.Speaker`; power via `M5.Power`.
  - Relevant examples: `Touch/` (approve/reject buttons, sliders), `Displays/`, `Microphone/`,
    `Speaker/`. Study `examples/Basic/HowToUse/HowToUse.ino` for the standard startup shape.
- Keep firmware non-blocking: drive networking and UI from the main loop (or FreeRTOS tasks),
  never `delay()` through a network wait.

## Working conventions

- **Match surrounding code** — comment density, naming, and idiom — in whichever part you edit
  (Arduino C++ firmware vs. backend).
- Before designing a new feature, **use the brainstorming skill** to align on intent and design.
- Keep the device↔backend message schema as the contract between the two halves; change it
  deliberately and update both sides plus its doc.
- Never commit the reference libraries into this repo.
- The backend's agent-SDK integration approach (Claude Code hooks/SDK, Codex SDK) is **still to
  be decided** — it will be worked out after reading through the SDKs. It drives the whole
  backend design, so confirm the approach before building it out.
