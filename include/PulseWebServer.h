#pragma once

#include <WebServer.h>

#include "AppSettings.h"
#include "PulseMonitor.h"

class PulseWebServer {
 public:
  PulseWebServer(PulseMonitor &monitor, AppSettingsStore &settings);

  void begin();
  void handleClient();

 private:
  void handleApiPulse();
  void handleApiConfigGet();
  void handleApiConfigPost();
  void handleApiThresholdGet();
  void handleApiThresholdPost();
  void handleApiOledGet();
  void handleApiOledPost();
  void handleApiStart();
  void handleApiCalibrate();
  void handleApiReference();
  void handleRoot();
  void handleNotFound();

  PulseMonitor &monitor_;
  AppSettingsStore &settings_;
  WebServer server_{80};
};
