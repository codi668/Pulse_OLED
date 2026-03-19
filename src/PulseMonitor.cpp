#include "PulseMonitor.h"

#include <math.h>

#include "AppConfig.h"
#include "heartRate.h"

extern int16_t IR_AC_Max;
extern int16_t IR_AC_Min;
extern int16_t IR_AC_Signal_Current;
extern int16_t IR_AC_Signal_Previous;
extern int16_t IR_AC_Signal_min;
extern int16_t IR_AC_Signal_max;
extern int16_t IR_Average_Estimated;
extern int16_t positiveEdge;
extern int16_t negativeEdge;
extern int32_t ir_avg_reg;
extern int16_t cbuf[32];
extern uint8_t offset;

namespace {

void resetHeartRateFilterState() {
  IR_AC_Max = 20;
  IR_AC_Min = -20;
  IR_AC_Signal_Current = 0;
  IR_AC_Signal_Previous = 0;
  IR_AC_Signal_min = 0;
  IR_AC_Signal_max = 0;
  IR_Average_Estimated = 0;
  positiveEdge = 0;
  negativeEdge = 0;
  ir_avg_reg = 0;
  offset = 0;

  for (int i = 0; i < 32; i++) {
    cbuf[i] = 0;
  }
}

float beatToleranceFraction(uint8_t sensitivity) {
  const uint8_t clamped = sensitivity < 1 ? 1 : (sensitivity > 10 ? 10 : sensitivity);
  return 0.20f + (static_cast<float>(clamped - 1) * (0.30f / 9.0f));
}

}  // namespace

PulseMonitor::PulseMonitor(AppSettingsStore &settings) : settings_(settings) {}

bool PulseMonitor::begin(TwoWire &wire) {
  wire_ = &wire;
  if (!sensor_.begin(wire, I2C_SPEED_FAST)) {
    return false;
  }

  applySensorConfig();

  preferencesReady_ = preferences_.begin("pulse", false);
  if (preferencesReady_) {
    const float storedFactor = preferences_.getFloat("corr", 1.0f);
    const float storedReference = preferences_.getFloat("ref", 0.0f);
    const float storedSource = preferences_.getFloat("refsrc", 0.0f);

    correctionFactor_ = (storedFactor >= 0.5f && storedFactor <= 2.0f) ? storedFactor : 1.0f;
    referenceBpm_ = storedReference > 0.0f ? storedReference : 0.0f;
    referenceSourceAvg_ = storedSource > 0.0f ? storedSource : 0.0f;
  }

  return true;
}

void PulseMonitor::update(unsigned long nowMs) {
  reloadConfigIfNeeded();
  snapshot_.fingerThreshold = activeFingerThreshold();

  const AppSettings &config = settings_.current();
  if (wire_ != nullptr) {
    wire_->setClock(I2C_SPEED_FAST);
  }

  const uint16_t newSamples = sensor_.check();
  if (newSamples > 0) {
    const unsigned long sampleIntervalMs =
        config.sensorSampleIntervalMs > 0 ? config.sensorSampleIntervalMs : 1;
    unsigned long sampleMs = 0;

    if (lastSensorSampleMs_ > 0) {
      sampleMs = lastSensorSampleMs_ + sampleIntervalMs;
    }

    unsigned long backlogStartMs = nowMs;
    if (newSamples > 1) {
      const unsigned long backlogSpan = sampleIntervalMs * static_cast<unsigned long>(newSamples - 1);
      backlogStartMs = backlogSpan < nowMs ? nowMs - backlogSpan : 0;
    }

    if (sampleMs == 0 || sampleMs < backlogStartMs) {
      sampleMs = backlogStartMs;
    }

    while (sensor_.available()) {
      const long rawIr = static_cast<long>(sensor_.getFIFOIR());
      sensor_.nextSample();

      if (rawIr > 0) {
        processSensorSample(rawIr, sampleMs);
        lastSensorSampleMs_ = sampleMs;
      }

      sampleMs += sampleIntervalMs;
    }
  }

  if (snapshot_.calibrationActive && nowMs - snapshot_.calibrationStartMs >= config.calibrationMs) {
    finishCalibration();
  }

  if (snapshot_.measurementActive && nowMs - snapshot_.measurementStartMs >= config.measureMs) {
    finishMeasurement();
  }
}

bool PulseMonitor::startMeasurement(unsigned long nowMs) {
  if (snapshot_.calibrationActive) {
    return false;
  }

  snapshot_.measurementActive = true;
  snapshot_.measurementDone = false;
  snapshot_.measurementStartMs = nowMs;
  snapshot_.sampleCount = 0;
  snapshot_.finalAvg = 0.0f;
  lastSampleMs_ = 0;
  return true;
}

void PulseMonitor::startCalibration(unsigned long nowMs) {
  resetMeasurement();
  snapshot_.calibrationActive = true;
  snapshot_.calibrationDone = false;
  snapshot_.calibrationStartMs = nowMs;
  snapshot_.calibratedIr = 0;
  snapshot_.fingerPresent = false;
  snapshot_.avgBpm = 0.0f;
  snapshot_.instantBpm = 0.0f;
  snapshot_.lastBeatMs = 0;
  calibrationLastSampleMs_ = 0;
  lastSensorSampleMs_ = 0;
  calibrationSum_ = 0;
  calibrationSamples_ = 0;
  fingerOnSamples_ = 0;
  fingerOffSamples_ = 0;
  bpm_ = 0.0f;
  beatEventPending_ = false;
  resetRates();
  resetBeatDetectionState();
}

bool PulseMonitor::applyReferenceCalibration(float referenceBpm) {
  if (snapshot_.measurementActive || snapshot_.finalAvg <= 0.0f) {
    return false;
  }

  if (referenceBpm < 30.0f || referenceBpm > 220.0f) {
    return false;
  }

  const float factor = referenceBpm / snapshot_.finalAvg;
  if (factor < 0.5f || factor > 2.0f) {
    return false;
  }

  correctionFactor_ = factor;
  referenceBpm_ = referenceBpm;
  referenceSourceAvg_ = snapshot_.finalAvg;

  if (preferencesReady_) {
    preferences_.putFloat("corr", correctionFactor_);
    preferences_.putFloat("ref", referenceBpm_);
    preferences_.putFloat("refsrc", referenceSourceAvg_);
  }

  return true;
}

PulseSnapshot PulseMonitor::snapshot() const {
  PulseSnapshot current = snapshot_;
  current.instantBpm = correctedBpm(bpm_);
  current.rawFinalAvg = snapshot_.finalAvg;
  current.avgBpm = correctedBpm(snapshot_.avgBpm);
  current.finalAvg = correctedBpm(snapshot_.finalAvg);
  current.correctionFactor = correctionFactor_;
  current.referenceBpm = referenceBpm_;
  current.referenceSourceAvg = referenceSourceAvg_;
  current.referenceCalibrated = referenceBpm_ > 0.0f;
  return current;
}

float PulseMonitor::instantBpm() const {
  return correctedBpm(bpm_);
}

bool PulseMonitor::consumeBeatEvent() {
  const bool beatDetected = beatEventPending_;
  beatEventPending_ = false;
  return beatDetected;
}

void PulseMonitor::processSensorSample(long rawIr, unsigned long sampleMs) {
  const AppSettings &config = settings_.current();
  snapshot_.rawIr = rawIr;

  if (snapshot_.calibrationActive &&
      sampleMs - snapshot_.calibrationStartMs >= config.calibrationMs) {
    finishCalibration();
  }

  snapshot_.fingerThreshold = activeFingerThreshold();

  if (snapshot_.calibrationActive) {
    if (calibrationSamples_ == 0 ||
        sampleMs - calibrationLastSampleMs_ >= config.calibrationSampleIntervalMs) {
      calibrationLastSampleMs_ = sampleMs;
      calibrationSum_ += static_cast<uint32_t>(rawIr);
      calibrationSamples_++;
    }

    snapshot_.avgBpm = 0.0f;
    snapshot_.instantBpm = 0.0f;
    snapshot_.lastBeatMs = 0;
    snapshot_.fingerPresent = false;
    snapshot_.calibratedIr = 0;
    bpm_ = 0.0f;
    beatEventPending_ = false;
    return;
  }

  snapshot_.calibratedIr = calibratedIr(rawIr);

  if (!ambientIrReady_) {
    ambientIr_ = rawIr;
    ambientIrReady_ = true;
  } else if (rawIr <= activeFingerThreshold()) {
    ambientIr_ = (ambientIr_ * 15 + rawIr) / 16;
  }

  const bool fingerWasPresent = snapshot_.fingerPresent;
  updateFingerPresence(rawIr);

  if (!fingerWasPresent && snapshot_.fingerPresent) {
    resetRates();
    resetBeatDetectionState();
    snapshot_.avgBpm = 0.0f;
    snapshot_.lastBeatMs = 0;
    bpm_ = 0.0f;
  }

  if (snapshot_.fingerPresent) {
    processBeatSample(rawIr, sampleMs);
  } else {
    snapshot_.avgBpm = 0.0f;
    snapshot_.lastBeatMs = 0;
    bpm_ = 0.0f;
    beatEventPending_ = false;
    if (fingerWasPresent) {
      resetRates();
      resetBeatDetectionState();
    }
  }

  if (!snapshot_.measurementActive) {
    return;
  }

  if (sampleMs - snapshot_.measurementStartMs >= config.measureMs) {
    finishMeasurement();
    return;
  }

  collectMeasurementSample(sampleMs);
}

void PulseMonitor::processBeatSample(long rawIr, unsigned long sampleMs) {
  if (rawIr <= 0) {
    return;
  }

  if (checkForBeat(rawIr)) {
    registerBeat(sampleMs);
  }
}

void PulseMonitor::registerBeat(unsigned long sampleMs) {
  const AppSettings &config = settings_.current();
  if (lastBeatMs_ == 0) {
    lastBeatMs_ = sampleMs;
    return;
  }

  const unsigned long delta = sampleMs - lastBeatMs_;
  lastBeatMs_ = sampleMs;

  if (delta <= config.minBeatIntervalMs || delta >= config.maxBeatIntervalMs) {
    return;
  }

  const float beatsPerMinute = 60000.0f / delta;
  if (beatsPerMinute < 30.0f || beatsPerMinute > 200.0f) {
    return;
  }

  const float tolerance = beatToleranceFraction(config.beatSensitivity);
  if (snapshot_.avgBpm > 0.0f &&
      (beatsPerMinute > snapshot_.avgBpm * (1.0f + tolerance) ||
       beatsPerMinute < snapshot_.avgBpm * (1.0f - tolerance))) {
    return;
  }

  bpm_ = beatsPerMinute;

  const uint8_t rateSize = config.rateSize > 0 ? config.rateSize : 1;
  rates_[rateSpot_] = static_cast<uint16_t>(beatsPerMinute);
  rateSpot_ = (rateSpot_ + 1) % rateSize;
  if (ratesFilled_ < rateSize) {
    ratesFilled_++;
  }

  float bpmSum = 0.0f;
  for (byte i = 0; i < ratesFilled_; i++) {
    bpmSum += rates_[i];
  }
  snapshot_.avgBpm = ratesFilled_ > 0 ? bpmSum / ratesFilled_ : 0.0f;
  snapshot_.lastBeatMs = sampleMs;
  beatEventPending_ = true;
}

void PulseMonitor::collectMeasurementSample(unsigned long sampleMs) {
  const AppSettings &config = settings_.current();
  if (snapshot_.sampleCount >= config.maxMeasurementSamples ||
      snapshot_.sampleCount >= AppSettingsStore::kMaxMeasurementSamplesLimit) {
    return;
  }

  if (snapshot_.sampleCount > 0 &&
      sampleMs - lastSampleMs_ < config.sampleIntervalMs) {
    return;
  }

  lastSampleMs_ = sampleMs;
  samples_[snapshot_.sampleCount] =
      (snapshot_.fingerPresent && snapshot_.avgBpm > 0.0f) ? snapshot_.avgBpm : 0.0f;
  snapshot_.sampleCount++;
}

void PulseMonitor::applySensorConfig() {
  const AppSettings &config = settings_.current();
  sensor_.setup(config.sensorLedBrightness, config.sensorSampleAverage, config.sensorLedMode,
                config.sensorSampleRate, config.sensorPulseWidth, config.sensorAdcRange);
  sensor_.setPulseAmplitudeRed(0x0A);
  sensor_.setPulseAmplitudeIR(config.sensorLedBrightness);
  sensor_.setPulseAmplitudeGreen(0x00);
  sensor_.clearFIFO();

  ambientIr_ = 0;
  ambientIrReady_ = false;
  fingerOnSamples_ = 0;
  fingerOffSamples_ = 0;
  snapshot_.fingerThreshold = activeFingerThreshold();
  snapshot_.fingerPresent = false;
  snapshot_.avgBpm = 0.0f;
  snapshot_.instantBpm = 0.0f;
  snapshot_.lastBeatMs = 0;
  lastSensorSampleMs_ = 0;
  bpm_ = 0.0f;
  beatEventPending_ = false;
  resetMeasurement();
  resetRates();
  resetBeatDetectionState();
  appliedSettingsRevision_ = settings_.revision();
}

void PulseMonitor::reloadConfigIfNeeded() {
  if (appliedSettingsRevision_ != settings_.revision()) {
    applySensorConfig();
  }
}

long PulseMonitor::calibratedIr(long rawIr) const {
  if (rawIr <= snapshot_.calibrationBaseline) {
    return 0;
  }
  return rawIr - snapshot_.calibrationBaseline;
}

long PulseMonitor::activeFingerThreshold() const {
  const AppSettings &config = settings_.current();
  const long baseline =
      ambientIrReady_ ? ambientIr_ : (snapshot_.calibrationDone ? snapshot_.calibrationBaseline : 0);
  if (baseline > 0) {
    return baseline + config.minCalibratedFingerThreshold;
  }
  return config.rawFingerThreshold;
}

long PulseMonitor::activeFingerReleaseThreshold() const {
  const AppSettings &config = settings_.current();
  const long baseline =
      ambientIrReady_ ? ambientIr_ : (snapshot_.calibrationDone ? snapshot_.calibrationBaseline : 0);
  if (baseline > 0) {
    return baseline + config.minCalibratedFingerReleaseThreshold;
  }
  return config.rawFingerReleaseThreshold;
}

void PulseMonitor::updateFingerPresence(long rawIr) {
  const AppSettings &config = settings_.current();
  const long detectThreshold = activeFingerThreshold();
  const long releaseThreshold = activeFingerReleaseThreshold();

  if (snapshot_.fingerPresent) {
    snapshot_.fingerThreshold = releaseThreshold;

    if (rawIr > releaseThreshold) {
      fingerOffSamples_ = 0;
      return;
    }

    if (fingerOffSamples_ < config.fingerDetectOffSamples) {
      fingerOffSamples_++;
    }
    if (fingerOffSamples_ >= config.fingerDetectOffSamples) {
      snapshot_.fingerPresent = false;
      fingerOnSamples_ = 0;
      fingerOffSamples_ = 0;
    }
    return;
  }

  snapshot_.fingerThreshold = detectThreshold;

  if (rawIr <= detectThreshold) {
    fingerOnSamples_ = 0;
    return;
  }

  if (fingerOnSamples_ < config.fingerDetectOnSamples) {
    fingerOnSamples_++;
  }
  if (fingerOnSamples_ >= config.fingerDetectOnSamples) {
    snapshot_.fingerPresent = true;
    snapshot_.fingerThreshold = releaseThreshold;
    fingerOnSamples_ = 0;
    fingerOffSamples_ = 0;
  }
}

float PulseMonitor::correctedBpm(float rawBpm) const {
  if (rawBpm <= 0.0f) {
    return 0.0f;
  }
  return rawBpm * correctionFactor_;
}

void PulseMonitor::resetMeasurement() {
  snapshot_.measurementActive = false;
  snapshot_.measurementDone = false;
  snapshot_.measurementStartMs = 0;
  snapshot_.sampleCount = 0;
  snapshot_.finalAvg = 0.0f;
  lastSampleMs_ = 0;
}

void PulseMonitor::resetRates() {
  ratesFilled_ = 0;
  rateSpot_ = 0;
}

void PulseMonitor::resetBeatDetectionState() {
  lastBeatMs_ = 0;
  resetHeartRateFilterState();
}

void PulseMonitor::finishCalibration() {
  const AppSettings &config = settings_.current();
  const long baseline =
      calibrationSamples_ > 0 ? static_cast<long>(calibrationSum_ / calibrationSamples_) : 0;
  const bool invalidBaseline =
      baseline > 0 && baseline > static_cast<long>(config.rawFingerThreshold) * 4L;

  snapshot_.calibrationActive = false;
  snapshot_.calibrationDone = calibrationSamples_ > 0 && !invalidBaseline;
  snapshot_.calibrationStartMs = 0;
  snapshot_.calibrationBaseline = invalidBaseline ? 0 : baseline;
  if (snapshot_.calibrationBaseline > 0) {
    ambientIr_ = snapshot_.calibrationBaseline;
    ambientIrReady_ = true;
  } else {
    ambientIr_ = 0;
    ambientIrReady_ = false;
  }
  snapshot_.fingerThreshold = activeFingerThreshold();
  snapshot_.calibratedIr = calibratedIr(snapshot_.rawIr);
  snapshot_.fingerPresent = false;
  snapshot_.avgBpm = 0.0f;
  snapshot_.instantBpm = 0.0f;
  snapshot_.lastBeatMs = 0;
  bpm_ = 0.0f;
  beatEventPending_ = false;
  calibrationLastSampleMs_ = 0;
  lastSensorSampleMs_ = 0;
  calibrationSum_ = 0;
  calibrationSamples_ = 0;
  fingerOnSamples_ = 0;
  fingerOffSamples_ = 0;
  resetRates();
  resetBeatDetectionState();
}

void PulseMonitor::finishMeasurement() {
  snapshot_.measurementActive = false;
  snapshot_.measurementDone = true;

  float sum = 0.0f;
  uint16_t validCount = 0;
  for (uint16_t i = 0; i < snapshot_.sampleCount; i++) {
    if (samples_[i] > 0.0f) {
      sum += samples_[i];
      validCount++;
    }
  }

  snapshot_.finalAvg = validCount > 0 ? sum / validCount : 0.0f;
  snapshot_.measurementStartMs = 0;
  lastSampleMs_ = 0;
}
