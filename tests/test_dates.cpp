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
}
