#include "tracking.h"
#include "geo.h"
#include <utility>

namespace
{
    const Train *trainAt(const Shift &shift, const Timetable &tt, int idx)
    {
        if (idx < 0 || idx >= (int)shift.trainNumbers.size())
            return nullptr;
        return tt.findTrain(shift.trainNumbers[idx]);
    }

    // The time that best represents when a train is at a stop: its departure if
    // posted, else its arrival, else none (an untimed request stop).
    std::optional<ClockTime> stopTime(const Stop &s)
    {
        if (s.departure)
            return s.departure;
        if (s.arrival)
            return s.arrival;
        return std::nullopt;
    }

    std::optional<ClockTime> firstTime(const Train &t)
    {
        for (const auto &s : t.stops)
            if (auto x = stopTime(s))
                return x;
        return std::nullopt;
    }

    std::optional<ClockTime> lastTime(const Train &t)
    {
        for (auto it = t.stops.rbegin(); it != t.stops.rend(); ++it)
            if (auto x = stopTime(*it))
                return x;
        return std::nullopt;
    }

    // {lastStopIdx, nextStopIdx} for `now`: the last timed stop at/behind now,
    // and the one after it (clamped to the terminus).
    std::pair<int, int> positionByClock(const Train &t, ClockTime now)
    {
        const int lastIdx = (int)t.stops.size() - 1;
        int last = 0;
        for (int i = 0; i <= lastIdx; ++i)
            if (auto x = stopTime(t.stops[i]); x && *x <= now)
                last = i;
        const int next = (last < lastIdx) ? last + 1 : lastIdx;
        return {last, next};
    }

    // The top dot: the first stop STRICTLY AFTER `nextIdx` that has a posted
    // departure (a timing point), or the terminus if none.
    int computeNextMajor(const Train &t, int nextIdx)
    {
        const int lastIdx = (int)t.stops.size() - 1;
        if (nextIdx < 0)
            return -1;
        for (int i = nextIdx + 1; i <= lastIdx; ++i)
            if (t.stops[i].departure.has_value())
                return i;
        return lastIdx;
    }

    double segmentProgressFor(const Train &t, const std::vector<Station> &stations,
                              int a, int b, geo::Point user)
    {
        if (a < 0 || b < 0 || a >= (int)t.stops.size() || b >= (int)t.stops.size())
            return 0.0;
        const Station *sa = findStation(stations, t.stops[a].stationSignature);
        const Station *sb = findStation(stations, t.stops[b].stationSignature);
        if (!sa || !sb)
            return 0.0;
        return geo::fractionAlongSegment({sa->lat, sa->lon}, {sb->lat, sb->lon}, user);
    }
}

TrackingState initialTracking(const Shift &shift, const Timetable &tt,
                              const std::vector<Station> &stations, ClockTime now)
{
    (void)stations; // no GPS at init; position comes from the clock
    TrackingState s;

    int chosen = -1;
    int lastResolvable = -1;
    bool pre = false;
    bool post = false;

    for (int i = 0; i < (int)shift.trainNumbers.size(); ++i)
    {
        const Train *tr = trainAt(shift, tt, i);
        if (!tr || tr->stops.empty())
            continue;
        lastResolvable = i;
        auto st = firstTime(*tr);
        auto en = lastTime(*tr);
        if (!st || !en)
            continue; // can't place a timeless train by clock
        if (now <= *en)
        {
            chosen = i;
            pre = (now < *st);
            break;
        }
    }

    if (chosen == -1)
    {
        if (lastResolvable == -1)
            return s; // nothing resolvable -> invalid
        chosen = lastResolvable; // every train already finished today
        post = true;
    }

    const Train *tr = trainAt(shift, tt, chosen);
    const int lastIdx = (int)tr->stops.size() - 1;

    s.valid = true;
    s.activeShiftIdx = chosen;
    s.activeTrain = tr->number;
    s.gpsLock = false;
    s.atStation = false;

    if (post)
    {
        s.lastStopIdx = lastIdx;
        s.nextStopIdx = lastIdx;
        s.shiftComplete = true;
        s.segmentProgress = 1.0;
    }
    else if (pre)
    {
        s.lastStopIdx = 0;
        s.nextStopIdx = (lastIdx >= 1) ? 1 : 0;
        s.segmentProgress = 0.0;
    }
    else
    {
        auto pos = positionByClock(*tr, now);
        s.lastStopIdx = pos.first;
        s.nextStopIdx = pos.second;
        s.segmentProgress = 0.0;
    }

    s.nextMajorStopIdx = computeNextMajor(*tr, s.nextStopIdx);
    return s;
}

TrackingState advanceTracking(const TrackingState &prev, const Shift &shift,
                              const Timetable &tt, const std::vector<Station> &stations,
                              const GpsFix &fix, ClockTime now)
{
    if (!prev.valid)
        return prev;

    TrackingState s = prev;
    const Train *train = trainAt(shift, tt, s.activeShiftIdx);
    if (!train || train->stops.empty())
    {
        s.valid = false;
        return s;
    }
    int lastIdx = (int)train->stops.size() - 1;

    if (fix.valid)
    {
        s.gpsLock = true;
        const geo::Point user{fix.lat, fix.lon};

        if (!s.shiftComplete && s.nextStopIdx >= 0 && s.nextStopIdx <= lastIdx)
        {
            const Station *ns = findStation(stations, train->stops[s.nextStopIdx].stationSignature);
            const bool arrived =
                ns && geo::distanceMeters(user, {ns->lat, ns->lon}) <= ns->radius_m;
            s.atStation = arrived;

            if (arrived)
            {
                if (s.nextStopIdx == lastIdx)
                {
                    // Reached this train's final stop -> auto-load the next
                    // train in the shift (skip any that don't resolve).
                    int ni = s.activeShiftIdx + 1;
                    const Train *nt = nullptr;
                    while (ni < (int)shift.trainNumbers.size())
                    {
                        nt = trainAt(shift, tt, ni);
                        if (nt && !nt->stops.empty())
                            break;
                        nt = nullptr;
                        ++ni;
                    }
                    if (nt)
                    {
                        s.activeShiftIdx = ni;
                        s.activeTrain = nt->number;
                        s.lastStopIdx = 0;
                        s.nextStopIdx = (nt->stops.size() > 1) ? 1 : 0;
                        s.segmentProgress = 0.0;
                        s.shiftComplete = false;
                        train = nt;
                        lastIdx = (int)nt->stops.size() - 1;
                    }
                    else
                    {
                        s.lastStopIdx = lastIdx;
                        s.nextStopIdx = lastIdx;
                        s.shiftComplete = true;
                        s.segmentProgress = 1.0;
                    }
                }
                else
                {
                    s.lastStopIdx = s.nextStopIdx;
                    s.nextStopIdx = s.nextStopIdx + 1;
                }
            }
        }

        if (!s.shiftComplete)
            s.segmentProgress =
                segmentProgressFor(*train, stations, s.lastStopIdx, s.nextStopIdx, user);
    }
    else
    {
        // No fix. Train advancement stays GPS-only; for position, distinguish
        // "still waiting for the first fix" (clock estimate) from "lost a fix
        // we had" (freeze the last known position).
        s.gpsLock = false;
        s.atStation = false;
        if (!prev.gpsLock && !s.shiftComplete)
        {
            auto pos = positionByClock(*train, now);
            s.lastStopIdx = pos.first;
            s.nextStopIdx = pos.second;
            s.segmentProgress = 0.0;
        }
    }

    s.nextMajorStopIdx = computeNextMajor(*train, s.nextStopIdx);
    return s;
}
