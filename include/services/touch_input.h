#pragma once

namespace services::touch {

enum class Gesture {
  None,
  Tap,
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown,
};

/** Reset the CST816S, start its I2C bus, probe for the chip. Call once. */
void init();

/** Poll the touch panel. Returns one gesture per physical action (Tap on a
 *  release with no drag; a Swipe* as soon as a horizontal/vertical drag is
 *  seen). Call every loop iteration. */
Gesture poll();

}  // namespace services::touch
