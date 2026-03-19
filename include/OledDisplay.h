#pragma once

#include <U8g2lib.h>
#include <Wire.h>

#include "AppSettings.h"
#include "PulseSnapshot.h"

class OledDisplay {
 public:
  explicit OledDisplay(AppSettingsStore &settings);

  bool begin(TwoWire &wire);
  void setNetworkLabel(const String &label);
  void render(const PulseSnapshot &snapshot, unsigned long nowMs, bool force = false);

 private:
  void selectDriver(uint8_t mode);
  void drawBeatAnimation(unsigned long beatAgeMs);
  void drawHeart(int16_t centerX, int16_t centerY, uint8_t size);
  bool probeI2cAddress(TwoWire &wire, uint8_t address);
  void drawCentered(const char *text, uint8_t y);

  AppSettingsStore &settings_;
  U8G2 *oled_ = nullptr;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C ssd1306_{U8G2_R0, U8X8_PIN_NONE};
  U8G2_SSD1306_128X64_ALT0_F_HW_I2C ssd1306Alt0_{U8G2_R0, U8X8_PIN_NONE};
  U8G2_SH1106_128X64_NONAME_F_HW_I2C sh1106_{U8G2_R0, U8X8_PIN_NONE};
  U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C ssd1306x32_{U8G2_R0, U8X8_PIN_NONE};
  TwoWire *wire_ = nullptr;
  bool available_ = false;
  uint8_t address_ = 0;
  uint8_t driverMode_ = 0;
  unsigned long lastRenderMs_ = 0;
  uint32_t appliedSettingsRevision_ = 0;
  String networkLabel_;
};
