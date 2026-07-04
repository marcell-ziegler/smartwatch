#pragma once
#include <stdint.h>

// Raw little-endian RGB565 tiles, 256x256, stored/streamed as <z>/<x>/<y>.bin
// (SD card on hardware, a plain folder on desktop). See README "Tiles".
static constexpr int TILE_PX    = 256;
static constexpr int TILE_WORDS = TILE_PX * TILE_PX;

class ITileStore {
public:
    virtual ~ITileStore() = default;
    virtual bool begin() = 0;
    // Fills out[0..TILE_WORDS) with the tile's pixels. Returns false (and
    // leaves out untouched) if the tile isn't present.
    virtual bool loadTile(int z, int x, int y, uint16_t* out) = 0;
};
