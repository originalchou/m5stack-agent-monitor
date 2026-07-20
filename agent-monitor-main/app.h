// app.h — shared declarations tying the tabs together.
#pragma once
#include <M5Unified.h>
#include "theme.h"
#include "model.h"

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
