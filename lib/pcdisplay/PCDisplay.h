#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// Adafruit_GFX sink that renders into an SDL2 window instead of a real
// ILI9341. Construct it with the panel's *native* dimensions (240x320, same as
// Adafruit_ILI9341) so that width()/height() track setRotation() exactly like
// hardware. The window shows the current *logical* (post-rotation) view -- what
// you'd actually see on the panel -- and resizes when the rotation changes.
class PCDisplay : public Adafruit_GFX {
public:
    PCDisplay(int16_t w, int16_t h, int scale);
    ~PCDisplay();

    bool begin(const char* title);
    void present();   // blit the framebuffer to the window; call once per frame

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillScreen(uint16_t color) override;

private:
    int _scale;
    int _stride;      // fb row stride (max of native w/h, fits either rotation)
    int _texW = 0;    // current texture/window logical size
    int _texH = 0;
    SDL_Window*   _win = nullptr;
    SDL_Renderer* _ren = nullptr;
    SDL_Texture*  _tex = nullptr;
    uint16_t* _fb   = nullptr;   // RGB565, indexed in logical coords via _stride
    uint32_t* _rgba = nullptr;   // expanded ARGB8888 scratch for SDL_UpdateTexture
};
