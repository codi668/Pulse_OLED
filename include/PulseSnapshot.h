#pragma once

#include <Arduino.h>

struct PulseSnapshot {
  float instantBpm = 0.0f;
  float avgBpm = 0.0f;
  long rawIr = 0;
  long calibratedIr = 0;
  long calibrationBaseline = 0;
  long fingerThreshold = 0;
  float rawFinalAvg = 0.0f;
  float correctionFactor = 1.0f;
  float referenceBpm = 0.0f;
  float referenceSourceAvg = 0.0f;
  bool fingerPresent = false;
  bool calibrationActive = false;
  bool calibrationDone = false;
  bool referenceCalibrated = false;
  unsigned long calibrationStartMs = 0;
  bool measurementActive = false;
  bool measurementDone = false;
  unsigned long measurementStartMs = 0;
  unsigned long lastBeatMs = 0;
  uint16_t sampleCount = 0;
  float finalAvg = 0.0f;
};
