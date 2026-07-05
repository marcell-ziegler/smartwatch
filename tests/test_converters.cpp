#include "framework.h"
#include "timetable.h"

void test_converters()
{
    SECTION("parseVehicleType");
    CHECK(parseVehicleType("BigSteam") == BigSteam);
    CHECK(parseVehicleType("SmallSteam") == SmallSteam);
    CHECK(parseVehicleType("Diesel") == Diesel);
    CHECK(parseVehicleType("Railbus") == Railbus);
    CHECK(parseVehicleType("bigsteam") == BigSteam);   // case-insensitive
    CHECK(parseVehicleType("RAILBUS") == Railbus);
    CHECK(!parseVehicleType("Steam"));                 // not a token
    CHECK(!parseVehicleType(""));                      // empty
    CHECK(!parseVehicleType("BigSteamX"));             // superstring

    SECTION("parseDirection");
    CHECK(parseDirection("Up") == Up);
    CHECK(parseDirection("Down") == Down);
    CHECK(parseDirection("down") == Down);
    CHECK(!parseDirection("Sideways"));
    CHECK(!parseDirection(""));

    SECTION("parseStopType");
    CHECK(parseStopType("Mandatory") == Mandatory);
    CHECK(parseStopType("Optional") == Optional);
    CHECK(parseStopType("OPTIONAL") == Optional);
    CHECK(!parseStopType("Maybe"));

    SECTION("parseExchangeType");
    CHECK(parseExchangeType("Embarking") == Embarking);
    CHECK(parseExchangeType("Disembarking") == Disembarking);
    CHECK(parseExchangeType("EmbarkingAndDisembarking") == EmbarkingAndDisembarking);
    CHECK(parseExchangeType("None") == None);
    CHECK(parseExchangeType("none") == None);
    CHECK(!parseExchangeType("Embark"));
    CHECK(!parseExchangeType(""));

    SECTION("parseBool");
    CHECK(parseBool("true") == true);
    CHECK(parseBool("false") == false);
    CHECK(parseBool("1") == true);
    CHECK(parseBool("0") == false);
    CHECK(parseBool("yes") == true);
    CHECK(parseBool("no") == false);
    CHECK(parseBool("Y") == true);
    CHECK(parseBool("N") == false);
    CHECK(parseBool("TRUE") == true);
    CHECK(!parseBool("maybe"));
    CHECK(!parseBool(""));
    CHECK(!parseBool("2"));

    SECTION("roleForShiftNumber");
    CHECK(roleForShiftNumber(10) == Forare);
    CHECK(roleForShiftNumber(19) == Forare);
    CHECK(roleForShiftNumber(31) == Tagbefalhavare);
    CHECK(roleForShiftNumber(33) == Tagbefalhavare);
    CHECK(roleForShiftNumber(40) == Konduktor);
    CHECK(roleForShiftNumber(49) == Konduktor);
    CHECK(!roleForShiftNumber(5));    // 0x -> no role
    CHECK(!roleForShiftNumber(20));   // 2x -> no role
    CHECK(!roleForShiftNumber(50));   // 5x -> no role
    CHECK(!roleForShiftNumber(100));  // 10x -> no role
}
