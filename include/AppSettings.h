#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "AppConfig.h"

struct AppSettings {
  uint8_t rateSize = AppConfig::kRateSize;
  long rawFingerThreshold = AppConfig::kRawFingerThreshold;
  long rawFingerReleaseThreshold = AppConfig::kRawFingerReleaseThreshold;
  long minCalibratedFingerThreshold = AppConfig::kMinCalibratedFingerThreshold;
  long minCalibratedFingerReleaseThreshold = AppConfig::kMinCalibratedFingerReleaseThreshold;
  uint8_t fingerDetectOnSamples = AppConfig::kFingerDetectOnSamples;
  uint8_t fingerDetectOffSamples = AppConfig::kFingerDetectOffSamples;
  unsigned long calibrationMs = AppConfig::kCalibrationMs;
  unsigned long calibrationSampleIntervalMs = AppConfig::kCalibrationSampleIntervalMs;
  uint8_t sensorLedBrightness = AppConfig::kSensorLedBrightness;
  uint8_t sensorSampleAverage = AppConfig::kSensorSampleAverage;
  uint8_t sensorLedMode = AppConfig::kSensorLedMode;
  uint16_t sensorSampleRate = AppConfig::kSensorSampleRate;
  int sensorPulseWidth = AppConfig::kSensorPulseWidth;
  int sensorAdcRange = AppConfig::kSensorAdcRange;
  unsigned long sensorSampleIntervalMs = AppConfig::kSensorSampleIntervalMs;
  unsigned long minBeatIntervalMs = AppConfig::kMinBeatIntervalMs;
  unsigned long maxBeatIntervalMs = AppConfig::kMaxBeatIntervalMs;
  unsigned long measureMs = AppConfig::kMeasureMs;
  unsigned long sampleIntervalMs = AppConfig::kSampleIntervalMs;
  uint16_t maxMeasurementSamples = AppConfig::kMaxMeasurementSamples;
  unsigned long debugIntervalMs = AppConfig::kDebugIntervalMs;
  uint8_t oledAddress1 = AppConfig::kOledAddresses[0];
  uint8_t oledAddress2 = AppConfig::kOledAddresses[1];
  uint8_t oledDriverMode = AppConfig::kOledDriverMode;
  unsigned long displayIntervalMs = AppConfig::kDisplayIntervalMs;
  unsigned long beatAnimationMs = AppConfig::kBeatAnimationMs;
  unsigned long beatAnimationFrameMs = AppConfig::kBeatAnimationFrameMs;
  uint8_t oledWidth = AppConfig::kOledWidth;
};

class AppSettingsStore {
 public:
  static constexpr uint8_t kMaxRateSize = 16;
  static constexpr uint16_t kMaxMeasurementSamplesLimit = 512;

  bool begin();
  const AppSettings &current() const;
  uint32_t revision() const;
  AppSettings defaults() const;
  bool update(const AppSettings &candidate, String &error);

 private:
  bool validate(const AppSettings &candidate, String &error) const;
  void load();
  void save();

  AppSettings settings_;
  Preferences preferences_;
  bool ready_ = false;
  uint32_t revision_ = 1;
};
