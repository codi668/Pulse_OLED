#include <Arduino.h>
#include <cstring>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <time.h>

#include "AppConfig.h"
#include "AppSettings.h"
#include "EspNowPulseLink.h"
#include "OledDisplay.h"
#include "ReceiverUiSettings.h"

namespace {

AppSettingsStore appSettings;
ReceiverUiSettingsStore uiSettings;
OledDisplay oledDisplay(appSettings);
PulseSnapshot latestSnapshot;
PulseSnapshot screenSnapshot;
bool havePacket = false;
unsigned long lastPacketMs = 0;
unsigned long lastStatusPrintMs = 0;
const unsigned long kOfflineTimeoutMs = AppConfig::kEspNowLinkTimeoutMs;
constexpr bool kLogEachPacket = false;
constexpr const char *kTzRule = "CET-1CEST,M3.5.0/2,M10.5.0/3";
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
WebServer webServer(80);

unsigned long safeAge(unsigned long nowMs, unsigned long thenMs) {
  return nowMs >= thenMs ? nowMs - thenMs : 0UL;
}

String connectWifi() {
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  const IPAddress apIp(192, 168, 4, 1);
  const IPAddress apGateway(192, 168, 4, 1);
  const IPAddress apSubnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIp, apGateway, apSubnet);
  WiFi.softAP("ESP32C3-RX");
  WiFi.begin(AppConfig::kWifiSsid, AppConfig::kWifiPass);

  Serial.print("AP aktiv. IP: ");
  Serial.println(WiFi.softAPIP());
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
    configTzTime(kTzRule, "pool.ntp.org", "time.nist.gov");
    Serial.print("AP weiter erreichbar unter: ");
    Serial.println(WiFi.softAPIP());
    return WiFi.softAPIP().toString();
  }

  Serial.println("WLAN nicht verbunden. ESP-NOW Empfang versucht trotzdem.");
  Serial.print("Nutze AP-IP: ");
  Serial.println(WiFi.softAPIP());
  return WiFi.softAPIP().toString();
}

void onEspNowReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != static_cast<int>(sizeof(EspNowPulseLink::Packet))) {
    return;
  }

  EspNowPulseLink::Packet packet;
  memcpy(&packet, data, sizeof(packet));
  if (!EspNowPulseLink::isValid(packet)) {
    return;
  }

  portENTER_CRITICAL(&snapshotMux);
  EspNowPulseLink::applyPacket(packet, latestSnapshot, millis());
  havePacket = true;
  lastPacketMs = millis();
  portEXIT_CRITICAL(&snapshotMux);

  if (kLogEachPacket) {
    Serial.print("RX seq=");
    Serial.print(packet.sequence);
    Serial.print(" bpm=");
    Serial.print(packet.avgBpm, 1);
    Serial.print(" finger=");
    Serial.print((packet.flags & EspNowPulseLink::kFlagFingerPresent) ? "1" : "0");
    Serial.print(" beat=");
    Serial.println((packet.flags & EspNowPulseLink::kFlagBeatEvent) ? "1" : "0");
  }

  (void)info;
}

void initEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init fehlgeschlagen.");
    return;
  }

  esp_now_register_recv_cb(onEspNowReceive);
  Serial.println("ESP-NOW Empfaenger bereit.");
}

String checkedAttr(bool value) {
  return value ? " checked" : "";
}

String selectedAttr(bool value) {
  return value ? " selected" : "";
}

const char *displayModeLabel(uint8_t mode) {
  switch (static_cast<ReceiverDisplayMode>(mode)) {
    case ReceiverDisplayMode::kBigBpm:
      return "BPM Fokus";
    case ReceiverDisplayMode::kConnection:
      return "Verbindung";
    case ReceiverDisplayMode::kStatus:
      return "Status";
    case ReceiverDisplayMode::kMinimal:
      return "Minimal";
    case ReceiverDisplayMode::kClockBpm:
      return "Uhr + BPM";
    case ReceiverDisplayMode::kClockDateBpm:
      return "Uhr + Datum + BPM";
    case ReceiverDisplayMode::kSignal:
      return "Signal";
    case ReceiverDisplayMode::kTiles:
    default:
      return "Kacheln";
  }
}

String buildUiPage() {
  const ReceiverUiSettings &ui = uiSettings.current();
  const unsigned long linkAgeMs = safeAge(millis(), lastPacketMs);
  const bool online = linkAgeMs <= kOfflineTimeoutMs;
  const PulseSnapshot snapshot = latestSnapshot;

  String html;
  html.reserve(4096);
  html += "<!doctype html><html lang='de'><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Receiver Übersicht</title>";
  html += "<style>";
  html += "body{margin:0;font-family:Arial,sans-serif;background:#edf4f7;color:#17232b;}";
  html += ".wrap{max-width:900px;margin:0 auto;padding:24px;}";
  html += ".card{background:#fff;border-radius:18px;padding:20px;margin:0 0 16px;box-shadow:0 12px 30px rgba(16,31,40,.12);}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;}";
  html += "label{display:flex;gap:10px;align-items:center;padding:12px 14px;border:1px solid #d8e2e8;border-radius:14px;}";
  html += "select{width:100%;padding:12px 14px;border:1px solid #d8e2e8;border-radius:14px;font-size:16px;}";
  html += ".status{display:flex;flex-wrap:wrap;gap:10px;}";
  html += ".pill{padding:8px 12px;border-radius:999px;background:#eff4f7;}";
  html += "button{border:0;border-radius:999px;padding:12px 18px;background:#dc6b4d;color:#fff;font-weight:700;cursor:pointer;}";
  html += ".muted{color:#61707c;}";
  html += "</style></head><body><div class='wrap'>";
  html += "<div class='card'><h1>Receiver Übersicht</h1><p class='muted'>Wähle Layout und Kacheln für das OLED.</p>";
  html += "<form method='POST' action='/save'>";
  html += "<label style='display:block;margin-bottom:14px'>Anzeige-Modus";
  html += "<select name='display_mode'>";
  html += "<option value='0'";
  html += selectedAttr(ui.displayMode == 0);
  html += ">Kacheln</option>";
  html += "<option value='1'";
  html += selectedAttr(ui.displayMode == 1);
  html += ">BPM Fokus</option>";
  html += "<option value='2'";
  html += selectedAttr(ui.displayMode == 2);
  html += ">Verbindung</option>";
  html += "<option value='3'";
  html += selectedAttr(ui.displayMode == 3);
  html += ">Status</option>";
  html += "<option value='4'";
  html += selectedAttr(ui.displayMode == 4);
  html += ">Minimal</option>";
  html += "<option value='5'";
  html += selectedAttr(ui.displayMode == 5);
  html += ">Große Uhr + BPM</option>";
  html += "<option value='6'";
  html += selectedAttr(ui.displayMode == 6);
  html += ">Uhr + Datum + BPM</option>";
  html += "<option value='7'";
  html += selectedAttr(ui.displayMode == 7);
  html += ">Signal / Status</option>";
  html += "</select></label>";
  html += "<div class='grid'>";
  html += "<label><input type='checkbox' name='show_bpm'" + checkedAttr(ui.showBpm) + "> Herz / BPM</label>";
  html += "<label><input type='checkbox' name='show_link'" + checkedAttr(ui.showLink) + "> WLAN / Link</label>";
  html += "<label><input type='checkbox' name='show_finger'" + checkedAttr(ui.showFinger) + "> Finger</label>";
  html += "<label><input type='checkbox' name='show_state'" + checkedAttr(ui.showState) + "> Status</label>";
  html += "</div><p style='margin-top:16px'><button type='submit'>Speichern</button></p></form></div>";
  html += "<div class='card'><h2>Aktueller Zustand</h2><div class='status'>";
  html += "<span class='pill'>Modus: ";
  html += displayModeLabel(ui.displayMode);
  html += "</span><span class='pill'>Link: ";
  html += online ? "OK" : "OFF";
  html += "</span><span class='pill'>Letztes Paket: ";
  html += String(linkAgeMs / 1000UL);
  html += " s</span><span class='pill'>BPM: ";
  const float bpm = snapshot.avgBpm > 0.0f ? snapshot.avgBpm : snapshot.instantBpm;
  html += bpm > 0.0f ? String(bpm, 1) : String("--");
  html += "</span><span class='pill'>Finger: ";
  html += snapshot.fingerPresent ? "ja" : "nein";
  html += "</span><span class='pill'>State: ";
  html += snapshot.calibrationActive ? "CAL" :
          snapshot.measurementActive ? "MEAS" :
          snapshot.measurementDone ? "DONE" : "IDLE";
  html += "</span></div></div>";
  html += "<div class='card'><p class='muted'>OLED-IP: ";
  html += WiFi.softAPIP().toString();
  html += " | WLAN: ";
  html += WiFi.localIP().toString();
  html += " | AP-SSID: ESP32C3-RX";
  html += "</p><p class='muted'>Direkt erreichbar ist der Access Point meist unter http://192.168.4.1</p>";
  html += "</div></div></body></html>";
  return html;
}

void handleRoot() {
  webServer.send(200, "text/html", buildUiPage());
}

void handleSave() {
  ReceiverUiSettings candidate = uiSettings.current();
  candidate.displayMode = static_cast<uint8_t>(webServer.arg("display_mode").toInt());
  if (candidate.displayMode > static_cast<uint8_t>(ReceiverDisplayMode::kSignal)) {
    candidate.displayMode = static_cast<uint8_t>(ReceiverDisplayMode::kTiles);
  }
  candidate.showBpm = webServer.hasArg("show_bpm");
  candidate.showLink = webServer.hasArg("show_link");
  candidate.showFinger = webServer.hasArg("show_finger");
  candidate.showState = webServer.hasArg("show_state");
  uiSettings.update(candidate);

  webServer.sendHeader("Location", "/");
  webServer.send(303, "text/plain", "saved");
}

void setupWebServer() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.onNotFound([]() { webServer.send(404, "text/plain", "not found"); });
  webServer.begin();
}

void renderDashboard(unsigned long nowMs, bool force) {
  screenSnapshot = latestSnapshot;
  const ReceiverUiSettings &ui = uiSettings.current();
  oledDisplay.renderOverview(screenSnapshot, nowMs, safeAge(nowMs, lastPacketMs),
                             static_cast<ReceiverDisplayMode>(ui.displayMode), ui.showBpm,
                             ui.showLink, ui.showFinger, ui.showState, force);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Receiver startet...");

  appSettings.begin();
  uiSettings.begin();
  Wire.begin(SDA, SCL);

  if (!oledDisplay.begin(Wire)) {
    Serial.println("OLED nicht gefunden. Verkabelung pruefen.");
    while (true) {
      delay(1000);
    }
  }

  oledDisplay.setNetworkLabel("WLAN wird verbunden...");
  renderDashboard(millis(), true);

  const String ipLabel = connectWifi();
  initEspNow();

  setupWebServer();
  oledDisplay.setNetworkLabel(ipLabel);
  renderDashboard(millis(), true);
  Serial.println("Receiver bereit.");
  Serial.print("Weboberflaeche AP: http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Weboberflaeche WLAN: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  const unsigned long nowMs = millis();
  bool shouldRender = false;

  webServer.handleClient();

  portENTER_CRITICAL(&snapshotMux);
  if (havePacket) {
    screenSnapshot = latestSnapshot;
    havePacket = false;
    shouldRender = true;
  }
  const bool linkAlive = safeAge(nowMs, lastPacketMs) <= kOfflineTimeoutMs;
  portEXIT_CRITICAL(&snapshotMux);

  if (!linkAlive) {
    renderDashboard(nowMs, true);
    if (nowMs - lastStatusPrintMs >= 2000) {
      Serial.println("Warte auf ESP-NOW Daten...");
      lastStatusPrintMs = nowMs;
    }
    delay(250);
    return;
  }

  if (shouldRender) {
    renderDashboard(nowMs, true);
  } else {
    renderDashboard(nowMs, false);
  }

  webServer.handleClient();
  if (nowMs - lastStatusPrintMs >= 2000) {
    Serial.print("Link aktiv, letzte Daten vor ");
    Serial.print(safeAge(nowMs, lastPacketMs));
    Serial.println(" ms");
    lastStatusPrintMs = nowMs;
  }

  delay(1);
}
