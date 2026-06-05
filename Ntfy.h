#pragma once
#include <HTTPClient.h>

// ── ntfy Notification Support ─────────────────────────────
// Configured at runtime via the KlippyMon web UI
// Variables defined in Settings.h:
//   ntfyEnabled, ntfyServer, ntfyTopic, ntfyToken, ntfyStallMin

static uint32_t ntfyLastProgressTime = 0;
static float    ntfyLastProgress     = 0.0f;
static bool     ntfyStallFired       = false;
static bool     ntfyDoneFired        = false;

void ntfySend(const char* message, const char* title, const char* priority) {
  if (!ntfyEnabled) return;
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = ntfyServer + "/" + ntfyTopic;
  http.begin(url);
  http.addHeader("Title",        title);
  http.addHeader("Priority",     priority);
  http.addHeader("Content-Type", "text/plain");
  if (ntfyToken != "") {
    http.addHeader("Authorization", "Bearer " + ntfyToken);
  }
  http.POST(message);
  http.end();
}

void ntfyCheckStall(float currentProgress) {
  if (currentProgress > ntfyLastProgress + 0.001f) {
    ntfyLastProgress     = currentProgress;
    ntfyLastProgressTime = millis();
    ntfyStallFired       = false;
    return;
  }
  if (!ntfyStallFired && ntfyLastProgressTime > 0) {
    uint32_t stallMs = (uint32_t)ntfyStallMin * 60UL * 1000UL;
    if ((millis() - ntfyLastProgressTime) >= stallMs) {
      ntfySend("No print progress — filament change, runout, or jam?",
               "Printer Needs Attention", "high");
      ntfyStallFired = true;
    }
  }
}

void ntfyPrintComplete(const String& rawPath, float totalSecs) {
  if (ntfyDoneFired) return;
  
  // Strip path and extension for clean display name
  String filename = rawPath;
  int slashIdx = filename.lastIndexOf('/');
  if (slashIdx >= 0) filename = filename.substring(slashIdx + 1);
  int dotIdx = filename.lastIndexOf('.');
  if (dotIdx > 0) filename = filename.substring(0, dotIdx);

  int hrs  = (int)totalSecs / 3600;
  int mins = ((int)totalSecs % 3600) / 60;
  char msg[80];
  if (hrs > 0)
    snprintf(msg, sizeof(msg), "%s finished in %dh %02dm", filename.c_str(), hrs, mins);
  else
    snprintf(msg, sizeof(msg), "%s finished in %dm", filename.c_str(), mins);
  ntfySend(msg, "Print Complete", "default");
  ntfyDoneFired = true;
}

void ntfyResetForNewPrint() {
  ntfyLastProgressTime = millis();
  ntfyLastProgress     = 0.0f;
  ntfyStallFired       = false;
  ntfyDoneFired        = false;
}