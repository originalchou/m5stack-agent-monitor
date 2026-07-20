// notification.ino — overlay for task-done / approval-needed alerts, plus the tone.
#include "app.h"

void playAlertTone() {
  // Two short beeps. M5.Speaker has a tone generator (see the Speaker example).
  M5.Speaker.tone(1660, 120);
  delay(140);
  M5.Speaker.tone(2200, 160);
}

// Wrap text to a width by characters (rough — the panel is narrow).
static void drawWrapped(const char* text, int x, int y, int w, int lineH, int maxLines) {
  canvas.setTextDatum(textdatum_t::top_left);
  String s = text;
  int line = 0, start = 0;
  while (start < (int)s.length() && line < maxLines) {
    int len = s.length() - start;
    // shrink until it fits
    while (len > 0 && canvas.textWidth(s.substring(start, start + len).c_str()) > w) len--;
    if (len <= 0) len = 1;
    // break on a space if possible
    if (start + len < (int)s.length()) {
      int sp = s.lastIndexOf(' ', start + len);
      if (sp > start) len = sp - start;
    }
    canvas.drawString(s.substring(start, start + len).c_str(), x, y + line * lineH);
    start += len;
    while (start < (int)s.length() && s[start] == ' ') start++;
    line++;
  }
}

void drawNotification() {
  const bool done = (g_notif.kind == NOTIF_TASK_DONE);
  const uint16_t accent = done ? COL_GOOD : COL_WARN;

  // Dim + card
  canvas.fillRect(0, 0, SCR_W, SCR_H, COL_BG);
  const int cx = 18, cy = 24, cw = SCR_W - 36, ch = SCR_H - 48;
  canvas.fillRoundRect(cx, cy, cw, ch, 14, COL_CARD);
  canvas.fillRoundRect(cx, cy, cw, 6, 3, accent); // accent strip on top

  // Heading
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(accent);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.drawString(done ? "Task done" : "Approval needed", cx + 16, cy + 18);

  // Body
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_TEXT);
  int bodyY = cy + 52;
  if (g_notif.title.length()) {
    drawWrapped(g_notif.title.c_str(), cx + 16, bodyY, cw - 32, 20, 2);
    bodyY += 44;
  }
  if (!done) {
    canvas.setTextColor(COL_MUTED);
    canvas.setFont(&fonts::Font2);
    if (g_notif.command.length()) {
      String c = String(g_notif.tool.length() ? g_notif.tool + ": " : "") + g_notif.command;
      drawWrapped(c.c_str(), cx + 16, bodyY, cw - 32, 16, 2);
    } else if (g_notif.description.length()) {
      drawWrapped(g_notif.description.c_str(), cx + 16, bodyY, cw - 32, 16, 2);
    }
  }

  // Button
  canvas.fillRoundRect(NBTN_X, NBTN_Y, NBTN_W, NBTN_H, 10, accent);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COL_BG);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.drawString(done ? "Confirm" : "Dismiss", NBTN_X + NBTN_W / 2, NBTN_Y + NBTN_H / 2);
}
