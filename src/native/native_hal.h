#pragma once
// ===========================================================================
//  Desktop stand-ins for the three HAL interfaces (native build only).
// ===========================================================================
#include <string>
#include <vector>

#include "hal/IGps.h"
#include "hal/ITouch.h"
#include "hal/ITileStore.h"

// ---- GPS: replays assets/tracks/demo.csv (lat,lon,speed_mps,course_deg),
//      or is manually driven with the arrow keys via nudge(). 'R' toggles
//      which source drives the fix. --------------------------------------
class SimGps : public IGps {
public:
    explicit SimGps(const char* csvPath);

    void   begin() override;
    void   update() override;
    GpsFix fix() const override { return _fix; }

    void nudge(double dlat, double dlon);
    void toggleReplay();

private:
    GpsFix _fix;
    std::vector<GpsFix> _track;
    size_t   _replayIdx  = 0;
    bool     _replaying  = false;
    uint32_t _lastStepMs = 0;
};

// ---- Touch: left mouse button over the (scaled) window acts as a finger. -
class SdlTouch : public ITouch {
public:
    explicit SdlTouch(int scale) : _scale(scale) {}
    void       begin() override {}
    TouchPoint get() override;

private:
    int _scale;
};

// ---- Tiles: reads pre-converted .bin tiles from a plain folder tree,
//      mirroring the SD-card layout the esp32 build reads. -----------------
class FolderTileStore : public ITileStore {
public:
    explicit FolderTileStore(const char* root) : _root(root) {}
    bool begin() override;
    bool loadTile(int z, int x, int y, uint16_t* out) override;

private:
    std::string _root;
};
