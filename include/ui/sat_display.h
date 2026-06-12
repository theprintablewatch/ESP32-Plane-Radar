#pragma once

namespace ui {

/** Draw the satellite sky plot (zenith centre, horizon rim) + satellites. */
void satDisplayDraw();

/** Redraw the sky plot with the latest satellite list. */
void satDisplayRefresh();

/** Full-screen prompt shown when no N2YO API key is configured yet. */
void satDisplayShowNoKey();

}  // namespace ui
