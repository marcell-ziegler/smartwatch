#include "framework.h"
#include "timetable.h"

using utils::parseClockTime;
using utils::parseTimeToMinutes;

void test_time_parsing()
{
    SECTION("parseClockTime");

    // valid, both 1- and 2-digit hours
    CHECK(parseClockTime("08:05") == (ClockTime{8, 5}));
    CHECK(parseClockTime("8:05") == (ClockTime{8, 5}));
    CHECK(parseClockTime("00:00") == (ClockTime{0, 0}));
    CHECK(parseClockTime("23:59") == (ClockTime{23, 59}));

    // out-of-range hour/minute rejected (authoring typos surface here)
    CHECK(!parseClockTime("24:00"));
    CHECK(!parseClockTime("08:60"));
    CHECK(!parseClockTime("99:99"));

    // structurally malformed
    CHECK(!parseClockTime(""));       // empty
    CHECK(!parseClockTime("0805"));   // no colon -- regression: used to THROW
    CHECK(!parseClockTime("8:0x"));   // trailing junk in minutes
    CHECK(!parseClockTime("x:00"));   // junk hour
    CHECK(!parseClockTime("8:"));     // missing minutes
    CHECK(!parseClockTime(":05"));    // missing hour
    CHECK(!parseClockTime("8:0:0"));  // extra colon

    SECTION("parseTimeToMinutes");
    CHECK(parseTimeToMinutes("08:05") == 485);
    CHECK(parseTimeToMinutes("00:00") == 0);
    CHECK(parseTimeToMinutes("23:59") == 1439);
    CHECK(!parseTimeToMinutes("24:00"));
    CHECK(!parseTimeToMinutes("nope"));
}
