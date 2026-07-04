#include "app.h"
#include <math.h>

// Chosen tile zoom level
#define ZOOM 13

namespace
{
    // Map square: top-right corner of the screen.
    constexpr int16_t MAP_W = 120;
    constexpr int16_t MAP_H = 120;
    constexpr uint16_t MARKER_COLOR = 0x001F; // blue, RGB565

    // lat/lon -> world pixel position at the given zoom (Web Mercator). This is
    // a continuous coordinate space -- tile (z,x,y) is just the TILE_PX-sized
    // chunk of it at [x*TILE_PX, y*TILE_PX].
    void lonLatToWorldPx(double lat, double lon, int z, double &px, double &py)
    {
        const double n = (double)(1 << z);
        const double latRad = lat * M_PI / 180.0;
        px = (lon + 180.0) / 360.0 * n * TILE_PX;
        py = (1.0 - asinh(tan(latRad)) / M_PI) / 2.0 * n * TILE_PX;
    }
} // namespace

App::App(Adafruit_GFX &gfx, IGps &gps, ITouch &touch, ITileStore &tiles)
    : _gfx(gfx), _gps(gps), _touch(touch), _tiles(tiles) {}

void App::begin()
{
    _tiles.begin();
    _touch.begin();
    _gps.begin();
    _gfx.setRotation(0); // landscape
    _gfx.fillScreen(0x0000);
}

void App::tick(uint32_t now_ms)
{
    _gps.update();
    TouchPoint tp = _touch.get();
    (void)tp;

    // Map redraw throttled to 1 Hz -- see README's product target.
    if (now_ms - _lastMapDraw >= 1000)
    {
        _lastMapDraw = now_ms;
        const GpsFix fix = _gps.fix();
        if (fix.valid)
            drawMap(fix.lat, fix.lon);
    }
}

void App::drawMap(double lat, double lon)
{
    const int16_t mapX0 = _gfx.width() - MAP_W; // top-right corner
    const int16_t mapY0 = 0;

    double userPx, userPy;
    lonLatToWorldPx(lat, lon, ZOOM, userPx, userPy);

    // World-pixel origin of the viewport: the user is always drawn dead
    // center, so it's the *map* that scrolls beneath them, not the marker
    // that moves across the map.
    const double viewLeft = userPx - MAP_W / 2.0;
    const double viewTop = userPy - MAP_H / 2.0;

    const int tileXMin = (int)floor(viewLeft / TILE_PX);
    const int tileXMax = (int)floor((viewLeft + MAP_W - 1) / TILE_PX);
    const int tileYMin = (int)floor(viewTop / TILE_PX);
    const int tileYMax = (int)floor((viewTop + MAP_H - 1) / TILE_PX);

    // One tile at a time -- a full decoded tile is TILE_WORDS*2 = 128KB.
    // Heap-allocated once and reused: as a `static` array this overflowed
    // the ESP32's fixed DRAM/BSS segment at link time even though the chip
    // has 320KB of RAM overall -- most of that is only reachable via the
    // heap, not the linker's static-data segment.
    static uint16_t *tileBuf = new uint16_t[TILE_WORDS];

    _gfx.startWrite();

    // Background fill first: covers any tile that's missing from the
    // pre-converted ribbon (loadTile() returns false) so stale pixels from
    // a previous draw don't linger there.
    _gfx.writeFillRect(mapX0, mapY0, MAP_W, MAP_H, 0x0000);

    for (int ty = tileYMin; ty <= tileYMax; ++ty)
    {
        for (int tx = tileXMin; tx <= tileXMax; ++tx)
        {
            if (!_tiles.loadTile(ZOOM, tx, ty, tileBuf))
                continue;

            // Screen position of this tile's (0,0) pixel.
            const int16_t tileScreenX =
                (int16_t)lround(mapX0 + (tx * (double)TILE_PX - viewLeft));
            const int16_t tileScreenY =
                (int16_t)lround(mapY0 + (ty * (double)TILE_PX - viewTop));

            // Manual per-pixel clip against the map rectangle: drawRGBBitmap
            // has no source-crop, and the map square's left and bottom edges
            // (mapX0, mapY0+MAP_H) are internal boundaries, not screen edges,
            // so the generic drawPixel clipping alone would let an
            // overhanging tile bleed into the panel around it.
            for (int16_t row = 0; row < TILE_PX; ++row)
            {
                const int16_t sy = tileScreenY + row;
                if (sy < mapY0 || sy >= mapY0 + MAP_H)
                    continue;
                for (int16_t col = 0; col < TILE_PX; ++col)
                {
                    const int16_t sx = tileScreenX + col;
                    if (sx < mapX0 || sx >= mapX0 + MAP_W)
                        continue;
                    _gfx.writePixel(sx, sy, tileBuf[row * TILE_PX + col]);
                }
            }
        }
    }

    // Position marker: always dead center of the map rectangle.
    _gfx.fillCircle(mapX0 + MAP_W / 2, mapY0 + MAP_H / 2, 3, MARKER_COLOR);

    _gfx.endWrite();
}
