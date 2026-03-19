#include "PulseWebServer.h"

#include <LittleFS.h>

namespace {

bool parseUnsignedArg(WebServer &server, const char *name, unsigned long &out, String &error) {
  if (!server.hasArg(name)) {
    error = String("missing_") + name;
    return false;
  }

  const String value = server.arg(name);
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str(), &end, 0);
  if (end == value.c_str() || *end != '\0') {
    error = String("invalid_") + name;
    return false;
  }

  out = parsed;
  return true;
}

bool parseSignedArg(WebServer &server, const char *name, long &out, String &error) {
  if (!server.hasArg(name)) {
    error = String("missing_") + name;
    return false;
  }

  const String value = server.arg(name);
  char *end = nullptr;
  const long parsed = strtol(value.c_str(), &end, 0);
  if (end == value.c_str() || *end != '\0') {
    error = String("invalid_") + name;
    return false;
  }

  out = parsed;
  return true;
}

void appendConfigJson(String &json, const AppSettings &config) {
  json += "\"rate_size\":" + String(config.rateSize) + ",";
  json += "\"raw_finger_threshold\":" + String(config.rawFingerThreshold) + ",";
  json += "\"raw_finger_release_threshold\":" + String(config.rawFingerReleaseThreshold) + ",";
  json += "\"min_calibrated_finger_threshold\":" +
          String(config.minCalibratedFingerThreshold) + ",";
  json += "\"min_calibrated_finger_release_threshold\":" +
          String(config.minCalibratedFingerReleaseThreshold) + ",";
  json += "\"finger_detect_on_samples\":" + String(config.fingerDetectOnSamples) + ",";
  json += "\"finger_detect_off_samples\":" + String(config.fingerDetectOffSamples) + ",";
  json += "\"calibration_ms\":" + String(config.calibrationMs) + ",";
  json += "\"calibration_sample_interval_ms\":" + String(config.calibrationSampleIntervalMs) + ",";
  json += "\"sensor_led_brightness\":" + String(config.sensorLedBrightness) + ",";
  json += "\"sensor_sample_average\":" + String(config.sensorSampleAverage) + ",";
  json += "\"sensor_led_mode\":" + String(config.sensorLedMode) + ",";
  json += "\"sensor_sample_rate\":" + String(config.sensorSampleRate) + ",";
  json += "\"sensor_pulse_width\":" + String(config.sensorPulseWidth) + ",";
  json += "\"sensor_adc_range\":" + String(config.sensorAdcRange) + ",";
  json += "\"sensor_sample_interval_ms\":" + String(config.sensorSampleIntervalMs) + ",";
  json += "\"min_beat_interval_ms\":" + String(config.minBeatIntervalMs) + ",";
  json += "\"max_beat_interval_ms\":" + String(config.maxBeatIntervalMs) + ",";
  json += "\"measure_ms\":" + String(config.measureMs) + ",";
  json += "\"sample_interval_ms\":" + String(config.sampleIntervalMs) + ",";
  json += "\"max_measurement_samples\":" + String(config.maxMeasurementSamples) + ",";
  json += "\"debug_interval_ms\":" + String(config.debugIntervalMs) + ",";
  json += "\"oled_address_1\":" + String(config.oledAddress1) + ",";
  json += "\"oled_address_2\":" + String(config.oledAddress2) + ",";
  json += "\"oled_driver_mode\":" + String(config.oledDriverMode) + ",";
  json += "\"display_interval_ms\":" + String(config.displayIntervalMs) + ",";
  json += "\"beat_animation_ms\":" + String(config.beatAnimationMs) + ",";
  json += "\"beat_animation_frame_ms\":" + String(config.beatAnimationFrameMs) + ",";
  json += "\"oled_width\":" + String(config.oledWidth);
}

}  // namespace

PulseWebServer::PulseWebServer(PulseMonitor &monitor, AppSettingsStore &settings)
    : monitor_(monitor), settings_(settings) {}

void PulseWebServer::begin() {
  server_.on("/api/pulse", HTTP_GET, [this]() { handleApiPulse(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleApiConfigGet(); });
  server_.on("/api/config", HTTP_POST, [this]() { handleApiConfigPost(); });
  server_.on("/api/threshold", HTTP_GET, [this]() { handleApiThresholdGet(); });
  server_.on("/api/threshold", HTTP_POST, [this]() { handleApiThresholdPost(); });
  server_.on("/api/oled", HTTP_GET, [this]() { handleApiOledGet(); });
  server_.on("/api/oled", HTTP_POST, [this]() { handleApiOledPost(); });
  server_.on("/api/start", HTTP_POST, [this]() { handleApiStart(); });
  server_.on("/api/calibrate", HTTP_POST, [this]() { handleApiCalibrate(); });
  server_.on("/api/reference", HTTP_POST, [this]() { handleApiReference(); });
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.serveStatic("/", LittleFS, "/");
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
}

void PulseWebServer::handleClient() {
  server_.handleClient();
}

void PulseWebServer::handleApiPulse() {
  const PulseSnapshot snapshot = monitor_.snapshot();
  const bool beat = monitor_.consumeBeatEvent();
  const AppSettings &config = settings_.current();

  String json = "{";
  json += "\"instant_bpm\":" + String(snapshot.instantBpm, 1) + ",";
  json += "\"avg_bpm\":" + String(snapshot.avgBpm, 1) + ",";
  json += "\"bpm\":" + String(snapshot.avgBpm, 1) + ",";
  json += "\"raw_ir\":" + String(snapshot.rawIr) + ",";
  json += "\"calibrated_ir\":" + String(snapshot.calibratedIr) + ",";
  json += "\"baseline_ir\":" + String(snapshot.calibrationBaseline) + ",";
  json += "\"finger_threshold\":" + String(snapshot.fingerThreshold) + ",";
  json += "\"finger\":" + String(snapshot.fingerPresent ? "true" : "false") + ",";
  json += "\"beat\":" + String(beat ? "true" : "false") + ",";
  json += "\"calibrating\":" + String(snapshot.calibrationActive ? "true" : "false") + ",";
  json += "\"calibrated\":" + String(snapshot.calibrationDone ? "true" : "false") + ",";
  json += "\"calibration_elapsed_ms\":";
  json += snapshot.calibrationActive ? String(millis() - snapshot.calibrationStartMs) : "0";
  json += ",";
  json += "\"active\":" + String(snapshot.measurementActive ? "true" : "false") + ",";
  json += "\"done\":" + String(snapshot.measurementDone ? "true" : "false") + ",";
  json += "\"elapsed_ms\":";
  json += snapshot.measurementActive ? String(millis() - snapshot.measurementStartMs) : "0";
  json += ",";
  json += "\"measure_ms\":" + String(config.measureMs) + ",";
  json += "\"sample_interval_ms\":" + String(config.sampleIntervalMs) + ",";
  json += "\"raw_final_avg\":" + String(snapshot.rawFinalAvg, 1) + ",";
  json += "\"final_avg\":" + String(snapshot.finalAvg, 1) + ",";
  json += "\"samples\":" + String(snapshot.sampleCount) + ",";
  json += "\"reference_bpm\":" + String(snapshot.referenceBpm, 1) + ",";
  json += "\"reference_source_avg\":" + String(snapshot.referenceSourceAvg, 1) + ",";
  json += "\"correction_factor\":" + String(snapshot.correctionFactor, 4) + ",";
  json += "\"reference_calibrated\":" +
          String(snapshot.referenceCalibrated ? "true" : "false") + ",";
  json += "\"detect_offset\":" + String(config.minCalibratedFingerThreshold) + ",";
  json += "\"release_offset\":" + String(config.minCalibratedFingerReleaseThreshold);
  json += "}";
  server_.send(200, "application/json", json);
}

void PulseWebServer::handleApiConfigGet() {
  String json = "{";
  appendConfigJson(json, settings_.current());
  json += "}";
  server_.send(200, "application/json", json);
}

void PulseWebServer::handleApiConfigPost() {
  AppSettings candidate = settings_.current();
  String error;
  unsigned long unsignedValue = 0;
  long signedValue = 0;

  if (!parseUnsignedArg(server_, "rate_size", unsignedValue, error)) goto fail;
  candidate.rateSize = static_cast<uint8_t>(unsignedValue);
  if (!parseSignedArg(server_, "raw_finger_threshold", signedValue, error)) goto fail;
  candidate.rawFingerThreshold = signedValue;
  if (!parseSignedArg(server_, "raw_finger_release_threshold", signedValue, error)) goto fail;
  candidate.rawFingerReleaseThreshold = signedValue;
  if (!parseSignedArg(server_, "min_calibrated_finger_threshold", signedValue, error)) goto fail;
  candidate.minCalibratedFingerThreshold = signedValue;
  if (!parseSignedArg(server_, "min_calibrated_finger_release_threshold", signedValue, error))
    goto fail;
  candidate.minCalibratedFingerReleaseThreshold = signedValue;
  if (!parseUnsignedArg(server_, "finger_detect_on_samples", unsignedValue, error)) goto fail;
  candidate.fingerDetectOnSamples = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "finger_detect_off_samples", unsignedValue, error)) goto fail;
  candidate.fingerDetectOffSamples = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "calibration_ms", unsignedValue, error)) goto fail;
  candidate.calibrationMs = unsignedValue;
  if (!parseUnsignedArg(server_, "calibration_sample_interval_ms", unsignedValue, error)) goto fail;
  candidate.calibrationSampleIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "sensor_led_brightness", unsignedValue, error)) goto fail;
  candidate.sensorLedBrightness = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "sensor_sample_average", unsignedValue, error)) goto fail;
  candidate.sensorSampleAverage = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "sensor_led_mode", unsignedValue, error)) goto fail;
  candidate.sensorLedMode = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "sensor_sample_rate", unsignedValue, error)) goto fail;
  candidate.sensorSampleRate = static_cast<uint16_t>(unsignedValue);
  if (!parseSignedArg(server_, "sensor_pulse_width", signedValue, error)) goto fail;
  candidate.sensorPulseWidth = static_cast<int>(signedValue);
  if (!parseSignedArg(server_, "sensor_adc_range", signedValue, error)) goto fail;
  candidate.sensorAdcRange = static_cast<int>(signedValue);
  if (!parseUnsignedArg(server_, "sensor_sample_interval_ms", unsignedValue, error)) goto fail;
  candidate.sensorSampleIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "min_beat_interval_ms", unsignedValue, error)) goto fail;
  candidate.minBeatIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "max_beat_interval_ms", unsignedValue, error)) goto fail;
  candidate.maxBeatIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "measure_ms", unsignedValue, error)) goto fail;
  candidate.measureMs = unsignedValue;
  if (!parseUnsignedArg(server_, "sample_interval_ms", unsignedValue, error)) goto fail;
  candidate.sampleIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "max_measurement_samples", unsignedValue, error)) goto fail;
  candidate.maxMeasurementSamples = static_cast<uint16_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "debug_interval_ms", unsignedValue, error)) goto fail;
  candidate.debugIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "oled_address_1", unsignedValue, error)) goto fail;
  candidate.oledAddress1 = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "oled_address_2", unsignedValue, error)) goto fail;
  candidate.oledAddress2 = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "oled_driver_mode", unsignedValue, error)) goto fail;
  candidate.oledDriverMode = static_cast<uint8_t>(unsignedValue);
  if (!parseUnsignedArg(server_, "display_interval_ms", unsignedValue, error)) goto fail;
  candidate.displayIntervalMs = unsignedValue;
  if (!parseUnsignedArg(server_, "beat_animation_ms", unsignedValue, error)) goto fail;
  candidate.beatAnimationMs = unsignedValue;
  if (!parseUnsignedArg(server_, "beat_animation_frame_ms", unsignedValue, error)) goto fail;
  candidate.beatAnimationFrameMs = unsignedValue;
  if (!parseUnsignedArg(server_, "oled_width", unsignedValue, error)) goto fail;
  candidate.oledWidth = static_cast<uint8_t>(unsignedValue);

  if (!settings_.update(candidate, error)) {
    goto fail;
  }

  server_.send(200, "text/plain", "ok");
  return;

fail:
  server_.send(400, "text/plain", error);
}

void PulseWebServer::handleApiThresholdGet() {
  const AppSettings &config = settings_.current();
  const PulseSnapshot snapshot = monitor_.snapshot();

  String json = "{";
  json += "\"detect_offset\":" + String(config.minCalibratedFingerThreshold) + ",";
  json += "\"release_offset\":" + String(config.minCalibratedFingerReleaseThreshold) + ",";
  json += "\"raw_detect_threshold\":" + String(config.rawFingerThreshold) + ",";
  json += "\"raw_release_threshold\":" + String(config.rawFingerReleaseThreshold) + ",";
  json += "\"active_threshold\":" + String(snapshot.fingerThreshold);
  json += "}";
  server_.send(200, "application/json", json);
}

void PulseWebServer::handleApiThresholdPost() {
  AppSettings candidate = settings_.current();
  String error;
  long signedValue = 0;

  if (!parseSignedArg(server_, "detect_offset", signedValue, error)) {
    server_.send(400, "text/plain", error);
    return;
  }
  candidate.minCalibratedFingerThreshold = signedValue;

  if (!parseSignedArg(server_, "release_offset", signedValue, error)) {
    server_.send(400, "text/plain", error);
    return;
  }
  candidate.minCalibratedFingerReleaseThreshold = signedValue;

  if (server_.hasArg("raw_detect_threshold")) {
    if (!parseSignedArg(server_, "raw_detect_threshold", signedValue, error)) {
      server_.send(400, "text/plain", error);
      return;
    }
    candidate.rawFingerThreshold = signedValue;
  }

  if (server_.hasArg("raw_release_threshold")) {
    if (!parseSignedArg(server_, "raw_release_threshold", signedValue, error)) {
      server_.send(400, "text/plain", error);
      return;
    }
    candidate.rawFingerReleaseThreshold = signedValue;
  }

  if (!settings_.update(candidate, error)) {
    server_.send(400, "text/plain", error);
    return;
  }

  server_.send(200, "text/plain", "ok");
}

void PulseWebServer::handleApiOledGet() {
  const AppSettings &config = settings_.current();
  String json = "{";
  json += "\"driver_mode\":" + String(config.oledDriverMode) + ",";
  json += "\"address_1\":" + String(config.oledAddress1) + ",";
  json += "\"address_2\":" + String(config.oledAddress2);
  json += "}";
  server_.send(200, "application/json", json);
}

void PulseWebServer::handleApiOledPost() {
  AppSettings candidate = settings_.current();
  String error;
  unsigned long unsignedValue = 0;

  if (!parseUnsignedArg(server_, "driver_mode", unsignedValue, error)) {
    server_.send(400, "text/plain", error);
    return;
  }
  candidate.oledDriverMode = static_cast<uint8_t>(unsignedValue);

  if (!settings_.update(candidate, error)) {
    server_.send(400, "text/plain", error);
    return;
  }

  server_.send(200, "text/plain", "ok");
}

void PulseWebServer::handleApiStart() {
  if (!monitor_.startMeasurement(millis())) {
    server_.send(409, "text/plain", "calibration_active");
    return;
  }
  server_.send(200, "text/plain", "ok");
}

void PulseWebServer::handleApiCalibrate() {
  monitor_.startCalibration(millis());
  server_.send(200, "text/plain", "ok");
}

void PulseWebServer::handleApiReference() {
  String value = server_.arg("plain");
  value.trim();

  if (value.isEmpty()) {
    server_.send(400, "text/plain", "reference_missing");
    return;
  }

  if (!monitor_.applyReferenceCalibration(value.toFloat())) {
    server_.send(409, "text/plain", "reference_invalid_or_measurement_missing");
    return;
  }

  server_.send(200, "text/plain", "ok");
}

void PulseWebServer::handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server_.send(404, "text/plain", "index.html nicht gefunden");
    return;
  }

  server_.streamFile(file, "text/html");
  file.close();
}

void PulseWebServer::handleNotFound() {
  server_.send(404, "text/plain", "Not found");
}
