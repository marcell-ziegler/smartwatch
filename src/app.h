#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "hal/IGps.h"
#include "hal/ITouch.h"
#include "hal/ITileStore.h"
#include "hal/IButtons.h"
#include "hal/ITimeTableStore.h"
#include "timetable.h"
#include "tracking.h"

// Top-level screens. The app always boots into ShiftSelection.
enum class AppState
{
    ShiftSelection,
    Menu,
    MainView,
    DataError,     // shift/train/timetable data was invalid; prompt for another shift
    GpsDebug,      // live GPS fix + RTC clock readout, for diagnosing the GPS
    TrainSelection // manual override of the active train within the current shift
};

// The application. Shared verbatim between the esp32 and native build
// targets -- only the concrete HAL drivers and the Adafruit_GFX sink differ.
class App
{
public:
    App(Adafruit_GFX &gfx, IGps &gps, ITouch &touch, ITileStore &tiles,
        IButtons &buttons, ITimeTableStore &timetables);

    void begin();
    void tick(uint32_t now_ms);

private:
    Adafruit_GFX &_gfx;
    IGps &_gps;
    ITouch &_touch;
    ITileStore &_tiles;
    IButtons &_buttons;
    ITimeTableStore &_timetableStore;

    AppState _state = AppState::ShiftSelection;
    bool _dirty = true; // current screen needs a full repaint

    uint32_t _lastMapDraw = 0;
    uint32_t _lastDebugDraw = 0; // GpsDebug live-readout refresh throttle
    int _lastClockSec = -2;      // last second value the clock was drawn for

    // ShiftSelection: shifts suggested for today, from seasons.csv (today's
    // category, via the GPS/RTC date) + shifts.csv. If the date can't be
    // resolved, falls back to listing every shift.
    std::vector<Shift> _shifts;
    int _shiftIndex = 0;
    Shift _selectedShift; // e.g. "31B" (empty until one is chosen)

    // Timetable tracking
    Timetable _timetable;                // the selected day's category (all trains)
    std::vector<Station> _stations;      // line-wide station geo, loaded once at boot
    TrackingState _tracking;             // where we are along the active train's run

    // Menu: fixed list of actions.
    int _menuIndex = 0;

    // TrainSelection: manual override of _tracking.activeShiftIdx, since
    // time-of-day auto-detection (initialTracking) can pick the wrong train.
    // Indexes into _selectedShift.trainNumbers, same as TrackingState::activeShiftIdx.
    int _trainIndex = 0;

    // DataError: human-readable reason the selected shift couldn't be loaded.
    std::string _errorMessage;

    void setState(AppState next);

    // Populates _shifts for the current day using _timetables + _gps.clock().
    void loadShiftSuggestions();

    // Loads the whole selected category (all trains + their stops) into
    // _timetable and validates it. Returns false (and sets _errorMessage) if
    // the data is missing, malformed, or referentially invalid; _timetable is
    // cleared in that case. On success every train has stops and every
    // meet/nextNumber and the shift's own trains all resolve.
    bool loadTimetable();

    // Input handling: mutate state for one button press; no drawing here.
    void handleButton(Button b);
    void handleShiftSelectionButton(Button b);
    void handleMenuButton(Button b);
    void handleMainViewButton(Button b);
    void handleDataErrorButton(Button b);
    void handleGpsDebugButton(Button b);
    void handleTrainSelectionButton(Button b);

    // Rendering: paint the current screen (honours _dirty; MainView's map is
    // additionally throttled to 1 Hz).
    void render(uint32_t now_ms);
    void renderShiftSelection();
    void renderMenu();
    void renderMainView(uint32_t now_ms);
    void renderDataError();
    void renderGpsDebug();          // static chrome (on _dirty)
    void drawGpsDebugValues();      // live fix + clock block, refreshed at 1 Hz
    void renderTrainSelection();

    // Draws the live clock. Position depends on state: bottom-right (below the
    // map) in MainView, top-right in the menus. Redrawn once per second.
    void drawClock(const GpsClock &c);

    // Draws the map square in the top-right corner of the screen, centered
    // on (lat, lon): the user stays fixed at the center and the map scrolls
    // beneath them.
    void drawMap(double lat, double lon);

    // Draws a representation of the current position on the line with
    // Last, current and next stops shown as dots. The users progress is then shown as a green segment.
    void drawLine();
};
