// theme.h — colors, layout constants, and small reusable draw helpers.
// Shared by every screen so the mock reads as one design system.
#pragma once
#include <M5Unified.h>

// --- Color helper: pack 8-8-8 RGB into RGB565 (the panel's native depth) ---
#define RGB565(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

// --- Palette (dark, modern dashboard) ---
static constexpr uint16_t COL_BG      = RGB565(15, 18, 28);    // near-black navy
static constexpr uint16_t COL_HEADER  = RGB565(23, 28, 42);    // header strip
static constexpr uint16_t COL_CARD    = RGB565(30, 36, 52);    // task card
static constexpr uint16_t COL_CARD_HI = RGB565(40, 47, 66);    // card accent / divider
static constexpr uint16_t COL_TEXT    = RGB565(236, 239, 245); // primary text
static constexpr uint16_t COL_MUTED   = RGB565(140, 150, 170); // secondary text
static constexpr uint16_t COL_TRACK   = RGB565(48, 55, 74);    // progress-bar track

static constexpr uint16_t COL_CLAUDE  = RGB565(217, 119, 66);  // Claude = warm clay/orange
static constexpr uint16_t COL_CODEX   = RGB565(64, 156, 255);  // Codex  = blue
static constexpr uint16_t COL_ACCENT  = RGB565(96, 141, 255);  // generic accent (blue)
static constexpr uint16_t COL_GOOD    = RGB565(74, 201, 126);  // green
static constexpr uint16_t COL_WARN    = RGB565(240, 180, 70);  // amber
static constexpr uint16_t COL_DOT_OFF = RGB565(70, 78, 98);    // inactive dot / idle

// --- Layout (screen is 320 x 240, landscape) ---
static constexpr int SCR_W        = 320;
static constexpr int SCR_H        = 240;
static constexpr int HEADER_H     = 34;
static constexpr int PAD          = 12;   // outer padding
static constexpr int CARD_H       = 50;   // dashboard task card height
static constexpr int CARD_GAP     = 8;
static constexpr int DOTS_Y       = SCR_H - 14;

// --- Gesture thresholds ---
static constexpr int SWIPE_THRESHOLD = 55; // px of horizontal travel = a swipe
static constexpr int TAP_MOVE        = 18; // px: below this in both axes = a tap

// --- Agent identity ---
enum Agent : uint8_t { AGENT_CLAUDE = 0, AGENT_CODEX = 1 };

inline uint16_t agentColor(Agent a) { return a == AGENT_CLAUDE ? COL_CLAUDE : COL_CODEX; }
inline const char* agentName(Agent a) { return a == AGENT_CLAUDE ? "CLAUDE" : "CODEX"; }
inline const char* agentLongName(Agent a) { return a == AGENT_CLAUDE ? "Claude Code" : "Codex"; }

// --- Format seconds as H:MM:SS (drops the hour when under an hour) ---
inline void formatDuration(uint32_t sec, char* out, size_t n) {
  uint32_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
  if (h > 0) snprintf(out, n, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else       snprintf(out, n, "%02u:%02u", (unsigned)m, (unsigned)s);
}

// --- Truncate a string to fit maxW pixels, appending "..." on overflow.
//     Set the font on `cv` before calling (textWidth depends on it). ---
inline String fitText(M5Canvas& cv, const String& s, int maxW) {
  if (cv.textWidth(s.c_str()) <= maxW) return s;
  const char* ell = "...";
  int ellW = cv.textWidth(ell);
  String out;
  for (int i = 0; i < (int)s.length(); i++) {
    String cand = s.substring(0, i + 1);
    if (cv.textWidth(cand.c_str()) + ellW > maxW) break;
    out = cand;
  }
  return out + ell;
}

// --- Small colored pill showing the agent name (returns pill width) ---
inline int drawAgentBadge(M5Canvas& cv, int x, int y, Agent a) {
  cv.setFont(&fonts::Font2);
  cv.setTextSize(1);
  const char* label = agentName(a);
  int tw = cv.textWidth(label);
  int w = tw + 14, h = 18;
  cv.fillRoundRect(x, y, w, h, h / 2, agentColor(a));
  cv.setTextColor(COL_BG);
  cv.setTextDatum(textdatum_t::middle_center);
  cv.drawString(label, x + w / 2, y + h / 2 + 1);
  return w;
}

// --- Rounded track + filled portion. pct is 0..100. ---
inline void drawProgressBar(M5Canvas& cv, int x, int y, int w, int h, int pct, uint16_t color) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int r = h / 2;
  cv.fillRoundRect(x, y, w, h, r, COL_TRACK);
  int fw = (w * pct) / 100;
  if (fw >= 2) cv.fillRoundRect(x, y, fw, h, r, color);
}

// --- Carousel page dots. count dots, `active` highlighted. ---
inline void drawPageDots(M5Canvas& cv, int active, int count) {
  const int r = 3, gap = 12;
  int total = (count - 1) * gap;
  int x0 = SCR_W / 2 - total / 2;
  for (int i = 0; i < count; i++) {
    cv.fillCircle(x0 + i * gap, DOTS_Y, r, i == active ? COL_TEXT : COL_DOT_OFF);
  }
}
