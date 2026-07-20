// screen_dashboard.ino — task dashboard (carousel centre). One card per live session.
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

static void drawSessionCard(const Session& s, int y) {
  const int x = PAD, w = SCR_W - 2 * PAD;
  canvas.fillRoundRect(x, y, w, CARD_H, 8, COL_CARD);
  canvas.fillRoundRect(x + 6, y + 9, 5, CARD_H - 18, 2, agentColor(s.agent));

  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::top_left);
  String title = fitText(canvas, s.title, w - 28); // truncate long titles to the card
  canvas.drawString(title.c_str(), x + 20, y + 8);

  drawAgentBadge(canvas, x + 20, y + CARD_H - 24, s.agent);

  char buf[16];
  formatDuration(s.elapsedNow(), buf, sizeof(buf));
  canvas.setFont(&fonts::FreeSans9pt7b);
  if (s.done) {
    canvas.setTextColor(COL_GOOD);
    canvas.setTextDatum(textdatum_t::bottom_right);
    canvas.drawString("DONE", x + w - 12, y + CARD_H - 8);
  } else {
    canvas.setTextColor(COL_MUTED);
    canvas.setTextDatum(textdatum_t::bottom_right);
    canvas.drawString(buf, x + w - 12, y + CARD_H - 8);
  }
}

void drawDashboard() {
  canvas.fillSprite(COL_BG);

  char count[24];
  snprintf(count, sizeof(count), "%d active", (int)g_sessions.size());
  drawHeader("Agent Monitor", count);

  if (g_sessions.empty()) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(COL_MUTED);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString("No active tasks", SCR_W / 2, SCR_H / 2);
  } else {
    int top = HEADER_H + PAD;
    int maxCards = (DOTS_Y - top) / (CARD_H + CARD_GAP);
    int n = (int)g_sessions.size();
    for (int i = 0; i < n && i < maxCards; i++) {
      drawSessionCard(g_sessions[i], top + i * (CARD_H + CARD_GAP));
    }
  }

  drawPageDots(canvas, 1, 3);
}
