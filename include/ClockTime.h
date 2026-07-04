#pragma once
#include <stdint.h>

// A wall-clock time-of-day, no date attached. Scoped to a single service
// day (see timetable data model discussion) -- arithmetic wraps silently
// across midnight in both directions rather than tracking day rollover.
//
// Fields are public and assumed to already be in range (hour < 24,
// minute < 60); construct out-of-range results via the arithmetic below
// or fromTotalMinutes(), not by hand-setting hour/minute past their range.
struct ClockTime
{
    uint8_t hours = 0;
    uint8_t minutes = 0;

    static constexpr int MINUTES_PER_DAY = 24 * 60;

    constexpr int totalMinutes() const { return (int)hours * 60 + (int)minutes; }

    // Normalizes an arbitrary (possibly negative, possibly >= 1440) minute
    // count into a wrapped time-of-day.
    static constexpr ClockTime fromTotalMinutes(int mins)
    {
        int m = mins % MINUTES_PER_DAY;
        if (m < 0)
            m += MINUTES_PER_DAY;
        return ClockTime{(uint8_t)(m / 60), (uint8_t)(m % 60)};
    }

    static constexpr ClockTime fromHoursAndMinutes(int hours, int minutes)
    {
        int totalMinutes = hours * 60 + minutes;
        return ClockTime::fromTotalMinutes(totalMinutes);
    }

    // Shift this time by a signed number of minutes, wrapping silently at
    // midnight in either direction (e.g. 23:50 + 20min -> 00:10,
    // 00:10 - 20min -> 23:50).
    ClockTime operator+(int minutesDelta) const
    {
        return fromTotalMinutes(totalMinutes() + minutesDelta);
    }
    ClockTime operator-(int minutesDelta) const
    {
        return fromTotalMinutes(totalMinutes() - minutesDelta);
    }
    ClockTime &operator+=(int minutesDelta) { return *this = *this + minutesDelta; }
    ClockTime &operator-=(int minutesDelta) { return *this = *this - minutesDelta; }

    // Signed difference between two times-of-day, in minutes. NOT wrapped:
    // a duration between two clock readings is ambiguous across midnight
    // (is 23:50 -> 00:10 "+20min" or "-1430min"?), so this is the plain
    // same-day delta -- pick the direction that makes sense at the call site.
    int operator-(const ClockTime &other) const
    {
        return totalMinutes() - other.totalMinutes();
    }

    bool operator==(const ClockTime &o) const { return totalMinutes() == o.totalMinutes(); }
    bool operator!=(const ClockTime &o) const { return !(*this == o); }
    bool operator<(const ClockTime &o) const { return totalMinutes() < o.totalMinutes(); }
    bool operator<=(const ClockTime &o) const { return totalMinutes() <= o.totalMinutes(); }
    bool operator>(const ClockTime &o) const { return totalMinutes() > o.totalMinutes(); }
    bool operator>=(const ClockTime &o) const { return totalMinutes() >= o.totalMinutes(); }
};
