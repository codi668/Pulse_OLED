#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct ReceiverUiSettings {
  uint8_t displayMode = 0;
  bool showBpm = true;
  bool showLink = true;
  bool showFinger = true;
  bool showState = true;
};

enum class ReceiverDisplayMode : uint8_t {
  kTiles = 0,
  kBigBpm = 1,
  kConnection = 2,
  kStatus = 3,
  kMinimal = 4,
  kClockBpm = 5,
  kClockDateBpm = 6,
  kSignal = 7,
};

class ReceiverUiSettingsStore {
 public:
  bool begin();
  const ReceiverUiSettings &current() const;
  uint32_t revision() const;
  bool update(const ReceiverUiSettings &candidate);

 private:
  void load();
  void save();

  ReceiverUiSettings settings_;
  Preferences preferences_;
  bool ready_ = false;
  uint32_t revision_ = 1;
};
