// screen_detail.ino — task detail (drilled into from a dashboard tap).
// Shows running time, context window remaining, subtask progress and the
// subagents working under this task. Swipe right (handled in the router) exits.
#include "app.h"

// Left-pointing chevron hinting "swipe right to go back".
static void drawBackChevron(int x, int y) {
  for (int i = 0; i < 2; i++) {  // 2px thick
    canvas.drawLine(x + 7, y - 7 + i, x, y + i, COL_MUTED);
    canvas.drawLine(x, y + i, x + 7, y + 7 + i, COL_MUTED);
  }
}

// Small status glyph: filled check when done, hollow ring when pending.
static void drawCheck(int cx, int cy, bool done, uint16_t doneColor) {
  if (done) {
    canvas.fillCircle(cx, cy, 6, doneColor);
    canvas.drawLine(cx - 3, cy, cx - 1, cy + 2, COL_BG);
    canvas.drawLine(cx - 1, cy + 2, cx + 3, cy - 2, COL_BG);
  } else {
    canvas.drawCircle(cx, cy, 6, COL_DOT_OFF);
  }
}

// A labelled stat tile (small caps label above a value).
static void drawTileLabel(int x, int y, const char* label) {
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.drawString(label, x, y);
}

void drawTaskDetail(const Task& t) {
  canvas.fillSprite(COL_BG);

  // --- Header: chevron + task name, agent badge on the right ---
  canvas.fillRect(0, 0, SCR_W, HEADER_H, COL_HEADER);
  drawBackChevron(12, HEADER_H / 2);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString(t.name, 28, HEADER_H / 2);
  drawAgentBadge(canvas, SCR_W - PAD - 62, (HEADER_H - 18) / 2, t.agent);

  // --- Stat tiles: running time + context left ---
  const int tileY = HEADER_H + 10;
  drawTileLabel(PAD, tileY, "RUNNING");
  char buf[16];
  formatDuration(taskElapsed(t), buf, sizeof(buf));
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.drawString(buf, PAD, tileY + 16);

  const int cx = 150;
  drawTileLabel(cx, tileY, "CONTEXT LEFT");
  drawProgressBar(canvas, cx, tileY + 20, SCR_W - PAD - cx, 10,
                  t.contextLeftPct, COL_ACCENT);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  snprintf(buf, sizeof(buf), "%d%%", t.contextLeftPct);
  canvas.drawString(buf, cx, tileY + 34);

  // --- Two columns: subtasks (left) and subagents (right) ---
  const int colY = tileY + 62;

  // Subtasks
  int done = 0;
  for (int i = 0; i < t.subtaskCount; i++) if (t.subtasks[i].done) done++;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  if (t.subtaskCount > 0) {
    snprintf(buf, sizeof(buf), "Subtasks  %d/%d", done, t.subtaskCount);
    canvas.drawString(buf, PAD, colY);
  } else {
    canvas.drawString("Subtasks", PAD, colY);
  }
  canvas.setFont(&fonts::Font2);
  if (t.subtaskCount == 0) {
    canvas.setTextColor(COL_MUTED);
    canvas.drawString("none", PAD, colY + 22);
  }
  for (int i = 0; i < t.subtaskCount; i++) {
    int ry = colY + 24 + i * 18;
    drawCheck(PAD + 6, ry + 5, t.subtasks[i].done, COL_GOOD);
    canvas.setTextColor(t.subtasks[i].done ? COL_MUTED : COL_TEXT);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(t.subtasks[i].name, PAD + 18, ry);
  }

  // Subagents
  const int rx = 175;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  int active = 0;
  for (int i = 0; i < t.subagentCount; i++) if (t.subagents[i].active) active++;
  snprintf(buf, sizeof(buf), "Subagents  %d", active);
  canvas.drawString(buf, rx, colY);
  canvas.setFont(&fonts::Font2);
  for (int i = 0; i < t.subagentCount; i++) {
    int ry = colY + 24 + i * 18;
    canvas.fillCircle(rx + 5, ry + 6, 4,
                      t.subagents[i].active ? COL_GOOD : COL_DOT_OFF);
    canvas.setTextColor(t.subagents[i].active ? COL_TEXT : COL_MUTED);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(t.subagents[i].name, rx + 16, ry);
  }
}
