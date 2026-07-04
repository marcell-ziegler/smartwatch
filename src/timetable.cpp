#include "timetable.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <utility>

const Train *Timetable::findTrain(const std::string &number) const
{
    auto it = std::find_if(this->trains.begin(), this->trains.end(), [&](const Train &train)
                           { return train.number == number; });
    return (it != this->trains.end()) ? &*it : nullptr;
}

namespace
{
    // Case-insensitive equality between a std::string and a C string literal.
    bool iequals(const std::string &a, const char *b)
    {
        size_t i = 0;
        for (; i < a.size(); ++i)
        {
            if (b[i] == '\0')
                return false;
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
                return false;
        }
        return b[i] == '\0';
    }

    // A line is skipped if it is blank or its first non-space char is '#'.
    bool isCommentOrBlank(const std::string &line)
    {
        for (char c : line)
        {
            if (c == ' ' || c == '\t')
                continue;
            return c == '#';
        }
        return true;
    }

    // Split a ';'-separated subfield (e.g. meets, shift train lists) into
    // non-empty tokens.
    std::vector<std::string> splitSemicolonList(const std::string &field)
    {
        std::vector<std::string> out;
        for (auto &tok : utils::splitCsvLine(field, ';'))
            if (!tok.empty())
                out.push_back(tok);
        return out;
    }

    // Parse an optional time cell: an empty field is a valid *absent* time
    // (out = nullopt), a populated field must be a valid time. Returns false
    // only when a populated field fails to parse (a data error).
    bool parseOptionalClockTime(const std::string &field, std::optional<ClockTime> &out)
    {
        if (field.empty())
        {
            out = std::nullopt;
            return true;
        }
        auto ct = utils::parseClockTime(field);
        if (!ct)
            return false;
        out = ct;
        return true;
    }
}

namespace utils
{
    std::vector<std::string> splitCsvLine(const std::string &line, char delim)
    {
        std::vector<std::string> result;
        std::string field;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); ++i)
        {
            const char c = line[i];

            if (c == '"')
            {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"')
                {
                    field.push_back('"');
                    ++i;
                }
                else
                {
                    inQuotes = !inQuotes;
                }
            }
            else if (c == delim && !inQuotes)
            {
                result.push_back(field);
                field.clear();
            }
            else
            {
                field.push_back(c);
            }
        }

        result.push_back(field);
        return result;
    }

    std::vector<std::string> splitLines(const std::string &content)
    {
        std::vector<std::string> lines;
        std::string line;
        for (char c : content)
        {
            if (c == '\n')
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back(); // tolerate CRLF endings
                lines.push_back(line);
                line.clear();
            }
            else
            {
                line.push_back(c);
            }
        }
        // Trailing line with no closing newline.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            lines.push_back(line);
        return lines;
    }

    std::optional<uint16_t> parseUint16(const std::string &s)
    {
        uint16_t value = 0;
        auto result = std::from_chars(s.data(), s.data() + s.size(), value);
        // Require success AND that the entire string was consumed, so
        // trailing junk ("05x", "8:00") is rejected rather than silently
        // parsing the leading digits.
        if (result.ec != std::errc() || result.ptr != s.data() + s.size())
            return std::nullopt;
        return value;
    }

    // Parse "H:MM" / "HH:MM" into a validated time-of-day. Requires exactly
    // one ':' with all-digit fields on both sides, and rejects out-of-range
    // values (hour >= 24 or minute >= 60) so authoring typos surface here
    // instead of silently wrapping later.
    std::optional<ClockTime> parseClockTime(const std::string &s)
    {
        const size_t colonIdx = s.find(':');
        if (colonIdx == std::string::npos)
            return std::nullopt;

        auto hours = parseUint16(s.substr(0, colonIdx));
        auto minutes = parseUint16(s.substr(colonIdx + 1));
        if (!hours || !minutes)
            return std::nullopt;
        if (*hours >= 24 || *minutes >= 60)
            return std::nullopt;

        return ClockTime{static_cast<uint8_t>(*hours), static_cast<uint8_t>(*minutes)};
    }

    std::optional<uint16_t> parseTimeToMinutes(const std::string &s)
    {
        auto ct = parseClockTime(s);
        if (!ct)
            return std::nullopt;
        return static_cast<uint16_t>(ct->totalMinutes());
    }
}

// ---------------------------------------------------------------------------
//  Field converters. Tokens are the enum identifier names, matched
//  case-insensitively. Adjust the string literals here if you prefer
//  different spellings in the CSV files.
// ---------------------------------------------------------------------------
std::optional<VehicleType> parseVehicleType(const std::string &s)
{
    if (iequals(s, "BigSteam"))
        return BigSteam;
    if (iequals(s, "SmallSteam"))
        return SmallSteam;
    if (iequals(s, "Diesel"))
        return Diesel;
    if (iequals(s, "Railbus"))
        return Railbus;
    return std::nullopt;
}

std::optional<Direction> parseDirection(const std::string &s)
{
    if (iequals(s, "Up"))
        return Up;
    if (iequals(s, "Down"))
        return Down;
    return std::nullopt;
}

std::optional<StopType> parseStopType(const std::string &s)
{
    if (iequals(s, "Mandatory"))
        return Mandatory;
    if (iequals(s, "Optional"))
        return Optional;
    return std::nullopt;
}

std::optional<ExchangeType> parseExchangeType(const std::string &s)
{
    if (iequals(s, "Embarking"))
        return Embarking;
    if (iequals(s, "Disembarking"))
        return Disembarking;
    if (iequals(s, "EmbarkingAndDisembarking"))
        return EmbarkingAndDisembarking;
    if (iequals(s, "None"))
        return None;
    return std::nullopt;
}

std::optional<bool> parseBool(const std::string &s)
{
    if (iequals(s, "true") || iequals(s, "1") || iequals(s, "yes") || iequals(s, "y"))
        return true;
    if (iequals(s, "false") || iequals(s, "0") || iequals(s, "no") || iequals(s, "n"))
        return false;
    return std::nullopt;
}

std::optional<Role> roleForShiftNumber(int number)
{
    switch (number / 10)
    {
    case 1:
        return Forare;
    case 3:
        return Tagbefalhavare;
    case 4:
        return Konduktor;
    default:
        return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
//  Per-file record parsers.
// ---------------------------------------------------------------------------
std::optional<std::vector<SeasonalTimetableRule>> parseSeasonsCsv(const std::string &content)
{
    std::vector<SeasonalTimetableRule> out;
    for (const auto &line : utils::splitLines(content))
    {
        if (isCommentOrBlank(line))
            continue;

        auto f = utils::splitCsvLine(line);
        if (f.size() != 4)
            return std::nullopt;

        auto dow = utils::parseUint16(f[2]);
        if (!dow || *dow < 1 || *dow > 7)
            return std::nullopt;
        if (f[0].empty() || f[1].empty() || f[3].empty())
            return std::nullopt;

        SeasonalTimetableRule r;
        r.validFrom = f[0];
        r.validTo = f[1];
        r.dayOfWeek = static_cast<int>(*dow);
        r.trafficCategory = f[3];
        out.push_back(std::move(r));
    }
    return out;
}

std::optional<std::vector<Shift>> parseShiftsCsv(const std::string &content)
{
    std::vector<Shift> out;
    for (const auto &line : utils::splitLines(content))
    {
        if (isCommentOrBlank(line))
            continue;

        auto f = utils::splitCsvLine(line);
        if (f.size() != 3)
            return std::nullopt;

        auto number = utils::parseUint16(f[0]);
        if (!number || f[1].empty())
            return std::nullopt;

        Shift s;
        s.number = static_cast<int>(*number);
        s.trafficCategory = f[1];
        s.trainNumbers = splitSemicolonList(f[2]);
        if (s.trainNumbers.empty())
            return std::nullopt; // a shift with no trains is a data error
        out.push_back(std::move(s));
    }
    return out;
}

std::optional<std::vector<Train>> parseTrainsCsv(const std::string &content)
{
    std::vector<Train> out;
    for (const auto &line : utils::splitLines(content))
    {
        if (isCommentOrBlank(line))
            continue;

        auto f = utils::splitCsvLine(line);
        if (f.size() != 4)
            return std::nullopt;

        auto vt = parseVehicleType(f[1]);
        auto dir = parseDirection(f[2]);
        if (f[0].empty() || !vt || !dir)
            return std::nullopt;

        Train t;
        t.number = f[0];
        t.vehicleType = *vt;
        t.direction = *dir;
        t.nextNumber = f[3]; // may be empty: no successor
        // stops are filled in later, per-train, by parseStopsCsv.
        out.push_back(std::move(t));
    }
    return out;
}

std::optional<std::vector<Stop>> parseStopsCsv(const std::string &content)
{
    std::vector<Stop> out;
    for (const auto &line : utils::splitLines(content))
    {
        if (isCommentOrBlank(line))
            continue;

        auto f = utils::splitCsvLine(line);
        if (f.size() != 8)
            return std::nullopt;

        std::optional<ClockTime> arrival, departure;
        if (!parseOptionalClockTime(f[1], arrival) || !parseOptionalClockTime(f[2], departure))
            return std::nullopt;

        auto stopType = parseStopType(f[3]);
        auto exchangeType = parseExchangeType(f[4]);
        auto staffed = parseBool(f[5]);
        if (f[0].empty() || !stopType || !exchangeType || !staffed)
            return std::nullopt;

        Stop s;
        s.stationSignature = f[0];
        s.arrival = arrival;
        s.departure = departure;
        s.stopType = *stopType;
        s.exchangeType = *exchangeType;
        s.staffed = *staffed;
        s.meets = splitSemicolonList(f[6]);
        s.remark = f[7];
        out.push_back(std::move(s));
    }
    return out;
}
