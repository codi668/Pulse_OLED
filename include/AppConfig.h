#pragma once

#include <Arduino.h>

namespace AppConfig {

static const char kWifiSsid[] = "speeed";
static const char kWifiPass[] = "The_Integtal_is_Real";

static const byte kRateSize = 5;
static const long kRawFingerThreshold = 1800;
static const long kRawFingerReleaseThreshold = 1100;
static const long kMinCalibratedFingerThreshold = 1200;
static const long kMinCalibratedFingerReleaseThreshold = 500;
static const uint8_t kFingerDetectOnSamples = 3;
static const uint8_t kFingerDetectOffSamples = 12;
static const unsigned long kCalibrationMs = 3000;
static const unsigned long kCalibrationSampleIntervalMs = 50;

static const uint8_t kSensorLedBrightness = 0x24;
static const uint8_t kSensorSampleAverage = 4;
static const uint8_t kSensorLedMode = 2;
static const uint16_t kSensorSampleRate = 400;
static const int kSensorPulseWidth = 411;
static const int kSensorAdcRange = 4096;
static const unsigned long kSensorSampleIntervalMs = 2;
static const unsigned long kMinBeatIntervalMs = 280;
static const unsigned long kMaxBeatIntervalMs = 2000;

static const unsigned long kMeasureMs = 10000;
static const unsigned long kSampleIntervalMs = 200;
static const uint16_t kMaxMeasurementSamples = 128;
static const unsigned long kDebugIntervalMs = 250;

static const uint8_t kOledAddresses[] = {0x3C, 0x3D};
static const uint8_t kOledDriverMode = 2;
static const unsigned long kDisplayIntervalMs = 250;
static const unsigned long kBeatAnimationMs = 320;
static const unsigned long kBeatAnimationFrameMs = 55;
static const uint8_t kOledWidth = 128;

}  // namespace AppConfig
