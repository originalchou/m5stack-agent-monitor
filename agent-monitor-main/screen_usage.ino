// screen_usage.ino — per-agent usage limit (carousel left = Claude, right = Codex).
// Big usage bar, tokens used vs limit, and a live reset countdown.
#include "app.h"

void drawUsage(const Usage& u, int dotIndex) {
  const uint16_t accent = agentColor(u.agent);
  canvas.fillSprite(COL_BG);

  // --- Header: agent name in its brand colour ---
  canvas.fillRect(0, 0, SCR_W, HEADER_H, COL_HEADER);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(accent);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString(agentLongName(u.agent), PAD, HEADER_H / 2);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::middle_right);
  canvas.drawString("Usage", SCR_W - PAD, HEADER_H / 2);

  // --- Big percentage ---
  char buf[32];
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.setTextSize(2);
  snprintf(buf, sizeof(buf), "%d%%", u.usedPct);
  canvas.drawString(buf, PAD, HEADER_H + 20);
  canvas.setTextSize(1);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.drawString("of limit used", SCR_W - PAD, HEADER_H + 56);

  // --- Usage bar ---
  const int barY = HEADER_H + 78;
  drawProgressBar(canvas, PAD, barY, SCR_W - 2 * PAD, 16, u.usedPct,
                  u.usedPct >= 85 ? COL_WARN : accent);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::top_left);
  snprintf(buf, sizeof(buf), "%dk / %dk tokens", u.used, u.limit);
  canvas.drawString(buf, PAD, barY + 24);

  // --- Reset countdown ---
  const int resetY = barY + 58;
  canvas.fillRoundRect(PAD, resetY, SCR_W - 2 * PAD, 42, 8, COL_CARD);
  uint32_t left = usageResetLeft(u);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString("Resets in", PAD + 12, resetY + 21);
  char dur[16];
  formatDuration(left, dur, sizeof(dur));
  snprintf(buf, sizeof(buf), "%s  (%s)", dur, u.resetClock);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_right);
  canvas.drawString(buf, SCR_W - PAD - 12, resetY + 21);

  drawPageDots(canvas, dotIndex, 3);
}
