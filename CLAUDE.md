# CLAUDE.md — agent guide for the railway pocket-watch

Context and conventions for working in this repo. Read this before making
changes. The user-facing `README.md` covers build/run mechanics; this file
covers *why things are the way they are* and what to watch out for.

## What this is

A smart pocket-watch for the user's volunteer work on the **Lennakatten**
heritage railway (a narrow-gauge, single-track line, Uppsala Östra ⇄ Faringe).
The device shows the time, a live 1 Hz map of the train's position along the
line, and the timetable for the user's current shift.

**Hardware target:** Adafruit Feather ESP32 V2 + 2.4" TFT FeatherWing
(ILI9341 display over SPI + TSC2007 resistive touch on **I2C**), a PA1010D GPS on **I2C**
(STEMMA QT, addr `0x10`, with a battery-backed RTC), and a MAX17048 I²C battery
gauge. Pin map is documented at the top of `src/main_esp32.cpp` (verified
against the V2 pinout: SPI CS pins for display/touch/SD, GPIO for the 5-way
d-pad, I2C SDA 22/SCL 20 shared by GPS + gauge; avoids flash 6-11, input-only
34/36/39/37, and strapping 0/2/12). Development happens in
VS Code / PlatformIO. The user is an experienced programmer but **new to
embedded C++ and Arduino** — explain embedded-specific reasoning, don't assume
it.

## Two build targets, one codebase

The whole point of the architecture is that application code is written **once**
and runs on both:

- **`native`** — a desktop SDL2 simulator. Compiles the *real* `Adafruit_GFX`
  so rendering is pixel-identical to hardware. Fast iteration. GPS is
  keyboard/CSV-driven, touch is mouse-driven. Binary: `.pio/build/native/program`.
- **`esp32`** — the real hardware.

```
pio run -e native && ./.pio/build/native/program   # simulator
pio run -e esp32 -t upload                          # flash hardware
```

This is achieved with **dependency injection through HAL interfaces**. `App`
(in `src/app.{h,cpp}`) never touches hardware directly — it only knows
`Adafruit_GFX&` plus three seams in `include/hal/`. Each target constructs its
own concrete drivers and hands them to the same `App`:

| Seam | native impl | esp32 impl |
|---|---|---|
| `IGps` | `SimGps` (CSV replay + arrow-key nudge; `clock()` = host clock) | `Esp32Gps` (Adafruit_GPS/PA1010D over I2C; `clock()` = GPS RTC) |
| `ITouch` | `SdlTouch` (mouse) | `Esp32Touch` (TSC2007 over I2C) |
| `IButtons` | `SdlButtons` (WASD + Enter) | `Esp32Buttons` (GPIO d-pad; placeholder pins) |
| `ITileStore` | `FolderTileStore` | `SdTileStore` (SD card) |
| `ITimeTableStore` | `FolderTimetableStore` (`assets/timetables/`) | `SdTimetableStore` (`/timetables/` on SD) |
| display | `PCDisplay` (SDL2) | `Adafruit_ILI9341` (is-a GFX) |

`IGps::clock()` returns a `GpsClock` (date + h/m/s + validity), tracked
separately from `fix()` because the RTC keeps time before a position fix. It
drives the always-on clock overlay and the "what category runs today" lookup.
Both impls return **Stockholm local time**: the GPS module reports UTC, and
`clock()` runs it through `utcToStockholm()` (in `timetable.cpp` —
`stockholmUtcOffsetHours` applies CET/CEST with EU DST rules, rolling the date
over if needed). `SimGps` reads the host clock as UTC (`gmtime_r`) then converts
too, so the sim matches hardware regardless of the dev machine's own timezone.

`ITimeTableStore` is just `readFile(path, out)` (raw bytes); parsing lives in
`src/timetable.cpp`. Paths are relative to the timetables root
(`readFile("B/80.csv", ...)`).

`IButtons` is edge-triggered: `poll()` returns the next button-*down* since the
last call (or `nullopt`). On native, the SDL loop injects presses via
`SdlButtons::pushDown()` (auto-repeat ignored); on esp32, `Esp32Buttons` reads
active-low GPIO with per-pin edge detection (**pins are placeholders — no
debounce yet**). `Button` is `{Up,Down,Left,Right,Select}` (the 5-way d-pad);
extra buttons get added here later.

`src/main_native.cpp` and `src/main_esp32.cpp` are the wiring; `build_src_filter`
in `platformio.ini` includes/excludes the target-specific files.

## Critical build constraints (these bite)

1. **Both envs must compile with C++17.** Shared code uses `std::optional`,
   `<charconv>`, and aggregate init of `ClockTime`. The Arduino-ESP32 core
   defaults to `-std=gnu++11`, so `[env:esp32]` sets
   `build_unflags = -std=gnu++11` / `build_flags = -std=gnu++17`. If you add
   C++17+ features and the esp32 build breaks with "only available from C++17",
   this is why. Don't lower the standard; the native env is already gnu++17.

2. **`Adafruit_GFX` is vendored, not from the registry, for `native`.** See
   `lib/adafruit_gfx_core/` — a trimmed copy of upstream `Adafruit_GFX.{h,cpp}`
   + `glcdfont.c` with the `#include <Adafruit_I2CDevice.h>` /
   `<Adafruit_SPIDevice.h>` lines removed. Newer upstream GFX pulls those in
   unconditionally (for its SPITFT/GrayOLED subclasses), which would drag
   BusIO/Wire/SPI onto the desktop build where they don't exist. `[env:native]`
   also needs `lib_compat_mode = off` (LDF otherwise rejects the vendored copy
   as "framework incompatible") and `-D ARDUINO=10808` (so GFX takes its
   Arduino code path, not `WProgram.h`). The esp32 build uses the *real*
   registry GFX + hardware libs normally.

3. **The native Arduino shim** lives in `src/native/arduino_compat/`
   (`Arduino.h`, `Print.h`, `WString.h`). It provides just enough of the
   Arduino core for `Adafruit_GFX` to compile on desktop. Gotcha already
   handled: STL headers are `#include`d *before* the `min`/`max`/`abs` macros
   are defined, because those macros otherwise clobber libstdc++ internals. If
   native/shared code starts using a **new** STL header (this already bit
   `<deque>`/`<optional>` when buttons were added) and you get
   `expected unqualified-id before '(' token` from inside a libstdc++ header,
   add that header to the pre-include list in `Arduino.h`. The shim also
   provides a `Serial` object (an `inline` `Print` subclass writing to stdout)
   so `Serial.print()/println()` from shared code appear in the terminal on
   native, matching the real UART `Serial` on esp32. `Serial.printf()` is
   **not** included (base `Print` has no `printf`) — add it to `SerialClass` if
   shared code needs it.

4. **ESP32 RAM: a full tile buffer must be heap-allocated, not static.** A
   256×256 RGB565 tile is `TILE_WORDS*2 = 128 KB`. As a `static` array this
   *overflows the fixed DRAM/BSS segment at link time* even though the chip has
   320 KB total — most of that RAM is only reachable via the heap. `App::drawMap`
   uses `static uint16_t* tileBuf = new uint16_t[TILE_WORDS];` for this reason.
   General rule: large buffers go on the heap on ESP32. The native build has no
   such limit, so the simulator will happily hide this class of bug — watch for
   it.

## Map rendering (implemented)

`App::drawMap(lat, lon)` in `src/app.cpp` draws a **120×120 square pinned to
the top-right** of the 320×240 landscape screen, at zoom `ZOOM` (currently 13).
Key ideas:

- Everything is computed in a continuous **world-pixel space** (Web Mercator,
  `lonLatToWorldPx`). A tile `(z,x,y)` is just the 256×256 chunk at
  `[x*256, y*256]`. Tiles, the marker, and (future) the route all share this
  one transform: `screen = world_pos - viewport_origin + map_region_origin`.
- The **user stays dead-center**; the viewport origin is `user_world_px -
  half_the_square`, so the map scrolls under a fixed marker.
- Tiles are blitted with a **manual per-pixel clip** against the square,
  because the square's left/bottom edges are *internal* boundaries (not screen
  edges), and `drawRGBBitmap` has no source-crop — naive blitting would bleed
  into the panel beside it.
- A `writeFillRect` black background is drawn first so **missing tiles**
  (outside the converted ribbon) show black, not stale pixels.
- Redraw is **throttled to 1 Hz** in `tick()` via the `now_ms - _lastMapDraw`
  pattern — the standard embedded "check elapsed millis" idiom (there is no
  async; `loop()`/`tick()` is a busy loop).

Note: `begin()` uses `setRotation(1)` = **landscape 320×240**. The sim's
`PCDisplay` is constructed with the ILI9341's *native* dims (240×320) so
`width()/height()` track rotation exactly like hardware; it stores/shows the
logical (post-rotation) view and resizes the SDL window when rotation changes.
So the sim now shows landscape at rotation 1 just like the panel.

## Tiles (map data)

Format: raw little-endian **RGB565**, 256×256, `131072` bytes, stored as
`<z>/<x>/<y>.bin`. No PNG decoder on either target. `ITileStore::loadTile()`
reads them (folder on desktop, SD on hardware).

**Pipeline** (offline, desktop): MOBAC exports an **MBTiles** atlas →
`tools/mbtiles_to_png.py` (unpacks + flips TMS→XYZ row order) →
`tools/png_to_bin.py` (RGB565). `tools/tile_calc.py` computes which z/x/y a
lat/lon falls into. The demo set in `assets/tiles/` (zoom 13) comes from
`assets/Lennakatten.mbtiles`, paired with `assets/tracks/demo.csv`.

**⚠️ OSM tile policy:** do **not** write scripts that bulk-download from
`tile.openstreetmap.org` — its usage policy forbids it. Tiles come via MOBAC
from providers whose terms allow offline caching, or self-rendered. Keep the
"© OpenStreetMap contributors" (ODbL) attribution in mind.

## Time: `include/ClockTime.h`

A small value type: `{uint8_t hours, minutes}`, no date. Arithmetic with an
`int` (minutes) **wraps silently at midnight** both ways; subtracting two
`ClockTime`s gives a *non-wrapped signed* minute delta (a duration across
midnight is ambiguous, so the caller decides direction). It's an **aggregate**
with default member initializers — construct with braces `ClockTime{8, 5}`, not
`ClockTime(8, 5)`. This is the single time type for both schedule times and
(eventually) the GPS RTC clock.

## Timetable data model & storage

Data model in `src/timetable.h`; CSV parsers in `src/timetable.cpp` (pure C++,
no HAL/Arduino dependency, shared by both targets). Storage is **plain CSV on
the SD card**, deliberately human-readable/hand-editable — no binary packing,
no ID interning (the dataset is tiny: ≤~18 trains per category, 14 stations, so
linear scans and full-file loads are fine, and inspectability was an explicit
user preference).

**Layout** under `assets/timetables/`:
```
seasons.csv              validFrom,validTo,dayOfWeek,category   (which category runs when)
shifts.csv               number,category,trainNumbers(;-sep)    (shift catalog, all categories)
<CATEGORY>/              one dir per traffic category: A B C D E ...
  trains.csv             number,vehicleType,direction,nextNumber   (roster)
  <trainNumber>.csv      station,arrival,departure,stopType,exchangeType,staffed,meets(;-sep),remark
```

**Parser conventions** (see header/`README.md` for full detail):
- No header row. Lines starting with `#` (after optional whitespace) and blank
  lines are skipped. CRLF and LF both tolerated. Sub-lists use `;`.
- Any malformed row fails the *whole file* (`nullopt`) — validate on desktop
  before the card reaches the device.
- Enum tokens are the enum identifier names, matched **case-insensitively**
  (`BigSteam`, `Up`/`Down`, `Mandatory`/`Optional`,
  `Embarking`/`Disembarking`/`EmbarkingAndDisembarking`/`None`).
- `arrival`/`departure` are `std::optional<ClockTime>`: an **empty cell** is a
  valid absent time; a non-empty malformed one is an error. Consumers must
  check `.has_value()` before dereferencing.
- **Role is derived, not stored**: shift number's tens digit → `1x` Förare
  (driver), `3x` Tågbefälhavare, `4x` Konduktör (`roleForShiftNumber`).

**Loading model:** the device loads only the *one* category running that day
(pick shift → its category → load that dir). One category per day is a hard
rule; special/event days get their own single-day category + directory.

### Domain reference (Lennakatten)

- **Stations, in `Down` order (Uö→Frg), km *decreasing* toward Faringe:**
  Uö (Uppsala Östra), Fl (Fyrislund), Ås (Årsta), Sa (Skölsta), B (Bärby),
  Ga (Gunsta), Ml (Marielund), Lh (Lövstahagen), Slä (Selknä), Löt, Lna
  (Länna), Alg (Almunge), Mg (Moga), Frg (Faringe). `Up` = Frg→Uö, km
  increasing. (The paper graphs also show Fb/Funbo and Lbr/Länna bruk, which
  are **not** in the 14-station model — omit them.)
- **Vehicle types:** BigSteam (loco "Tor"), SmallSteam ("Långshyttan"),
  Diesel, Railbus.
- A **meet** ("möte") is which train(s) you must wait for before proceeding —
  safety-relevant on a single-track line. Authored by hand into the `meets`
  column (algorithmically complex to derive, intuitive for a human).
- Traffic categories (A, B, …) run on different days/seasons; `seasons.csv`
  maps date-range + weekday → category.

## Current status

**Implemented & tested:** map rendering (`drawMap`); `ClockTime`; all timetable
data structures; all CSV parsers (`parseSeasonsCsv`, `parseShiftsCsv`,
`parseTrainsCsv`, `parseStopsCsv`) + field converters; `Timetable::findTrain`;
cross-record validators (`isValidIsoDate`, `validateSeasons`, `validateShifts`,
`validateTimetable`); date helpers (`isoDayOfWeek`, `toIsoDate`,
`categoryForDate`); the `geo` distance module, `stations.csv` parsing, and the
`tracking` state machine (below). The `App` state machine, clock overlay, shift
auto-loading, and timetable tracking are wired end-to-end; `IGps::clock()` and
both `ITimeTableStore` impls are done. All of the pure logic is covered by the
`tests/` suite (300 checks; see conventions below). Both `pio run -e native` and `-e esp32` build clean. Real B-traffic data
is being entered by hand in `assets/timetables/`; the user is actively filling
per-train files.

**App structure:** `App` is a small state machine over
`AppState { ShiftSelection, Menu, MainView, DataError, GpsDebug }` (in `app.h`).
ShiftSelection scrolls (windowed list clamped around `_shiftIndex`, with CP437
up/down arrows). The Menu has a **GPS / Clock** item → `GpsDebug`, a live readout
of `_gps.clock()` + `_gps.fix()` (refreshed ~2×/sec) for diagnosing the GPS:
"Clock: -- no time" means the module isn't talking (I2C); "Fix: NO FIX" with a
valid clock means it's talking but hasn't locked satellites. `tick()`
drains all queued button presses (`handle*Button`, input-only, no drawing) then
`render()`s the current screen. Screens repaint only when a `_dirty` flag is set
(state or selection change); `MainView`'s map additionally redraws at 1 Hz.
Boots into `ShiftSelection`. Transitions: ShiftSelection --Select--> MainView
*(or DataError if the load fails)*; MainView --Select--> Menu; Menu
{Resume→MainView, Change shift→ShiftSelection}; DataError --any button-->
ShiftSelection. Reusable `drawButton()` (in `src/ui.{h,cpp}`) draws a labelled
button, highlighted black-on-white when selected, white-on-black otherwise.

**DataError screen:** selecting a shift runs `loadTimetable()`, which (a)
`loadCategory`-loads the day's category, (b) runs `validateTimetable`, and (c)
checks the shift's own train numbers all resolve. Any failure sets
`_errorMessage`, logs it via `Serial`, clears `_timetable`, and routes to
`DataError` (which shows the message + "pick another shift") instead of entering
MainView with unusable data — so bad/incomplete CSVs can't crash the app. Since
most per-train files are still empty, real shifts currently land here until the
data is filled.

**Clock overlay:** `App::drawClock()` draws the live time (HH:MM:SS, size 2) on
*every* screen, redrawn whenever the second changes (independent of the `_dirty`
full-repaints, so menus don't flicker). Position is state-dependent: bottom-right
(below the map) in MainView, top-right in the menus. Note the top-right menu
clock sits at the top-right; the screen is 320×240 landscape on both sim and
hardware (rotation 1), so layout is now consistent between the two.

**Shift auto-loading:** `App::loadShiftSuggestions()` (called on boot and on
re-entry to ShiftSelection) reads `seasons.csv` + `shifts.csv` via
`ITimeTableStore`, resolves today's category from `_gps.clock()` via
`categoryForDate()` (date helpers `isoDayOfWeek`/`toIsoDate`/`categoryForDate`
in `timetable.cpp`), and lists that category's shifts (labelled `"<number><cat>"`,
e.g. `31B`). Falls back to listing *all* shifts if the date can't be resolved
(e.g. GPS cold start), and shows "No shifts for today" if the list is empty.

**Shift → timetable load:** selecting a shift stores it (`_selectedShift`) and
calls `App::loadTimetable()` → `loadCategory(store, category)` (in
`timetable.cpp`), which reads `<cat>/trains.csv` for the roster then
`<cat>/<number>.csv` for **every** train's stops, assembling one in-RAM
`Timetable` (`App::_timetable`). The whole day's category is loaded (not just the
shift's trains) so meets/nextNumber references resolve; the shift's own trains
are then found via `_timetable.findTrain(number)`. A train with a
missing/empty/malformed stop file makes the whole load fail (`nullopt` →
`_timetable` cleared) — a `Train` is only valid with a populated stop list.
Note: most per-train CSVs are still empty, so `loadCategory` will currently fail
for real categories until the data is filled.

**Timetable tracking (implemented):** three pure, unit-tested modules feed the
progress view.
- `src/geo.{h,cpp}` — `distanceMeters` + `fractionAlongSegment` (equirectangular
  straight-line for now; a polyline/along-track variant slots behind the same
  interface using the KML "Banan" line later).
- `Station` + `parseStationsCsv` (`signature,name,lat,lon,radius_m`, in
  `assets/timetables/stations.csv`, all 14 stations filled) + `findStation`.
- `src/tracking.{h,cpp}` — `initialTracking()` picks the active train from the
  shift by **time-of-day** at shift entry; `advanceTracking()` then lets **GPS**
  drive: you reach the next stop when inside its `radius_m` (monotonic — jitter
  can't un-pass a stop), and reaching a train's final stop auto-loads the next
  train in the shift. No fix → clock estimate while *waiting* for the first fix,
  freeze after a fix is *lost*; `gpsLock` exposes which. Output `TrackingState`
  has last/next/next-major stop indices (top = next departure-posted stop or
  terminus, strictly after the middle) + `segmentProgress` (0..1).
`App` loads `stations.csv` once at boot, inits `_tracking` on shift select, and
recomputes it each 1 Hz in `renderMainView`. `App::drawLine()` renders it: three
dots (last/next/next-major) with `SIG a<arr> d<dep>` labels, the last→next
segment filled green by `segmentProgress` (white remainder), the next→major
segment dotted, and a GPS-lock indicator. The whole day's category is loaded, so
meets/other trains resolve.

**Stubbed / not yet done:**
- **Map polyline / real along-track distance + speed:** the KML "Banan" line is
  captured (`assets/Lennakatten.kml`) but not yet parsed/used; tracking uses
  straight-line distance and the map draws no track overlay.
- Delay display (`actual − scheduled`) is deferred — labels show scheduled times
  only for now.
- Touch input is polled each tick but not acted on (navigation is button-based).

## Known open decisions / caveats

- **Timetable graphs are not machine-readable.** The source timetables are
  dense graphical (string-line) diagrams — see the conversation history. They
  can't be reliably OCR'd/transcribed automatically, and this is
  *safety-relevant operational data* (times + meets). The plan is to build a
  helper authoring tool/script later; until then the empty `<train>.csv` files
  are filled **by hand** by the user. Do **not** mass-generate timetable times
  from images — a wrong meet/time is a real hazard, and looks authoritative
  once in a file.
- **Swedish characters (solved):** data is UTF-8 (needs å/ä/ö in
  names/remarks/station sigs like "Uö"). The built-in `glcdfont.c` is actually
  **Code Page 437**, which *contains* å/ä/ö/Å/Ä/Ö — the earlier scrambling was
  just UTF-8's multi-byte encoding rendering as two glyphs each. `toCp437()` (in
  `src/ui.cpp`) transcodes UTF-8 → the single CP437 byte at the display layer
  (data stays UTF-8), and `begin()` calls `_gfx.cp437(true)`. Wrap any
  data-derived text in `toCp437(...)` before `print()` (already done for station
  labels in `drawStopLabel` and the DataError message). No custom `GFXfont`
  needed unless glyphs beyond CP437 are ever required.
- The timetable enums in `timetable.h` are **unscoped** `enum` (values like
  `None`, `Up`, `Disembarking` leak into global scope). `None` in particular
  collides with X11's `#define None` on Linux — if `timetable.h` ever gets
  included in a TU that transitively pulls X11 (e.g. via SDL), it'll break.
  `enum class` would fix it if you're refactoring anyway.

## Working conventions in this repo

- **Verify, then clean up.** Changes to shared/render/parse code are checked by
  building *both* envs and (for logic) a throwaway `g++ -std=gnu++17` test in
  the scratch dir with real assertions. Temporary demo code / synthetic test
  assets are removed afterward; don't leave them in `src/` or `assets/`.
- **Unit tests live in `tests/`** (a plain-g++ suite, *not* `test/` — that name
  is reserved by PlatformIO's `pio test`). Run with `make -C tests` (or `cd
  tests && make`). It compiles `tests/*.cpp` + `src/timetable.cpp` (pure logic,
  no HAL/Arduino/SDL) into `tests/run_tests`, and returns non-zero on any
  failure. `tests/framework.h` is a tiny dependency-free CHECK harness that
  reports *every* failing case (doesn't abort on first). Covers `ClockTime`,
  the CSV/text utils, time parsing, enum converters, all four record parsers
  (valid + malformed rows), the cross-record validators, and `findTrain`. Add
  to it whenever you touch `timetable.{h,cpp}` or `ClockTime.h`.
- **Don't commit unless asked.** The user drives git and commits frequently
  with short messages. `main` is the working branch. `.pio/` and most
  `.vscode/` are gitignored.
- The user **fixes/refactors things between turns** (formatting via clang-format
  to Allman braces, small renames). Re-read files before editing; don't revert
  their changes.
- When touching `App`, keep it hardware-agnostic — it must compile and behave
  on both targets. If something works in the simulator, still reason about ESP32
  RAM/flash/SPI cost (the sim won't catch those).
