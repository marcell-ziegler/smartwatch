#include "app.h"
#include "ui.h"
#include "timetable.h"
#include <math.h>
#include <stdio.h>
#include "Arduino.h"

// Chosen tile zoom level

namespace
{
    constexpr int ZOOM = 13;
    constexpr int MARGIN_X = 5;
    constexpr int MARGIN_Y = 5;
    constexpr int BASE_TEXT_SIZE = 1;

    // Map square: top-right corner of the screen.
    constexpr int16_t MAP_W = 120;
    constexpr int16_t MAP_H = 120;
    constexpr uint16_t MARKER_COLOR = 0x001F; // blue, RGB565

    constexpr uint16_t BLACK = 0x0000;
    constexpr uint16_t WHITE = 0xFFFF;
    constexpr uint16_t GREEN = 0x47AD;

    // Menu actions, in display order.
    const char *const MENU_ITEMS[] = {"Resume", "Change shift", "Select train", "GPS / Clock"};
    constexpr int MENU_COUNT = (int)(sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]));

    // Shared list layout.
    constexpr int16_t LIST_TOP = 34;
    constexpr int16_t BTN_H = 24;
    constexpr int16_t BTN_GAP = 4;

    // Short display label for a shift, e.g. Shift{31, "B", ...} -> "31B".
    std::string shiftLabel(const Shift &s)
    {
        return std::to_string(s.number) + s.trafficCategory;
    }

    // Draws a stop's label next to a dot: "SIG a08:15 d08:35" (arrival/departure
    // shown only when posted). Assumes text size/colour already set.
    void drawStopLabel(Adafruit_GFX &gfx, int16_t x, int16_t y, const Stop &s)
    {
        std::string label = s.stationSignature;
        char buf[16];
        if (s.arrival)
        {
            snprintf(buf, sizeof(buf), " a%02d:%02d", (int)s.arrival->hours, (int)s.arrival->minutes);
            label += buf;
        }
        if (s.departure)
        {
            snprintf(buf, sizeof(buf), " d%02d:%02d", (int)s.departure->hours, (int)s.departure->minutes);
            label += buf;
        }
        gfx.setCursor(x, y - 3);
        gfx.print(toCp437(label).c_str()); // signatures contain å/ä/ö
    }

    // lat/lon -> world pixel position at the given zoom (Web Mercator). This is
    // a continuous coordinate space -- tile (z,x,y) is just the TILE_PX-sized
    // chunk of it at [x*TILE_PX, y*TILE_PX].
    void lonLatToWorldPx(double lat, double lon, int z, double &px, double &py)
    {
        const double n = (double)(1 << z);
        const double latRad = lat * M_PI / 180.0;
        px = (lon + 180.0) / 360.0 * n * TILE_PX;
        py = (1.0 - asinh(tan(latRad)) / M_PI) / 2.0 * n * TILE_PX;
    }
} // namespace

App::App(Adafruit_GFX &gfx, IGps &gps, ITouch &touch, ITileStore &tiles,
         IButtons &buttons, ITimeTableStore &timetables)
    : _gfx(gfx), _gps(gps), _touch(touch), _tiles(tiles), _buttons(buttons),
      _timetableStore(timetables) {}

void App::begin()
{
    _tiles.begin();
    _touch.begin();
    _gps.begin();
    _buttons.begin();
    _timetableStore.begin();
    _gfx.setRotation(1);
    _gfx.cp437(true); // true CP437 charset, so transcoded å/ä/ö render correctly
    _gfx.fillScreen(BLACK);

    // Line-wide station geo (centre + radius), loaded once. Tracking degrades
    // gracefully if this is missing (no progress fill), so don't hard-fail.
    std::string stationsRaw;
    if (_timetableStore.readFile("stations.csv", stationsRaw))
        if (auto st = parseStationsCsv(stationsRaw))
            _stations = std::move(*st);

    loadShiftSuggestions();

    _dirty = true; // force the first paint on the next tick()
}

void App::loadShiftSuggestions()
{
    _shifts.clear();
    _shiftIndex = 0;

    std::string seasonsRaw, shiftsRaw;
    if (!_timetableStore.readFile("seasons.csv", seasonsRaw) ||
        !_timetableStore.readFile("shifts.csv", shiftsRaw))
        return; // no data -> empty list (screen shows a placeholder)

    auto seasons = parseSeasonsCsv(seasonsRaw);
    auto shifts = parseShiftsCsv(shiftsRaw);
    if (!seasons || !shifts)
        return; // malformed data -> empty list

    // Resolve today's category from the clock; if the clock has no valid date
    // (e.g. GPS cold start) fall back to listing every shift so the user can
    // still pick manually.
    const GpsClock c = _gps.clock();
    std::optional<std::string> category;
    if (c.valid)
        category = categoryForDate(*seasons, c.year, c.month, c.day);

    for (const auto &s : *shifts)
    {
        if (category && s.trafficCategory != *category)
            continue;
        _shifts.push_back(s); // keep the whole Shift (number, category, trains)
    }
}

bool App::loadTimetable()
{
    // Load the entire day's category (every train, fully populated with stops)
    // so that meets/nextNumber references all resolve.
    auto tt = loadCategory(_timetableStore, _selectedShift.trafficCategory);
    if (!tt)
    {
        _timetable = Timetable{};
        _errorMessage = "No timetable data for " + shiftLabel(_selectedShift);
        Serial.print("loadTimetable: ");
        Serial.println(_errorMessage.c_str());
        return false;
    }
    _timetable = std::move(*tt);

    // Cross-record checks: duplicate train numbers, a station repeated within a
    // train, and dangling meet / nextNumber references.
    const auto errs = validateTimetable(_timetable);
    if (!errs.empty())
    {
        _errorMessage = errs.front(); // surface the first problem
        Serial.print("loadTimetable invalid: ");
        Serial.println(_errorMessage.c_str());
        _timetable = Timetable{};
        return false;
    }

    // Every train this shift works must exist in the loaded category.
    for (const auto &num : _selectedShift.trainNumbers)
    {
        if (_timetable.findTrain(num) == nullptr)
        {
            _errorMessage = "Shift " + shiftLabel(_selectedShift) +
                            " references unknown train " + num;
            Serial.print("loadTimetable: ");
            Serial.println(_errorMessage.c_str());
            _timetable = Timetable{};
            return false;
        }
    }
    return true;
}

void App::tick(uint32_t now_ms)
{
    _gps.update();
    (void)_touch.get(); // polled but not acted on yet

    // Drain every button press queued since the last tick.
    while (auto ev = _buttons.poll())
        handleButton(*ev);

    render(now_ms);
}

// ---------------------------------------------------------------------------
//  State machine
// ---------------------------------------------------------------------------
void App::setState(AppState next)
{
    if (next == _state)
        return;
    if (next == AppState::ShiftSelection)
        loadShiftSuggestions(); // refresh for the current day on re-entry
    _state = next;
    _dirty = true;
}

void App::handleButton(Button b)
{
    switch (_state)
    {
    case AppState::ShiftSelection:
        handleShiftSelectionButton(b);
        break;
    case AppState::Menu:
        handleMenuButton(b);
        break;
    case AppState::MainView:
        handleMainViewButton(b);
        break;
    case AppState::DataError:
        handleDataErrorButton(b);
        break;
    case AppState::GpsDebug:
        handleGpsDebugButton(b);
        break;
    case AppState::TrainSelection:
        handleTrainSelectionButton(b);
        break;
    }
}

void App::handleShiftSelectionButton(Button b)
{
    const int n = (int)_shifts.size();
    switch (b)
    {
    case Button::Up:
        if (n > 0)
        {
            _shiftIndex = (_shiftIndex - 1 + n) % n;
            _dirty = true;
        }
        break;
    case Button::Down:
        if (n > 0)
        {
            _shiftIndex = (_shiftIndex + 1) % n;
            _dirty = true;
        }
        break;
    case Button::Select:
        if (n > 0)
        {
            _selectedShift = _shifts[_shiftIndex];
            // Load + validate the day's category; on bad data show the error
            // screen instead of entering MainView with an unusable timetable.
            if (loadTimetable())
            {
                // Time-of-day picks the active train; GPS takes over from here.
                _tracking = initialTracking(_selectedShift, _timetable, _stations,
                                            _gps.clock().timeOfDay());
                setState(AppState::MainView);
            }
            else
            {
                setState(AppState::DataError);
            }
        }
        break;
    default:
        break;
    }
}

void App::handleMenuButton(Button b)
{
    switch (b)
    {
    case Button::Up:
        _menuIndex = (_menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
        _dirty = true;
        break;
    case Button::Down:
        _menuIndex = (_menuIndex + 1) % MENU_COUNT;
        _dirty = true;
        break;
    case Button::Select:
        switch (_menuIndex)
        {
        case 0:
            setState(AppState::MainView);
            break; // Resume
        case 1:
            setState(AppState::ShiftSelection);
            break; // Change shift
        case 2:
            // Start the cursor on the currently active train, if tracking is
            // valid; otherwise the first train in the shift.
            _trainIndex = _tracking.valid ? _tracking.activeShiftIdx : 0;
            setState(AppState::TrainSelection);
            break; // Select train
        case 3:
            setState(AppState::GpsDebug);
            break; // GPS / Clock
        }
        break;
    default:
        break;
    }
}

void App::handleMainViewButton(Button b)
{
    if (b == Button::Select)
    {
        _menuIndex = 0;
        setState(AppState::Menu);
    }
}

void App::handleDataErrorButton(Button b)
{
    (void)b; // any press returns to shift selection to pick another
    setState(AppState::ShiftSelection);
}

void App::handleGpsDebugButton(Button b)
{
    (void)b; // any press returns to the menu
    setState(AppState::Menu);
}

void App::handleTrainSelectionButton(Button b)
{
    const int n = (int)_selectedShift.trainNumbers.size();
    switch (b)
    {
    case Button::Up:
        if (n > 0)
        {
            _trainIndex = (_trainIndex - 1 + n) % n;
            _dirty = true;
        }
        break;
    case Button::Down:
        if (n > 0)
        {
            _trainIndex = (_trainIndex + 1) % n;
            _dirty = true;
        }
        break;
    case Button::Select:
        if (n > 0)
        {
            // Manual override: reposition onto the chosen train by clock (same
            // placement initialTracking would use for a fresh pick); GPS takes
            // over from the next advanceTracking() tick.
            _tracking = trackingForTrain(_selectedShift, _timetable, _trainIndex,
                                         _gps.clock().timeOfDay());
            setState(AppState::MainView);
        }
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
//  Rendering
// ---------------------------------------------------------------------------
void App::render(uint32_t now_ms)
{
    const bool wasDirty = _dirty;

    switch (_state)
    {
    case AppState::ShiftSelection:
        renderShiftSelection();
        break;
    case AppState::Menu:
        renderMenu();
        break;
    case AppState::MainView:
        renderMainView(now_ms);
        break;
    case AppState::DataError:
        renderDataError();
        break;
    case AppState::GpsDebug:
        renderGpsDebug();
        break;
    case AppState::TrainSelection:
        renderTrainSelection();
        break;
    }

    // Clock overlay: redraw when the second changes (or after a full repaint
    // wiped it). Reading the clock every tick is cheap on both targets.
    const GpsClock c = _gps.clock();
    const int sec = c.valid ? (int)c.second : -1;
    if (wasDirty || sec != _lastClockSec)
    {
        _lastClockSec = sec;
        drawClock(c);
    }

    // GpsDebug readout refreshes ~2x/sec regardless of clock validity (the
    // clock being invalid is exactly what you may be diagnosing).
    if (_state == AppState::GpsDebug && (wasDirty || now_ms - _lastDebugDraw >= 500))
    {
        _lastDebugDraw = now_ms;
        drawGpsDebugValues();
    }
}

void App::drawClock(const GpsClock &c)
{
    char buf[16];
    if (c.valid)
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", c.hour, c.minute, c.second);
    else
        snprintf(buf, sizeof(buf), "--:--:--");

    // "HH:MM:SS" at text size 2 == 8 * 12 px wide, 16 px tall.
    constexpr int16_t clockW = 8 * 12;
    constexpr int16_t clockH = 16;
    const int16_t x = _gfx.width() - clockW - MARGIN_X;
    const int16_t y = (_state == AppState::MainView)
                          ? _gfx.height() - clockH - MARGIN_Y // below the map
                          : MARGIN_Y;                         // top-right in menus

    _gfx.fillRect(x, y, clockW, clockH, BLACK);
    _gfx.setTextSize(2);
    _gfx.setTextColor(WHITE);
    _gfx.setCursor(x, y);
    _gfx.print(buf);
    _gfx.setTextSize(BASE_TEXT_SIZE);
}

void App::renderShiftSelection()
{
    if (!_dirty)
        return;
    _dirty = false;

    const int16_t w = _gfx.width();
    _gfx.fillScreen(BLACK);
    _gfx.setTextColor(WHITE);
    _gfx.setTextSize(2);
    _gfx.setCursor(MARGIN_X, MARGIN_Y);
    _gfx.print("Select shift");

    if (_shifts.empty())
    {
        _gfx.setTextSize(1);
        _gfx.setCursor(MARGIN_X, LIST_TOP);
        _gfx.print("No shifts for today");
        return;
    }

    // Scrolling window: show a slice of the list around the selection so it's
    // always visible even when there are more shifts than fit on screen.
    const int n = (int)_shifts.size();
    const int rowH = BTN_H + BTN_GAP;
    int maxVisible = (_gfx.height() - LIST_TOP - 8) / rowH;
    if (maxVisible < 1)
        maxVisible = 1;

    int first = 0;
    if (n > maxVisible)
    {
        first = _shiftIndex - maxVisible / 2; // keep the selection centred-ish
        if (first < 0)
            first = 0;
        if (first > n - maxVisible)
            first = n - maxVisible;
    }
    const int last = (first + maxVisible < n) ? first + maxVisible : n;

    int16_t y = LIST_TOP;
    for (int i = first; i < last; ++i)
    {
        drawButton(_gfx, MARGIN_X, y, w - 2 * MARGIN_X, BTN_H,
                   shiftLabel(_shifts[i]).c_str(), i == _shiftIndex);
        y += rowH;
    }

    // More-above / more-below arrows (CP437 up/down triangles).
    _gfx.setTextSize(1);
    _gfx.setTextColor(WHITE);
    if (first > 0)
    {
        _gfx.setCursor(w - 12, LIST_TOP - 9);
        _gfx.write(0x1E);
    }
    if (last < n)
    {
        _gfx.setCursor(w - 12, _gfx.height() - 9);
        _gfx.write(0x1F);
    }
}

void App::renderTrainSelection()
{
    if (!_dirty)
        return;
    _dirty = false;

    const int16_t w = _gfx.width();
    _gfx.fillScreen(BLACK);
    _gfx.setTextColor(WHITE);
    _gfx.setTextSize(2);
    _gfx.setCursor(MARGIN_X, MARGIN_Y);
    _gfx.print("Select train");

    const int n = (int)_selectedShift.trainNumbers.size();
    if (n == 0)
    {
        _gfx.setTextSize(1);
        _gfx.setCursor(MARGIN_X, LIST_TOP);
        _gfx.print("No trains in this shift");
        return;
    }

    // Same scrolling-window layout as ShiftSelection.
    const int rowH = BTN_H + BTN_GAP;
    int maxVisible = (_gfx.height() - LIST_TOP - 8) / rowH;
    if (maxVisible < 1)
        maxVisible = 1;

    int first = 0;
    if (n > maxVisible)
    {
        first = _trainIndex - maxVisible / 2;
        if (first < 0)
            first = 0;
        if (first > n - maxVisible)
            first = n - maxVisible;
    }
    const int last = (first + maxVisible < n) ? first + maxVisible : n;

    int16_t y = LIST_TOP;
    for (int i = first; i < last; ++i)
    {
        const std::string &num = _selectedShift.trainNumbers[i];
        std::string label = num;
        if (const Train *tr = _timetable.findTrain(num))
        {
            // Route summary from the first/last stop signatures, e.g.
            // "12 Uo->Frg". Currently-tracked train gets a leading marker.
            if (!tr->stops.empty())
                label += " " + tr->stops.front().stationSignature + (char)0x1A /* CP437 -> */ +
                         tr->stops.back().stationSignature;
        }
        if (_tracking.valid && _tracking.activeShiftIdx == i)
            label = "* " + label;
        drawButton(_gfx, MARGIN_X, y, w - 2 * MARGIN_X, BTN_H,
                   toCp437(label).c_str(), i == _trainIndex);
        y += rowH;
    }

    _gfx.setTextSize(1);
    _gfx.setTextColor(WHITE);
    if (first > 0)
    {
        _gfx.setCursor(w - 12, LIST_TOP - 9);
        _gfx.write(0x1E);
    }
    if (last < n)
    {
        _gfx.setCursor(w - 12, _gfx.height() - 9);
        _gfx.write(0x1F);
    }
}

void App::renderDataError()
{
    if (!_dirty)
        return;
    _dirty = false;

    _gfx.fillScreen(BLACK);
    _gfx.setTextColor(WHITE);
    _gfx.setTextSize(2);
    _gfx.setCursor(MARGIN_X, MARGIN_Y);
    _gfx.print("Data error");

    // Detail line(s). Adafruit_GFX wraps at the screen edge by default, so a
    // long validator message flows onto multiple lines rather than clipping.
    _gfx.setTextSize(1);
    _gfx.setCursor(MARGIN_X, LIST_TOP);
    _gfx.print(toCp437(_errorMessage).c_str()); // may embed station sigs (å/ä/ö)

    _gfx.setCursor(MARGIN_X, _gfx.height() - 20);
    _gfx.print("Press any key to pick another shift");
}

void App::renderGpsDebug()
{
    if (!_dirty)
        return;
    _dirty = false;

    // Static chrome only; the live values are drawn by drawGpsDebugValues(),
    // which render() calls on entry and then ~2x/sec.
    _gfx.fillScreen(BLACK);
    _gfx.setTextColor(WHITE);
    _gfx.setTextSize(2);
    _gfx.setCursor(MARGIN_X, MARGIN_Y);
    _gfx.print("GPS / Clock");
    _gfx.setTextSize(BASE_TEXT_SIZE);
}

void App::drawGpsDebugValues()
{
    const int16_t x = MARGIN_X;
    int16_t y = LIST_TOP;

    // Clear just the value area (below the title / clock overlay).
    _gfx.fillRect(0, LIST_TOP, _gfx.width(), _gfx.height() - LIST_TOP, BLACK);
    _gfx.setTextSize(1);
    _gfx.setTextColor(WHITE);

    const GpsClock c = _gps.clock();
    const GpsFix f = _gps.fix();
    char buf[48];
    auto line = [&](const char *s)
    { _gfx.setCursor(x, y); _gfx.print(s); y += 12; };

    // Clock invalid => the module isn't sending time (check I2C wiring/addr).
    line(c.valid ? "Clock: VALID" : "Clock: -- no time (check I2C)");
    if (c.valid)
    {
        snprintf(buf, sizeof(buf), " %04d-%02d-%02d  %02d:%02d:%02d",
                 c.year, c.month, c.day, c.hour, c.minute, c.second);
        line(buf);
    }
    y += 6;

    // Fix invalid but clock valid => talking, but no satellite lock yet.
    line(f.valid ? "Fix: VALID" : "Fix: NO FIX (needs sky view)");
    if (f.valid)
    {
        snprintf(buf, sizeof(buf), " lat %.6f", f.lat);
        line(buf);
        snprintf(buf, sizeof(buf), " lon %.6f", f.lon);
        line(buf);
        snprintf(buf, sizeof(buf), " spd %.1f m/s  crs %.0f", f.speed_mps, f.course_deg);
        line(buf);
    }
    y += 6;
    line("Select: back to menu");
}

void App::renderMenu()
{
    if (!_dirty)
        return;
    _dirty = false;

    const int16_t w = _gfx.width();
    _gfx.fillScreen(BLACK);
    _gfx.setTextColor(WHITE);
    _gfx.setTextSize(2);
    _gfx.setCursor(MARGIN_X, MARGIN_Y);
    _gfx.print("Menu");

    int16_t y = LIST_TOP;
    for (int i = 0; i < MENU_COUNT; ++i)
    {
        drawButton(_gfx, MARGIN_X, y, w - 2 * MARGIN_X, BTN_H,
                   MENU_ITEMS[i], i == _menuIndex);
        y += BTN_H + BTN_GAP;
    }
}

void App::renderMainView(uint32_t now_ms)
{
    const GpsFix fix = _gps.fix();
    const bool full = _dirty;

    if (full)
    {
        _dirty = false;
        _gfx.fillScreen(BLACK);
        _gfx.setTextColor(WHITE);
        _gfx.setTextSize(2);
        _gfx.setCursor(MARGIN_X, MARGIN_Y);
        _gfx.print("Lennakatten");
        _gfx.setTextSize(BASE_TEXT_SIZE);
    }

    // Map + tracking recompute + line, throttled to 0.33 Hz (also on entry).
    if (full || now_ms - _lastMapDraw >= 3000)
    {
        _lastMapDraw = now_ms;
        if (fix.valid)
            drawMap(fix.lat, fix.lon);
        _tracking = advanceTracking(_tracking, _selectedShift, _timetable, _stations,
                                    fix, _gps.clock().timeOfDay());
        drawLine();
    }
}

void App::drawMap(double lat, double lon)
{
    const int16_t mapX0 = _gfx.width() - MAP_W; // top-right corner
    const int16_t mapY0 = 0;

    double userPx, userPy;
    lonLatToWorldPx(lat, lon, ZOOM, userPx, userPy);

    // World-pixel origin of the viewport: the user is always drawn dead
    // center, so it's the *map* that scrolls beneath them, not the marker
    // that moves across the map.
    const double viewLeft = userPx - MAP_W / 2.0;
    const double viewTop = userPy - MAP_H / 2.0;

    const int tileXMin = (int)floor(viewLeft / TILE_PX);
    const int tileXMax = (int)floor((viewLeft + MAP_W - 1) / TILE_PX);
    const int tileYMin = (int)floor(viewTop / TILE_PX);
    const int tileYMax = (int)floor((viewTop + MAP_H - 1) / TILE_PX);

    // One tile at a time -- a full decoded tile is TILE_WORDS*2 = 128KB.
    // Heap-allocated once and reused: as a `static` array this overflowed
    // the ESP32's fixed DRAM/BSS segment at link time even though the chip
    // has 320KB of RAM overall -- most of that is only reachable via the
    // heap, not the linker's static-data segment.
    static uint16_t *tileBuf = new uint16_t[TILE_WORDS];

    // Background fill first (its own transaction): covers any tile missing from
    // the pre-converted ribbon (loadTile() returns false) so stale pixels from
    // a previous draw don't linger there.
    // _gfx.startWrite();
    // _gfx.writeFillRect(mapX0, mapY0, MAP_W, MAP_H, 0x0000);
    // _gfx.endWrite();

    for (int ty = tileYMin; ty <= tileYMax; ++ty)
    {
        for (int tx = tileXMin; tx <= tileXMax; ++tx)
        {
            // The SD card and the display share the SPI bus, so the tile read
            // must NOT happen inside a display startWrite()/endWrite() -- that
            // nests SPI.beginTransaction() and deadlocks. Read first, then open
            // a display transaction to blit.
            if (!_tiles.loadTile(ZOOM, tx, ty, tileBuf))
                continue;

            // Screen position of this tile's (0,0) pixel.
            const int16_t tileScreenX =
                (int16_t)lround(mapX0 + (tx * (double)TILE_PX - viewLeft));
            const int16_t tileScreenY =
                (int16_t)lround(mapY0 + (ty * (double)TILE_PX - viewTop));

            // Manual per-pixel clip against the map rectangle: drawRGBBitmap
            // has no source-crop, and the map square's left and bottom edges
            // (mapX0, mapY0+MAP_H) are internal boundaries, not screen edges,
            // so the generic drawPixel clipping alone would let an
            // overhanging tile bleed into the panel around it.
            _gfx.startWrite();
            for (int16_t row = 0; row < TILE_PX; ++row)
            {
                const int16_t sy = tileScreenY + row;
                if (sy < mapY0 || sy >= mapY0 + MAP_H)
                    continue;
                for (int16_t col = 0; col < TILE_PX; ++col)
                {
                    const int16_t sx = tileScreenX + col;
                    if (sx < mapX0 || sx >= mapX0 + MAP_W)
                        continue;
                    _gfx.writePixel(sx, sy, tileBuf[row * TILE_PX + col]);
                }
            }
            _gfx.endWrite();
        }
    }

    // Position marker: always dead center. fillCircle self-brackets its own
    // transaction, so it must be OUTSIDE any manual startWrite()/endWrite().
    _gfx.fillCircle(mapX0 + MAP_W / 2, mapY0 + MAP_H / 2, 3, MARKER_COLOR);
}

void App::drawLine()
{
    constexpr int STOP_RADIUS = 3;
    constexpr int LEFT_MARGIN = 10;
    constexpr int Y_MARGIN = 15;
    constexpr int TEXT_X = LEFT_MARGIN + 10;

    const int bottomY = _gfx.height() - Y_MARGIN; // last stop (behind you)
    const int middleY = _gfx.height() / 2;        // next stop
    const int topY = Y_MARGIN + 20;               // next major stop
    // Clear only the strip left of the map so we never erase the map/clock.
    const int clearW = _gfx.width() - MAP_W;

    _gfx.fillRect(0, topY - STOP_RADIUS - 1, clearW,
                  (bottomY + 10) - (topY - STOP_RADIUS - 1), BLACK);

    _gfx.setTextSize(1);
    _gfx.setTextColor(WHITE);

    const Train *train = _tracking.valid ? _timetable.findTrain(_tracking.activeTrain) : nullptr;
    if (!train)
    {
        _gfx.setCursor(LEFT_MARGIN, middleY);
        _gfx.print("No tracking");
        return;
    }
    const int nStops = (int)train->stops.size();
    auto valid = [&](int i)
    { return i >= 0 && i < nStops; };

    // --- last -> next: solid, green up to progress, white for the rest ------
    int greenEndY = bottomY - (int)(_tracking.segmentProgress * (bottomY - middleY));
    if (greenEndY < middleY)
        greenEndY = middleY;
    if (greenEndY > bottomY)
        greenEndY = bottomY;
    _gfx.drawLine(LEFT_MARGIN, bottomY, LEFT_MARGIN, greenEndY, GREEN); // travelled
    _gfx.drawLine(LEFT_MARGIN, greenEndY, LEFT_MARGIN, middleY, WHITE); // remaining

    // --- next -> next-major: dotted -----------------------------------------
    for (int yy = middleY; yy >= topY; yy -= 4)
        _gfx.drawPixel(LEFT_MARGIN, yy, WHITE);

    // --- dots + labels -------------------------------------------------------
    _gfx.fillCircle(LEFT_MARGIN, bottomY, STOP_RADIUS, WHITE);
    _gfx.fillCircle(LEFT_MARGIN, middleY, STOP_RADIUS, WHITE);
    _gfx.fillCircle(LEFT_MARGIN, topY, STOP_RADIUS, WHITE);

    if (valid(_tracking.lastStopIdx))
        drawStopLabel(_gfx, TEXT_X, bottomY, train->stops[_tracking.lastStopIdx]);
    if (valid(_tracking.nextStopIdx))
        drawStopLabel(_gfx, TEXT_X, middleY, train->stops[_tracking.nextStopIdx]);
    if (valid(_tracking.nextMajorStopIdx))
        drawStopLabel(_gfx, TEXT_X, topY, train->stops[_tracking.nextMajorStopIdx]);

    // --- GPS-lock state (per your "make it visible" note) --------------------
    _gfx.setCursor(LEFT_MARGIN, bottomY + 8);
    _gfx.print(_tracking.gpsLock ? "GPS ok" : "no GPS");
}
