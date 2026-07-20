// model.h — live data model, populated from the backend over WebSocket.
// (Replaces the old mock_data.h.) Mirrors the device DTOs the backend sends.
#pragma once
#include <Arduino.h>
#include <vector>
#include "theme.h"

enum PlanStatus : uint8_t { STEP_PENDING = 0, STEP_IN_PROGRESS = 1, STEP_DONE = 2 };

struct PlanStep {
  String step;
  uint8_t status; // PlanStatus
};

struct SubAgent {
  String type;
  bool running;
};

struct Session {
  String id;
  String title;
  Agent agent = AGENT_CODEX;
  bool done = false;            // status == "done"
  uint32_t elapsedBase = 0;     // seconds elapsed at last sync
  uint32_t syncMillis = 0;      // millis() at last sync
  int contextLeftPercent = -1;  // -1 = unknown
  std::vector<PlanStep> plan;   // = detail "subtasks"
  std::vector<SubAgent> agents; // subagents
  int activeAgents = 0;

  // Running time ticks locally between snapshots.
  uint32_t elapsedNow() const { return elapsedBase + (millis() - syncMillis) / 1000; }
};

struct Usage {
  bool valid = false;
  int usedPercent = -1; // -1 = unknown
  uint32_t resetsAt = 0; // epoch seconds (0 = unknown)
  int windowMinutes = 0;
  String planType;
};

inline Agent agentFromString(const char* s) {
  return (s && strcmp(s, "claude") == 0) ? AGENT_CLAUDE : AGENT_CODEX;
}

// Live state, defined in agent-monitor-main.ino.
extern std::vector<Session> g_sessions;
extern Usage g_codexUsage;
extern Usage g_claudeUsage;
