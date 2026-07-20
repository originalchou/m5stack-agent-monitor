// net_config.ino — WiFi credentials (NVS), USB-serial provisioning protocol, and the
// WiFi connection state machine. Talks to the backend's serial bridge in JSON lines.
#include "app.h"
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino_JSON.h>

static const char* FW_VERSION = "0.1.0";

static Preferences prefs;
static String s_ssid, s_pass, s_wsUrl;
static String s_serialBuf;
static bool s_pendingWifiResult = false;
static bool s_wsStarted = false;
static uint32_t s_connectStart = 0;

// --- credentials ---
bool hasWifiCredentials() { return s_ssid.length() > 0; }

static void saveCredentials(const String& ssid, const String& pass, const String& wsUrl) {
  s_ssid = ssid;
  s_pass = pass;
  if (wsUrl.length()) s_wsUrl = wsUrl;
  prefs.putString("ssid", s_ssid);
  prefs.putString("pass", s_pass);
  prefs.putString("wsurl", s_wsUrl);
}

// --- serial protocol ---
static void sendJson(JSONVar& doc) {
  Serial.print(JSON.stringify(doc));
  Serial.print('\n');
}

static void sendPong() {
  JSONVar doc;
  doc["type"] = "pong";
  doc["device"] = "cores3";
  doc["fw"] = FW_VERSION;
  bool connected = WiFi.status() == WL_CONNECTED;
  JSONVar wifi;
  wifi["connected"] = connected;
  wifi["ssid"] = s_ssid;
  wifi["ip"] = connected ? WiFi.localIP().toString() : String("");
  doc["wifi"] = wifi;
  sendJson(doc);
}

static void sendWifiResult(bool ok) {
  JSONVar doc;
  doc["type"] = "wifi_result";
  doc["ok"] = ok;
  doc["ssid"] = s_ssid;
  doc["ip"] = ok ? WiFi.localIP().toString() : String("");
  sendJson(doc);
}

static void doScan() {
  int n = WiFi.scanNetworks();
  JSONVar doc;
  doc["type"] = "wifi_list";
  JSONVar arr = JSON.parse("[]"); // ensure networks is always an array
  for (int i = 0; i < n; i++) {
    JSONVar net;
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = (int)WiFi.RSSI(i);
    net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    arr[i] = net;
  }
  doc["networks"] = arr;
  sendJson(doc);
  WiFi.scanDelete();
}

static void startConnect() {
  g_appState = APP_CONNECTING;
  g_statusLine = "Connecting to " + s_ssid + "…";
  g_needRedraw = true;
  s_wsStarted = false;
  WiFi.disconnect();
  WiFi.begin(s_ssid.c_str(), s_pass.c_str());
  s_connectStart = millis();
}

static String jField(JSONVar& doc, const char* key) {
  return doc.hasOwnProperty(key) ? (const char*)doc[key] : String();
}

static void handleSetWifi(JSONVar& doc) {
  String ssid = jField(doc, "ssid");
  String pass = jField(doc, "password");
  String wsUrl = jField(doc, "backendWs");
  if (!ssid.length()) return;
  saveCredentials(ssid, pass, wsUrl);
  s_pendingWifiResult = true;
  startConnect();
}

static void processSerialLine(const String& line) {
  if (line.length() == 0 || line[0] != '{') return; // ignore non-JSON
  JSONVar doc = JSON.parse(line);
  if (JSON.typeof(doc) != "object") return;
  String type = jField(doc, "type");
  if (type == "ping") sendPong();
  else if (type == "scan_wifi") doScan();
  else if (type == "set_wifi") handleSetWifi(doc);
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      processSerialLine(s_serialBuf);
      s_serialBuf = "";
    } else if (c != '\r' && s_serialBuf.length() < 512) {
      s_serialBuf += c;
    }
  }
}

// --- backend WebSocket URL parsing (ws://host:port/path) ---
static void beginWsFromUrl() {
  if (!s_wsUrl.length()) {
    g_statusLine = "No backend URL — re-run setup";
    g_needRedraw = true;
    return;
  }
  String u = s_wsUrl;
  int scheme = u.indexOf("://");
  if (scheme >= 0) u = u.substring(scheme + 3);
  String host;
  uint16_t port = 3050;
  String path = "/ws/device";
  int slash = u.indexOf('/');
  String hostport = slash >= 0 ? u.substring(0, slash) : u;
  if (slash >= 0) path = u.substring(slash);
  int colon = hostport.indexOf(':');
  if (colon >= 0) {
    host = hostport.substring(0, colon);
    port = (uint16_t)hostport.substring(colon + 1).toInt();
  } else {
    host = hostport;
  }
  g_statusLine = "Connecting to backend…";
  g_needRedraw = true;
  wsBegin(host, port, path);
}

// --- public API ---
void netSetup() {
  Serial.begin(115200);
  prefs.begin("agentmon", false);
  s_ssid = prefs.getString("ssid", "");
  s_pass = prefs.getString("pass", "");
  s_wsUrl = prefs.getString("wsurl", "");
  WiFi.mode(WIFI_STA);
  if (hasWifiCredentials()) {
    startConnect();
  } else {
    g_appState = APP_PROVISIONING;
    g_statusLine = "Connect USB and open the setup page";
  }
}

void netLoop() {
  handleSerial();

  if (g_appState == APP_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      if (s_pendingWifiResult) {
        sendWifiResult(true);
        s_pendingWifiResult = false;
      }
      if (!s_wsStarted) {
        s_wsStarted = true;
        configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC, for usage reset countdown
        beginWsFromUrl();
      }
    } else if (millis() - s_connectStart > 15000) {
      if (s_pendingWifiResult) {
        sendWifiResult(false);
        s_pendingWifiResult = false;
      }
      g_appState = APP_PROVISIONING;
      g_statusLine = "WiFi failed — check setup";
      g_needRedraw = true;
    }
  }

  wsLoop();
}
