#include "app.h"

App::App(Adafruit_GFX& gfx, IGps& gps, ITouch& touch, ITileStore& tiles)
    : _gfx(gfx), _gps(gps), _touch(touch), _tiles(tiles) {}

void App::begin() {
    _tiles.begin();
    _touch.begin();
    _gps.begin();
    _gfx.setRotation(1);       // landscape
    _gfx.fillScreen(0x0000);
}

void App::tick(uint32_t now_ms) {
    (void)now_ms;
    _gps.update();
    TouchPoint tp = _touch.get();
    (void)tp;

    // Your app goes here.
}
