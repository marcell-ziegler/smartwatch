#pragma once
#include <stdint.h>
#include "ClockTime.h"

// Minimal fix state. Any concrete GPS source (real UART module on hardware,
// a CSV-replay / keyboard-nudged fake on desktop) fills this in.
struct GpsFix {
    bool   valid       = false;
    double lat         = 0.0;
    double lon         = 0.0;
    double speed_mps   = 0.0;
    double course_deg  = 0.0;
};

// Wall-clock date + time. On hardware this comes from the GPS module's
// battery-backed RTC, which is usable *before* a position fix (so its validity
// is tracked separately from GpsFix). In the simulator it's the host system
// clock. The date feeds the seasons.csv "what category runs today" lookup.
struct GpsClock {
    bool     valid  = false;
    uint16_t year   = 0;
    uint8_t  month  = 0;   // 1-12
    uint8_t  day    = 0;   // 1-31
    uint8_t  hour   = 0;   // 0-23
    uint8_t  minute = 0;   // 0-59
    uint8_t  second = 0;   // 0-59

    ClockTime timeOfDay() const { return ClockTime{hour, minute}; }
};

class IGps {
public:
    virtual ~IGps() = default;
    virtual void     begin()  = 0;
    virtual void     update() = 0;   // pump the parser / advance the sim
    virtual GpsFix   fix() const = 0;
    virtual GpsClock clock() const = 0;
};
