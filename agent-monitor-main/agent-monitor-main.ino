// agent-monitor-main.ino — M5Stack CoreS3 agent-monitor UI mock.
//
// Carousel navigation (dashboard in the centre):
//   [Claude usage]  <-- swipe right --  [ DASHBOARD ]  -- swipe left -->  [Codex usage]
// Tap a task on the dashboard to drill into its detail page; swipe right there
// to go back. All data is mocked (see mock_data.h).
#include "app.h"

// The off-screen canvas declared extern in app.h.
M5Canvas canvas(&M5.Display);

// --- Screen state machine ---------------------------------------------------
enum Screen : uint8_t {
  SCR_USAGE_CLAUDE = 0,  // carousel: left of dashboard
  SCR_DASHBOARD    = 1,  // carousel: centre
  SCR_USAGE_CODEX  = 2,  // carousel: right of dashboard
  SCR_DETAIL       = 3,  // overlay drilled into from the dashboard
};

Screen   g_screen       = SCR_DASHBOARD;
int      g_selectedTask = 0;      // index into g_tasks for the detail page
bool     g_needRedraw   = true;
uint32_t g_lastDraw     = 0;

// --- Rendering --------------------------------------------------------------
void render() {
  switch (g_screen) {
    case SCR_USAGE_CLAUDE: drawUsage(g_claudeUsage, 0); break;
    case SCR_DASHBOARD:    drawDashboard();             break;
    case SCR_USAGE_CODEX:  drawUsage(g_codexUsage, 2);  break;
    case SCR_DETAIL:       drawTaskDetail(g_tasks[g_selectedTask]); break;
  }
  canvas.pushSprite(0, 0);
}

// --- Input handling ---------------------------------------------------------
// Swipe left = go to the next screen on the right; swipe right = the one left.
void handleSwipe(int dir) {  // dir: -1 = swipe left, +1 = swipe right
  switch (g_screen) {
    case SCR_DASHBOARD:
      g_screen = (dir < 0) ? SCR_USAGE_CODEX : SCR_USAGE_CLAUDE;
      break;
    case SCR_USAGE_CLAUDE:                       // reached by swipe right
      if (dir < 0) g_screen = SCR_DASHBOARD;     // swipe left returns
      break;
    case SCR_USAGE_CODEX:                        // reached by swipe left
      if (dir > 0) g_screen = SCR_DASHBOARD;     // swipe right returns
      break;
    case SCR_DETAIL:
      if (dir > 0) g_screen = SCR_DASHBOARD;     // swipe right dismisses detail
      break;
  }
  g_needRedraw = true;
}

// A tap only means something on the dashboard: pick the task card under the y.
void handleTap(int x, int y) {
  if (g_screen != SCR_DASHBOARD) return;
  int top = HEADER_H + PAD;
  for (int i = 0; i < g_taskCount; i++) {
    int cardY = top + i * (CARD_H + CARD_GAP);
    if (y >= cardY && y <= cardY + CARD_H) {
      g_selectedTask = i;
      g_screen = SCR_DETAIL;
      g_needRedraw = true;
      return;
    }
  }
}

// --- Arduino lifecycle ------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);          // 320x240 landscape

  canvas.setPsram(true);              // 320x240x16bpp buffer lives in PSRAM
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  g_needRedraw = true;
}

void loop() {
  M5.update();

  auto t = M5.Touch.getDetail();
  if (t.wasReleased()) {
    int dx = t.distanceX();
    int dy = t.distanceY();
    if (abs(dx) > SWIPE_THRESHOLD && abs(dx) > abs(dy)) {
      handleSwipe(dx < 0 ? -1 : +1);
    } else if (abs(dx) < TAP_MOVE && abs(dy) < TAP_MOVE) {
      handleTap(t.x, t.y);
    }
  }

  // Redraw on any state change, and once a second so running times tick.
  uint32_t now = millis();
  if (g_needRedraw || now - g_lastDraw >= 1000) {
    render();
    g_lastDraw = now;
    g_needRedraw = false;
  }
}
