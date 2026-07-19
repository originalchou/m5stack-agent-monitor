// mock_data.h — data model + mock instances for the UI mock.
// Later these structs get populated from the backend instead of hard-coded.
#pragma once
#include "theme.h"

static constexpr int MAX_SUBTASKS  = 6;
static constexpr int MAX_SUBAGENTS = 4;

struct Subtask {
  const char* name;
  bool done;
};

struct Subagent {
  const char* name;
  bool active;   // active = running, else finished/idle
};

struct Task {
  const char* name;
  Agent       agent;
  uint32_t    baseElapsedSec;   // running time at mock start; ticks up live
  int         contextLeftPct;   // context window remaining, 0..100
  Subtask     subtasks[MAX_SUBTASKS];
  int         subtaskCount;     // 0 = task has no subtasks
  Subagent    subagents[MAX_SUBAGENTS];
  int         subagentCount;    // 0 = no subagents
};

struct Usage {
  Agent       agent;
  int         usedPct;          // 0..100 of the limit consumed
  int         used;             // tokens used (in thousands, for display)
  int         limit;            // token limit  (in thousands)
  uint32_t    resetInSec;       // seconds until the limit resets; counts down
  const char* resetClock;       // human wall-clock time of reset, e.g. "16:30"
};

// ----------------------------------------------------------------------------
// Mock data. Defined inline here; single translation unit (Arduino concatenates
// the .ino tabs) so these globals are visible to every screen file.
// ----------------------------------------------------------------------------

Task g_tasks[] = {
  { "Refactor auth module", AGENT_CLAUDE, 12 * 60 + 47, 68,
    { {"Parse token config", true}, {"Extract validators", true},
      {"Rewrite session store", false}, {"Update call sites", false},
      {"Add migration notes", false} }, 5,
    { {"code-explorer", true}, {"test-runner", true} }, 2 },

  { "Write integration tests", AGENT_CODEX, 4 * 60 + 12, 84,
    { {"Scaffold fixtures", true}, {"Happy-path cases", false},
      {"Error-path cases", false} }, 3,
    { {"test-runner", true} }, 1 },

  { "Migrate DB schema", AGENT_CLAUDE, 28 * 60 + 33, 41,
    {}, 0,
    { {"explorer", false} }, 1 },
};
const int g_taskCount = sizeof(g_tasks) / sizeof(g_tasks[0]);

Usage g_claudeUsage = { AGENT_CLAUDE, 72, 144, 200, 2 * 3600 + 14 * 60, "16:30" };
Usage g_codexUsage  = { AGENT_CODEX,  38,  95, 250, 5 * 3600 + 2 * 60,  "19:18" };
