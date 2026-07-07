#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>
#include <string>

// Transcode a UTF-8 string to the single-byte encoding the built-in glcdfont
// uses (Code Page 437), so Swedish å/ä/ö/Å/Ä/Ö render as one correct glyph
// instead of two scrambled ones. Plain ASCII passes through unchanged; other
// non-ASCII code points become '?'. Data stays UTF-8 -- only rendering
// transcodes. Call gfx.cp437(true) once so high glyphs map straight through.
std::string toCp437(const std::string &utf8);

// Shared drawing helpers (target-agnostic -- Adafruit_GFX only).

// Draws a labelled button: a filled w*h rectangle at (x, y) with the label
// centered at text size 1. When isSelected is true the button is highlighted
// black-on-white (black text on a white fill); otherwise it is white-on-black.
// The 1px border is drawn in the text colour so an unselected button stays
// visible against a black background.
void drawButton(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                const char *label, bool isSelected);
