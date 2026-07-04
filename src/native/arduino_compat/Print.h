#pragma once
// ===========================================================================
//  Minimal stand-in for Arduino's Print.h (native builds only).
//  Adafruit_GFX derives from this and only calls write(uint8_t); everything
//  else (print/println overloads) is implemented here in terms of write()
//  exactly like the real Arduino core, so app code that calls gfx.print(...)
//  behaves the same on both targets.
// ===========================================================================
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "WString.h"

class __FlashStringHelper;

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;

    virtual size_t write(const uint8_t* buf, size_t size) {
        size_t n = 0;
        for (; n < size; ++n) write(buf[n]);
        return n;
    }
    size_t write(const char* str) {
        if (!str) return 0;
        return write((const uint8_t*)str, strlen(str));
    }

    size_t print(const char* str)   { return write(str); }
    size_t print(char c)            { return write((uint8_t)c); }
    size_t print(const String& s)   { return write(s.c_str()); }
    size_t print(const __FlashStringHelper* f) { return write((const char*)f); }

    size_t print(int n)              { return printNumber((long)n, 10); }
    size_t print(unsigned int n)     { return printNumber((unsigned long)n); }
    size_t print(long n)             { return printNumber(n, 10); }
    size_t print(unsigned long n)    { return printNumber(n); }
    size_t print(double n, int digits = 2) { return printFloat(n, digits); }

    size_t println() { return write("\r\n"); }
    size_t println(const char* str)  { size_t n = print(str); return n + println(); }
    size_t println(char c)           { size_t n = print(c);   return n + println(); }
    size_t println(const String& s)  { size_t n = print(s);   return n + println(); }
    size_t println(int n)            { size_t r = print(n); return r + println(); }
    size_t println(unsigned int n)   { size_t r = print(n); return r + println(); }
    size_t println(long n)           { size_t r = print(n); return r + println(); }
    size_t println(unsigned long n)  { size_t r = print(n); return r + println(); }
    size_t println(double n, int digits = 2) { size_t r = print(n, digits); return r + println(); }

private:
    size_t printNumber(long n, uint8_t base) {
        char buf[24];
        snprintf(buf, sizeof(buf), base == 16 ? "%lx" : "%ld", n);
        return write(buf);
    }
    size_t printNumber(unsigned long n) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lu", n);
        return write(buf);
    }
    size_t printFloat(double n, int digits) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", digits, n);
        return write(buf);
    }
};
