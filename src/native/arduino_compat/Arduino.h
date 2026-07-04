#pragma once
// ===========================================================================
//  Arduino-compat shim (native builds only)
//  Provides the sliver of the Arduino core that base Adafruit_GFX depends on:
//  fixed-width types, the PROGMEM/pgm_read_* macros, min/max/abs, a tiny
//  Print base class and a minimal String. NOT a full Arduino core - just
//  enough to compile GFX pixel-identically on the desktop.
// ===========================================================================
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <chrono>

// Any STL header a native-only .cpp might pull in must be fully parsed
// BEFORE min/max/abs become macros below -- those macros shadow
// identically-named functions/parameters used inside libstdc++ headers
// (std::min, std::abs, std::vector<bool>'s internals, etc.). Since this
// header is transitively included by every native TU (via Adafruit_GFX.h),
// pre-including the common containers here means whichever file first drags
// one in doesn't matter -- it's already resolved.
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>

#include "WString.h"
#include "Print.h"

typedef bool     boolean;
typedef uint8_t  byte;

// --- PROGMEM is a no-op on a normal desktop -------------------------------
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif

// pgm_read_* just dereference real memory here. Define pointer variant first
// so GFX's own (32-bit-truncating) fallback is skipped -> 64-bit safe.
#ifndef pgm_read_byte
#define pgm_read_byte(a)    (*(const uint8_t  *)(a))
#endif
#ifndef pgm_read_word
#define pgm_read_word(a)    (*(const uint16_t *)(a))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(a)   (*(const uint32_t *)(a))
#endif
#ifndef pgm_read_pointer
#define pgm_read_pointer(a) (*(void * const *)(a))
#endif

// --- math helpers Arduino code expects as macros --------------------------
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef abs
#define abs(x) ((x) > 0 ? (x) : -(x))
#endif
#ifndef constrain
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#endif
#ifndef radians
#define radians(deg) ((deg) * M_PI / 180.0)
#endif
#ifndef degrees
#define degrees(rad) ((rad) * 180.0 / M_PI)
#endif
#ifndef _BV
#define _BV(b) (1UL << (b))
#endif

// --- Flash-string helper --------------------------------------------------
class __FlashStringHelper;
#ifndef F
#define F(str) (reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#endif

// --- timing (used by native main / sketches, not by GFX) ------------------
static inline uint32_t millis() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
static inline void delay(uint32_t) {}   // no-op on desktop
