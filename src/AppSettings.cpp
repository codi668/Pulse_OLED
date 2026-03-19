#include "AppSettings.h"

namespace {

bool isAllowedSampleAverage(uint8_t value) {
  return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 || value == 32;
}

bool isAllowedSampleRate(uint16_t value) {
  return value == 50 || value == 100 || value == 200 || value == 400 || value == 800 ||
         value == 1000 || value == 1600 || value == 3200;
}

bool isAllowedPulseWidth(int value) {
  return value == 69 || value == 118 || value == 215 || value == 411;
}

bool isAllowedAdcRange(int value) {
  return value == 2048 || value == 4096 || value == 8192 || value == 16384;
}

}  // namespace

bool AppSettingsStore::begin() {
  settings_ = defaults();
  ready_ = preferences_.begin("pulsecfg", false);
  if (ready_) {
    load();
  }
  return ready_;
}

const AppSettings &AppSettingsStore::current() const {
  return settings_;
}

uint32_t AppSettingsStore::revision() const {
  return revision_;
}

AppSettings AppSettingsStore::defaults() const {
  return AppSettings{};
}

bool AppSettingsStore::update(const AppSettings &candidate, String &error) {
  if (!validate(candidate, error)) {
    return false;
  }

  settings_ = candidate;
  if (ready_) {
    save();
  }
  revision_++;
  return true;
}

bool AppSettingsStore::validate(const AppSettings &candidate, String &error) const {
  if (candidate.rateSize < 1 || candidate.rateSize > kMaxRateSize) {
    error = "rate_size muss zwischen 1 und 16 liegen";
    return false;
  }
  if (candidate.rawFingerReleaseThreshold > candidate.rawFingerThreshold) {
    error = "raw_finger_release_threshold muss <= raw_finger_threshold sein";
    return false;
  }
  if (candidate.minCalibratedFingerReleaseThreshold > candidate.minCalibratedFingerThreshold) {
    error = "min_calibrated_finger_release_threshold muss <= min_calibrated_finger_threshold sein";
    return false;
  }
  if (candidate.fingerDetectOnSamples < 1 || candidate.fingerDetectOnSamples > 50) {
    error = "finger_detect_on_samples muss zwischen 1 und 50 liegen";
    return false;
  }
  if (candidate.fingerDetectOffSamples < 1 || candidate.fingerDetectOffSamples > 100) {
    error = "finger_detect_off_samples muss zwischen 1 und 100 liegen";
    return false;
  }
  if (candidate.calibrationMs < 100 || candidate.calibrationMs > 60000) {
    error = "calibration_ms muss zwischen 100 und 60000 liegen";
    return false;
  }
  if (candidate.calibrationSampleIntervalMs < 1 || candidate.calibrationSampleIntervalMs > 5000) {
    error = "calibration_sample_interval_ms muss zwischen 1 und 5000 liegen";
    return false;
  }
  if (!isAllowedSampleAverage(candidate.sensorSampleAverage)) {
    error = "sensor_sample_average muss 1,2,4,8,16 oder 32 sein";
    return false;
  }
  if (candidate.sensorLedMode < 1 || candidate.sensorLedMode > 3) {
    error = "sensor_led_mode muss zwischen 1 und 3 liegen";
    return false;
  }
  if (!isAllowedSampleRate(candidate.sensorSampleRate)) {
    error = "sensor_sample_rate ist ungueltig";
    return false;
  }
  if (!isAllowedPulseWidth(candidate.sensorPulseWidth)) {
    error = "sensor_pulse_width muss 69, 118, 215 oder 411 sein";
    return false;
  }
  if (!isAllowedAdcRange(candidate.sensorAdcRange)) {
    error = "sensor_adc_range muss 2048, 4096, 8192 oder 16384 sein";
    return false;
  }
  if (candidate.sensorSampleIntervalMs < 1 || candidate.sensorSampleIntervalMs > 1000) {
    error = "sensor_sample_interval_ms muss zwischen 1 und 1000 liegen";
    return false;
  }
  if (candidate.minBeatIntervalMs < 100 || candidate.minBeatIntervalMs >= candidate.maxBeatIntervalMs) {
    error = "min_beat_interval_ms muss >=100 und < max_beat_interval_ms sein";
    return false;
  }
  if (candidate.maxBeatIntervalMs > 10000) {
    error = "max_beat_interval_ms darf hoechstens 10000 sein";
    return false;
  }
  if (candidate.beatSensitivity < 1 || candidate.beatSensitivity > 10) {
    error = "beat_sensitivity muss zwischen 1 und 10 liegen";
    return false;
  }
  if (candidate.measureMs < 1000 || candidate.measureMs > 120000) {
    error = "measure_ms muss zwischen 1000 und 120000 liegen";
    return false;
  }
  if (candidate.sampleIntervalMs < 10 || candidate.sampleIntervalMs > 5000) {
    error = "sample_interval_ms muss zwischen 10 und 5000 liegen";
    return false;
  }
  if (candidate.maxMeasurementSamples < 1 ||
      candidate.maxMeasurementSamples > kMaxMeasurementSamplesLimit) {
    error = "max_measurement_samples muss zwischen 1 und 512 liegen";
    return false;
  }
  if (candidate.debugIntervalMs < 10 || candidate.debugIntervalMs > 10000) {
    error = "debug_interval_ms muss zwischen 10 und 10000 liegen";
    return false;
  }
  if (candidate.oledAddress1 == 0 || candidate.oledAddress1 > 0x7F ||
      candidate.oledAddress2 == 0 || candidate.oledAddress2 > 0x7F) {
    error = "oled addresses muessen zwischen 1 und 127 liegen";
    return false;
  }
  if (candidate.oledDriverMode > 3) {
    error = "oled_driver_mode muss zwischen 0 und 3 liegen";
    return false;
  }
  if (candidate.displayIntervalMs < 10 || candidate.displayIntervalMs > 5000) {
    error = "display_interval_ms muss zwischen 10 und 5000 liegen";
    return false;
  }
  if (candidate.beatAnimationMs < 10 || candidate.beatAnimationMs > 5000) {
    error = "beat_animation_ms muss zwischen 10 und 5000 liegen";
    return false;
  }
  if (candidate.beatAnimationFrameMs < 10 || candidate.beatAnimationFrameMs > 5000) {
    error = "beat_animation_frame_ms muss zwischen 10 und 5000 liegen";
    return false;
  }
  if (candidate.oledWidth < 32 || candidate.oledWidth > 255) {
    error = "oled_width muss zwischen 32 und 255 liegen";
    return false;
  }

  return true;
}

void AppSettingsStore::load() {
  settings_.rateSize = preferences_.getUChar("rate_size", settings_.rateSize);
  settings_.rawFingerThreshold =
      preferences_.getLong("raw_finger_threshold", settings_.rawFingerThreshold);
  settings_.rawFingerReleaseThreshold =
      preferences_.getLong("raw_finger_release_threshold", settings_.rawFingerReleaseThreshold);
  settings_.minCalibratedFingerThreshold =
      preferences_.getLong("min_cal_finger_threshold", settings_.minCalibratedFingerThreshold);
  settings_.minCalibratedFingerReleaseThreshold = preferences_.getLong(
      "min_cal_finger_release_threshold", settings_.minCalibratedFingerReleaseThreshold);
  settings_.fingerDetectOnSamples =
      preferences_.getUChar("finger_detect_on_samples", settings_.fingerDetectOnSamples);
  settings_.fingerDetectOffSamples =
      preferences_.getUChar("finger_detect_off_samples", settings_.fingerDetectOffSamples);
  settings_.calibrationMs = preferences_.getULong("calibration_ms", settings_.calibrationMs);
  settings_.calibrationSampleIntervalMs = preferences_.getULong(
      "calibration_sample_interval_ms", settings_.calibrationSampleIntervalMs);
  settings_.sensorLedBrightness =
      preferences_.getUChar("sensor_led_brightness", settings_.sensorLedBrightness);
  settings_.sensorSampleAverage =
      preferences_.getUChar("sensor_sample_average", settings_.sensorSampleAverage);
  settings_.sensorLedMode = preferences_.getUChar("sensor_led_mode", settings_.sensorLedMode);
  settings_.sensorSampleRate =
      preferences_.getUShort("sensor_sample_rate", settings_.sensorSampleRate);
  settings_.sensorPulseWidth =
      preferences_.getInt("sensor_pulse_width", settings_.sensorPulseWidth);
  settings_.sensorAdcRange = preferences_.getInt("sensor_adc_range", settings_.sensorAdcRange);
  settings_.sensorSampleIntervalMs = preferences_.getULong(
      "sensor_sample_interval_ms", settings_.sensorSampleIntervalMs);
  settings_.minBeatIntervalMs =
      preferences_.getULong("min_beat_interval_ms", settings_.minBeatIntervalMs);
  settings_.maxBeatIntervalMs =
      preferences_.getULong("max_beat_interval_ms", settings_.maxBeatIntervalMs);
  settings_.beatSensitivity =
      preferences_.getUChar("beat_sensitivity", settings_.beatSensitivity);
  settings_.measureMs = preferences_.getULong("measure_ms", settings_.measureMs);
  settings_.sampleIntervalMs = preferences_.getULong("sample_interval_ms", settings_.sampleIntervalMs);
  settings_.maxMeasurementSamples =
      preferences_.getUShort("max_measurement_samples", settings_.maxMeasurementSamples);
  settings_.debugIntervalMs =
      preferences_.getULong("debug_interval_ms", settings_.debugIntervalMs);
  settings_.oledAddress1 = preferences_.getUChar("oled_address_1", settings_.oledAddress1);
  settings_.oledAddress2 = preferences_.getUChar("oled_address_2", settings_.oledAddress2);
  settings_.oledDriverMode =
      preferences_.getUChar("oled_driver_mode", settings_.oledDriverMode);
  settings_.displayIntervalMs =
      preferences_.getULong("display_interval_ms", settings_.displayIntervalMs);
  settings_.beatAnimationMs =
      preferences_.getULong("beat_animation_ms", settings_.beatAnimationMs);
  settings_.beatAnimationFrameMs =
      preferences_.getULong("beat_animation_frame_ms", settings_.beatAnimationFrameMs);
  settings_.oledWidth = preferences_.getUChar("oled_width", settings_.oledWidth);

  bool migratedLegacySensorTiming = false;
  if (settings_.sensorSampleRate == 100 && settings_.sensorSampleIntervalMs == 10) {
    settings_.sensorSampleRate = AppConfig::kSensorSampleRate;
    settings_.sensorSampleIntervalMs = AppConfig::kSensorSampleIntervalMs;
    migratedLegacySensorTiming = true;
  }

  String error;
  if (!validate(settings_, error)) {
    settings_ = defaults();
    if (ready_) {
      save();
    }
    return;
  }

  if (migratedLegacySensorTiming && ready_) {
    save();
  }
}

void AppSettingsStore::save() {
  preferences_.putUChar("rate_size", settings_.rateSize);
  preferences_.putLong("raw_finger_threshold", settings_.rawFingerThreshold);
  preferences_.putLong("raw_finger_release_threshold", settings_.rawFingerReleaseThreshold);
  preferences_.putLong("min_cal_finger_threshold", settings_.minCalibratedFingerThreshold);
  preferences_.putLong("min_cal_finger_release_threshold",
                       settings_.minCalibratedFingerReleaseThreshold);
  preferences_.putUChar("finger_detect_on_samples", settings_.fingerDetectOnSamples);
  preferences_.putUChar("finger_detect_off_samples", settings_.fingerDetectOffSamples);
  preferences_.putULong("calibration_ms", settings_.calibrationMs);
  preferences_.putULong("calibration_sample_interval_ms", settings_.calibrationSampleIntervalMs);
  preferences_.putUChar("sensor_led_brightness", settings_.sensorLedBrightness);
  preferences_.putUChar("sensor_sample_average", settings_.sensorSampleAverage);
  preferences_.putUChar("sensor_led_mode", settings_.sensorLedMode);
  preferences_.putUShort("sensor_sample_rate", settings_.sensorSampleRate);
  preferences_.putInt("sensor_pulse_width", settings_.sensorPulseWidth);
  preferences_.putInt("sensor_adc_range", settings_.sensorAdcRange);
  preferences_.putULong("sensor_sample_interval_ms", settings_.sensorSampleIntervalMs);
  preferences_.putULong("min_beat_interval_ms", settings_.minBeatIntervalMs);
  preferences_.putULong("max_beat_interval_ms", settings_.maxBeatIntervalMs);
  preferences_.putUChar("beat_sensitivity", settings_.beatSensitivity);
  preferences_.putULong("measure_ms", settings_.measureMs);
  preferences_.putULong("sample_interval_ms", settings_.sampleIntervalMs);
  preferences_.putUShort("max_measurement_samples", settings_.maxMeasurementSamples);
  preferences_.putULong("debug_interval_ms", settings_.debugIntervalMs);
  preferences_.putUChar("oled_address_1", settings_.oledAddress1);
  preferences_.putUChar("oled_address_2", settings_.oledAddress2);
  preferences_.putUChar("oled_driver_mode", settings_.oledDriverMode);
  preferences_.putULong("display_interval_ms", settings_.displayIntervalMs);
  preferences_.putULong("beat_animation_ms", settings_.beatAnimationMs);
  preferences_.putULong("beat_animation_frame_ms", settings_.beatAnimationFrameMs);
  preferences_.putUChar("oled_width", settings_.oledWidth);
}
