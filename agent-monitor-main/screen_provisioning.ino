// screen_provisioning.ino — shown before live data: waiting for USB WiFi setup, or
// connecting to WiFi / the backend. Text driven by g_statusLine.
#include "app.h"

void drawProvisioning() {
  canvas.fillSprite(COL_BG);

  const bool connecting = (g_appState == APP_CONNECTING);
  const uint16_t accent = connecting ? COL_ACCENT : COL_CODEX;

  // Title
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(COL_TEXT);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.drawString("Agent Monitor", SCR_W / 2, 66);

  // Status ring / dot
  canvas.fillCircle(SCR_W / 2, 118, 8, accent);

  // Headline
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(accent);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.drawString(connecting ? "Connecting…" : "Setup required", SCR_W / 2, 150);

  // Sub-line (status detail)
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(COL_MUTED);
  canvas.setTextDatum(textdatum_t::middle_center);
  String line = g_statusLine.length() ? g_statusLine : String("Connect USB and open the setup page");
  canvas.drawString(line.c_str(), SCR_W / 2, 182);
}
