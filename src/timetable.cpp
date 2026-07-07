#include "timetable.h"
#include "hal/ITimeTableStore.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>
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

// ---------------------------------------------------------------------------
//  Cross-record validation.
// ---------------------------------------------------------------------------
bool isValidIsoDate(const std::string &s)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-')
        return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (!std::isdigit((unsigned char)s[i]))
            return false;

    const int year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    const int month = (s[5] - '0') * 10 + (s[6] - '0');
    const int day = (s[8] - '0') * 10 + (s[9] - '0');
    if (month < 1 || month > 12)
        return false;

    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dmax = mdays[month - 1];
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (month == 2 && leap)
        dmax = 29;
    return day >= 1 && day <= dmax;
}

int isoDayOfWeek(int year, int month, int day)
{
    // Sakamoto's algorithm -> 0=Sunday .. 6=Saturday.
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    if (month < 3)
        y -= 1;
    const int w = (y + y / 4 - y / 100 + y / 400 + t[month - 1] + day) % 7;
    return w == 0 ? 7 : w; // 0=Sunday -> ISO 7
}

std::string toIsoDate(int year, int month, int day)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return buf;
}

std::optional<std::string> categoryForDate(const std::vector<SeasonalTimetableRule> &rules,
                                           int year, int month, int day)
{
    const std::string today = toIsoDate(year, month, day);
    const int weekday = isoDayOfWeek(year, month, day);
    for (const auto &r : rules)
        if (r.dayOfWeek == weekday && r.validFrom <= today && today <= r.validTo)
            return r.trafficCategory;
    return std::nullopt;
}

std::vector<std::string> validateSeasons(const std::vector<SeasonalTimetableRule> &rules)
{
    std::vector<std::string> errs;

    // Per-row: dates well-formed, range not reversed.
    for (size_t i = 0; i < rules.size(); ++i)
    {
        const auto &r = rules[i];
        const bool fromOk = isValidIsoDate(r.validFrom);
        const bool toOk = isValidIsoDate(r.validTo);
        if (!fromOk)
            errs.push_back("season " + std::to_string(i) + ": invalid validFrom '" + r.validFrom + "'");
        if (!toOk)
            errs.push_back("season " + std::to_string(i) + ": invalid validTo '" + r.validTo + "'");
        // ISO YYYY-MM-DD sorts chronologically as a string.
        if (fromOk && toOk && r.validFrom > r.validTo)
            errs.push_back("season " + std::to_string(i) + ": reversed range (" + r.validFrom + " > " + r.validTo + ")");
    }

    // Pairwise: exact duplicates, and same-weekday overlap into different
    // categories (endpoints inclusive).
    for (size_t i = 0; i < rules.size(); ++i)
    {
        for (size_t j = i + 1; j < rules.size(); ++j)
        {
            const auto &a = rules[i];
            const auto &b = rules[j];

            if (a.validFrom == b.validFrom && a.validTo == b.validTo &&
                a.dayOfWeek == b.dayOfWeek && a.trafficCategory == b.trafficCategory)
            {
                errs.push_back("seasons " + std::to_string(i) + " and " + std::to_string(j) + ": exact duplicate rows");
                continue;
            }

            if (a.dayOfWeek != b.dayOfWeek || a.trafficCategory == b.trafficCategory)
                continue; // different weekday can't collide; same category is allowed to overlap
            // Only meaningful if all endpoints are well-formed dates.
            if (!isValidIsoDate(a.validFrom) || !isValidIsoDate(a.validTo) ||
                !isValidIsoDate(b.validFrom) || !isValidIsoDate(b.validTo))
                continue;
            // Inclusive overlap: [af,at] and [bf,bt] overlap iff af <= bt && bf <= at.
            if (a.validFrom <= b.validTo && b.validFrom <= a.validTo)
                errs.push_back("seasons " + std::to_string(i) + " and " + std::to_string(j) +
                               ": weekday " + std::to_string(a.dayOfWeek) +
                               " overlap maps to different categories (" + a.trafficCategory +
                               " vs " + b.trafficCategory + ")");
        }
    }
    return errs;
}

std::vector<std::string> validateShifts(const std::vector<Shift> &shifts)
{
    std::vector<std::string> errs;
    for (size_t i = 0; i < shifts.size(); ++i)
        for (size_t j = i + 1; j < shifts.size(); ++j)
            if (shifts[i].number == shifts[j].number &&
                shifts[i].trafficCategory == shifts[j].trafficCategory)
                errs.push_back("shifts " + std::to_string(i) + " and " + std::to_string(j) +
                               ": duplicate key (" + std::to_string(shifts[i].number) + "," +
                               shifts[i].trafficCategory + ")");
    return errs;
}

std::vector<std::string> validateTimetable(const Timetable &tt)
{
    std::vector<std::string> errs;

    // Train numbers unique within the category.
    for (size_t i = 0; i < tt.trains.size(); ++i)
        for (size_t j = i + 1; j < tt.trains.size(); ++j)
            if (tt.trains[i].number == tt.trains[j].number)
                errs.push_back("category " + tt.trafficCategory + ": duplicate train number '" +
                               tt.trains[i].number + "'");

    for (const auto &t : tt.trains)
    {
        // Each station called at most once per train.
        for (size_t i = 0; i < t.stops.size(); ++i)
            for (size_t j = i + 1; j < t.stops.size(); ++j)
                if (t.stops[i].stationSignature == t.stops[j].stationSignature)
                    errs.push_back("train " + t.number + ": station '" +
                                   t.stops[i].stationSignature + "' appears more than once");

        // Referential: nextNumber resolves.
        if (!t.nextNumber.empty() && tt.findTrain(t.nextNumber) == nullptr)
            errs.push_back("train " + t.number + ": nextNumber '" + t.nextNumber +
                           "' is not a train in this category");

        // Referential: every meet resolves.
        for (const auto &stop : t.stops)
            for (const auto &meet : stop.meets)
                if (tt.findTrain(meet) == nullptr)
                    errs.push_back("train " + t.number + ": meet '" + meet +
                                   "' is not a train in this category");
    }
    return errs;
}

std::optional<Timetable> loadCategory(ITimeTableStore &store, const std::string &category)
{
    if (category.empty())
        return std::nullopt;

    std::string trainsRaw;
    if (!store.readFile((category + "/trains.csv").c_str(), trainsRaw))
        return std::nullopt;

    auto roster = parseTrainsCsv(trainsRaw);
    if (!roster)
        return std::nullopt;

    Timetable tt;
    tt.trafficCategory = category;
    tt.trains = std::move(*roster);

    // parseTrainsCsv only fills the roster fields; each train's stops live in
    // "<category>/<number>.csv". A train without a populated stop list is
    // invalid, so a missing / malformed / empty stop file fails the whole load.
    for (auto &train : tt.trains)
    {
        std::string stopsRaw;
        if (!store.readFile((category + "/" + train.number + ".csv").c_str(), stopsRaw))
            return std::nullopt;

        auto stops = parseStopsCsv(stopsRaw);
        if (!stops || stops->empty())
            return std::nullopt;

        train.stops = std::move(*stops);
    }

    return tt;
}
