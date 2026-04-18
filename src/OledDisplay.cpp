#include "OledDisplay.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

OledDisplay::OledDisplay(AppSettingsStore &settings) : settings_(settings) {}

bool OledDisplay::begin(TwoWire &wire) {
  wire_ = &wire;
  address_ = 0;
  available_ = false;

  const AppSettings &config = settings_.current();
  const uint8_t addresses[] = {config.oledAddress1, config.oledAddress2};
  for (uint8_t address : addresses) {
    if (probeI2cAddress(wire, address)) {
      address_ = address;
      break;
    }
  }

  if (address_ == 0) {
    Serial.println("OLED nicht gefunden (erwartet 0x3C oder 0x3D).");
    return false;
  }

  selectDriver(config.oledDriverMode);
  oled_->setI2CAddress(address_ << 1);
  oled_->setBusClock(100000);
  delay(50);
  oled_->begin();
  oled_->setPowerSave(0);
  oled_->clearBuffer();
  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("OLED bereit", oled_->getDisplayHeight() > 32 ? 18 : 12);
  drawCentered("Pulsmessung startet", oled_->getDisplayHeight() > 32 ? 34 : 26);
  oled_->sendBuffer();

  available_ = true;
  driverMode_ = config.oledDriverMode;
  appliedSettingsRevision_ = settings_.revision();
  Serial.print("OLED gefunden auf 0x");
  Serial.print(address_, HEX);
  Serial.print("  Modus=");
  Serial.println(driverMode_);
  return true;
}

void OledDisplay::setNetworkLabel(const String &label) {
  networkLabel_ = label;
}

void OledDisplay::render(const PulseSnapshot &snapshot, unsigned long nowMs, bool force) {
  if (wire_ != nullptr && appliedSettingsRevision_ != settings_.revision()) {
    begin(*wire_);
  }

  if (!available_ || oled_ == nullptr) {
    return;
  }

  const AppSettings &config = settings_.current();
  const bool compact = oled_->getDisplayHeight() <= 32;
  const bool beatAnimationActive =
      snapshot.lastBeatMs > 0 && nowMs - snapshot.lastBeatMs < config.beatAnimationMs;
  const unsigned long refreshInterval =
      beatAnimationActive ? config.beatAnimationFrameMs : config.displayIntervalMs;

  if (!force && nowMs - lastRenderMs_ < refreshInterval) {
    return;
  }
  lastRenderMs_ = nowMs;

  char line[32];
  oled_->clearBuffer();

  if (snapshot.calibrationActive) {
    oled_->setFont(u8g2_font_6x12_tf);
    drawCentered("Kalibrierung", compact ? 12 : 16);
    drawCentered("Finger weg", compact ? 26 : 32);

    if (!compact) {
      unsigned long elapsedSec = (nowMs - snapshot.calibrationStartMs) / 1000UL;
      const unsigned long totalSec = config.calibrationMs / 1000UL;
      if (elapsedSec > totalSec) {
        elapsedSec = totalSec;
      }
      snprintf(line, sizeof(line), "%lu/%lu s", elapsedSec, totalSec);
      drawCentered(line, 48);
      snprintf(line, sizeof(line), "RAW %ld", snapshot.rawIr);
      drawCentered(line, 60);
    }

    oled_->sendBuffer();
    return;
  }

  if (!snapshot.fingerPresent) {
    oled_->setFont(u8g2_font_6x12_tf);
    drawCentered("Finger auflegen", compact ? 12 : 14);

    if (compact) {
      if (snapshot.measurementDone && snapshot.finalAvg > 0.0f) {
        snprintf(line, sizeof(line), "10s %.0f BPM", snapshot.finalAvg);
      } else if (!networkLabel_.isEmpty()) {
        snprintf(line, sizeof(line), "%s", networkLabel_.c_str());
      } else {
        snprintf(line, sizeof(line), "I2C 0x%02X", address_);
      }
      drawCentered(line, 28);
      oled_->sendBuffer();
      return;
    }

    if (!networkLabel_.isEmpty()) {
      drawCentered("IP:", 30);
      drawCentered(networkLabel_.c_str(), 46);
    } else {
      drawCentered("Puls wird hier", 32);
      drawCentered("angezeigt", 44);
    }

    if (snapshot.measurementDone && snapshot.finalAvg > 0.0f) {
      snprintf(line, sizeof(line), "10s %.1f BPM", snapshot.finalAvg);
    } else if (snapshot.calibrationDone) {
      snprintf(line, sizeof(line), "Basis %ld", snapshot.calibrationBaseline);
    } else {
      snprintf(line, sizeof(line), "I2C 0x%02X", address_);
    }
    drawCentered(line, 60);
    oled_->sendBuffer();
    return;
  }

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("Puls", compact ? 10 : 10);
  if (beatAnimationActive) {
    drawBeatAnimation(nowMs - snapshot.lastBeatMs);
  }

  if (compact) {
    oled_->setFont(u8g2_font_logisoso18_tn);
    if (snapshot.avgBpm > 0.0f) {
      snprintf(line, sizeof(line), "%u", static_cast<unsigned>(lroundf(snapshot.avgBpm)));
    } else if (snapshot.instantBpm > 0.0f) {
      snprintf(line, sizeof(line), "%u", static_cast<unsigned>(lroundf(snapshot.instantBpm)));
    } else {
      snprintf(line, sizeof(line), "--");
    }
    drawCentered(line, 30);
    oled_->sendBuffer();
    return;
  }

  oled_->setFont(u8g2_font_logisoso24_tn);
  if (snapshot.avgBpm > 0.0f) {
    snprintf(line, sizeof(line), "%u", static_cast<unsigned>(lroundf(snapshot.avgBpm)));
  } else if (snapshot.instantBpm > 0.0f) {
    snprintf(line, sizeof(line), "%u", static_cast<unsigned>(lroundf(snapshot.instantBpm)));
  } else {
    snprintf(line, sizeof(line), "--");
  }
  drawCentered(line, 40);

  oled_->setFont(u8g2_font_6x12_tf);
  if (snapshot.measurementActive) {
    unsigned long elapsedSec = (nowMs - snapshot.measurementStartMs) / 1000UL;
    const unsigned long totalSec = config.measureMs / 1000UL;
    if (elapsedSec > totalSec) {
      elapsedSec = totalSec;
    }
    snprintf(line, sizeof(line), "Messung %lu/%lu s", elapsedSec, totalSec);
  } else if (snapshot.measurementDone && snapshot.finalAvg > 0.0f) {
    snprintf(line, sizeof(line), "10s %.1f BPM", snapshot.finalAvg);
  } else if (snapshot.instantBpm > 0.0f && snapshot.avgBpm > 0.0f) {
    snprintf(line, sizeof(line), "Jetzt %.0f | Avg %.0f", snapshot.instantBpm, snapshot.avgBpm);
  } else {
    snprintf(line, sizeof(line), "Signal ok");
  }
  drawCentered(line, 60);

  oled_->sendBuffer();
}

void OledDisplay::renderOverview(const PulseSnapshot &snapshot, unsigned long nowMs,
                                 unsigned long linkAgeMs, ReceiverDisplayMode mode, bool showBpm,
                                 bool showLink, bool showFinger, bool showState, bool force) {
  if (wire_ != nullptr && appliedSettingsRevision_ != settings_.revision()) {
    begin(*wire_);
  }

  if (!available_ || oled_ == nullptr) {
    return;
  }

  const AppSettings &config = settings_.current();
  const unsigned long refreshInterval = config.displayIntervalMs;
  if (!force && nowMs - lastRenderMs_ < refreshInterval) {
    return;
  }
  lastRenderMs_ = nowMs;

  const bool compact = oled_->getDisplayHeight() <= 32;
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  oled_->clearBuffer();
  switch (mode) {
    case ReceiverDisplayMode::kBigBpm:
      drawBigBpmOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kConnection:
      drawConnectionOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kStatus:
      drawStatusOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kMinimal:
      drawMinimalOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kClockBpm:
      drawClockBpmOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kClockDateBpm:
      drawClockDateBpmOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kSignal:
      drawSignalOverview(snapshot, linkAgeMs);
      break;
    case ReceiverDisplayMode::kTiles:
    default:
      drawTilesOverview(snapshot, linkAgeMs, showBpm, showLink, showFinger, showState);
      break;
  }

  oled_->sendBuffer();
}

void OledDisplay::drawTilesOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs,
                                    bool showBpm, bool showLink, bool showFinger, bool showState) {
  char line[32];
  const bool compact = oled_->getDisplayHeight() <= 32;
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const unsigned long linkSec = linkAgeMs / 1000UL;
  const char *stateLabel = snapshot.calibrationActive
                               ? "CAL"
                               : (snapshot.measurementActive ? "MEAS" :
                                  (snapshot.measurementDone ? "DONE" : "IDLE"));

  oled_->setFont(u8g2_font_6x12_tf);

  if (compact) {
    drawCentered("PULSE HUB", 10);
    if (!showBpm && !showLink && !showFinger && !showState) {
      drawCentered("Keine Kacheln aktiv", 26);
      return;
    }

    int16_t slotX = 4;
    if (showBpm) {
      drawHeart(slotX + 6, 23, 6);
      snprintf(line, sizeof(line), "%u",
               static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                             : lroundf(snapshot.instantBpm)));
      oled_->drawStr(slotX + 16, 27, line);
      slotX += 30;
    }
    if (showLink) {
      drawWifiIcon(slotX, 19, online);
      snprintf(line, sizeof(line), "%lus", static_cast<unsigned long>(linkAgeMs / 1000UL));
      oled_->drawStr(slotX + 18, 27, line);
      slotX += 30;
    }
    if (showFinger) {
      drawFingerIcon(slotX, 18, snapshot.fingerPresent);
      oled_->drawStr(slotX + 18, 27, snapshot.fingerPresent ? "YES" : "NO");
      slotX += 30;
    }
    if (showState) {
      drawStatusIcon(slotX, 18, snapshot);
      oled_->drawStr(slotX + 14, 27, stateLabel);
    }
    return;
  }

  drawCentered("PULSE DASHBOARD", 10);
  oled_->drawHLine(0, 14, oled_->getDisplayWidth());
  if (!showBpm && !showLink && !showFinger && !showState) {
    drawCentered("Keine Kacheln aktiv", 36);
    return;
  }

  if (showBpm) {
    oled_->drawFrame(2, 18, 58, 22);
    drawHeart(12, 29, 8);
    oled_->setFont(u8g2_font_logisoso18_tn);
    snprintf(line, sizeof(line), "%u",
             static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                           : lroundf(snapshot.instantBpm)));
    oled_->drawStr(28, 38, line);
    oled_->setFont(u8g2_font_6x12_tf);
  } else {
    drawHiddenTile(2, 18, 58, 22);
  }

  if (showLink) {
    oled_->drawFrame(66, 18, 60, 22);
    drawWifiIcon(76, 22, online);
    snprintf(line, sizeof(line), "%lu s", linkSec);
    oled_->drawStr(94, 29, line);
    oled_->drawStr(94, 39, online ? "LINK OK" : "LINK OFF");
  } else {
    drawHiddenTile(66, 18, 60, 22);
  }

  if (showFinger) {
    oled_->drawFrame(2, 44, 60, 18);
    drawFingerIcon(10, 47, snapshot.fingerPresent);
    oled_->drawStr(24, 57, snapshot.fingerPresent ? "FINGER YES" : "FINGER NO");
  } else {
    drawHiddenTile(2, 44, 60, 18);
  }

  if (showState) {
    oled_->drawFrame(66, 44, 60, 18);
    drawStatusIcon(74, 46, snapshot);
    oled_->drawStr(88, 57, stateLabel);
  } else {
    drawHiddenTile(66, 44, 60, 18);
  }
}

void OledDisplay::drawBigBpmOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const bool compact = oled_->getDisplayHeight() <= 32;

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("BPM FOKUS", 10);
  drawHeart(compact ? 16 : 20, compact ? 18 : 22, compact ? 10 : 14);
  oled_->setFont(u8g2_font_logisoso24_tn);
  if (snapshot.avgBpm > 0.0f) {
    snprintf(line, sizeof(line), "%u", static_cast<unsigned>(lroundf(snapshot.avgBpm)));
  } else if (snapshot.instantBpm > 0.0f) {
    snprintf(line, sizeof(line), "%u", static_cast<unsigned>(lroundf(snapshot.instantBpm)));
  } else {
    snprintf(line, sizeof(line), "--");
  }
  drawCentered(line, compact ? 33 : 39);
  oled_->setFont(u8g2_font_6x12_tf);
  snprintf(line, sizeof(line), "%s | %s | %lu s",
           snapshot.fingerPresent ? "Finger da" : "Kein Finger",
           online ? "Link ok" : "Link off",
           linkAgeMs / 1000UL);
  drawCentered(line, compact ? 28 : 58);
}

void OledDisplay::drawConnectionOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const bool compact = oled_->getDisplayHeight() <= 32;

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("VERBINDUNG", 10);
  drawWifiIcon(compact ? 12 : 20, compact ? 15 : 20, online);
  drawCentered(online ? "ESP-NOW ONLINE" : "ESP-NOW OFFLINE", compact ? 26 : 34);
  snprintf(line, sizeof(line), "Letzte Daten %lus", linkAgeMs / 1000UL);
  drawCentered(line, compact ? 27 : 48);
  if (!compact) {
    snprintf(line, sizeof(line), "BPM %u  Finger %s",
             static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                           : lroundf(snapshot.instantBpm)),
             snapshot.fingerPresent ? "ja" : "nein");
    drawCentered(line, 60);
  }
}

void OledDisplay::drawStatusOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const bool compact = oled_->getDisplayHeight() <= 32;
  const char *stateLabel = snapshot.calibrationActive
                               ? "KALIBRIERUNG"
                               : (snapshot.measurementActive ? "MESSUNG" :
                                  (snapshot.measurementDone ? "FERTIG" : "IDLE"));

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("STATUS", 10);
  drawStatusIcon(compact ? 12 : 20, compact ? 16 : 20, snapshot);
  drawCentered(stateLabel, compact ? 26 : 34);
  snprintf(line, sizeof(line), "Finger %s", snapshot.fingerPresent ? "ja" : "nein");
  drawCentered(line, compact ? 28 : 48);
  if (!compact) {
    snprintf(line, sizeof(line), "Link %s | %lus", online ? "ok" : "off", linkAgeMs / 1000UL);
    drawCentered(line, 60);
  }
}

void OledDisplay::drawMinimalOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const unsigned bpm = static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                                    : lroundf(snapshot.instantBpm));

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("MINIMAL", 10);
  if (bpm > 0) {
    snprintf(line, sizeof(line), "BPM %u", bpm);
  } else {
    snprintf(line, sizeof(line), "BPM --");
  }
  drawCentered(line, 28);
  snprintf(line, sizeof(line), "Link %s", online ? "ok" : "off");
  drawCentered(line, 40);
  snprintf(line, sizeof(line), "Finger %s", snapshot.fingerPresent ? "ja" : "nein");
  drawCentered(line, 52);
}

void OledDisplay::drawClockBpmOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  char clock[16];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const bool compact = oled_->getDisplayHeight() <= 32;

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("UHR + BPM", 10);

  if (formatClock(clock, sizeof(clock))) {
    oled_->setFont(compact ? u8g2_font_logisoso18_tn : u8g2_font_logisoso32_tn);
    drawCentered(clock, compact ? 27 : 40);
  } else {
    oled_->setFont(compact ? u8g2_font_logisoso18_tn : u8g2_font_logisoso32_tn);
    drawCentered("--:--", compact ? 27 : 40);
  }

  oled_->setFont(u8g2_font_6x12_tf);
  const unsigned bpm = static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                                    : lroundf(snapshot.instantBpm));
  char bpmText[16];
  if (bpm > 0) {
    snprintf(bpmText, sizeof(bpmText), "%u", bpm);
  } else {
    snprintf(bpmText, sizeof(bpmText), "--");
  }
  if (compact) {
    snprintf(line, sizeof(line), "BPM %s | %s", bpmText, online ? "Link ok" : "Link off");
    drawCentered(line, 29);
    return;
  }

  snprintf(line, sizeof(line), "BPM %s  |  Finger %s  |  %s", bpmText,
           snapshot.fingerPresent ? "ja" : "nein", online ? "Link ok" : "Link off");
  drawCentered(line, 58);
}

void OledDisplay::drawClockDateBpmOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  char clock[16];
  char date[20];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const bool compact = oled_->getDisplayHeight() <= 32;

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("UHR + DATUM + BPM", 10);

  oled_->setFont(compact ? u8g2_font_logisoso18_tn : u8g2_font_logisoso32_tn);
  if (formatClock(clock, sizeof(clock))) {
    drawCentered(clock, compact ? 25 : 40);
  } else {
    drawCentered("--:--", compact ? 25 : 40);
  }

  oled_->setFont(u8g2_font_6x12_tf);
  if (!compact) {
    if (formatDate(date, sizeof(date))) {
      drawCentered(date, 52);
    } else {
      drawCentered("Datum nicht verfuegbar", 52);
    }
  }

  const unsigned bpm = static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                                    : lroundf(snapshot.instantBpm));
  if (bpm > 0) {
    snprintf(line, sizeof(line), "BPM %u | %s", bpm, online ? "Link ok" : "Link off");
  } else {
    snprintf(line, sizeof(line), "BPM -- | %s", online ? "Link ok" : "Link off");
  }
  drawCentered(line, compact ? 29 : 63);
}

void OledDisplay::drawSignalOverview(const PulseSnapshot &snapshot, unsigned long linkAgeMs) {
  char line[32];
  const bool online = linkAgeMs <= AppConfig::kEspNowLinkTimeoutMs;
  const unsigned bpm = static_cast<unsigned>(snapshot.avgBpm > 0.0f ? lroundf(snapshot.avgBpm)
                                                                    : lroundf(snapshot.instantBpm));
  const char *stateLabel = snapshot.calibrationActive
                               ? "CAL"
                               : (snapshot.measurementActive ? "MEAS" :
                                  (snapshot.measurementDone ? "DONE" : "IDLE"));
  const bool compact = oled_->getDisplayHeight() <= 32;

  oled_->setFont(u8g2_font_6x12_tf);
  drawCentered("SIGNAL", 10);

  if (compact) {
    drawWifiIcon(6, 14, online);
    drawFingerIcon(28, 14, snapshot.fingerPresent);
    drawStatusIcon(50, 14, snapshot);
    snprintf(line, sizeof(line), "%s | %s | %s | BPM %s", online ? "Link OK" : "Link OFF",
             snapshot.fingerPresent ? "Finger JA" : "Finger NEIN", stateLabel,
             bpm > 0 ? "ok" : "--");
    drawCentered(line, 29);
    return;
  } else {
    drawWifiIcon(8, 18, online);
    drawFingerIcon(36, 18, snapshot.fingerPresent);
    drawStatusIcon(60, 18, snapshot);
  }

  snprintf(line, sizeof(line), "Link %s", online ? "OK" : "OFF");
  oled_->drawStr(compact ? 74 : 84, compact ? 23 : 25, line);
  snprintf(line, sizeof(line), "Finger %s", snapshot.fingerPresent ? "JA" : "NEIN");
  oled_->drawStr(compact ? 74 : 84, compact ? 31 : 38, line);
  snprintf(line, sizeof(line), "State %s", stateLabel);
  oled_->drawStr(compact ? 74 : 84, compact ? 39 : 51, line);

  if (bpm > 0) {
    snprintf(line, sizeof(line), "BPM %u", bpm);
  } else {
    snprintf(line, sizeof(line), "BPM --");
  }
  oled_->drawStr(8, compact ? 31 : 58, line);
}

void OledDisplay::selectDriver(uint8_t mode) {
  switch (mode) {
    case 1:
      oled_ = &ssd1306Alt0_;
      break;
    case 2:
      oled_ = &sh1106_;
      break;
    case 3:
      oled_ = &ssd1306x32_;
      break;
    case 0:
    default:
      oled_ = &ssd1306_;
      break;
  }
}

void OledDisplay::drawBeatAnimation(unsigned long beatAgeMs) {
  if (oled_ == nullptr) {
    return;
  }

  const bool compact = oled_->getDisplayHeight() <= 32;
  uint8_t heartSize = compact ? 5 : 9;

  if (beatAgeMs < 70) {
    heartSize = compact ? 7 : 12;
  } else if (beatAgeMs < 140) {
    heartSize = compact ? 6 : 10;
  } else if (beatAgeMs < 220) {
    heartSize = compact ? 6 : 11;
  }

  drawHeart(compact ? 110 : 104, compact ? 9 : 15, heartSize);
}

void OledDisplay::drawHeart(int16_t centerX, int16_t centerY, uint8_t size) {
  if (oled_ == nullptr) {
    return;
  }

  const int16_t radius = size / 2;
  const int16_t leftX = centerX - radius;
  const int16_t rightX = centerX + radius;
  const int16_t bottomY = centerY + size;
  const int16_t boltTopX = centerX + size - 1;
  const int16_t boltTopY = centerY - radius + 1;
  const int16_t boltMidRightX = centerX + 2;
  const int16_t boltMidRightY = centerY + 1;
  const int16_t boltMidLeftX = centerX + 5;
  const int16_t boltMidLeftY = centerY + 1;
  const int16_t boltBottomX = centerX - size + 2;
  const int16_t boltBottomY = centerY + size - 1;

  oled_->drawDisc(leftX, centerY, radius, U8G2_DRAW_ALL);
  oled_->drawDisc(rightX, centerY, radius, U8G2_DRAW_ALL);
  oled_->drawTriangle(centerX - size, centerY, centerX + size, centerY, centerX, bottomY);

  oled_->setDrawColor(0);
  oled_->drawTriangle(boltTopX, boltTopY, boltMidLeftX, boltMidLeftY, boltMidRightX, boltMidRightY);
  oled_->drawTriangle(centerX + 1, centerY + 1, boltBottomX, boltBottomY, centerX - 4, centerY + 4);
  oled_->setDrawColor(1);
}

void OledDisplay::drawWifiIcon(int16_t x, int16_t y, bool online) {
  if (oled_ == nullptr) {
    return;
  }

  const int16_t width = 14;
  const int16_t height = 10;
  const int16_t cx = x + width / 2;
  const int16_t cy = y + height / 2;

  oled_->drawCircle(cx, cy + 2, 2, U8G2_DRAW_ALL);
  oled_->drawLine(cx - 6, cy - 1, cx, cy + 5);
  oled_->drawLine(cx + 6, cy - 1, cx, cy + 5);
  oled_->drawLine(cx - 4, cy - 3, cx, cy + 1);
  oled_->drawLine(cx + 4, cy - 3, cx, cy + 1);
  oled_->drawLine(cx - 2, cy - 5, cx, cy - 1);
  oled_->drawLine(cx + 2, cy - 5, cx, cy - 1);
  if (!online) {
    oled_->drawLine(cx - 7, cy - 6, cx + 7, cy + 6);
  }
}

void OledDisplay::drawFingerIcon(int16_t x, int16_t y, bool active) {
  if (oled_ == nullptr) {
    return;
  }

  // Simple fingertip + hand outline that reads better on 128x32 and 128x64.
  oled_->drawBox(x + 4, y + 1, 3, 9);
  oled_->drawBox(x + 7, y + 3, 3, 7);
  oled_->drawBox(x + 2, y + 5, 3, 5);
  oled_->drawLine(x + 2, y + 10, x + 12, y + 10);
  oled_->drawLine(x + 2, y + 10, x + 2, y + 7);
  oled_->drawLine(x + 12, y + 10, x + 12, y + 8);
  oled_->drawLine(x + 3, y + 4, x + 3, y + 3);
  oled_->drawLine(x + 4, y + 2, x + 4, y + 1);
  oled_->drawLine(x + 7, y + 3, x + 10, y + 3);
  oled_->drawLine(x + 10, y + 3, x + 10, y + 6);
  if (active) {
    oled_->drawDisc(x + 12, y + 6, 2, U8G2_DRAW_ALL);
    oled_->drawLine(x + 11, y + 6, x + 13, y + 6);
  }
}

void OledDisplay::drawStatusIcon(int16_t x, int16_t y, const PulseSnapshot &snapshot) {
  if (oled_ == nullptr) {
    return;
  }

  if (snapshot.calibrationActive) {
    oled_->drawCircle(x + 5, y + 5, 5, U8G2_DRAW_ALL);
    oled_->drawLine(x + 5, y + 1, x + 5, y + 5);
    oled_->drawLine(x + 5, y + 5, x + 8, y + 7);
    return;
  }

  if (snapshot.measurementActive) {
    oled_->drawFrame(x, y, 10, 10);
    oled_->drawLine(x + 3, y + 2, x + 3, y + 8);
    oled_->drawLine(x + 7, y + 2, x + 7, y + 8);
    return;
  }

  if (snapshot.measurementDone) {
    oled_->drawCircle(x + 5, y + 5, 5, U8G2_DRAW_ALL);
    oled_->drawLine(x + 2, y + 5, x + 4, y + 7);
    oled_->drawLine(x + 4, y + 7, x + 8, y + 3);
    return;
  }

  oled_->drawCircle(x + 5, y + 5, 5, U8G2_DRAW_ALL);
  oled_->drawLine(x + 3, y + 5, x + 7, y + 5);
}

void OledDisplay::drawHiddenTile(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (oled_ == nullptr) {
    return;
  }

  oled_->setDrawColor(0);
  oled_->drawBox(x + 1, y + 1, w - 2, h - 2);
  oled_->setDrawColor(1);
}

bool OledDisplay::probeI2cAddress(TwoWire &wire, uint8_t address) {
  wire.beginTransmission(address);
  return wire.endTransmission() == 0;
}

void OledDisplay::drawCentered(const char *text, uint8_t y) {
  if (oled_ == nullptr) {
    return;
  }

  int16_t x = (oled_->getDisplayWidth() - oled_->getStrWidth(text)) / 2;
  if (x < 0) {
    x = 0;
  }
  oled_->drawStr(x, y, text);
}

bool OledDisplay::formatClock(char *buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize < 6) {
    return false;
  }

  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return false;
  }

  struct tm localTime;
  if (localtime_r(&now, &localTime) == nullptr) {
    return false;
  }

  snprintf(buffer, bufferSize, "%02d:%02d", localTime.tm_hour, localTime.tm_min);
  return true;
}

bool OledDisplay::formatDate(char *buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize < 12) {
    return false;
  }

  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return false;
  }

  struct tm localTime;
  if (localtime_r(&now, &localTime) == nullptr) {
    return false;
  }

  snprintf(buffer, bufferSize, "%02d.%02d.%04d", localTime.tm_mday, localTime.tm_mon + 1,
           localTime.tm_year + 1900);
  return true;
}
