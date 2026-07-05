#include "framework.h"
#include "timetable.h"

namespace
{
    // Build a season rule quickly.
    SeasonalTimetableRule rule(const char *from, const char *to, int dow, const char *cat)
    {
        SeasonalTimetableRule r;
        r.validFrom = from;
        r.validTo = to;
        r.dayOfWeek = dow;
        r.trafficCategory = cat;
        return r;
    }

    Train train(const char *number, const char *next)
    {
        Train t;
        t.number = number;
        t.vehicleType = BigSteam;
        t.direction = Down;
        t.nextNumber = next;
        return t;
    }

    Stop stopAt(const char *sig, std::vector<std::string> meets = {})
    {
        Stop s;
        s.stationSignature = sig;
        s.stopType = Mandatory;
        s.exchangeType = None;
        s.staffed = false;
        s.meets = std::move(meets);
        return s;
    }
}

void test_validation()
{
    SECTION("isValidIsoDate");
    CHECK(isValidIsoDate("2026-07-01"));
    CHECK(isValidIsoDate("2024-02-29"));    // leap year
    CHECK(!isValidIsoDate("2026-02-29"));   // not a leap year
    CHECK(!isValidIsoDate("2026-13-01"));   // month > 12
    CHECK(!isValidIsoDate("2026-00-01"));   // month 0
    CHECK(!isValidIsoDate("2026-07-32"));   // day too large
    CHECK(!isValidIsoDate("2026-07-00"));   // day 0
    CHECK(!isValidIsoDate("2026-7-01"));    // not zero-padded / wrong length
    CHECK(!isValidIsoDate("2026/07/01"));   // wrong separators
    CHECK(!isValidIsoDate("2026-07-0x"));   // non-digit
    CHECK(!isValidIsoDate(""));

    SECTION("validateSeasons - valid");
    {
        // Disjoint Saturdays + same date range on different weekdays: all fine.
        std::vector<SeasonalTimetableRule> rules = {
            rule("2026-05-30", "2026-06-20", 6, "A"),
            rule("2026-06-28", "2026-09-11", 6, "A"),
            rule("2026-07-01", "2026-08-06", 3, "A"),
            rule("2026-07-01", "2026-08-06", 4, "A"),
            rule("2026-07-01", "2026-08-16", 7, "B"),
        };
        CHECK(validateSeasons(rules).empty());

        // Overlapping same-weekday ranges with the SAME category is allowed.
        std::vector<SeasonalTimetableRule> sameCat = {
            rule("2026-06-01", "2026-07-01", 6, "A"),
            rule("2026-06-15", "2026-08-01", 6, "A"),
        };
        CHECK(validateSeasons(sameCat).empty());
    }

    SECTION("validateSeasons - errors");
    {
        // reversed range
        CHECK(!validateSeasons({rule("2026-08-31", "2026-06-01", 3, "A")}).empty());
        // invalid date content (row-parse can't catch this; validation must)
        CHECK(!validateSeasons({rule("2026-02-29", "2026-08-01", 3, "A")}).empty());
        // exact duplicate rows
        CHECK(!validateSeasons({rule("2026-06-01", "2026-08-01", 3, "A"),
                                rule("2026-06-01", "2026-08-01", 3, "A")}).empty());
        // same weekday, overlapping dates, DIFFERENT category -> conflict
        CHECK(!validateSeasons({rule("2026-06-01", "2026-07-15", 3, "A"),
                                rule("2026-07-01", "2026-08-01", 3, "B")}).empty());
        // inclusive endpoints: ranges that only touch on one day still conflict
        CHECK(!validateSeasons({rule("2026-06-01", "2026-07-01", 3, "A"),
                                rule("2026-07-01", "2026-08-01", 3, "B")}).empty());
        // ...but a one-day gap is fine (07-01 vs 07-02)
        CHECK(validateSeasons({rule("2026-06-01", "2026-07-01", 3, "A"),
                               rule("2026-07-02", "2026-08-01", 3, "B")}).empty());
        // different weekday never conflicts even if dates overlap + categories differ
        CHECK(validateSeasons({rule("2026-06-01", "2026-08-01", 3, "A"),
                               rule("2026-06-01", "2026-08-01", 4, "B")}).empty());
    }

    SECTION("validateShifts");
    {
        // 31 appears in both A and B -> distinct keys, OK.
        std::vector<Shift> ok = {
            {31, "A", {"70"}}, {32, "A", {"78"}}, {31, "B", {"80"}}};
        CHECK(validateShifts(ok).empty());

        std::vector<Shift> dup = {{31, "A", {"70"}}, {31, "A", {"99"}}};
        CHECK(!validateShifts(dup).empty());
    }

    SECTION("validateTimetable");
    {
        // Valid category: unique numbers, stations once each, refs resolve.
        Timetable tt;
        tt.trafficCategory = "B";
        Train a = train("80", "81");
        a.stops = {stopAt("Frg"), stopAt("Ml", {"81"}), stopAt("Uo")};
        Train b = train("81", "");           // no successor
        b.stops = {stopAt("Uo"), stopAt("Frg")};
        tt.trains = {a, b};
        CHECK(validateTimetable(tt).empty());

        // Duplicate train number.
        Timetable dupNum = tt;
        dupNum.trains.push_back(train("80", ""));
        CHECK(!validateTimetable(dupNum).empty());

        // Station twice within one train.
        Timetable dupStation;
        dupStation.trafficCategory = "B";
        Train c = train("90", "");
        c.stops = {stopAt("Ml"), stopAt("Ml")};
        dupStation.trains = {c};
        CHECK(!validateTimetable(dupStation).empty());

        // Dangling nextNumber.
        Timetable badNext;
        badNext.trafficCategory = "B";
        badNext.trains = {train("80", "999")};
        CHECK(!validateTimetable(badNext).empty());

        // Dangling meet reference.
        Timetable badMeet;
        badMeet.trafficCategory = "B";
        Train d = train("80", "");
        d.stops = {stopAt("Ml", {"404"})};
        badMeet.trains = {d};
        CHECK(!validateTimetable(badMeet).empty());
    }
}
