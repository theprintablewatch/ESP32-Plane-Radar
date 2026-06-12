#include "hardware/io_expander_ch422g.h"

#if defined(BOARD_WAVESHARE_43B)

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

namespace {

// The CH422G is "register-less": each function uses a distinct I2C address and
// the data byte is the value. We keep a shadow of the output register because it
// is write-only.
uint8_t s_out_state = 0xFF;

void writeOut() {
  Wire.beginTransmission(config::kCh422gAddrOut);
  Wire.write(s_out_state);
  Wire.endTransmission();
}

void setBit(uint8_t bit, bool high) {
  if (high) {
    s_out_state |= bit;
  } else {
    s_out_state &= static_cast<uint8_t>(~bit);
  }
  writeOut();
}

}  // namespace

void ch422gInit() {
  Wire.begin(static_cast<int>(config::kTouchPinSda),
             static_cast<int>(config::kTouchPinScl));

  // Mode register: 0x01 = push-pull output on the EXIO pins.
  Wire.beginTransmission(config::kCh422gAddrMode);
  Wire.write(0x01);
  Wire.endTransmission();

  s_out_state = 0xFF;  // default all-high (resets released, backlight on)
  writeOut();
}

void ch422gResetPanelAndTouch() {
  setBit(config::kCh422gBitLcdRst | config::kCh422gBitTouchRst, false);
  delay(20);
  setBit(config::kCh422gBitLcdRst | config::kCh422gBitTouchRst, true);
  delay(120);  // GT911 needs ~50ms+ after reset before it answers on I2C
}

void ch422gBacklight(bool on) {
  setBit(config::kCh422gBitBacklight, on);
}

#endif
