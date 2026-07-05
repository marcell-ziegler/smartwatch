#pragma once
// ===========================================================================
//  Tiny dependency-free test framework for the shared C++ logic.
//
//  These tests cover pure code (timetable.cpp + ClockTime.h) with no HAL,
//  Arduino, SDL, or PlatformIO involvement -- just g++ (see the Makefile).
//  CHECK does NOT abort on failure, so one run reports every failing case.
// ===========================================================================
#include <cstdio>

namespace tf
{
    inline int checks = 0;   // C++17 inline vars: one shared instance across TUs
    inline int failures = 0;

    inline void fail(const char *file, int line, const char *expr)
    {
        ++failures;
        std::printf("  FAIL  %s:%d  %s\n", file, line, expr);
    }
}

// Record a boolean expectation. Variadic so braced initializers like
// ClockTime{8, 5} (whose comma the preprocessor would otherwise treat as an
// argument separator) can be passed directly.
#define CHECK(...)                                          \
    do                                                      \
    {                                                       \
        ++tf::checks;                                       \
        if (!(__VA_ARGS__))                                 \
            tf::fail(__FILE__, __LINE__, #__VA_ARGS__);     \
    } while (0)

// Label a group of checks in the output.
#define SECTION(name) std::printf("[%s]\n", name)
