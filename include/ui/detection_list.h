#pragma once

// Recent-detections side panel for the Waveshare 4.3B (800x480). The round
// 240px display has no room for this, so the whole feature is board-gated.
#if defined(BOARD_WAVESHARE_43B)

#include "hardware/lgfx_config.hpp"

namespace ui {

/** Draw the static panel frame + header into the background surface (sprite). */
void detectionListDrawStatic(lgfx::LovyanGFX& gfx);

/** Draw the current aircraft rows to the live display (call after blit). */
void detectionListDraw();

}  // namespace ui

#endif
