#include "framework.h"
#include "timetable.h"
#include "hal/ITimeTableStore.h"

#include <map>
#include <string>

namespace
{
    // In-memory ITimeTableStore: serves files from a path->contents map.
    struct MapStore : ITimeTableStore
    {
        std::map<std::string, std::string> files;
        bool begin() override { return true; }
        bool readFile(const char *path, std::string &out) override
        {
            auto it = files.find(path);
            if (it == files.end())
                return false;
            out = it->second;
            return true;
        }
    };

    // A minimal but complete category: two trains, each with a stop file.
    MapStore completeCategory()
    {
        MapStore s;
        s.files["B/trains.csv"] =
            "80,BigSteam,Up,81\n"
            "81,BigSteam,Down,\n";
        s.files["B/80.csv"] =
            "Frg,,07:40,Mandatory,Embarking,false,,\n"
            "Uo,09:20,,Mandatory,Disembarking,false,81,\n";
        s.files["B/81.csv"] =
            "Uo,,10:00,Mandatory,Embarking,false,,\n"
            "Frg,11:00,,Mandatory,Disembarking,false,,\n";
        return s;
    }
}

void test_loadcategory()
{
    SECTION("loadCategory - success");
    {
        MapStore s = completeCategory();
        auto tt = loadCategory(s, "B");
        CHECK(tt.has_value());
        CHECK(tt->trafficCategory == "B");
        CHECK((int)tt->trains.size() == 2);

        // Stops are populated (the whole point -- parseTrainsCsv alone leaves
        // them empty).
        const Train *t80 = tt->findTrain("80");
        CHECK(t80 != nullptr);
        CHECK(t80 != nullptr && (int)t80->stops.size() == 2);
        CHECK(t80 != nullptr && t80->stops[0].stationSignature == "Frg");
        CHECK(t80 != nullptr && t80->stops[1].arrival.has_value());
        CHECK(t80 != nullptr && t80->stops[1].arrival == (ClockTime{9, 20}));

        CHECK(tt->findTrain("81") != nullptr);
        CHECK(tt->findTrain("99") == nullptr);

        // A fully-loaded, referentially-consistent category validates clean.
        CHECK(validateTimetable(*tt).empty());
    }

    SECTION("loadCategory - failure paths");
    {
        // Missing trains.csv.
        MapStore empty;
        CHECK(!loadCategory(empty, "B").has_value());

        // Empty category string.
        MapStore s = completeCategory();
        CHECK(!loadCategory(s, "").has_value());

        // A train listed in the roster but with NO stop file -> invalid.
        MapStore missingStops = completeCategory();
        missingStops.files.erase("B/81.csv");
        CHECK(!loadCategory(missingStops, "B").has_value());

        // A train whose stop file exists but is empty -> invalid (a train must
        // have stops).
        MapStore emptyStops = completeCategory();
        emptyStops.files["B/81.csv"] = "# no data rows\n";
        CHECK(!loadCategory(emptyStops, "B").has_value());

        // Malformed roster.
        MapStore badRoster = completeCategory();
        badRoster.files["B/trains.csv"] = "80,NotAVehicle,Up,81\n";
        CHECK(!loadCategory(badRoster, "B").has_value());

        // Malformed stop row.
        MapStore badStop = completeCategory();
        badStop.files["B/80.csv"] = "Frg,25:99,,Mandatory,Embarking,false,,\n";
        CHECK(!loadCategory(badStop, "B").has_value());
    }
}
