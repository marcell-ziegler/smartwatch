#include "framework.h"
#include "timetable.h"

void test_record_parsers()
{
    SECTION("parseSeasonsCsv");
    {
        auto ok = parseSeasonsCsv(
            "# from,to,dow,category\n"
            "2026-06-01,2026-08-31,3,A\n"
            "\n"                              // blank line skipped
            "2026-07-01,2026-08-16,7,B\n");
        CHECK(ok.has_value());
        CHECK((int)ok->size() == 2);
        CHECK((*ok)[0].dayOfWeek == 3 && (*ok)[0].trafficCategory == "A");
        CHECK((*ok)[1].validFrom == "2026-07-01");

        CHECK(!parseSeasonsCsv("2026-06-01,2026-08-31,3"));      // too few columns
        CHECK(!parseSeasonsCsv("2026-06-01,2026-08-31,3,A,x"));  // too many columns
        CHECK(!parseSeasonsCsv("2026-06-01,2026-08-31,0,A"));    // dow < 1
        CHECK(!parseSeasonsCsv("2026-06-01,2026-08-31,8,A"));    // dow > 7
        CHECK(!parseSeasonsCsv("2026-06-01,2026-08-31,x,A"));    // dow non-numeric
        CHECK(!parseSeasonsCsv(",2026-08-31,3,A"));              // empty validFrom
        CHECK(!parseSeasonsCsv("2026-06-01,2026-08-31,3,"));     // empty category
        // NB: row-level parse does not check date *content* -- validateSeasons does.
    }

    SECTION("parseShiftsCsv");
    {
        auto ok = parseShiftsCsv(
            "31,A,70;71;74;75\n"
            "40,B,80\n");
        CHECK(ok.has_value());
        CHECK((int)ok->size() == 2);
        CHECK((*ok)[0].number == 31);
        CHECK((int)(*ok)[0].trainNumbers.size() == 4);
        CHECK((*ok)[0].trainNumbers[1] == "71");
        CHECK((int)(*ok)[1].trainNumbers.size() == 1);

        // trailing ';' should not create an empty train entry
        auto trail = parseShiftsCsv("31,A,70;71;\n");
        CHECK(trail.has_value());
        CHECK((int)(*trail)[0].trainNumbers.size() == 2);

        CHECK(!parseShiftsCsv("31,A"));        // too few columns
        CHECK(!parseShiftsCsv("x,A,70"));      // non-numeric number
        CHECK(!parseShiftsCsv("31,,70"));      // empty category
        CHECK(!parseShiftsCsv("31,A,"));       // no trains
    }

    SECTION("parseTrainsCsv");
    {
        auto ok = parseTrainsCsv(
            "70,BigSteam,Down,71\n"
            "71,Railbus,Up,\n");              // empty nextNumber is allowed
        CHECK(ok.has_value());
        CHECK((int)ok->size() == 2);
        CHECK((*ok)[0].vehicleType == BigSteam && (*ok)[0].direction == Down);
        CHECK((*ok)[0].nextNumber == "71");
        CHECK((*ok)[1].nextNumber.empty());

        CHECK(!parseTrainsCsv("70,BigSteam,Down"));      // too few columns
        CHECK(!parseTrainsCsv("70,Rocket,Down,71"));     // bad vehicle type
        CHECK(!parseTrainsCsv("70,BigSteam,Sideways,71"));// bad direction
        CHECK(!parseTrainsCsv(",BigSteam,Down,71"));     // empty number
    }

    SECTION("parseStopsCsv");
    {
        auto ok = parseStopsCsv(
            "# station,arr,dep,stoptype,exchange,staffed,meets,remark\n"
            "Uo,08:00,08:05,Mandatory,Embarking,true,62;91Y,\"Meet 62, then go\"\n"
            "Fl,,08:12,Optional,Embarking,false,,\n"     // departure only
            "As,,,Optional,None,false,,\n"               // neither time
            "Frg,09:10,,Mandatory,Disembarking,true,,\n");// arrival only
        CHECK(ok.has_value());
        CHECK((int)ok->size() == 4);

        CHECK((*ok)[0].arrival.has_value() && (*ok)[0].departure.has_value());
        CHECK((*ok)[0].arrival == (ClockTime{8, 0}));
        CHECK((int)(*ok)[0].meets.size() == 2 && (*ok)[0].meets[1] == "91Y");
        CHECK((*ok)[0].remark == "Meet 62, then go");     // comma survived quoting

        CHECK(!(*ok)[1].arrival.has_value() && (*ok)[1].departure.has_value());
        CHECK(!(*ok)[2].arrival.has_value() && !(*ok)[2].departure.has_value());
        CHECK((*ok)[2].meets.empty() && (*ok)[2].exchangeType == None);
        CHECK((*ok)[3].arrival.has_value() && !(*ok)[3].departure.has_value());

        // a populated-but-malformed time still fails the whole file
        CHECK(!parseStopsCsv("Uo,8:99,,Mandatory,None,true,,"));  // minute out of range
        CHECK(!parseStopsCsv("Uo,0800,,Mandatory,None,true,,"));  // no colon
        CHECK(!parseStopsCsv("Uo,08:00,08:05,Mandatory,Embarking,true,62")); // 7 cols
        CHECK(!parseStopsCsv(",08:00,,Mandatory,None,true,,"));   // empty station
        CHECK(!parseStopsCsv("Uo,,,Maybe,None,true,,"));         // bad stopType
        CHECK(!parseStopsCsv("Uo,,,Mandatory,Nope,true,,"));     // bad exchangeType
        CHECK(!parseStopsCsv("Uo,,,Mandatory,None,perhaps,,"));  // bad staffed bool
    }
}
