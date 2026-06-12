#pragma once

#include <cstdint>

namespace ui::radar {

#if defined(BOARD_WAVESHARE_43B)

// 800x480 panel: radar circle (~400px) in the left region, the rest of the
// surface is the recent-detections list (see ui/detection_list).
constexpr int kSpriteW = 800;  // background sprite / draw surface = full screen
constexpr int kSpriteH = 480;
constexpr int kSize = 480;     // radar region (square) — round-only callers
constexpr int kCenterX = 235;  // radar centered in the left ~470px column
constexpr int kCenterY = 240;
constexpr int kGridOuterRadius = 195;
/** Scale px-tuned symbols/fonts up from the 240px round design. */
constexpr float kUiScale = 1.85f;

#else

constexpr int kSize = 240;
constexpr int kSpriteW = kSize;  // round display: surface == radar
constexpr int kSpriteH = kSize;
constexpr int kCenterX = kSize / 2;
constexpr int kCenterY = kSize / 2;
/** Outermost grid ring (inside edge labels). */
constexpr int kGridOuterRadius = 107;
constexpr float kUiScale = 1.0f;  // 1.0 -> all scaled() values are byte-identical

#endif

/** Round px values to ints scaled by kUiScale (identity when scale == 1.0). */
constexpr int scaledPx(int v) {
  return static_cast<int>(static_cast<float>(v) * kUiScale + 0.5f);
}

/** N: offset from top edge (top_center, negative = up). */
constexpr int kCardinalNorthOffsetY = -1;
/** S: offset from bottom edge (bottom_center, positive = down). */
constexpr int kCardinalSouthOffsetY = 3;

/** Gap between scale label right edge and outer ring on the east spoke (px). */
constexpr int kScaleGapFromOuterRing = scaledPx(6);

/** Target cap height (px) for N/S/E/W. */
constexpr int kCardinalLabelHeightPx = scaledPx(14);
/** Scale label is this many px shorter than cardinals. */
constexpr int kScaleBelowCardinalPx = scaledPx(3);

constexpr int kRingCount = 4;

/** Shared grid stroke: drawWideLine half-width (~2 px total); rings use the same px count. */
constexpr float kGridStrokeHalfWidth = 1.0f * kUiScale;

constexpr int kCenterDotRadius = scaledPx(2);

/** Filled aircraft symbol (nose triangle). */
constexpr int kAircraftNoseLenPx = scaledPx(8);
constexpr int kAircraftTailLenPx = scaledPx(3);
constexpr int kAircraftTailHalfPx = scaledPx(4);
/** Track vector: ground distance covered in this many seconds at current gs. */
constexpr float kAircraftTrackHorizonSec = 60.0f;
/** Minimum visible vector when gs > 0 (px). */
constexpr int kAircraftSpeedLineMinPx = scaledPx(2);
/** Track line length uses this outer_km, not the active range preset.
 *  (Length scales with kGridOuterRadius, so no per-board change needed here.) */
constexpr float kAircraftTrackRefOuterKm = 13.3f;
/** Shorter than full 60 s horizon at ref scale; ×1.5 length boost applied. */
constexpr float kAircraftTrackLengthScale = 1.5f / 5.0f;
/** drawWideLine half-width for speed vectors (~2 px total). */
constexpr float kAircraftTrackLineHalfWidth = 1.0f * kUiScale;
/** Gap from triangle edge to tag block (px). */
constexpr int kAircraftLabelGapPx = scaledPx(1);
/** Keep symbol centroid inside outer ring by at least this inset (px). */
constexpr int kAircraftInsideRingInsetPx =
    kAircraftNoseLenPx + kAircraftTailHalfPx + 1;

/** Beyond-ring traffic: bearing cues on screen rim (correct direction, fixed radius). */
constexpr int kBeyondRingDotRadiusPx = scaledPx(4);
constexpr int kBeyondRingScreenMarginPx = scaledPx(2);
/** Target cap height (px) for aircraft tags (bold, slightly above scale label). */
constexpr int kAircraftTagLabelHeightPx = scaledPx(13);

/** RGB565 palette targets (applied in initPalette). */
constexpr uint8_t kBgR = 4;
constexpr uint8_t kBgG = 10;
constexpr uint8_t kBgB = 28;
constexpr uint8_t kGridR = 16;
constexpr uint8_t kGridG = 100;
constexpr uint8_t kGridB = 32;
constexpr uint8_t kAircraftR = 255;
constexpr uint8_t kAircraftG = 0;
constexpr uint8_t kAircraftB = 0;
constexpr uint8_t kTrackR = 255;
constexpr uint8_t kTrackG = 0;
constexpr uint8_t kTrackB = 255;
constexpr uint8_t kTagTypeR = 255;
constexpr uint8_t kTagTypeG = 200;
constexpr uint8_t kTagTypeB = 0;
constexpr uint8_t kTagAltR = 90;
constexpr uint8_t kTagAltG = 200;
constexpr uint8_t kTagAltB = 255;

extern uint16_t kColorBackground;
extern uint16_t kColorGrid;
extern uint16_t kColorLabel;
extern uint16_t kColorCenter;
extern uint16_t kColorAircraft;
extern uint16_t kColorTrackVector;
extern uint16_t kColorTagType;
extern uint16_t kColorTagAltitude;

}  // namespace ui::radar
