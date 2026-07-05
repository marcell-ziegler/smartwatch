#include "framework.h"
#include "ClockTime.h"

void test_clocktime()
{
    SECTION("ClockTime");

    // totalMinutes
    CHECK((ClockTime{0, 0}.totalMinutes()) == 0);
    CHECK((ClockTime{8, 5}.totalMinutes()) == 485);
    CHECK((ClockTime{23, 59}.totalMinutes()) == 1439);

    // fromTotalMinutes: in range, wrap forward, wrap backward, exact day
    CHECK(ClockTime::fromTotalMinutes(485) == (ClockTime{8, 5}));
    CHECK(ClockTime::fromTotalMinutes(1440) == (ClockTime{0, 0}));   // exactly midnight
    CHECK(ClockTime::fromTotalMinutes(1445) == (ClockTime{0, 5}));   // wrap forward
    CHECK(ClockTime::fromTotalMinutes(-10) == (ClockTime{23, 50}));  // wrap backward
    CHECK(ClockTime::fromTotalMinutes(-1440) == (ClockTime{0, 0}));  // whole day back
    CHECK(ClockTime::fromTotalMinutes(2 * 1440 + 3) == (ClockTime{0, 3}));

    // fromHoursAndMinutes (incl. minute overflow normalisation)
    CHECK(ClockTime::fromHoursAndMinutes(8, 5) == (ClockTime{8, 5}));
    CHECK(ClockTime::fromHoursAndMinutes(8, 75) == (ClockTime{9, 15}));
    CHECK(ClockTime::fromHoursAndMinutes(25, 0) == (ClockTime{1, 0}));

    // operator+/- with minutes, midnight wrap both ways
    CHECK((ClockTime{23, 50} + 20) == (ClockTime{0, 10}));
    CHECK((ClockTime{0, 10} - 20) == (ClockTime{23, 50}));
    CHECK((ClockTime{8, 5} + 0) == (ClockTime{8, 5}));
    CHECK((ClockTime{12, 0} + 1440) == (ClockTime{12, 0}));   // full day is identity

    // compound assignment
    {
        ClockTime t{23, 50};
        t += 20;
        CHECK(t == (ClockTime{0, 10}));
        t -= 20;
        CHECK(t == (ClockTime{23, 50}));
    }

    // difference between two times: signed, NOT wrapped
    CHECK((ClockTime{9, 0} - ClockTime{8, 5}) == 55);
    CHECK((ClockTime{8, 5} - ClockTime{9, 0}) == -55);
    CHECK((ClockTime{0, 10} - ClockTime{23, 50}) == -1420);   // same-day delta, not +20

    // comparisons
    CHECK(ClockTime{8, 5} == ClockTime{8, 5});
    CHECK(ClockTime{8, 5} != ClockTime{8, 6});
    CHECK(ClockTime{8, 5} < ClockTime{8, 6});
    CHECK(ClockTime{8, 5} < ClockTime{9, 0});
    CHECK(ClockTime{8, 5} <= ClockTime{8, 5});
    CHECK(ClockTime{9, 0} > ClockTime{8, 59});
    CHECK(ClockTime{9, 0} >= ClockTime{9, 0});
    CHECK(!(ClockTime{8, 5} < ClockTime{8, 5}));
    CHECK(!(ClockTime{8, 6} < ClockTime{8, 5}));
}
