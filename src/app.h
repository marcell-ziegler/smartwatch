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

// Top-level screens. The app always boots into ShiftSelection.
enum class AppState
{
    ShiftSelection,
    Menu,
    MainView
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
    ITimeTableStore &_timetables;

    AppState _state = AppState::ShiftSelection;
    bool _dirty = true; // current screen needs a full repaint

    uint32_t _lastMapDraw = 0;
    int _lastClockSec = -2; // last second value the clock was drawn for

    // ShiftSelection: shifts suggested for today, from seasons.csv (today's
    // category, via the GPS/RTC date) + shifts.csv. If the date can't be
    // resolved, falls back to listing every shift.
    std::vector<std::string> _shifts;
    int _shiftIndex = 0;
    std::string _selectedShift; // e.g. "31B" (empty until one is chosen)

    // Menu: fixed list of actions.
    int _menuIndex = 0;

    void setState(AppState next);

    // Populates _shifts for the current day using _timetables + _gps.clock().
    void loadShiftSuggestions();

    // Input handling: mutate state for one button press; no drawing here.
    void handleButton(Button b);
    void handleShiftSelectionButton(Button b);
    void handleMenuButton(Button b);
    void handleMainViewButton(Button b);

    // Rendering: paint the current screen (honours _dirty; MainView's map is
    // additionally throttled to 1 Hz).
    void render(uint32_t now_ms);
    void renderShiftSelection();
    void renderMenu();
    void renderMainView(uint32_t now_ms);

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
