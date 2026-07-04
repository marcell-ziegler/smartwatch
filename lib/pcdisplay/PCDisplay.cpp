#include "PCDisplay.h"
#include <SDL2/SDL.h>
#include <cstdlib>
#include <cstring>

PCDisplay::PCDisplay(int16_t w, int16_t h, int scale)
    : Adafruit_GFX(w, h), _scale(scale) {
    _fb   = (uint16_t*)calloc((size_t)w * h, sizeof(uint16_t));
    _rgba = (uint32_t*)calloc((size_t)w * h, sizeof(uint32_t));
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
    _win = SDL_CreateWindow(title,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            WIDTH * _scale, HEIGHT * _scale, 0);
    if (!_win) return false;
    _ren = SDL_CreateRenderer(_win, -1, SDL_RENDERER_ACCELERATED);
    if (!_ren) return false;
    _tex = SDL_CreateTexture(_ren, SDL_PIXELFORMAT_ARGB8888,
                             SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    return _tex != nullptr;
}

void PCDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
    // Respect GFX rotation exactly like a real driver does.
    int16_t w = WIDTH, h = HEIGHT;
    switch (rotation) {
        case 1: { int16_t t = x; x = w - 1 - y; y = t; break; }
        case 2: { x = w - 1 - x; y = h - 1 - y; break; }
        case 3: { int16_t t = x; x = y;         y = h - 1 - t; break; }
        default: break;
    }
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    _fb[(size_t)y * w + x] = color;
}

void PCDisplay::fillScreen(uint16_t color) {
    const size_t n = (size_t)WIDTH * HEIGHT;
    for (size_t i = 0; i < n; ++i) _fb[i] = color;
}

void PCDisplay::present() {
    const size_t n = (size_t)WIDTH * HEIGHT;
    for (size_t i = 0; i < n; ++i) {
        uint16_t c = _fb[i];
        uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
        r = (r << 3) | (r >> 2);
        g = (g << 2) | (g >> 4);
        b = (b << 3) | (b >> 2);
        _rgba[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    SDL_UpdateTexture(_tex, nullptr, _rgba, WIDTH * sizeof(uint32_t));
    SDL_RenderClear(_ren);
    SDL_RenderCopy(_ren, _tex, nullptr, nullptr);
    SDL_RenderPresent(_ren);
}
