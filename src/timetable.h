#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include "ClockTime.h"

// Denotes wether the stop is mandatory for the given station.
enum StopType
{
    Mandatory,
    Optional
};

// Shows what type of exchange happens at the station.
enum ExchangeType
{
    // People / goods may get loaded
    Embarking,
    // People / goods may get unloaded
    Disembarking,
    // People / goods may get loaded and/or unloaded
    EmbarkingAndDisembarking,
    // No exchange of people / goods is planned
    None
};

// Direction of the train
enum Direction
{
    // Trains going to Uppsala
    Up,
    // Trains going to Faringe
    Down
};

// Type of vehicle used for a given train
enum VehicleType
{
    // Tor
    BigSteam,
    // Långshyttan
    SmallSteam,
    Diesel,
    Railbus
};

// Role of the current shift
enum Role
{
    Forare,
    Tagbefalhavare,
    Konduktor
};

// Represents a single stop in the timetable
struct Stop
{
    std::string stationSignature;
    // Either may be absent: some stations list only a departure, some list
    // neither (an empty CSV cell parses to nullopt).
    std::optional<ClockTime> arrival, departure;
    StopType stopType;
    ExchangeType exchangeType;
    // I.e. "Bevakad"
    bool staffed;
    // Any "möte" at the station
    std::vector<std::string> meets;
    std::string remark;
};

// Represents the timetable unit of a Train
struct Train
{
    // Alphanumeric train id
    std::string number;
    VehicleType vehicleType;
    Direction direction;
    // Next train to use the same train set
    std::string nextNumber;
    // List of all stops passed in order
    std::vector<Stop> stops;
};

// Represents a singe timetable for a single day
struct Timetable
{
    // Type of traffic: A, B, etc.
    std::string trafficCategory;
    // The trains of the day in no guaranteed order.
    std::vector<Train> trains;
    // Lookup a train based on number.
    const Train *findTrain(const std::string &number) const;
};

struct Shift
{
    int number;
    // Type of traffic: A, B, etc.
    std::string trafficCategory;
    // List of trains in order for the given shift.
    std::vector<std::string> trainNumbers;
};

// A rule for determining what category applies to a given date.
struct SeasonalTimetableRule
{
    // ISO Date strings for begin and end of the rule.
    // Both are inclusive.
    std::string validFrom, validTo;
    // An int 1-7 for which day of the week this applies to.
    int dayOfWeek;
    // What category is to be used on the given weekday.
    std::string trafficCategory;
};

// ---------------------------------------------------------------------------
//  Low-level text helpers (no timetable knowledge).
// ---------------------------------------------------------------------------
namespace utils
{
    // Split one CSV line into fields on `delim`, honouring "double quotes"
    // and the "" escape for a literal quote inside a quoted field.
    std::vector<std::string> splitCsvLine(const std::string &line, char delim = ',');

    // Split file content into lines, tolerating both LF and CRLF endings.
    // A final line without a trailing newline is still returned.
    std::vector<std::string> splitLines(const std::string &content);

    // Parse a whole decimal string into a uint16_t. Requires the ENTIRE
    // string to be digits (no leading/trailing junk); nullopt otherwise.
    std::optional<uint16_t> parseUint16(const std::string &s);

    // Parse "H:MM"/"HH:MM" into a validated wall-clock time.
    std::optional<ClockTime> parseClockTime(const std::string &s);

    // Same as parseClockTime but yields minutes-since-midnight. Kept for
    // callers that want the raw integer.
    std::optional<uint16_t> parseTimeToMinutes(const std::string &s);
}

// ---------------------------------------------------------------------------
//  Field converters (string token <-> enum) and derived values.
// ---------------------------------------------------------------------------
std::optional<VehicleType> parseVehicleType(const std::string &s);
std::optional<Direction> parseDirection(const std::string &s);
std::optional<StopType> parseStopType(const std::string &s);
std::optional<ExchangeType> parseExchangeType(const std::string &s);
std::optional<bool> parseBool(const std::string &s);

// Role is derived from the shift number's tens digit (1x=Forare,
// 3x=Tagbefalhavare, 4x=Konduktor); nullopt for any other range.
std::optional<Role> roleForShiftNumber(int number);

// ---------------------------------------------------------------------------
//  Per-file record parsers. Each takes the whole file's contents and returns
//  the parsed records, or nullopt if ANY data row is malformed. Blank lines
//  and lines whose first non-space char is '#' are ignored (so you can label
//  columns with a leading '#' comment, matching assets/tracks/demo.csv);
//  there is no header row.
//
//  Column schemas (see the timetable data-model discussion):
//    seasons.csv : validFrom,validTo,dayOfWeek,category
//    shifts.csv  : number,category,trainNumbers            (';'-separated)
//    trains.csv  : number,vehicleType,direction,nextNumber (roster; no stops)
//    stops.csv   : stationSignature,arrival,departure,stopType,exchangeType,
//                  staffed,meets,remark                    (meets ';'-separated)
// ---------------------------------------------------------------------------
std::optional<std::vector<SeasonalTimetableRule>> parseSeasonsCsv(const std::string &content);
std::optional<std::vector<Shift>> parseShiftsCsv(const std::string &content);
std::optional<std::vector<Train>> parseTrainsCsv(const std::string &content);
std::optional<std::vector<Stop>> parseStopsCsv(const std::string &content);

// ---------------------------------------------------------------------------
//  Cross-record validation (desktop pre-flight). Row-level well-formedness is
//  already enforced by the parsers; these check invariants that span multiple
//  records. Each returns a list of human-readable error messages -- an EMPTY
//  vector means valid. Intended to run on the desktop before an SD card ships;
//  a wrong meet/overlap is safety-relevant, so fail loudly here.
// ---------------------------------------------------------------------------

// True iff s is a real ISO calendar date "YYYY-MM-DD" (month 1-12, valid
// day-of-month incl. leap years). Date range endpoints are treated INCLUSIVE.
bool isValidIsoDate(const std::string &s);

// ---------------------------------------------------------------------------
//  Date helpers used to resolve "what runs today" from a calendar date.
// ---------------------------------------------------------------------------

// ISO 8601 weekday for a Gregorian date: Monday=1 .. Sunday=7.
int isoDayOfWeek(int year, int month, int day);

// Formats a date as "YYYY-MM-DD" (zero-padded), matching seasons.csv.
std::string toIsoDate(int year, int month, int day);

// The traffic category running on the given date: the first season rule whose
// weekday matches and whose (inclusive) date range contains the date. nullopt
// if none applies. Assumes `rules` has passed validateSeasons (which
// guarantees no conflicting overlaps, so "first match" is unambiguous).
std::optional<std::string> categoryForDate(const std::vector<SeasonalTimetableRule> &rules,
                                           int year, int month, int day);

// seasons.csv: each date well-formed and validFrom <= validTo; no exact
// duplicate rows; and no two rules for the same weekday whose (inclusive) date
// ranges overlap while mapping to different categories (the "one category per
// day" rule). Overlapping same-weekday ranges with the SAME category are OK.
std::vector<std::string> validateSeasons(const std::vector<SeasonalTimetableRule> &rules);

// shifts.csv: the (number, category) key is unique.
std::vector<std::string> validateShifts(const std::vector<Shift> &shifts);

// A loaded category (roster + each train's stops): train numbers unique; no
// station repeated within one train; every nextNumber and every meets entry
// resolves to a train in this category's roster.
std::vector<std::string> validateTimetable(const Timetable &tt);

// ---------------------------------------------------------------------------
//  Category assembly: roster + every train's stops -> one in-RAM Timetable.
// ---------------------------------------------------------------------------
class ITimeTableStore; // defined in hal/ITimeTableStore.h

// Loads a whole traffic category into a Timetable: reads "<category>/trains.csv"
// for the roster, then "<category>/<number>.csv" for each train's stops.
//
// A train is only valid with a fully populated stop list, so this returns
// nullopt if trains.csv is missing/malformed, or if ANY train's stop file is
// missing, malformed, or empty. On success every returned Train has its stops.
// (Cross-record checks like meet/nextNumber resolution are separate --
// run validateTimetable() on the result if you want them.)
std::optional<Timetable> loadCategory(ITimeTableStore &store, const std::string &category);