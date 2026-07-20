// agent-monitor-main.ino — M5Stack CoreS3 agent monitor (live).
//
// Boot → if no WiFi credentials, show the provisioning screen (set up over USB via
// the admin page). Once on WiFi, connect to the backend WebSocket and show live data:
//   [Claude usage]  <-- swipe right --  [ DASHBOARD ]  -- swipe left -->  [Codex usage]
// Tap a task → detail (plan/subtasks, subagents, context, time); swipe right to go back.
// Notifications (task done / approval needed) overlay with a tone; task-done Confirm
// tells the backend to clear the session.
#include "app.h"

// --- Globals (single translation unit; visible to the other tabs) ---
M5Canvas canvas(&M5.Display);
std::vector<Session> g_sessions;
Usage g_codexUsage;
Usage g_claudeUsage;
AppState g_appState = APP_PROVISIONING;
String g_statusLine;
bool g_needRedraw = true;
int g_detailScroll = 0;
Notification g_notif;

// --- Carousel state (within APP_RUNNING) ---
enum Screen : uint8_t { SCR_USAGE_CLAUDE = 0, SCR_DASHBOARD = 1, SCR_USAGE_CODEX = 2, SCR_DETAIL = 3 };
static Screen s_screen = SCR_DASHBOARD;
static int s_selected = 0; // index into g_sessions for the detail page
static uint32_t s_lastDraw = 0;

// --- Rendering ---
static void renderRunning() {
  switch (s_screen) {
    case SCR_USAGE_CLAUDE: drawUsage(g_claudeUsage, AGENT_CLAUDE, 0); break;
    case SCR_DASHBOARD:    drawDashboard(); break;
    case SCR_USAGE_CODEX:  drawUsage(g_codexUsage, AGENT_CODEX, 2); break;
    case SCR_DETAIL:
      if (s_selected >= 0 && s_selected < (int)g_sessions.size()) drawTaskDetail(g_sessions[s_selected]);
      else { s_screen = SCR_DASHBOARD; drawDashboard(); }
      break;
  }
}

static void render() {
  if (g_appState == APP_RUNNING) renderRunning();
  else drawProvisioning(); // provisioning + connecting share the status screen
  if (g_notif.active) drawNotification();
  canvas.pushSprite(0, 0);
}

// --- Input ---
static void handleSwipe(int dir) { // -1 left, +1 right
  switch (s_screen) {
    case SCR_DASHBOARD:    s_screen = (dir < 0) ? SCR_USAGE_CODEX : SCR_USAGE_CLAUDE; break;
    case SCR_USAGE_CLAUDE: if (dir < 0) s_screen = SCR_DASHBOARD; break;
    case SCR_USAGE_CODEX:  if (dir > 0) s_screen = SCR_DASHBOARD; break;
    case SCR_DETAIL:       if (dir > 0) s_screen = SCR_DASHBOARD; break;
  }
  g_needRedraw = true;
}

static void handleTapDashboard(int y) {
  int top = HEADER_H + PAD;
  for (int i = 0; i < (int)g_sessions.size(); i++) {
    int cardY = top + i * (CARD_H + CARD_GAP);
    if (y >= cardY && y <= cardY + CARD_H) {
      s_selected = i;
      s_screen = SCR_DETAIL;
      g_detailScroll = 0; // start at the top of the lists
      g_needRedraw = true;
      return;
    }
  }
}

static bool inNotifButton(int x, int y) {
  return x >= NBTN_X && x <= NBTN_X + NBTN_W && y >= NBTN_Y && y <= NBTN_Y + NBTN_H;
}

static void handleNotifTap(int x, int y) {
  // task_done: Confirm clears the session; permission: Dismiss just closes.
  if (g_notif.kind == NOTIF_TASK_DONE && inNotifButton(x, y)) {
    wsSendConfirmDone(g_notif.sessionId);
    g_notif.active = false;
    g_needRedraw = true;
  } else if (g_notif.kind == NOTIF_PERMISSION && inNotifButton(x, y)) {
    g_notif.active = false;
    g_needRedraw = true;
  }
}

// --- Arduino lifecycle ---
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Speaker.begin();

  canvas.setPsram(true);
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  netSetup(); // loads credentials, starts serial provisioning + wifi/ws
  g_needRedraw = true;
}

void loop() {
  M5.update();
  netLoop();

  auto t = M5.Touch.getDetail();

  // Live vertical scroll on the detail lists (drag up/down). The upper bound is
  // clamped in drawTaskDetail once it knows the content height.
  if (g_appState == APP_RUNNING && s_screen == SCR_DETAIL && !g_notif.active && t.isPressed()) {
    int ddy = t.deltaY();
    if (ddy != 0 && abs(ddy) >= abs(t.deltaX())) {
      g_detailScroll -= ddy; // drag up -> scroll down
      if (g_detailScroll < 0) g_detailScroll = 0;
      g_needRedraw = true;
    }
  }

  if (t.wasReleased()) {
    int dx = t.distanceX(), dy = t.distanceY();
    if (g_notif.active) {
      if (abs(dx) < TAP_MOVE && abs(dy) < TAP_MOVE) handleNotifTap(t.x, t.y);
    } else if (g_appState == APP_RUNNING) {
      if (abs(dx) > SWIPE_THRESHOLD && abs(dx) > abs(dy)) handleSwipe(dx < 0 ? -1 : +1);
      else if (abs(dx) < TAP_MOVE && abs(dy) < TAP_MOVE && s_screen == SCR_DASHBOARD) handleTapDashboard(t.y);
    }
  }

  uint32_t now = millis();
  if (g_needRedraw || now - s_lastDraw >= 1000) { // 1s cadence keeps running times ticking
    render();
    s_lastDraw = now;
    g_needRedraw = false;
  }
}
