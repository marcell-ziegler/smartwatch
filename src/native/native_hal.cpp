#include "native_hal.h"

#include <Arduino.h>   // millis()
#include <SDL2/SDL.h>
#include <cstdio>

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
