// ===========================================================================
//  Feather ESP32 V2 + 2.4" TFT FeatherWing: ILI9341 display over SPI, TSC2007
//  touch + PA1010D GPS over I2C (STEMMA QT), buttons on GPIO.
//
//  The SAME App runs here as in the simulator. Only the concrete drivers,
//  wrapped in the HAL interfaces, differ. The ILI9341 already *is* an
//  Adafruit_GFX, so it's handed to App directly.
//
//  Pin map (Feather ESP32 V2 -- verified against the V2 pinout):
//    SPI bus (shared): SCK 5, MOSI 19, MISO 21   (hardware pins, not #defined)
//    Display:  TFT_CS 15, TFT_DC 33
//    SD card:  SD_CS 14
//    Buttons:  Up 27, Down 4, Left 13, Right 25, Select 26
//    I2C (STEMMA QT): SDA 22, SCL 20 -- TSC2007 touch @ 0x48, PA1010D GPS
//                     @ 0x10, MAX17048 gauge. (GPIO 32 is now free.)
//  Avoided: 6-11 (flash), 34/36/39/37 (input-only), 0/2/12 (strapping).
// ===========================================================================
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_TSC2007.h>
#include <Adafruit_GPS.h>

#include "hal/IGps.h"
#include "hal/ITouch.h"
#include "hal/ITileStore.h"
#include "hal/IButtons.h"
#include "hal/ITimeTableStore.h"
#include "app.h"

// ---- SPI peripheral chip-selects + display data/command ------------------
#define TFT_CS 15
#define TFT_DC 33
#define SD_CS 14

// ---- I2C peripheral addresses (STEMMA QT / Wire bus) ---------------------
#define GPS_I2C_ADDR 0x10 // PA1010D

// ---- 5-way d-pad: active-low with internal pull-ups ----------------------
#define BTN_UP 27
#define BTN_DOWN 4
#define BTN_LEFT 13
#define BTN_RIGHT 25
#define BTN_SELECT 26

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
Adafruit_TSC2007 ts;     // TSC2007 touch over I2C (2.4" FeatherWing, newer rev)
Adafruit_GPS GPS(&Wire); // PA1010D over I2C

// ---- Touch adapter: TSC2007 over I2C -> display coords -------------------
class Esp32Touch : public ITouch
{
public:
    void begin() override { ts.begin(); } // I2C @ 0x48 on the STEMMA QT bus
    TouchPoint get() override
    {
        TouchPoint tp;
        uint16_t rx, ry, rz1, rz2;
        // z1 is the touch pressure; ~0 when not touched.
        if (ts.read_touch(&rx, &ry, &rz1, &rz2) && rz1 > TOUCH_Z_MIN)
        {
            // Raw 12-bit ADC -> landscape 320x240. TODO: calibrate to the panel.
            tp.x = map(rx, TS_MINX, TS_MAXX, 0, 320);
            tp.y = map(ry, TS_MINY, TS_MAXY, 0, 240);
            tp.touched = true;
        }
        return tp;
    }

private:
    static constexpr int TS_MINX = 130, TS_MAXX = 3900;
    static constexpr int TS_MINY = 130, TS_MAXY = 3900;
    static constexpr int TOUCH_Z_MIN = 100; // pressure floor for a real touch
};

// ---- GPS adapter: Adafruit_GPS (PA1010D) over I2C ------------------------
class Esp32Gps : public IGps
{
public:
    void begin() override
    {
        GPS.begin(GPS_I2C_ADDR);                       // also brings up Wire
        GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);  // position + date/time
        GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    }
    void update() override
    {
        // Pump bytes from the I2C GPS and parse any completed sentence. read()
        // mostly pulls from an internal buffer, refilled over I2C in chunks.
        for (int i = 0; i < 64; ++i)
        {
            GPS.read();
            if (GPS.newNMEAreceived())
                GPS.parse(GPS.lastNMEA());
        }
        _fix.valid = GPS.fix; // clears when a fix is lost -> tracking freezes
        if (GPS.fix)
        {
            _fix.lat = GPS.latitudeDegrees;
            _fix.lon = GPS.longitudeDegrees;
            _fix.speed_mps = GPS.speed * 0.514444f; // knots -> m/s
            _fix.course_deg = GPS.angle;
        }
    }
    GpsFix fix() const override { return _fix; }

    GpsClock clock() const override
    {
        // The module's battery-backed RTC feeds RMC date/time even before a
        // position fix, so this is independent of GPS.fix. The PA1010D reports
        // UTC; convert to Stockholm local (CET/CEST with DST) for display/use.
        GpsClock c;
        if (GPS.year != 0) // set once a dated sentence has been parsed
        {
            c.valid = true;
            c.year = 2000 + GPS.year;
            c.month = GPS.month;
            c.day = GPS.day;
            c.hour = GPS.hour;
            c.minute = GPS.minute;
            c.second = GPS.seconds;
        }
        return utcToStockholm(c);
    }

private:
    GpsFix _fix;
};

// ---- Buttons: 5-way d-pad on GPIO, active-low with internal pull-ups. -----
class Esp32Buttons : public IButtons
{
public:
    void begin() override
    {
        for (auto &b : _btns)
            pinMode(b.pin, INPUT_PULLUP);
    }
    std::optional<Button> poll() override
    {
        // Falling edge (pressed pulls the pin LOW) = one press. Returns the
        // first new press found; other pins' states are still refreshed so no
        // edge is lost across calls.
        // TODO: add debouncing (a few-ms settle filter) for real switches.
        std::optional<Button> hit;
        for (auto &b : _btns)
        {
            const bool down = digitalRead(b.pin) == LOW;
            if (down && !b.prev && !hit)
                hit = b.button;
            b.prev = down;
        }
        return hit;
    }

private:
    struct Btn
    {
        uint8_t pin;
        Button button;
        bool prev;
    };
    Btn _btns[5] = {
        {BTN_UP, Button::Up, false},
        {BTN_DOWN, Button::Down, false},
        {BTN_LEFT, Button::Left, false},
        {BTN_RIGHT, Button::Right, false},
        {BTN_SELECT, Button::Select, false},
    };
};

// ---- Tile store on SD: /<z>/<x>/<y>.bin (RGB565 raw) ---------------------
class SdTileStore : public ITileStore
{
public:
    bool begin() override { return SD.begin(SD_CS); }
    bool loadTile(int z, int x, int y, uint16_t *out) override
    {
        char path[48];
        snprintf(path, sizeof(path), "/%d/%d/%d.bin", z, x, y);
        File f = SD.open(path, FILE_READ);
        if (!f)
            return false;
        size_t got = f.read((uint8_t *)out, TILE_WORDS * sizeof(uint16_t));
        f.close();
        return got == TILE_WORDS * sizeof(uint16_t);
    }
};

// ---- Timetable CSVs on SD: /timetables/<path> ---------------------------
class SdTimetableStore : public ITimeTableStore
{
public:
    bool begin() override { return SD.begin(SD_CS); } // idempotent; tiles may init too
    bool readFile(const char *path, std::string &out) override
    {
        char full[96];
        snprintf(full, sizeof(full), "/timetables/%s", path);
        File f = SD.open(full, FILE_READ);
        if (!f)
            return false;
        out.clear();
        out.reserve(f.size());
        while (f.available())
            out += (char)f.read();
        f.close();
        return true;
    }
};

Esp32Touch touch;
Esp32Gps gps;
SdTileStore tiles;
Esp32Buttons buttons;
SdTimetableStore timetables;
App app(tft, gps, touch, tiles, buttons, timetables);

void setup()
{
    Serial.begin(115200);
    tft.begin();
    app.begin();
}

void loop()
{
    app.tick(millis());
    // No artificial delay; App throttles the map redraw to 1 Hz internally.
}
