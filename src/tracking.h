#pragma once
#include <string>
#include <vector>

#include "timetable.h"
#include "hal/IGps.h" // GpsFix
#include "ClockTime.h"

// Where the user is along the active train's run, for drawLine to render.
// Stop indices refer to the *active* train's `stops` vector.
struct TrackingState
{
    bool valid = false;         // false = no resolvable train / not initialised
    int activeShiftIdx = -1;    // index into Shift::trainNumbers
    std::string activeTrain;    // resolved train number (convenience)

    int lastStopIdx = -1;       // bottom dot: most recently reached stop
    int nextStopIdx = -1;       // middle dot: the immediate next stop
    int nextMajorStopIdx = -1;  // top dot: next stop with a departure, or the
                                // terminus -- strictly after the middle

    double segmentProgress = 0; // 0..1 from last -> next (geo straight-line)
    bool gpsLock = false;       // true = position came from a live fix
    bool atStation = false;     // within the next stop's radius right now
    bool shiftComplete = false; // reached the final stop of the last train
};

// Initialise tracking when a shift is entered: time-of-day picks the active
// train and the position along it. No GPS is used here (that takes over on the
// next advanceTracking with a fix). Returns an invalid state if no shift train
// resolves in the timetable.
TrackingState initialTracking(const Shift &shift, const Timetable &tt,
                              const std::vector<Station> &stations, ClockTime now);

// Advance one step. With a valid fix, GPS drives progress: you reach the next
// stop when inside its radius (monotonic -- jitter can't un-pass a stop), and
// reaching a train's final stop auto-loads the next train in the shift. Without
// a fix, position is re-estimated from the clock within the *same* train
// (train advancement stays GPS-only) and gpsLock is cleared.
TrackingState advanceTracking(const TrackingState &prev, const Shift &shift,
                              const Timetable &tt, const std::vector<Station> &stations,
                              const GpsFix &fix, ClockTime now);
