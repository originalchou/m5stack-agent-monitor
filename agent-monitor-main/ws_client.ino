// ws_client.ino — WebSocket client to the backend (/ws/device). Parses snapshot and
// notification messages (ArduinoJson) into the live model and sends confirm_done.
//
// Requires the "WebSockets" library by Markus Sattler (arduinoWebSockets), installed
// via the Arduino Library Manager.
#include "app.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient ws;
static bool s_connected = false;

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

static void onText(uint8_t* payload, size_t length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  const char* type = doc["type"] | "";
  if (strcmp(type, "snapshot") == 0) applySnapshot(doc);
  else if (strcmp(type, "notification") == 0) handleNotification(doc);
}

static void wsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      s_connected = true;
      g_appState = APP_RUNNING;
      g_needRedraw = true;
      break;
    case WStype_DISCONNECTED:
      s_connected = false;
      g_statusLine = "Reconnecting to backend…";
      g_needRedraw = true;
      break;
    case WStype_TEXT:
      onText(payload, length);
      break;
    default:
      break;
  }
}

void wsBegin(const String& host, uint16_t port, const String& path) {
  ws.begin(host.c_str(), port, path.c_str());
  ws.onEvent(wsEvent);
  ws.setReconnectInterval(3000);
}

void wsLoop() {
  if (s_connected || WiFi.status() == WL_CONNECTED) ws.loop();
}

void wsSendConfirmDone(const String& sessionId) {
  JsonDocument doc;
  doc["type"] = "confirm_done";
  doc["sessionId"] = sessionId;
  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}
