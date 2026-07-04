// ===========================================================================
//  Feather ESP32 V2 + 2.4" TFT FeatherWing (ILI9341 + STMPE610) + UART GPS.
//
//  The SAME App runs here as in the simulator. Only the concrete drivers,
//  wrapped in the HAL interfaces, differ. The ILI9341 already *is* an
//  Adafruit_GFX, so it's handed to App directly.
// ===========================================================================
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <TinyGPSPlus.h>

#include "hal/IGps.h"
#include "hal/ITouch.h"
#include "hal/ITileStore.h"
#include "app.h"

// ---- FeatherWing default pins (2.4" TFT FeatherWing) ---------------------
#define TFT_CS    9
#define TFT_DC   10
#define STMPE_CS  6
#define SD_CS     5

// ---- UART GPS wiring (adjust to how you connected the module) ------------
#define GPS_RX   16   // ESP32 RX  <- GPS TX
#define GPS_TX   17   // ESP32 TX  -> GPS RX
#define GPS_BAUD 9600

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
Adafruit_STMPE610 ts(STMPE_CS);
TinyGPSPlus gpsParser;

// ---- Touch adapter: raw STMPE610 -> display coords -----------------------
class Esp32Touch : public ITouch {
public:
    void begin() override { ts.begin(); }
    TouchPoint get() override {
        TouchPoint tp;
        if (ts.touched()) {
            uint16_t rx, ry; uint8_t rz;
            ts.readData(&rx, &ry, &rz);
            // Rough calibration for landscape 320x240 - tune to your panel.
            tp.x = map(rx, TS_MINX, TS_MAXX, 0, 320);
            tp.y = map(ry, TS_MINY, TS_MAXY, 0, 240);
            tp.touched = true;
        }
        return tp;
    }
private:
    static constexpr int TS_MINX = 150, TS_MAXX = 3800;
    static constexpr int TS_MINY = 130, TS_MAXY = 4000;
};

// ---- GPS adapter: TinyGPSPlus on Serial1 ---------------------------------
class Esp32Gps : public IGps {
public:
    void begin() override {
        Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
    }
    void update() override {
        while (Serial1.available()) gpsParser.encode(Serial1.read());
        if (gpsParser.location.isValid()) {
            _fix.lat = gpsParser.location.lat();
            _fix.lon = gpsParser.location.lng();
            _fix.valid = true;
            if (gpsParser.speed.isValid())  _fix.speed_mps  = gpsParser.speed.mps();
            if (gpsParser.course.isValid()) _fix.course_deg = gpsParser.course.deg();
        }
    }
    GpsFix fix() const override { return _fix; }
private:
    GpsFix _fix;
};

// ---- Tile store on SD: /<z>/<x>/<y>.bin (RGB565 raw) ---------------------
class SdTileStore : public ITileStore {
public:
    bool begin() override { return SD.begin(SD_CS); }
    bool loadTile(int z, int x, int y, uint16_t* out) override {
        char path[48];
        snprintf(path, sizeof(path), "/%d/%d/%d.bin", z, x, y);
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        size_t got = f.read((uint8_t*)out, TILE_WORDS * sizeof(uint16_t));
        f.close();
        return got == TILE_WORDS * sizeof(uint16_t);
    }
};

Esp32Touch  touch;
Esp32Gps    gps;
SdTileStore tiles;
App app(tft, gps, touch, tiles);

void setup() {
    Serial.begin(115200);
    tft.begin();
    app.begin();
}

void loop() {
    app.tick(millis());
    // No artificial delay; App throttles the map redraw to 1 Hz internally.
}
