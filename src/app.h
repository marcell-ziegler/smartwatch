#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

#include "hal/IGps.h"
#include "hal/ITouch.h"
#include "hal/ITileStore.h"

// The application. Shared verbatim between the esp32 and native build
// targets -- only the concrete IGps/ITouch/ITileStore drivers and the
// Adafruit_GFX sink handed in differ.
class App {
public:
    App(Adafruit_GFX& gfx, IGps& gps, ITouch& touch, ITileStore& tiles);

    void begin();
    void tick(uint32_t now_ms);

private:
    Adafruit_GFX& _gfx;
    IGps&         _gps;
    ITouch&       _touch;
    ITileStore&   _tiles;

    uint32_t _lastMapDraw = 0;

    // Draws the map square in the top-right corner of the screen, centered
    // on (lat, lon): the user stays fixed at the center and the map scrolls
    // beneath them.
    void drawMap(double lat, double lon);
};
