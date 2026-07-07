#include "framework.h"
#include "tracking.h"
#include "timetable.h"
#include "hal/IGps.h"

namespace
{
    // Stations on a due-north line (lon fixed), 0.02 deg (~2.2 km) apart.
    std::vector<Station> makeStations()
    {
        auto mk = [](const char *sig, double lat)
        {
            Station s;
            s.signature = sig;
            s.name = sig;
            s.lat = lat;
            s.lon = 17.60;
            s.radius_m = 100.0;
            return s;
        };
        return {mk("A", 59.80), mk("B", 59.82), mk("C", 59.84), mk("D", 59.86)};
    }

    Stop mkStop(const char *sig, std::optional<ClockTime> arr, std::optional<ClockTime> dep)
    {
        Stop s;
        s.stationSignature = sig;
        s.arrival = arr;
        s.departure = dep;
        s.stopType = Mandatory;
        s.exchangeType = None;
        s.staffed = false;
        return s;
    }

    // Shift of two trains: T1 A->D (north), T2 D->A (south).
    Timetable makeTimetable()
    {
        Timetable tt;
        tt.trafficCategory = "X";
        Train t1;
        t1.number = "T1";
        t1.stops = {
            mkStop("A", std::nullopt, ClockTime{8, 0}), // origin, major (has dep)
            mkStop("B", ClockTime{8, 15}, std::nullopt), // minor (arr only)
            mkStop("C", ClockTime{8, 30}, ClockTime{8, 35}), // major
            mkStop("D", ClockTime{8, 50}, std::nullopt), // terminus
        };
        Train t2;
        t2.number = "T2";
        t2.stops = {
            mkStop("D", std::nullopt, ClockTime{9, 0}),
            mkStop("C", ClockTime{9, 20}, ClockTime{9, 25}),
            mkStop("B", ClockTime{9, 40}, std::nullopt),
            mkStop("A", ClockTime{9, 55}, std::nullopt),
        };
        tt.trains = {t1, t2};
        return tt;
    }

    Shift makeShift() { return Shift{31, "X", {"T1", "T2"}}; }

    GpsFix fixAt(double lat, double lon)
    {
        GpsFix f;
        f.valid = true;
        f.lat = lat;
        f.lon = lon;
        return f;
    }
}

void test_tracking()
{
    const auto stations = makeStations();
    const auto tt = makeTimetable();
    const auto shift = makeShift();

    SECTION("initialTracking - before start (pre)");
    {
        auto s = initialTracking(shift, tt, stations, ClockTime{7, 0});
        CHECK(s.valid);
        CHECK(s.activeShiftIdx == 0 && s.activeTrain == "T1");
        CHECK(s.lastStopIdx == 0 && s.nextStopIdx == 1); // at origin A, heading B
        CHECK(s.nextMajorStopIdx == 2);                  // strictly after B -> C
        CHECK(!s.gpsLock);
        CHECK(!s.shiftComplete);
    }

    SECTION("initialTracking - mid-run by clock");
    {
        // 08:20: past A(08:00) and B(08:15), before C(08:30).
        auto s = initialTracking(shift, tt, stations, ClockTime{8, 20});
        CHECK(s.lastStopIdx == 1 && s.nextStopIdx == 2); // last=B, next=C
        CHECK(s.nextMajorStopIdx == 3);                  // strictly after C -> terminus D
    }

    SECTION("initialTracking - whole shift already over");
    {
        auto s = initialTracking(shift, tt, stations, ClockTime{23, 0});
        CHECK(s.valid && s.shiftComplete);
        CHECK(s.activeShiftIdx == 1); // last train
    }

    SECTION("advanceTracking - GPS advances stop by stop");
    {
        auto s = initialTracking(shift, tt, stations, ClockTime{7, 0});
        // Sitting at A: not within B's radius, no advance.
        s = advanceTracking(s, shift, tt, stations, fixAt(59.80, 17.60), ClockTime{8, 0});
        CHECK(s.gpsLock && s.lastStopIdx == 0 && s.nextStopIdx == 1);
        // Halfway A->B: progress ~0.5, still no advance.
        s = advanceTracking(s, shift, tt, stations, fixAt(59.81, 17.60), ClockTime{8, 7});
        CHECK(s.segmentProgress > 0.4 && s.segmentProgress < 0.6);
        CHECK(s.lastStopIdx == 0 && s.nextStopIdx == 1);
        // Reach B: advance last=B, next=C.
        s = advanceTracking(s, shift, tt, stations, fixAt(59.82, 17.60), ClockTime{8, 15});
        CHECK(s.atStation && s.lastStopIdx == 1 && s.nextStopIdx == 2);
        // Reach C.
        s = advanceTracking(s, shift, tt, stations, fixAt(59.84, 17.60), ClockTime{8, 30});
        CHECK(s.lastStopIdx == 2 && s.nextStopIdx == 3);
        // Reach D (final stop of T1) -> auto-load T2.
        s = advanceTracking(s, shift, tt, stations, fixAt(59.86, 17.60), ClockTime{8, 50});
        CHECK(s.activeShiftIdx == 1 && s.activeTrain == "T2");
        CHECK(s.lastStopIdx == 0 && s.nextStopIdx == 1); // at D, heading C on T2
        CHECK(!s.shiftComplete);
    }

    SECTION("advanceTracking - monotonic (jitter can't un-pass)");
    {
        auto s = initialTracking(shift, tt, stations, ClockTime{7, 0});
        s = advanceTracking(s, shift, tt, stations, fixAt(59.82, 17.60), ClockTime{8, 15}); // at B
        CHECK(s.lastStopIdx == 1);
        // GPS jumps back toward A: must not move lastStopIdx backward.
        s = advanceTracking(s, shift, tt, stations, fixAt(59.80, 17.60), ClockTime{8, 16});
        CHECK(s.lastStopIdx == 1 && s.nextStopIdx == 2);
    }

    SECTION("advanceTracking - no fix while waiting (clock estimate)");
    {
        auto s = initialTracking(shift, tt, stations, ClockTime{7, 0}); // gpsLock=false
        GpsFix none; // valid=false
        s = advanceTracking(s, shift, tt, stations, none, ClockTime{8, 20});
        CHECK(!s.gpsLock);
        CHECK(s.lastStopIdx == 1 && s.nextStopIdx == 2); // clock places us B->C
    }

    SECTION("advanceTracking - lost fix after lock (freeze)");
    {
        auto s = initialTracking(shift, tt, stations, ClockTime{7, 0});
        s = advanceTracking(s, shift, tt, stations, fixAt(59.82, 17.60), ClockTime{8, 15}); // lock @ B
        CHECK(s.gpsLock && s.lastStopIdx == 1);
        GpsFix none;
        s = advanceTracking(s, shift, tt, stations, none, ClockTime{9, 30}); // clock much later
        CHECK(!s.gpsLock);
        CHECK(s.lastStopIdx == 1 && s.nextStopIdx == 2); // frozen, NOT jumped by clock
    }
}
