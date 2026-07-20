// ws_client.ino — WebSocket client to the backend (/ws/device). Parses snapshot and
// notification messages into the live model and sends confirm_done.
//
// Uses ArduinoHttpClient's WebSocketClient (polling API over WiFiClient) and the
// Arduino_JSON library (JSONVar) — both from the Arduino Library Manager.
#include "app.h" // brings in WiFi.h, ArduinoHttpClient.h, Arduino_JSON (see app.h include order)

static WiFiClient s_wifi;
static WebSocketClient* s_ws = nullptr;
static bool s_connected = false;
static String s_host, s_path = "/ws/device";
static uint16_t s_port = 3050;
static uint32_t s_lastAttempt = 0;

bool wsIsConnected() { return s_connected; }

// --- small typed accessors over JSONVar ---
static String jStr(JSONVar v) { return (JSON.typeof(v) == "string") ? (const char*)v : String(); }
static int jNum(JSONVar v, int def) { return (JSON.typeof(v) == "number") ? (int)v : def; }
static bool isArray(JSONVar v) { return JSON.typeof(v) == "array"; }

static uint8_t planStatus(const String& s) {
  if (s == "completed") return STEP_DONE;
  if (s == "in_progress") return STEP_IN_PROGRESS;
  return STEP_PENDING;
}

static void applyUsage(Usage& u, JSONVar o) {
  if (JSON.typeof(o) != "object") {
    u.valid = false;
    return;
  }
  u.usedPercent = (JSON.typeof(o["usedPercent"]) == "number") ? (int)o["usedPercent"] : -1;
  u.resetsAt = (JSON.typeof(o["resetsAt"]) == "number") ? (unsigned long)o["resetsAt"] : 0UL;
  u.windowMinutes = jNum(o["windowMinutes"], 0);
  u.planType = jStr(o["planType"]);
  u.valid = u.usedPercent >= 0;
}

static void applySnapshot(JSONVar& doc) {
  g_sessions.clear();
  JSONVar sessions = doc["sessions"];
  int n = isArray(sessions) ? sessions.length() : 0;
  for (int i = 0; i < n; i++) {
    JSONVar so = sessions[i];
    Session s;
    s.id = jStr(so["id"]);
    s.title = jStr(so["title"]);
    s.agent = agentFromString(jStr(so["agent"]).c_str());
    s.done = (jStr(so["status"]) == "done");
    s.elapsedBase = (JSON.typeof(so["elapsedSeconds"]) == "number") ? (unsigned long)so["elapsedSeconds"] : 0UL;
    s.syncMillis = millis();
    s.contextLeftPercent = (JSON.typeof(so["contextLeftPercent"]) == "number") ? (int)so["contextLeftPercent"] : -1;

    JSONVar plan = so["plan"];
    int pn = isArray(plan) ? plan.length() : 0;
    for (int j = 0; j < pn; j++) {
      JSONVar p = plan[j];
      PlanStep ps;
      ps.step = jStr(p["step"]);
      ps.status = planStatus(jStr(p["status"]));
      s.plan.push_back(ps);
    }
    JSONVar agents = so["agents"];
    int an = isArray(agents) ? agents.length() : 0;
    for (int j = 0; j < an; j++) {
      JSONVar a = agents[j];
      SubAgent sa;
      sa.type = jStr(a["type"]);
      sa.running = (jStr(a["status"]) == "running");
      s.agents.push_back(sa);
    }
    s.activeAgents = jNum(so["activeAgents"], 0);
    g_sessions.push_back(s);
  }

  JSONVar usage = doc["usage"];
  applyUsage(g_codexUsage, usage["codex"]);
  g_needRedraw = true;
}

static void handleNotification(JSONVar& doc) {
  String kind = jStr(doc["kind"]);
  if (kind == "task_done") g_notif.kind = NOTIF_TASK_DONE;
  else if (kind == "permission_request") g_notif.kind = NOTIF_PERMISSION;
  else return;
  g_notif.sessionId = jStr(doc["sessionId"]);
  g_notif.title = jStr(doc["title"]);
  g_notif.tool = jStr(doc["tool"]);
  g_notif.command = jStr(doc["command"]);
  g_notif.description = jStr(doc["description"]);
  g_notif.active = true;
  playAlertTone();
  g_needRedraw = true;
}

static void onText(const String& msg) {
  JSONVar doc = JSON.parse(msg);
  if (JSON.typeof(doc) != "object") return;
  String type = jStr(doc["type"]);
  if (type == "snapshot") applySnapshot(doc);
  else if (type == "notification") handleNotification(doc);
}

static void tryConnect() {
  if (!s_ws) return;
  s_ws->stop(); // ensure a clean underlying socket before (re)connecting
  int rc = s_ws->begin(s_path);
  if (rc == 0 && s_ws->connected()) {
    s_connected = true;
    g_appState = APP_RUNNING;
    g_needRedraw = true;
  }
}

void wsBegin(const String& host, uint16_t port, const String& path) {
  s_host = host;
  s_port = port;
  if (path.length()) s_path = path;
  if (s_ws) {
    delete s_ws;
    s_ws = nullptr;
  }
  s_ws = new WebSocketClient(s_wifi, s_host, s_port);
  s_connected = false;
  s_lastAttempt = 0; // connect on the next wsLoop tick
}

void wsLoop() {
  if (!s_ws) return;
  if (WiFi.status() != WL_CONNECTED) {
    s_connected = false;
    return;
  }

  if (!s_connected) {
    if (millis() - s_lastAttempt >= 3000) {
      s_lastAttempt = millis();
      g_statusLine = "Connecting to backend…";
      g_needRedraw = true;
      tryConnect();
    }
    return;
  }

  int size;
  while ((size = s_ws->parseMessage()) > 0) {
    onText(s_ws->readString());
  }
  if (!s_ws->connected()) {
    s_connected = false;
    g_appState = APP_CONNECTING;
    g_statusLine = "Reconnecting to backend…";
    g_needRedraw = true;
  }
}

void wsSendConfirmDone(const String& sessionId) {
  if (!s_ws || !s_connected) return;
  JSONVar doc;
  doc["type"] = "confirm_done";
  doc["sessionId"] = sessionId;
  String out = JSON.stringify(doc);
  s_ws->beginMessage(TYPE_TEXT);
  s_ws->print(out);
  s_ws->endMessage();
}
