#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 sensor;
WebServer server(80);

const char *WIFI_SSID = "speeed";
const char *WIFI_PASS = "The_Integtal_is_Real";

static const byte RATE_SIZE = 5;
byte rates[RATE_SIZE];
byte rateSpot = 0;
byte ratesFilled = 0;
long lastBeat = 0;

float bpm = 0.0f;
float avgBpm = 0.0f;
long lastIr = 0;
const long FINGER_THRESHOLD = 500;

const unsigned long MEASURE_MS = 10000;
const unsigned long SAMPLE_INTERVAL_MS = 200;
bool measureActive = false;
bool measureDone = false;
unsigned long measureStartMs = 0;
unsigned long lastSampleMs = 0;
float samples[128];
uint16_t sampleCount = 0;
float finalAvg = 0.0f;

volatile bool beatDetected = false;

void handleApiPulse() {
  bool finger = lastIr > FINGER_THRESHOLD;
  bool beat = beatDetected;
  beatDetected = false;
  String json = "{";
  json += "\"bpm\":" + String(avgBpm, 1) + ",";
  json += "\"raw_ir\":" + String(lastIr) + ",";
  json += "\"finger\":" + String(finger ? "true" : "false") + ",";
  json += "\"beat\":" + String(beat ? "true" : "false") + ",";
  json += "\"active\":" + String(measureActive ? "true" : "false") + ",";
  json += "\"done\":" + String(measureDone ? "true" : "false") + ",";
  json += "\"elapsed_ms\":" + String(measureActive ? (millis() - measureStartMs) : 0) + ",";
  json += "\"final_avg\":" + String(finalAvg, 1) + ",";
  json += "\"samples\":" + String(sampleCount);
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiStart() {
  measureActive = true;
  measureDone = false;
  measureStartMs = millis();
  lastSampleMs = 0;
  sampleCount = 0;
  finalAvg = 0.0f;
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // XIAO ESP32C3: SDA=D4(GPIO6), SCL=D5(GPIO7)
  Wire.begin(SDA, SCL);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS init fehlgeschlagen.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Verbinde mit WLAN");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Verbunden. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WLAN nicht verbunden. Starte AP: ESP32C3-PULSE");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32C3-PULSE");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX3010x nicht gefunden. Verkabelung pruefen.");
    while (true) { delay(1000); }
  }

  // powerLevel, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange
  sensor.setup(0x1F, 4, 2, 100, 411, 4096);
  sensor.setPulseAmplitudeRed(0x1F);
  sensor.setPulseAmplitudeIR(0x1F);

  server.on("/api/pulse", handleApiPulse);
  server.on("/api/start", HTTP_POST, handleApiStart);
  server.on("/", HTTP_GET, []() {
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      server.send(404, "text/plain", "index.html nicht gefunden");
      return;
    }
    server.streamFile(f, "text/html");
    f.close();
  });
  server.serveStatic("/", LittleFS, "/");
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void loop() {
  long ir = sensor.getIR();
  lastIr = ir;

  if (checkForBeat(ir)) {
    long now = millis();
    long delta = now - lastBeat;
    lastBeat = now;

    // Valid heart rate range: 30..200 BPM
    if (delta > 300 && delta < 2000) {
      bpm = 60.0f / (delta / 1000.0f);

      // reject wild jumps (>35% from current avg)
      if (avgBpm > 0 && (bpm > avgBpm * 1.35f || bpm < avgBpm * 0.65f)) {
        // ignore outlier
      } else if (bpm > 30 && bpm < 200) {
        rates[rateSpot++] = (byte)bpm;
        rateSpot %= RATE_SIZE;
        if (ratesFilled < RATE_SIZE) ratesFilled++;

        float sum = 0;
        for (byte i = 0; i < ratesFilled; i++) {
          sum += rates[i];
        }
        avgBpm = sum / ratesFilled;
        beatDetected = true;
      }
    }
  }

  if (ir < FINGER_THRESHOLD) {
    avgBpm = 0;
    ratesFilled = 0;
    bpm = 0;
  }

  if (measureActive) {
    unsigned long now = millis();
    if (now - measureStartMs >= MEASURE_MS) {
      measureActive = false;
      measureDone = true;
      float sum = 0;
      uint16_t valid = 0;
      for (uint16_t i = 0; i < sampleCount; i++) {
        if (samples[i] > 0) {
          sum += samples[i];
          valid++;
        }
      }
      finalAvg = valid > 0 ? sum / valid : 0.0f;
    } else if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
      lastSampleMs = now;
      if (sampleCount < (sizeof(samples) / sizeof(samples[0]))) {
        samples[sampleCount++] = (avgBpm > 0 ? avgBpm : 0.0f);
      }
    }
  }

  Serial.print("IR: ");
  Serial.print(ir);
  Serial.print("  BPM: ");
  Serial.print(bpm, 1);
  Serial.print("  AVG: ");
  Serial.print(avgBpm, 1);
  Serial.print("  ACTIVE: ");
  Serial.print(measureActive ? "1" : "0");
  Serial.print("  DONE: ");
  Serial.print(measureDone ? "1" : "0");
  Serial.print("  FINAL: ");
  Serial.println(finalAvg, 1);

  server.handleClient();
  delay(20);
}
