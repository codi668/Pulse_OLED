#include "ReceiverUiSettings.h"

bool ReceiverUiSettingsStore::begin() {
  ready_ = preferences_.begin("rxui", false);
  settings_ = ReceiverUiSettings{};
  if (ready_) {
    load();
  }
  return ready_;
}

const ReceiverUiSettings &ReceiverUiSettingsStore::current() const {
  return settings_;
}

uint32_t ReceiverUiSettingsStore::revision() const {
  return revision_;
}

bool ReceiverUiSettingsStore::update(const ReceiverUiSettings &candidate) {
  settings_ = candidate;
  if (ready_) {
    save();
  }
  revision_++;
  return true;
}

void ReceiverUiSettingsStore::load() {
  settings_.displayMode = preferences_.getUChar("display_mode", settings_.displayMode);
  settings_.showBpm = preferences_.getBool("show_bpm", settings_.showBpm);
  settings_.showLink = preferences_.getBool("show_link", settings_.showLink);
  settings_.showFinger = preferences_.getBool("show_finger", settings_.showFinger);
  settings_.showState = preferences_.getBool("show_state", settings_.showState);
}

void ReceiverUiSettingsStore::save() {
  preferences_.putUChar("display_mode", settings_.displayMode);
  preferences_.putBool("show_bpm", settings_.showBpm);
  preferences_.putBool("show_link", settings_.showLink);
  preferences_.putBool("show_finger", settings_.showFinger);
  preferences_.putBool("show_state", settings_.showState);
}
