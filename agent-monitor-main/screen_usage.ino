// screen_usage.ino — per-agent usage limit (carousel left = Claude, right = Codex).
#include "app.h" // brings in time.h (see app.h include order)

static void fmtResetIn(uint32_t resetsAt, char* out, size_t n) {
  time_t now = time(nullptr);
  if (resetsAt == 0 || now < 1000000000) { // clock not synced / no data
    snprintf(out, n, "—");
    return;
  }
  long secs = (long)resetsAt - (long)now;
  if (secs < 0) secs = 0;
  long h = secs / 3600, m = (secs % 3600) / 60;
  if (h >= 24) snprintf(out, n, "%ldd %ldh", h / 24, h % 24);
  else if (h > 0) snprintf(out, n, "%ldh %ldm", h, m);
  else snprintf(out, n, "%ldm", m);
}

void drawUsage(const Usage& u, Agent agent, int dotIndex) {
  const uint16_t accent = agentColor(agent);
  canvas.fillSprite(COL_BG);

  // Header
  canvas.fillRect(0, 0, SCR_W, HEADER_H, COL_HEADER);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(accent);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString(agentLongName(agent), PAD, HEADER_H / 2);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::middle_right);
  canvas.drawString("Usage", SCR_W - PAD, HEADER_H / 2);

  if (!u.valid) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(COL_MUTED);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString("No usage data yet", SCR_W / 2, SCR_H / 2);
    drawPageDots(canvas, dotIndex, 3);
    return;
  }

  char buf[40];
  // Big percentage
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.setTextSize(2);
  snprintf(buf, sizeof(buf), "%d%%", u.usedPercent);
  canvas.drawString(buf, PAD, HEADER_H + 20);
  canvas.setTextSize(1);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.drawString("of limit used", SCR_W - PAD, HEADER_H + 56);

  // Usage bar
  const int barY = HEADER_H + 78;
  drawProgressBar(canvas, PAD, barY, SCR_W - 2 * PAD, 16, u.usedPercent,
                  u.usedPercent >= 85 ? COL_WARN : accent);
  if (u.planType.length()) {
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COL_MUTED);
    canvas.setTextDatum(textdatum_t::top_left);
    snprintf(buf, sizeof(buf), "plan: %s", u.planType.c_str());
    canvas.drawString(buf, PAD, barY + 24);
  }

  // Reset countdown
  const int resetY = barY + 58;
  canvas.fillRoundRect(PAD, resetY, SCR_W - 2 * PAD, 42, 8, COL_CARD);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::middle_left);
  canvas.drawString("Resets in", PAD + 12, resetY + 21);
  char dur[24];
  fmtResetIn(u.resetsAt, dur, sizeof(dur));
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_right);
  canvas.drawString(dur, SCR_W - PAD - 12, resetY + 21);

  drawPageDots(canvas, dotIndex, 3);
}
