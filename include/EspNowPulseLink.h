#pragma once

#include <Arduino.h>

#include "PulseSnapshot.h"

namespace EspNowPulseLink {

static constexpr uint32_t kMagic = 0x53554C50UL;  // "PULS"
static constexpr uint8_t kVersion = 1;

enum PacketFlags : uint8_t {
  kFlagFingerPresent = 1 << 0,
  kFlagCalibrationActive = 1 << 1,
  kFlagCalibrationDone = 1 << 2,
  kFlagReferenceCalibrated = 1 << 3,
  kFlagMeasurementActive = 1 << 4,
  kFlagMeasurementDone = 1 << 5,
  kFlagBeatEvent = 1 << 6,
};

struct __attribute__((packed)) Packet {
  uint32_t magic = kMagic;
  uint8_t version = kVersion;
  uint8_t flags = 0;
  uint16_t sequence = 0;
  uint32_t senderUptimeMs = 0;
  uint32_t calibrationElapsedMs = 0;
  uint32_t measurementElapsedMs = 0;
  float instantBpm = 0.0f;
  float avgBpm = 0.0f;
  float rawFinalAvg = 0.0f;
  float finalAvg = 0.0f;
  long rawIr = 0;
  long calibratedIr = 0;
  long calibrationBaseline = 0;
  long fingerThreshold = 0;
  float correctionFactor = 1.0f;
  float referenceBpm = 0.0f;
  float referenceSourceAvg = 0.0f;
  uint16_t sampleCount = 0;
};

static_assert(sizeof(Packet) <= 250, "ESP-NOW payload is too large");

inline Packet makePacket(const PulseSnapshot &snapshot, uint16_t sequence, uint32_t nowMs,
                         bool beatEvent) {
  Packet packet;
  packet.flags = 0;
  if (snapshot.fingerPresent) packet.flags |= kFlagFingerPresent;
  if (snapshot.calibrationActive) packet.flags |= kFlagCalibrationActive;
  if (snapshot.calibrationDone) packet.flags |= kFlagCalibrationDone;
  if (snapshot.referenceCalibrated) packet.flags |= kFlagReferenceCalibrated;
  if (snapshot.measurementActive) packet.flags |= kFlagMeasurementActive;
  if (snapshot.measurementDone) packet.flags |= kFlagMeasurementDone;
  if (beatEvent) packet.flags |= kFlagBeatEvent;

  packet.sequence = sequence;
  packet.senderUptimeMs = nowMs;
  packet.calibrationElapsedMs =
      snapshot.calibrationActive && snapshot.calibrationStartMs > 0
          ? nowMs - snapshot.calibrationStartMs
          : 0;
  packet.measurementElapsedMs =
      snapshot.measurementActive && snapshot.measurementStartMs > 0
          ? nowMs - snapshot.measurementStartMs
          : 0;
  packet.instantBpm = snapshot.instantBpm;
  packet.avgBpm = snapshot.avgBpm;
  packet.rawFinalAvg = snapshot.rawFinalAvg;
  packet.finalAvg = snapshot.finalAvg;
  packet.rawIr = snapshot.rawIr;
  packet.calibratedIr = snapshot.calibratedIr;
  packet.calibrationBaseline = snapshot.calibrationBaseline;
  packet.fingerThreshold = snapshot.fingerThreshold;
  packet.correctionFactor = snapshot.correctionFactor;
  packet.referenceBpm = snapshot.referenceBpm;
  packet.referenceSourceAvg = snapshot.referenceSourceAvg;
  packet.sampleCount = snapshot.sampleCount;
  return packet;
}

inline bool isValid(const Packet &packet) {
  return packet.magic == kMagic && packet.version == kVersion;
}

inline void applyPacket(const Packet &packet, PulseSnapshot &snapshot, uint32_t nowMs) {
  snapshot.rawIr = packet.rawIr;
  snapshot.calibratedIr = packet.calibratedIr;
  snapshot.calibrationBaseline = packet.calibrationBaseline;
  snapshot.fingerThreshold = packet.fingerThreshold;
  snapshot.instantBpm = packet.instantBpm;
  snapshot.avgBpm = packet.avgBpm;
  snapshot.rawFinalAvg = packet.rawFinalAvg;
  snapshot.finalAvg = packet.finalAvg;
  snapshot.correctionFactor = packet.correctionFactor;
  snapshot.referenceBpm = packet.referenceBpm;
  snapshot.referenceSourceAvg = packet.referenceSourceAvg;
  snapshot.sampleCount = packet.sampleCount;
  snapshot.fingerPresent = (packet.flags & kFlagFingerPresent) != 0;
  snapshot.calibrationActive = (packet.flags & kFlagCalibrationActive) != 0;
  snapshot.calibrationDone = (packet.flags & kFlagCalibrationDone) != 0;
  snapshot.referenceCalibrated = (packet.flags & kFlagReferenceCalibrated) != 0;
  snapshot.measurementActive = (packet.flags & kFlagMeasurementActive) != 0;
  snapshot.measurementDone = (packet.flags & kFlagMeasurementDone) != 0;

  if (snapshot.calibrationActive) {
    snapshot.calibrationStartMs =
        packet.calibrationElapsedMs > 0 ? nowMs - packet.calibrationElapsedMs : nowMs;
  } else {
    snapshot.calibrationStartMs = 0;
  }

  if (snapshot.measurementActive) {
    snapshot.measurementStartMs =
        packet.measurementElapsedMs > 0 ? nowMs - packet.measurementElapsedMs : nowMs;
  } else {
    snapshot.measurementStartMs = 0;
  }

  if (packet.flags & kFlagBeatEvent) {
    snapshot.lastBeatMs = nowMs;
  }
}

}  // namespace EspNowPulseLink

