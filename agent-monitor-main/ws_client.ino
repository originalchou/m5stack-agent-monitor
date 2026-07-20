// ws_client.ino — WebSocket client to the backend (/ws/device). Parses snapshot and
// notification messages (ArduinoJson) into the live model and sends confirm_done.
//
// Uses the WebSocketClient from the ArduinoHttpClient library (polling API over a
// WiFiClient) plus ArduinoJson — both installed via the Arduino Library Manager.
#include "app.h"
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

static WiFiClient s_wifi;
static WebSocketClient* s_ws = nullptr;
static bool s_connected = false;
static String s_host, s_path = "/ws/device";
static uint16_t s_port = 3050;
static uint32_t s_lastAttempt = 0;

bool wsIsConnected() { return s_connected; }

static void applyUsage(Usage& u, JsonObject o) {
  if (o.isNull()) {
    u.valid = false;
    return;
  }
  u.usedPercent = o["usedPercent"].isNull() ? -1 : (int)o["usedPercent"];
  u.resetsAt = o["resetsAt"] | 0UL;
  u.windowMinutes = o["windowMinutes"] | 0;
  u.planType = (const char*)(o["planType"] | "");
  u.valid = u.usedPercent >= 0;
}

static uint8_t planStatus(const char* s) {
  if (strcmp(s, "completed") == 0) return STEP_DONE;
  if (strcmp(s, "in_progress") == 0) return STEP_IN_PROGRESS;
  return STEP_PENDING;
}

static void applySnapshot(JsonDocument& doc) {
  g_sessions.clear();
  for (JsonObject so : doc["sessions"].as<JsonArray>()) {
    Session s;
    s.id = (const char*)(so["id"] | "");
    s.title = (const char*)(so["title"] | "");
    s.agent = agentFromString(so["agent"] | "codex");
    s.done = strcmp((const char*)(so["status"] | "running"), "done") == 0;
    s.elapsedBase = so["elapsedSeconds"] | 0UL;
    s.syncMillis = millis();
    s.contextLeftPercent = so["contextLeftPercent"].isNull() ? -1 : (int)(so["contextLeftPercent"] | -1);
    for (JsonObject p : so["plan"].as<JsonArray>()) {
      PlanStep ps;
      ps.step = (const char*)(p["step"] | "");
      ps.status = planStatus(p["status"] | "pending");
      s.plan.push_back(ps);
    }
    for (JsonObject a : so["agents"].as<JsonArray>()) {
      SubAgent sa;
      sa.type = (const char*)(a["type"] | "");
      sa.running = strcmp((const char*)(a["status"] | ""), "running") == 0;
      s.agents.push_back(sa);
    }
    s.activeAgents = so["activeAgents"] | 0;
    g_sessions.push_back(s);
  }
  applyUsage(g_codexUsage, doc["usage"]["codex"].as<JsonObject>());
  g_needRedraw = true;
}

static void handleNotification(JsonDocument& doc) {
  const char* kind = doc["kind"] | "";
  if (strcmp(kind, "task_done") == 0) g_notif.kind = NOTIF_TASK_DONE;
  else if (strcmp(kind, "permission_request") == 0) g_notif.kind = NOTIF_PERMISSION;
  else return;
  g_notif.sessionId = (const char*)(doc["sessionId"] | "");
  g_notif.title = (const char*)(doc["title"] | "");
  g_notif.tool = (const char*)(doc["tool"] | "");
  g_notif.command = (const char*)(doc["command"] | "");
  g_notif.description = (const char*)(doc["description"] | "");
  g_notif.active = true;
  playAlertTone();
  g_needRedraw = true;
}

static void onText(const String& msg) {
  JsonDocument doc;
  if (deserializeJson(doc, msg)) return;
  const char* type = doc["type"] | "";
  if (strcmp(type, "snapshot") == 0) applySnapshot(doc);
  else if (strcmp(type, "notification") == 0) handleNotification(doc);
}

static void tryConnect() {
  if (!s_ws) return;
  s_ws->stop(); // ensure the underlying socket is clean before (re)connecting
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

  // Connected: drain any queued messages (backend pushes snapshots + notifications).
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
  JsonDocument doc;
  doc["type"] = "confirm_done";
  doc["sessionId"] = sessionId;
  String out;
  serializeJson(doc, out);
  s_ws->beginMessage(TYPE_TEXT);
  s_ws->print(out);
  s_ws->endMessage();
}
