// app.h — shared declarations tying the tabs together.
// Every screen tab includes this so it sees the canvas, the mock data, and the
// screen render prototypes regardless of file order.
#pragma once
#include <M5Unified.h>
#include "theme.h"
#include "mock_data.h"

// Off-screen buffer every screen draws into (flicker-free push in render()).
extern M5Canvas canvas;

// Live running time / countdown helpers (mock data ticks up from boot).
inline uint32_t bootSeconds() { return millis() / 1000; }
inline uint32_t taskElapsed(const Task& t) { return t.baseElapsedSec + bootSeconds(); }
inline uint32_t usageResetLeft(const Usage& u) {
  uint32_t e = bootSeconds();
  return u.resetInSec > e ? u.resetInSec - e : 0;
}

// Screen renderers (defined one per screen_*.ino tab).
void drawDashboard();
void drawTaskDetail(const Task& t);
void drawUsage(const Usage& u, int dotIndex);
