#include "OledDisplay.h"

#include <math.h>
#include <stdio.h>

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
