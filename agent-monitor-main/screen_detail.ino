// screen_detail.ino — task detail: running time, context window, plan steps
// (subtasks), and subagents. Swipe right (handled in main) returns to the dashboard.
#include "app.h"

static void drawBackChevron(int x, int y) {
  for (int i = 0; i < 2; i++) {
    canvas.drawLine(x + 7, y - 7 + i, x, y + i, COL_MUTED);
    canvas.drawLine(x, y + i, x + 7, y + 7 + i, COL_MUTED);
  }
}

// Plan-step glyph: filled check (done), amber dot (in progress), hollow (pending).
static void drawPlanGlyph(int cx, int cy, uint8_t status) {
  if (status == STEP_DONE) {
    canvas.fillCircle(cx, cy, 6, COL_GOOD);
    canvas.drawLine(cx - 3, cy, cx - 1, cy + 2, COL_BG);
    canvas.drawLine(cx - 1, cy + 2, cx + 3, cy - 2, COL_BG);
  } else if (status == STEP_IN_PROGRESS) {
    canvas.fillCircle(cx, cy, 6, COL_WARN);
  } else {
    canvas.drawCircle(cx, cy, 6, COL_DOT_OFF);
  }
}

static void tileLabel(int x, int y, const char* label) {
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.drawString(label, x, y);
}

void drawTaskDetail(const Session& s) {
  canvas.fillSprite(COL_BG);

  // Header
  canvas.fillRect(0, 0, SCR_W, HEADER_H, COL_HEADER);
  drawBackChevron(12, HEADER_H / 2);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString(s.title.c_str(), 28, HEADER_H / 2);
  drawAgentBadge(canvas, SCR_W - PAD - 62, (HEADER_H - 18) / 2, s.agent);

  // Stat tiles: running time + context left
  const int tileY = HEADER_H + 10;
  tileLabel(PAD, tileY, s.done ? "FINISHED" : "RUNNING");
  char buf[24];
  formatDuration(s.elapsedNow(), buf, sizeof(buf));
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.drawString(buf, PAD, tileY + 16);

  const int cx = 150;
  tileLabel(cx, tileY, "CONTEXT LEFT");
  if (s.contextLeftPercent >= 0) {
    drawProgressBar(canvas, cx, tileY + 20, SCR_W - PAD - cx, 10, s.contextLeftPercent, COL_ACCENT);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(COL_TEXT);
    canvas.setTextDatum(textdatum_t::top_left);
    snprintf(buf, sizeof(buf), "%d%%", s.contextLeftPercent);
    canvas.drawString(buf, cx, tileY + 34);
  } else {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(COL_MUTED);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString("—", cx, tileY + 22);
  }

  const int colY = tileY + 62;

  // Plan steps (left column)
  int done = 0;
  for (auto& p : s.plan) if (p.status == STEP_DONE) done++;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  if (!s.plan.empty()) {
    snprintf(buf, sizeof(buf), "Plan  %d/%d", done, (int)s.plan.size());
    canvas.drawString(buf, PAD, colY);
  } else {
    canvas.drawString("Plan", PAD, colY);
  }
  canvas.setFont(&fonts::Font2);
  if (s.plan.empty()) {
    canvas.setTextColor(COL_MUTED);
    canvas.drawString("none", PAD, colY + 22);
  }
  int rows = (SCR_H - (colY + 24)) / 18; // clip to available height
  for (int i = 0; i < (int)s.plan.size() && i < rows; i++) {
    int ry = colY + 24 + i * 18;
    drawPlanGlyph(PAD + 6, ry + 5, s.plan[i].status);
    canvas.setTextColor(s.plan[i].status == STEP_DONE ? COL_MUTED : COL_TEXT);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(s.plan[i].step.c_str(), PAD + 18, ry);
  }

  // Subagents (right column)
  const int rx = 175;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  snprintf(buf, sizeof(buf), "Agents  %d", s.activeAgents);
  canvas.drawString(buf, rx, colY);
  canvas.setFont(&fonts::Font2);
  if (s.agents.empty()) {
    canvas.setTextColor(COL_MUTED);
    canvas.drawString("none", rx, colY + 22);
  }
  for (int i = 0; i < (int)s.agents.size() && i < rows; i++) {
    int ry = colY + 24 + i * 18;
    canvas.fillCircle(rx + 5, ry + 6, 4, s.agents[i].running ? COL_GOOD : COL_DOT_OFF);
    canvas.setTextColor(s.agents[i].running ? COL_TEXT : COL_MUTED);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(s.agents[i].type.c_str(), rx + 16, ry);
  }
}
