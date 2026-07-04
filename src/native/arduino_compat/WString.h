#pragma once
// ===========================================================================
//  Minimal stand-in for Arduino's WString.h (native builds only).
//  Adafruit_GFX only needs .length() and .c_str(); this wraps std::string so
//  the rest of the usual String API (concatenation, construction from
//  numbers) comes along for free for any app code that wants it.
// ===========================================================================
#include <string>
#include <stdio.h>

class String {
public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(char c) : _s(1, c) {}
    String(int n)            { char b[24]; snprintf(b, sizeof(b), "%d", n);  _s = b; }
    String(unsigned int n)   { char b[24]; snprintf(b, sizeof(b), "%u", n);  _s = b; }
    String(long n)           { char b[24]; snprintf(b, sizeof(b), "%ld", n); _s = b; }
    String(unsigned long n)  { char b[24]; snprintf(b, sizeof(b), "%lu", n); _s = b; }
    String(double n, int digits = 2) {
        char b[32]; snprintf(b, sizeof(b), "%.*f", digits, n); _s = b;
    }

    size_t     length() const { return _s.size(); }
    const char* c_str() const { return _s.c_str(); }

    String& operator+=(const String& o) { _s += o._s; return *this; }
    String  operator+(const String& o) const { return String((_s + o._s).c_str()); }

    bool operator==(const String& o) const { return _s == o._s; }
    char operator[](size_t i) const { return _s[i]; }

private:
    std::string _s;
};
