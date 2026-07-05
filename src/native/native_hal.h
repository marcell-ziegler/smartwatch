#pragma once
// ===========================================================================
//  Desktop stand-ins for the HAL interfaces (native build only).
// ===========================================================================
#include <string>
#include <vector>
#include <deque>

#include "hal/IGps.h"
#include "hal/ITouch.h"
#include "hal/ITileStore.h"
#include "hal/IButtons.h"
#include "hal/ITimeTableStore.h"

// ---- GPS: replays assets/tracks/demo.csv (lat,lon,speed_mps,course_deg),
//      or is manually driven with the arrow keys via nudge(). 'R' toggles
//      which source drives the fix. --------------------------------------
class SimGps : public IGps {
public:
    explicit SimGps(const char* csvPath);

    void     begin() override;
    void     update() override;
    GpsFix   fix() const override { return _fix; }
    GpsClock clock() const override;   // reads the host system clock

    void nudge(double dlat, double dlon);
    void toggleReplay();

private:
    GpsFix _fix;
    std::vector<GpsFix> _track;
    size_t   _replayIdx  = 0;
    bool     _replaying  = false;
    uint32_t _lastStepMs = 0;
};

// ---- Buttons: the SDL event loop injects key presses via pushDown(); App
//      drains them through poll(). WASD = d-pad, Enter = Select (see
//      main_native.cpp). ----------------------------------------------------
class SdlButtons : public IButtons {
public:
    void begin() override {}
    std::optional<Button> poll() override {
        if (_queue.empty()) return std::nullopt;
        Button b = _queue.front();
        _queue.pop_front();
        return b;
    }
    // Native-only: called from the SDL event loop on a key-down edge.
    void pushDown(Button b) { _queue.push_back(b); }

private:
    std::deque<Button> _queue;
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

// ---- Timetable CSVs from a plain folder tree (mirrors the SD layout the
//      esp32 build reads). readFile("B/80.csv") -> <root>/B/80.csv. --------
class FolderTimetableStore : public ITimeTableStore {
public:
    explicit FolderTimetableStore(const char* root) : _root(root) {}
    bool begin() override { return true; }
    bool readFile(const char* path, std::string& out) override;

private:
    std::string _root;
};
