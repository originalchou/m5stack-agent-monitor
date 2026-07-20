// screen_detail.ino — task detail: running time, context window, plan steps
// (subtasks), and subagents. The two lists scroll vertically (drag) when they
// overflow; swipe right (handled in main) returns to the dashboard.
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

  // --- Header: chevron + (truncated) title, agent badge ---
  canvas.fillRect(0, 0, SCR_W, HEADER_H, COL_HEADER);
  drawBackChevron(12, HEADER_H / 2);
  const int badgeX = SCR_W - PAD - 62;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString(fitText(canvas, s.title, badgeX - 6 - 28).c_str(), 28, HEADER_H / 2);
  drawAgentBadge(canvas, badgeX, (HEADER_H - 18) / 2, s.agent);

  // --- Stat tiles: running time + context left ---
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

  // --- Two columns: plan (left) + subagents (right), scrollable together ---
  const int colY = tileY + 62;
  const int rx = 175;
  const int listTop = colY + 24;
  const int viewportBottom = SCR_H - 6;
  const int viewportH = viewportBottom - listTop;
  const int rowH = 18;
  const int leftTextW = (rx - 8) - (PAD + 18);
  const int rightTextW = (SCR_W - PAD - 6) - (rx + 16);

  const int planCount = (int)s.plan.size();
  const int agentCount = (int)s.agents.size();
  int done = 0;
  for (auto& p : s.plan) if (p.status == STEP_DONE) done++;

  // Column headers (fixed)
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  if (planCount > 0) snprintf(buf, sizeof(buf), "Plan  %d/%d", done, planCount);
  else snprintf(buf, sizeof(buf), "Plan");
  canvas.drawString(buf, PAD, colY);
  snprintf(buf, sizeof(buf), "Agents  %d", s.activeAgents);
  canvas.drawString(buf, rx, colY);

  // Empty-state text (fixed)
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COL_MUTED);
  if (planCount == 0) canvas.drawString("none", PAD, listTop);
  if (agentCount == 0) canvas.drawString("none", rx, listTop);

  // Clamp scroll to the taller of the two columns.
  const int contentH = (planCount > agentCount ? planCount : agentCount) * rowH;
  int maxScroll = contentH - viewportH;
  if (maxScroll < 0) maxScroll = 0;
  if (g_detailScroll > maxScroll) g_detailScroll = maxScroll;

  canvas.setClipRect(0, listTop, SCR_W, viewportH);
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(textdatum_t::top_left);
  for (int i = 0; i < planCount; i++) {
    int ry = listTop + i * rowH - g_detailScroll;
    if (ry + rowH <= listTop || ry >= viewportBottom) continue;
    drawPlanGlyph(PAD + 6, ry + 5, s.plan[i].status);
    canvas.setTextColor(s.plan[i].status == STEP_DONE ? COL_MUTED : COL_TEXT);
    canvas.drawString(fitText(canvas, s.plan[i].step, leftTextW).c_str(), PAD + 18, ry);
  }
  for (int i = 0; i < agentCount; i++) {
    int ry = listTop + i * rowH - g_detailScroll;
    if (ry + rowH <= listTop || ry >= viewportBottom) continue;
    // running = green filled dot; finished = hollow muted ring (clearly distinct)
    if (s.agents[i].running) canvas.fillCircle(rx + 5, ry + 6, 4, COL_GOOD);
    else canvas.drawCircle(rx + 5, ry + 6, 4, COL_MUTED);
    canvas.setTextColor(s.agents[i].running ? COL_TEXT : COL_MUTED);
    canvas.drawString(fitText(canvas, s.agents[i].type, rightTextW).c_str(), rx + 16, ry);
  }
  canvas.clearClipRect();

  // Scrollbar thumb (only when there's overflow).
  if (maxScroll > 0) {
    int thumbH = viewportH * viewportH / contentH;
    if (thumbH < 12) thumbH = 12;
    int thumbY = listTop + (viewportH - thumbH) * g_detailScroll / maxScroll;
    canvas.fillRoundRect(SCR_W - 4, thumbY, 3, thumbH, 1, COL_DOT_OFF);
  }
}
