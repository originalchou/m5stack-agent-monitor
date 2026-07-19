// screen_dashboard.ino — task dashboard (carousel centre).
// One card per running task: name, agent badge, live running time.
#include "app.h"

static void drawHeader(const char* title, const char* right) {
  canvas.fillRect(0, 0, SCR_W, HEADER_H, COL_HEADER);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString(title, PAD, HEADER_H / 2);
  if (right) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(COL_MUTED);
    canvas.setTextDatum(textdatum_t::middle_right);
    canvas.drawString(right, SCR_W - PAD, HEADER_H / 2);
  }
}

static void drawTaskCard(const Task& t, int y) {
  const int x = PAD, w = SCR_W - 2 * PAD;
  canvas.fillRoundRect(x, y, w, CARD_H, 8, COL_CARD);
  // agent accent bar on the left edge
  canvas.fillRoundRect(x + 6, y + 9, 5, CARD_H - 18, 2, agentColor(t.agent));

  // task name
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.drawString(t.name, x + 20, y + 8);

  // agent badge (bottom-left)
  drawAgentBadge(canvas, x + 20, y + CARD_H - 24, t.agent);

  // live running time (bottom-right)
  char buf[16];
  formatDuration(taskElapsed(t), buf, sizeof(buf));
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.drawString(buf, x + w - 12, y + CARD_H - 8);
}

void drawDashboard() {
  canvas.fillSprite(COL_BG);

  char count[24];
  snprintf(count, sizeof(count), "%d running", g_taskCount);
  drawHeader("Agent Monitor", count);

  int top = HEADER_H + PAD;
  for (int i = 0; i < g_taskCount; i++) {
    drawTaskCard(g_tasks[i], top + i * (CARD_H + CARD_GAP));
  }

  drawPageDots(canvas, 1, 3);
}
