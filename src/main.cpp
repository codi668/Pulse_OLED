#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Wire.h>

#include "AppConfig.h"
#include "AppSettings.h"
#include "OledDisplay.h"
#include "PulseMonitor.h"
#include "PulseWebServer.h"

namespace {

AppSettingsStore appSettings;
PulseMonitor pulseMonitor(appSettings);
OledDisplay oledDisplay(appSettings);
PulseWebServer webServer(pulseMonitor, appSettings);

String connectWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32C3-PULSE");
  Serial.print("AP aktiv. IP: ");
  Serial.println(WiFi.softAPIP());

  WiFi.begin(AppConfig::kWifiSsid, AppConfig::kWifiPass);
  Serial.print("Verbinde mit WLAN");

  const unsigned long wifiStartMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartMs < 20000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Verbunden. WLAN-IP: ");
    Serial.println(WiFi.localIP());
    return WiFi.localIP().toString();
  }

  Serial.println("WLAN nicht verbunden. Nutze Access Point ESP32C3-PULSE");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  return WiFi.softAPIP().toString();
}

void initFileSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS init fehlgeschlagen.");
  }
}

void printDebug(const PulseSnapshot &snapshot) {
  Serial.print("IR=");
  Serial.print(snapshot.rawIr);
  Serial.print("  INST=");
  Serial.print(snapshot.instantBpm, 1);
  Serial.print("  AVG=");
  Serial.print(snapshot.avgBpm, 1);
  Serial.print("  TH=");
  Serial.print(snapshot.fingerThreshold);
  Serial.print("  FINGER=");
  Serial.print(snapshot.fingerPresent ? "1" : "0");
  Serial.print("  ACTIVE=");
  Serial.print(snapshot.measurementActive ? "1" : "0");
  Serial.print("  DONE=");
  Serial.print(snapshot.measurementDone ? "1" : "0");
  Serial.print("  FINAL=");
  Serial.println(snapshot.finalAvg, 1);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  appSettings.begin();

  // XIAO ESP32C3: SDA=D4(GPIO6), SCL=D5(GPIO7)
  Wire.begin(SDA, SCL);

  initFileSystem();
  const String networkLabel = connectWifi();

  if (!pulseMonitor.begin(Wire)) {
    Serial.println("MAX3010x nicht gefunden. Verkabelung pruefen.");
    while (true) {
      delay(1000);
    }
  }

  pulseMonitor.startCalibration(millis());

  oledDisplay.begin(Wire);
  oledDisplay.setNetworkLabel(networkLabel);
  oledDisplay.render(pulseMonitor.snapshot(), millis(), true);
  webServer.begin();
}

void loop() {
  static unsigned long lastDebugMs = 0;
  const unsigned long nowMs = millis();

  pulseMonitor.update(nowMs);

  const PulseSnapshot snapshot = pulseMonitor.snapshot();
  if (nowMs - lastDebugMs >= appSettings.current().debugIntervalMs) {
    printDebug(snapshot);
    lastDebugMs = nowMs;
  }

  webServer.handleClient();
  oledDisplay.render(snapshot, nowMs);
  delay(1);
}
