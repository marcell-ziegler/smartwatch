#include "framework.h"
#include "timetable.h"

namespace
{
    SeasonalTimetableRule rule(const char *from, const char *to, int dow, const char *cat)
    {
        SeasonalTimetableRule r;
        r.validFrom = from;
        r.validTo = to;
        r.dayOfWeek = dow;
        r.trafficCategory = cat;
        return r;
    }
}

void test_dates()
{
    SECTION("isoDayOfWeek");
    CHECK(isoDayOfWeek(2026, 7, 5) == 7);   // Sunday ("Går söndag 5 juli")
    CHECK(isoDayOfWeek(2026, 7, 1) == 3);   // Wednesday
    CHECK(isoDayOfWeek(2026, 1, 1) == 4);   // Thursday
    CHECK(isoDayOfWeek(2024, 2, 29) == 4);  // leap day, Thursday
    CHECK(isoDayOfWeek(2000, 1, 1) == 6);   // Saturday (400-year leap rule)

    SECTION("toIsoDate");
    CHECK(toIsoDate(2026, 7, 5) == "2026-07-05");
    CHECK(toIsoDate(2026, 12, 31) == "2026-12-31");

    SECTION("categoryForDate");
    {
        // The real seasons.csv shape.
        std::vector<SeasonalTimetableRule> rules = {
            rule("2026-05-30", "2026-06-20", 6, "A"),
            rule("2026-06-28", "2026-09-11", 6, "A"),
            rule("2026-07-01", "2026-08-06", 3, "A"),
            rule("2026-07-01", "2026-08-06", 4, "A"),
            rule("2026-07-01", "2026-08-16", 7, "B"),
        };
        // 2026-07-05 is a Sunday -> only the weekday-7 rule applies -> B.
        auto c = categoryForDate(rules, 2026, 7, 5);
        CHECK(c.has_value() && *c == "B");

        // 2026-07-01 is a Wednesday -> weekday-3 rule -> A.
        auto d = categoryForDate(rules, 2026, 7, 1);
        CHECK(d.has_value() && *d == "A");

        // Inclusive endpoints: the last day of a range still matches.
        auto e = categoryForDate(rules, 2026, 8, 16); // Sunday, end of B range
        CHECK(isoDayOfWeek(2026, 8, 16) == 7);
        CHECK(e.has_value() && *e == "B");

        // A date outside every range -> nothing runs.
        CHECK(!categoryForDate(rules, 2026, 12, 25).has_value());
        // A date in range but on a weekday no rule covers -> nothing.
        // 2026-07-06 is a Monday; no Monday rule exists.
        CHECK(isoDayOfWeek(2026, 7, 6) == 1);
        CHECK(!categoryForDate(rules, 2026, 7, 6).has_value());
    }

    SECTION("daysInMonth");
    CHECK(daysInMonth(2026, 1) == 31);
    CHECK(daysInMonth(2026, 2) == 28);
    CHECK(daysInMonth(2024, 2) == 29); // leap
    CHECK(daysInMonth(2000, 2) == 29); // 400-year leap
    CHECK(daysInMonth(1900, 2) == 28); // 100-year non-leap
    CHECK(daysInMonth(2026, 4) == 30);
    CHECK(daysInMonth(2026, 13) == 0);

    SECTION("stockholmUtcOffsetHours");
    // Deep winter / summer.
    CHECK(stockholmUtcOffsetHours(2026, 1, 15, 12) == 1); // CET
    CHECK(stockholmUtcOffsetHours(2026, 7, 15, 12) == 2); // CEST
    // 2026 spring switch is last Sunday of March = the 29th, at 01:00 UTC.
    CHECK(stockholmUtcOffsetHours(2026, 3, 28, 12) == 1); // day before -> CET
    CHECK(stockholmUtcOffsetHours(2026, 3, 29, 0) == 1);  // before 01:00 UTC -> CET
    CHECK(stockholmUtcOffsetHours(2026, 3, 29, 1) == 2);  // at 01:00 UTC -> CEST
    CHECK(stockholmUtcOffsetHours(2026, 3, 30, 12) == 2); // day after -> CEST
    // 2026 autumn switch is last Sunday of October = the 25th, at 01:00 UTC.
    CHECK(stockholmUtcOffsetHours(2026, 10, 24, 12) == 2); // day before -> CEST
    CHECK(stockholmUtcOffsetHours(2026, 10, 25, 0) == 2);  // before 01:00 UTC -> CEST
    CHECK(stockholmUtcOffsetHours(2026, 10, 25, 1) == 1);  // at 01:00 UTC -> CET
    CHECK(stockholmUtcOffsetHours(2026, 10, 26, 12) == 1); // day after -> CET

    SECTION("utcToStockholm");
    {
        // Summer: +2h.
        GpsClock u; u.valid = true;
        u.year = 2026; u.month = 7; u.day = 5; u.hour = 8; u.minute = 5; u.second = 9;
        GpsClock l = utcToStockholm(u);
        CHECK(l.hour == 10 && l.minute == 5 && l.second == 9);
        CHECK(l.year == 2026 && l.month == 7 && l.day == 5);

        // Winter: +1h.
        u.month = 1; u.day = 15; u.hour = 8;
        CHECK(utcToStockholm(u).hour == 9);

        // Rollover across midnight (and month end): 2026-07-31 23:30 UTC -> +2h.
        u.month = 7; u.day = 31; u.hour = 23; u.minute = 30;
        GpsClock r = utcToStockholm(u);
        CHECK(r.hour == 1 && r.minute == 30);
        CHECK(r.month == 8 && r.day == 1); // rolled into August

        // Invalid clock passes through untouched.
        GpsClock inv; inv.valid = false; inv.hour = 8;
        CHECK(utcToStockholm(inv).hour == 8 && !utcToStockholm(inv).valid);
    }
}
