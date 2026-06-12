#include "hardware/display.h"

#include "hardware/display_font.h"

#if defined(BOARD_WAVESHARE_43B)
#include "hardware/io_expander_ch422g.h"
#endif

LGFX tft;

void displayInit() {
#if defined(BOARD_WAVESHARE_43B)
  // Backlight + LCD/touch reset hang off the CH422G expander; bring them up
  // before initialising the RGB panel and GT911 touch.
  ch422gInit();
  ch422gResetPanelAndTouch();
  ch422gBacklight(true);
#endif
  tft.init();
  tft.setRotation(0);
  tft.setBrightness(255);  // no-op on the RGB panel (backlight via expander)
  tft.setTextWrap(false);
  displayFontInit();
}
