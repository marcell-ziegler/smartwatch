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
(ILI9341 display + STMPE610 resistive touch), a PA1010D UART GPS (with a
battery-backed RTC), and a MAX17048 I²C battery gauge. Development happens in
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
| `IGps` | `SimGps` (CSV replay + arrow-key nudge) | `Esp32Gps` (TinyGPSPlus/UART) |
| `ITouch` | `SdlTouch` (mouse) | `Esp32Touch` (STMPE610) |
| `ITileStore` | `FolderTileStore` | `SdTileStore` (SD card) |
| `ITimeTableStore` | *(not yet implemented)* | *(not yet implemented)* |
| display | `PCDisplay` (SDL2) | `Adafruit_ILI9341` (is-a GFX) |

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
   handled: STL headers are included *before* the `min`/`max`/`abs` macros are
   defined, because those macros otherwise clobber libstdc++ internals.

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

Note: the user set `setRotation(0)` in `begin()` themselves ("fixed the
orientation") — leave it.

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
`parseTrainsCsv`, `parseStopsCsv`) + field converters; `Timetable::findTrain`.
Both `pio run -e native` and `-e esp32` build clean. Some real B-traffic data
is entered (`assets/timetables/`): `seasons.csv`, `shifts.csv`, `B/trains.csv`,
`B/80.csv` are populated and parse; the rest are empty placeholders.

**Stubbed / not yet done:**
- `App::drawLine()` — declared, empty. Intended: progress along the line
  (last/current/next stops as dots, completed segment green).
- Touch input is polled each tick but not acted on. No shift-picker UI yet.
- `ITimeTableStore` has **no concrete implementations** (need
  `FolderTimetableStore` for native + an SD one for esp32) and there is **no
  `loadCategory`** assembly step (roster + per-train stops → `Timetable`) yet.
- **No `stations.csv`** or station parser yet, though the `Station` struct
  exists — station geo/km data still needs a home.
- GPS **RTC not wired**: `IGps` has no `clock()` accessor yet. Plan: add a
  `clock()` returning a `ClockTime` (+ validity) read from TinyGPSPlus's
  time/date, independent of position validity — the module's clock is usable
  before a position fix. `gps.date` also feeds the `seasons.csv` "what's today"
  lookup.

## Known open decisions / caveats

- **Timetable graphs are not machine-readable.** The source timetables are
  dense graphical (string-line) diagrams — see the conversation history. They
  can't be reliably OCR'd/transcribed automatically, and this is
  *safety-relevant operational data* (times + meets). The plan is to build a
  helper authoring tool/script later; until then the empty `<train>.csv` files
  are filled **by hand** by the user. Do **not** mass-generate timetable times
  from images — a wrong meet/time is a real hazard, and looks authoritative
  once in a file.
- **Swedish characters:** data is UTF-8 (needs å/ä/ö in names/remarks/station
  sigs like "Uö"), but `Adafruit_GFX`'s built-in font (`glcdfont.c`) is
  **ASCII-only**. A custom `GFXfont` covering Swedish glyphs is needed before
  those render correctly on screen. Store correctly now; fix rendering later.
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
- **Don't commit unless asked.** The user drives git and commits frequently
  with short messages. `main` is the working branch. `.pio/` and most
  `.vscode/` are gitignored.
- The user **fixes/refactors things between turns** (formatting via clang-format
  to Allman braces, small renames). Re-read files before editing; don't revert
  their changes.
- When touching `App`, keep it hardware-agnostic — it must compile and behave
  on both targets. If something works in the simulator, still reason about ESP32
  RAM/flash/SPI cost (the sim won't catch those).
