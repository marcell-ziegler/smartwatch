#include "ui.h"

namespace
{
    constexpr uint16_t BLACK = 0x0000;
    constexpr uint16_t WHITE = 0xFFFF;
}

void drawButton(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                const char *label, bool isSelected)
{
    const uint16_t bg = isSelected ? WHITE : BLACK;
    const uint16_t fg = isSelected ? BLACK : WHITE;

    gfx.fillRect(x, y, w, h, bg);
    gfx.drawRect(x, y, w, h, fg);

    // Center the label. getTextBounds gives the real glyph box (and works for
    // a custom GFXfont later, not just the built-in 6x8 font).
    gfx.setTextSize(1);
    gfx.setTextColor(fg);
    int16_t bx, by;
    uint16_t bw, bh;
    gfx.getTextBounds(label, 0, 0, &bx, &by, &bw, &bh);
    const int16_t cx = x + (w - (int16_t)bw) / 2 - bx;
    const int16_t cy = y + (h - (int16_t)bh) / 2 - by;
    gfx.setCursor(cx, cy);
    gfx.print(label);
}
