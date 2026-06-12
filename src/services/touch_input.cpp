#include "services/touch_input.h"

#include <Arduino.h>

#include "config.h"

#if !defined(BOARD_WAVESHARE_43B)
#include <Wire.h>
#include <cstdlib>
#endif

namespace services::touch {

namespace {

#if !defined(BOARD_WAVESHARE_43B)

// CST816S register map (subset): 0x01 GestureID, 0x02 FingerNum,
// 0x03/0x04 X hi/lo, 0x05/0x06 Y hi/lo. Gesture ids: 1=up, 2=down,
// 3=left, 4=right. The chip only reports gestures on some firmware, so we
// also fall back to the touch-down -> touch-up delta (which is robust to any
// panel mirroring: both horizontal directions just toggle the view anyway).
constexpr uint8_t kRegGesture = 0x01;
constexpr int kSwipeMinPx = 40;  // min drag to count as a swipe
constexpr int kTapMaxPx = 20;    // max drag still counted as a tap
constexpr int kCoordMax = 280;   // reject out-of-range (noise) coords
constexpr unsigned long kTapMaxMs = 2000;
// If the INT line goes idle mid-contact, treat the finger as lifted after this.
constexpr unsigned long kReleaseTimeoutMs = 90;

bool s_present = false;
bool s_prev_down = false;  // finger state on the previous poll (edge detect)
bool s_gestured = false;   // a swipe was already emitted this contact
unsigned long s_contact_start_ms = 0;
unsigned long s_last_report_ms = 0;
int s_start_x = 0;
int s_start_y = 0;
int s_last_x = 0;
int s_last_y = 0;

bool readRegs(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(config::kTouchI2cAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const uint8_t got = Wire.requestFrom(static_cast<int>(config::kTouchI2cAddr),
                                       static_cast<int>(len));
  if (got < len) {
    return false;
  }
  for (uint8_t i = 0; i < len; ++i) {
    buf[i] = Wire.read();
  }
  return true;
}

Gesture chipGesture(uint8_t id) {
  switch (id) {
    case 0x01: return Gesture::SwipeUp;
    case 0x02: return Gesture::SwipeDown;
    case 0x03: return Gesture::SwipeLeft;
    case 0x04: return Gesture::SwipeRight;
    default: return Gesture::None;
  }
}

#endif  // !BOARD_WAVESHARE_43B

}  // namespace

void init() {
#if !defined(BOARD_WAVESHARE_43B)
  pinMode(config::kTouchPinRst, OUTPUT);
  digitalWrite(config::kTouchPinRst, LOW);
  delay(10);
  digitalWrite(config::kTouchPinRst, HIGH);
  delay(60);
  pinMode(config::kTouchPinInt, INPUT_PULLUP);

  Wire.begin(config::kTouchPinSda, config::kTouchPinScl, config::kTouchI2cHz);

  // MotionMask: enable continuous left/right + up/down gesture reporting.
  Wire.beginTransmission(config::kTouchI2cAddr);
  Wire.write(0xEC);
  Wire.write(0x03);
  Wire.endTransmission();

  uint8_t id = 0;
  s_present = readRegs(0x07, &id, 1) || readRegs(0xA7, &id, 1);
  Serial.printf("touch: CST816S %s\n", s_present ? "ready" : "not found");
#endif
}

Gesture poll() {
#if defined(BOARD_WAVESHARE_43B)
  return Gesture::None;
#else
  if (!s_present) {
    return Gesture::None;
  }

  // Only touch the I2C bus when the INT line is asserted (a touch frame is
  // ready). The CST816S auto-sleeps when idle and NAKs reads while asleep, so
  // polling it constantly floods the bus with errors. INT idle (high) => skip.
  const bool int_active = digitalRead(config::kTouchPinInt) == LOW;

  uint8_t gesture_id = 0;
  bool down = false;
  bool have_frame = false;
  int x = 0;
  int y = 0;

  if (int_active) {
    uint8_t d[6] = {0};
    have_frame = readRegs(kRegGesture, d, sizeof(d));
    if (have_frame) {
      gesture_id = d[0];
      const uint8_t finger = d[1];
      x = ((d[2] & 0x0F) << 8) | d[3];
      y = ((d[4] & 0x0F) << 8) | d[5];
      down = finger > 0 && x >= 0 && x <= kCoordMax && y >= 0 && y <= kCoordMax;
      s_last_report_ms = millis();
    }
  }

  if (!have_frame) {
    // No fresh frame. Synthesise a release if a contact's reports have stopped;
    // otherwise nothing changed this poll.
    if (s_prev_down && millis() - s_last_report_ms > kReleaseTimeoutMs) {
      down = false;  // fall through to the release-edge handling below
    } else {
      return Gesture::None;
    }
  }

  Gesture out = Gesture::None;

  if (down && !s_prev_down) {
    // Press edge: start a fresh contact.
    s_contact_start_ms = millis();
    s_gestured = false;
    s_start_x = x;
    s_start_y = y;
    s_last_x = x;
    s_last_y = y;
  } else if (down && s_prev_down) {
    s_last_x = x;
    s_last_y = y;
    if (!s_gestured) {
      const Gesture g = chipGesture(gesture_id);
      if (g != Gesture::None) {
        s_gestured = true;
        out = g;
      }
    }
  } else if (!down && s_prev_down) {
    // Release edge: classify the completed contact.
    const unsigned long dur = millis() - s_contact_start_ms;
    if (!s_gestured && dur <= kTapMaxMs) {
      const int dx = s_last_x - s_start_x;
      const int dy = s_last_y - s_start_y;
      if (std::abs(dx) >= kSwipeMinPx && std::abs(dx) > std::abs(dy)) {
        out = dx < 0 ? Gesture::SwipeLeft : Gesture::SwipeRight;
      } else if (std::abs(dy) >= kSwipeMinPx && std::abs(dy) > std::abs(dx)) {
        out = dy < 0 ? Gesture::SwipeUp : Gesture::SwipeDown;
      } else if (std::abs(dx) <= kTapMaxPx && std::abs(dy) <= kTapMaxPx) {
        out = Gesture::Tap;
      }
    }
  }

  s_prev_down = down;
  return out;
#endif
}

}  // namespace services::touch
