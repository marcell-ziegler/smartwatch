#include "native_hal.h"

// STL headers must precede <Arduino.h>: the shim's min/max/abs macros clobber
// libstdc++ internals if they're active when these are parsed.
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include "timetable.h"   // utcToStockholm (also pulls STL; keep before Arduino.h)

#include <SDL2/SDL.h>
#include <Arduino.h>   // millis()

// ---------------------------------------------------------------------------
//  SimGps
// ---------------------------------------------------------------------------
SimGps::SimGps(const char* csvPath) {
    FILE* f = fopen(csvPath, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        GpsFix row;
        double lat, lon, spd = 0, crs = 0;
        int got = sscanf(line, "%lf,%lf,%lf,%lf", &lat, &lon, &spd, &crs);
        if (got >= 2) {
            row.valid = true;
            row.lat = lat;
            row.lon = lon;
            row.speed_mps = spd;
            row.course_deg = crs;
            _track.push_back(row);
        }
    }
    fclose(f);
}

void SimGps::begin() {
    // Start parked at the first replay point (if any) even before 'R' is hit,
    // so the map has something sensible to centre on immediately.
    if (!_track.empty()) _fix = _track[0];
}

void SimGps::update() {
    if (!_replaying || _track.empty()) return;
    uint32_t now = millis();
    if (now - _lastStepMs < 1000) return;   // one row per second
    _lastStepMs = now;
    _fix = _track[_replayIdx];
    _replayIdx = (_replayIdx + 1) % _track.size();
}

void SimGps::nudge(double dlat, double dlon) {
    _replaying = false;   // manual driving overrides replay
    if (!_fix.valid) _fix.valid = true;
    _fix.lat += dlat;
    _fix.lon += dlon;
}

void SimGps::toggleReplay() {
    _replaying = !_replaying;
    if (_replaying) _lastStepMs = 0;   // step immediately on the next update()
}

GpsClock SimGps::clock() const {
    // Stand-in for the module's RTC: read the host clock as UTC (like the real
    // GPS) then convert to Stockholm local, so the sim matches hardware
    // regardless of the dev machine's own timezone.
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    GpsClock c;
    c.valid  = true;
    c.year   = (uint16_t)(tm.tm_year + 1900);
    c.month  = (uint8_t)(tm.tm_mon + 1);
    c.day    = (uint8_t)tm.tm_mday;
    c.hour   = (uint8_t)tm.tm_hour;
    c.minute = (uint8_t)tm.tm_min;
    c.second = (uint8_t)tm.tm_sec;
    return utcToStockholm(c);
}

// ---------------------------------------------------------------------------
//  SdlTouch
// ---------------------------------------------------------------------------
TouchPoint SdlTouch::get() {
    TouchPoint tp;
    int x, y;
    uint32_t buttons = SDL_GetMouseState(&x, &y);
    if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        tp.x = x / _scale;
        tp.y = y / _scale;
        tp.touched = true;
    }
    return tp;
}

// ---------------------------------------------------------------------------
//  FolderTileStore
// ---------------------------------------------------------------------------
bool FolderTileStore::begin() {
    return true;   // missing tiles are handled per-lookup in loadTile()
}

bool FolderTileStore::loadTile(int z, int x, int y, uint16_t* out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%d/%d/%d.bin", _root.c_str(), z, x, y);
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(out, sizeof(uint16_t), TILE_WORDS, f);
    fclose(f);
    return got == (size_t)TILE_WORDS;
}

// ---------------------------------------------------------------------------
//  FolderTimetableStore
// ---------------------------------------------------------------------------
bool FolderTimetableStore::readFile(const char* path, std::string& out) {
    std::ifstream f(_root + "/" + path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
