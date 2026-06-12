#pragma once

// CH422G IO expander on the Waveshare ESP32-S3-Touch-LCD-4.3B.
// Controls the LCD backlight, LCD reset and GT911 touch reset lines, which are
// NOT wired to ESP32 GPIO. Compiled only for that board.
#if defined(BOARD_WAVESHARE_43B)

/** Put the CH422G into push-pull output mode. Call after Wire.begin(). */
void ch422gInit();

/** Pulse LCD_RST + TP_RST low then high (run before tft.init()). */
void ch422gResetPanelAndTouch();

/** Turn the LCD backlight on/off via the expander. */
void ch422gBacklight(bool on);

#endif
