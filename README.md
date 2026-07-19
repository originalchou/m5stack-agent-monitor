# m5stack-agent-monitor

A physical **agent monitor** for AI coding agents (Claude Code, Codex, …), built on an
**M5Stack CoreS3** (ESP32-S3). It shows, per agent, how many tasks are running, their names and
elapsed time, notifies you when a task **needs approval** (with on-device **Approve / Reject**
buttons) or **finishes**, and displays each agent's **usage limits** — with notifications
**spoken aloud** through the device speaker.

## Components

- **Firmware** (`agent-monitor-main/`) — CoreS3 app: Wi-Fi, touch UI, speaker notifications.
- **Backend** (`backend/`, to add) — runs on your computer, collects agent status via each
  agent's SDK/hooks and pushes it to the device over WebSocket.
- **Frontend** (`frontend/`, optional) — admin/config web page.

## Hardware

M5Stack CoreS3 — ESP32-S3, 320×240 IPS capacitive touch screen, camera, dual mic, speaker,
Wi-Fi/BLE, 16 MB flash, 8 MB PSRAM.

## Building the firmware (Arduino IDE)

1. Install the M5Stack ESP32 board package; select board **M5CoreS3**.
2. Ensure the **M5Unified** and **M5GFX** libraries are installed (they pull in each other).
3. Open `agent-monitor-main/agent-monitor-main.ino`, build, and flash over USB-C.

## Status

Early scaffolding. See [`AGENTS.md`](./AGENTS.md) for architecture, the device↔backend
protocol plan, and development conventions.
