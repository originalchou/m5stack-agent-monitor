// app.h — shared declarations tying the tabs together.
#pragma once

// All system/library headers used by any tab are included here, BEFORE Arduino_JSON.
// Two reasons:
//  1. JSONVar must be declared before Arduino hoists the auto-generated prototypes
//     for cross-tab functions (which use it) to the top of the combined sketch.
//  2. Arduino_JSON.h does `#define null nullptr` and `#define typeof typeof_`, which
//     would corrupt ESP-IDF/system headers (they use `typeof` in macros). Pulling
//     every system header in first means those macros only ever reach our own code.
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoHttpClient.h>
#include <time.h>
#include "theme.h"
#include "model.h"
#include <Arduino_JSON.h> // must be last

// Off-screen buffer every screen draws into (flicker-free push in render()).
extern M5Canvas canvas;

// Top-level app state.
enum AppState : uint8_t {
  APP_PROVISIONING, // no wifi credentials yet — waiting for USB admin setup
  APP_CONNECTING,   // have credentials, joining wifi / backend
  APP_RUNNING,      // connected — showing live data
};
extern AppState g_appState;
extern String g_statusLine; // sub-text on the provisioning/connecting screen
extern bool g_needRedraw;   // set by net code when live data changes

// Notification overlay.
enum NotifKind : uint8_t { NOTIF_NONE, NOTIF_TASK_DONE, NOTIF_PERMISSION };
struct Notification {
  bool active = false;
  NotifKind kind = NOTIF_NONE;
  String sessionId, title, tool, command, description;
};
extern Notification g_notif;

// Notification "Confirm/Dismiss" button rect (shared by the overlay + tap handling).
static constexpr int NBTN_W = 150, NBTN_H = 42;
static constexpr int NBTN_X = (SCR_W - NBTN_W) / 2;
static constexpr int NBTN_Y = SCR_H - NBTN_H - 18;

// --- Screen renderers (one per screen_*.ino tab) ---
void drawDashboard();
void drawTaskDetail(const Session& s);
void drawUsage(const Usage& u, Agent agent, int dotIndex);
void drawProvisioning();
void drawNotification();

// --- Networking (net_config.ino + ws_client.ino) ---
void netSetup();
void netLoop();
void wsBegin(const String& host, uint16_t port, const String& path);
void wsLoop();
void wsSendConfirmDone(const String& sessionId);
bool wsIsConnected();

// Called by ws_client when a notification arrives, to play the alert tone.
void playAlertTone();
