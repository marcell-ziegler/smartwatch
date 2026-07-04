#pragma once
#include <stdint.h>

// Minimal fix state. Any concrete GPS source (real UART module on hardware,
// a CSV-replay / keyboard-nudged fake on desktop) fills this in.
struct GpsFix {
    bool   valid       = false;
    double lat         = 0.0;
    double lon         = 0.0;
    double speed_mps   = 0.0;
    double course_deg  = 0.0;
};

class IGps {
public:
    virtual ~IGps() = default;
    virtual void   begin()  = 0;
    virtual void   update() = 0;   // pump the parser / advance the sim
    virtual GpsFix fix() const = 0;
};
