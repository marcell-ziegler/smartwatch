#include "framework.h"
#include "timetable.h"

void test_findtrain()
{
    SECTION("Timetable::findTrain");

    Timetable tt;
    tt.trafficCategory = "B";
    Train a;
    a.number = "80";
    Train b;
    b.number = "91Y";
    tt.trains = {a, b};

    const Train *found = tt.findTrain("91Y");
    CHECK(found != nullptr);
    CHECK(found != nullptr && found->number == "91Y");

    CHECK(tt.findTrain("80") != nullptr);
    CHECK(tt.findTrain("nope") == nullptr);   // absent
    CHECK(tt.findTrain("") == nullptr);       // empty query
    CHECK(tt.findTrain("91") == nullptr);     // partial must not match

    Timetable empty;
    CHECK(empty.findTrain("80") == nullptr);  // empty roster
}
