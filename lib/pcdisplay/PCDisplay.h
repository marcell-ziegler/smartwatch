#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// Adafruit_GFX sink that renders into an SDL2 window instead of a real
// ILI9341, so app code compiled against Adafruit_GFX is pixel-identical
// between the desktop simulator and hardware.
class PCDisplay : public Adafruit_GFX {
public:
    static constexpr int16_t WIDTH  = 320;
    static constexpr int16_t HEIGHT = 240;

    PCDisplay(int16_t w, int16_t h, int scale);
    ~PCDisplay();

    bool begin(const char* title);
    void present();   // blit the framebuffer to the window; call once per frame

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillScreen(uint16_t color) override;

private:
    int _scale;
    SDL_Window*   _win = nullptr;
    SDL_Renderer* _ren = nullptr;
    SDL_Texture*  _tex = nullptr;
    uint16_t* _fb   = nullptr;   // RGB565 framebuffer, WIDTH*HEIGHT
    uint32_t* _rgba = nullptr;   // expanded ARGB8888 scratch for SDL_UpdateTexture
};
