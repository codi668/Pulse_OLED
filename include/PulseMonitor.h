#pragma once

#include <Preferences.h>

#include "AppSettings.h"
#include "MAX30105.h"
#include "PulseSnapshot.h"

class PulseMonitor {
 public:
  explicit PulseMonitor(AppSettingsStore &settings);

  bool begin(TwoWire &wire);
  void update(unsigned long nowMs);
  bool startMeasurement(unsigned long nowMs);
  void startCalibration(unsigned long nowMs);
  bool applyReferenceCalibration(float referenceBpm);

  PulseSnapshot snapshot() const;
  float instantBpm() const;
  bool consumeBeatEvent();

 private:
  void processSensorSample(long rawIr, unsigned long sampleMs);
  void processBeatSample(long rawIr, unsigned long sampleMs);
  void registerBeat(unsigned long sampleMs);
  void collectMeasurementSample(unsigned long sampleMs);
  void applySensorConfig();
  void reloadConfigIfNeeded();
  long calibratedIr(long rawIr) const;
  long activeFingerThreshold() const;
  long activeFingerReleaseThreshold() const;
  void updateFingerPresence(long rawIr);
  float correctedBpm(float rawBpm) const;
  void resetMeasurement();
  void resetRates();
  void resetBeatDetectionState();
  void finishCalibration();
  void finishMeasurement();

  AppSettingsStore &settings_;
  MAX30105 sensor_;
  uint16_t rates_[AppSettingsStore::kMaxRateSize] = {};
  byte rateSpot_ = 0;
  byte ratesFilled_ = 0;
  unsigned long lastBeatMs_ = 0;
  float bpm_ = 0.0f;
  float samples_[AppSettingsStore::kMaxMeasurementSamplesLimit] = {};
  PulseSnapshot snapshot_;
  bool beatEventPending_ = false;
  unsigned long lastSampleMs_ = 0;
  unsigned long calibrationLastSampleMs_ = 0;
  uint32_t calibrationSum_ = 0;
  uint16_t calibrationSamples_ = 0;
  uint8_t fingerOnSamples_ = 0;
  uint8_t fingerOffSamples_ = 0;
  long ambientIr_ = 0;
  bool ambientIrReady_ = false;
  uint32_t appliedSettingsRevision_ = 0;
  float correctionFactor_ = 1.0f;
  float referenceBpm_ = 0.0f;
  float referenceSourceAvg_ = 0.0f;
  Preferences preferences_;
  bool preferencesReady_ = false;
};
