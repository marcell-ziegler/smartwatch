#pragma once
#include <stdint.h>

struct TouchPoint {
    int  x = 0;
    int  y = 0;
    bool touched = false;
};

class ITouch {
public:
    virtual ~ITouch() = default;
    virtual void        begin() = 0;
    virtual TouchPoint  get()   = 0;   // single most-recent touch, or {touched=false}
};
