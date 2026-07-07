#include "ui.h"

namespace
{
    constexpr uint16_t BLACK = 0x0000;
    constexpr uint16_t WHITE = 0xFFFF;
}

std::string toCp437(const std::string &utf8)
{
    std::string out;
    out.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size(); ++i)
    {
        const unsigned char c = (unsigned char)utf8[i];
        if (c < 0x80)
        {
            out.push_back((char)c); // plain ASCII
            continue;
        }
        // The Swedish letters live in the Latin-1 block: 2-byte UTF-8 starting
        // with 0xC3. Map each to its CP437 code point.
        if (c == 0xC3 && i + 1 < utf8.size())
        {
            switch ((unsigned char)utf8[++i])
            {
            case 0xA5: out.push_back((char)0x86); break; // å
            case 0xA4: out.push_back((char)0x84); break; // ä
            case 0xB6: out.push_back((char)0x94); break; // ö
            case 0x85: out.push_back((char)0x8F); break; // Å
            case 0x84: out.push_back((char)0x8E); break; // Ä
            case 0x96: out.push_back((char)0x99); break; // Ö
            default:   out.push_back('?'); break;        // other Latin-1
            }
            continue;
        }
        // Any other lead byte -> placeholder; skip its continuation bytes.
        out.push_back('?');
        while (i + 1 < utf8.size() && ((unsigned char)utf8[i + 1] & 0xC0) == 0x80)
            ++i;
    }
    return out;
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
