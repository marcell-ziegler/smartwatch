#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// Shared drawing helpers (target-agnostic -- Adafruit_GFX only).

// Draws a labelled button: a filled w*h rectangle at (x, y) with the label
// centered at text size 1. When isSelected is true the button is highlighted
// black-on-white (black text on a white fill); otherwise it is white-on-black.
// The 1px border is drawn in the text colour so an unselected button stays
// visible against a black background.
void drawButton(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                const char *label, bool isSelected);
