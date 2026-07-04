#pragma once
#include <cstdint>
#include <string>
#include <vector>
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
    ClockTime arrival, departure;
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