#include "PCDisplay.h"
#include <SDL2/SDL.h>
#include <cstdlib>

PCDisplay::PCDisplay(int16_t w, int16_t h, int scale)
    : Adafruit_GFX(w, h), _scale(scale) {
    _stride = (w > h) ? w : h;   // fits the wider dimension of either rotation
    _fb   = (uint16_t*)calloc((size_t)_stride * _stride, sizeof(uint16_t));
    _rgba = (uint32_t*)calloc((size_t)_stride * _stride, sizeof(uint32_t));
}

PCDisplay::~PCDisplay() {
    if (_tex) SDL_DestroyTexture(_tex);
    if (_ren) SDL_DestroyRenderer(_ren);
    if (_win) SDL_DestroyWindow(_win);
    free(_fb);
    free(_rgba);
}

bool PCDisplay::begin(const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    // Sized to the current logical dims; present() resizes on rotation change.
    _win = SDL_CreateWindow(title,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            width() * _scale, height() * _scale, 0);
    if (!_win) return false;
    _ren = SDL_CreateRenderer(_win, -1, SDL_RENDERER_ACCELERATED);
    return _ren != nullptr;   // texture is created lazily in present()
}

void PCDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
    // Store in logical (current-rotation) coordinates -- Adafruit_GFX already
    // hands us post-rotation coords, and the window shows this view directly,
    // matching what appears on the panel.
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    _fb[(size_t)y * _stride + x] = color;
}

void PCDisplay::fillScreen(uint16_t color) {
    for (int y = 0; y < height(); ++y)
        for (int x = 0; x < width(); ++x)
            _fb[(size_t)y * _stride + x] = color;
}

void PCDisplay::present() {
    const int lw = width(), lh = height();

    // (Re)create the texture and resize the window if the logical size changed
    // (e.g. after setRotation swaps width/height).
    if (!_tex || lw != _texW || lh != _texH) {
        if (_tex) SDL_DestroyTexture(_tex);
        _tex = SDL_CreateTexture(_ren, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, lw, lh);
        _texW = lw;
        _texH = lh;
        SDL_SetWindowSize(_win, lw * _scale, lh * _scale);
    }

    for (int y = 0; y < lh; ++y) {
        for (int x = 0; x < lw; ++x) {
            uint16_t c = _fb[(size_t)y * _stride + x];
            uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            _rgba[(size_t)y * lw + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    SDL_UpdateTexture(_tex, nullptr, _rgba, lw * sizeof(uint32_t));
    SDL_RenderClear(_ren);
    SDL_RenderCopy(_ren, _tex, nullptr, nullptr);
    SDL_RenderPresent(_ren);
}
