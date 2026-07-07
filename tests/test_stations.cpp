#include "framework.h"
#include "timetable.h"

void test_stations()
{
    SECTION("parseDouble");
    CHECK(utils::parseDouble("59.85") == 59.85);
    CHECK(utils::parseDouble("-17.6") == -17.6);
    CHECK(utils::parseDouble("0") == 0.0);
    CHECK(utils::parseDouble("250") == 250.0);
    CHECK(!utils::parseDouble(""));
    CHECK(!utils::parseDouble("12.3x"));   // trailing junk
    CHECK(!utils::parseDouble("x"));
    CHECK(!utils::parseDouble("."));

    SECTION("parseStationsCsv");
    {
        auto st = parseStationsCsv(
            "# sig,name,lat,lon,radius_m\n"
            "Uö,Uppsala Östra,59.8586,17.6461,150\n"
            "\n"
            "Frg,Faringe,59.9205,18.1159,120\n");
        CHECK(st.has_value());
        CHECK((int)st->size() == 2);
        CHECK((*st)[0].signature == "Uö" && (*st)[0].name == "Uppsala Östra");
        CHECK((*st)[0].lat == 59.8586 && (*st)[0].lon == 17.6461);
        CHECK((*st)[0].radius_m == 150.0);
        CHECK((*st)[1].signature == "Frg");

        // failure paths
        CHECK(!parseStationsCsv("Uö,Uppsala,59.85,17.64"));       // too few cols
        CHECK(!parseStationsCsv("Uö,Uppsala,59.85,17.64,150,9"));  // too many
        CHECK(!parseStationsCsv(",Uppsala,59.85,17.64,150"));      // empty sig
        CHECK(!parseStationsCsv("Uö,Uppsala,x,17.64,150"));        // bad lat
        CHECK(!parseStationsCsv("Uö,Uppsala,91.0,17.64,150"));     // lat out of range
        CHECK(!parseStationsCsv("Uö,Uppsala,59.85,17.64,-5"));     // negative radius
    }

    SECTION("findStation");
    {
        auto st = parseStationsCsv(
            "Uö,Uppsala Östra,59.8586,17.6461,150\n"
            "Ml,Marielund,59.87,17.9,100\n");
        CHECK(st.has_value());
        const Station *ml = findStation(*st, "Ml");
        CHECK(ml != nullptr);
        CHECK(ml != nullptr && ml->name == "Marielund");
        CHECK(findStation(*st, "Uö") != nullptr);
        CHECK(findStation(*st, "ZZ") == nullptr);
        CHECK(findStation(*st, "") == nullptr);
    }
}
