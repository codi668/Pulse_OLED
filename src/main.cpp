#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Wire.h>

#include "AppConfig.h"
#include "OledDisplay.h"
#include "PulseMonitor.h"
#include "PulseWebServer.h"

namespace {

PulseMonitor pulseMonitor;
OledDisplay oledDisplay;
PulseWebServer webServer(pulseMonitor);

String connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(AppConfig::kWifiSsid, AppConfig::kWifiPass);
  Serial.print("Verbinde mit WLAN");

  const unsigned long wifiStartMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartMs < 20000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Verbunden. IP: ");
    Serial.println(WiFi.localIP());
    return WiFi.localIP().toString();
  }

  Serial.println("WLAN nicht verbunden. Starte AP: ESP32C3-PULSE");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32C3-PULSE-ameli");
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
  Serial.print("IR: ");
  Serial.print(snapshot.rawIr);
  Serial.print("  BPM: ");
  Serial.print(pulseMonitor.instantBpm(), 1);
  Serial.print("  AVG: ");
  Serial.print(snapshot.avgBpm, 1);
  Serial.print("  ACTIVE: ");
  Serial.print(snapshot.measurementActive ? "1" : "0");
  Serial.print("  DONE: ");
  Serial.print(snapshot.measurementDone ? "1" : "0");
  Serial.print("  FINAL: ");
  Serial.println(snapshot.finalAvg, 1);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

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

  oledDisplay.begin(Wire);
  oledDisplay.setNetworkLabel(networkLabel);
  oledDisplay.render(pulseMonitor.snapshot(), millis(), true);
  webServer.begin();
}

void loop() {
  const unsigned long nowMs = millis();
  pulseMonitor.update(nowMs);

  const PulseSnapshot snapshot = pulseMonitor.snapshot();
  printDebug(snapshot);
  webServer.handleClient();
  oledDisplay.render(snapshot, nowMs);
  delay(20);
}
