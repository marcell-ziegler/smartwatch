# Railway pocket-watch — firmware + desktop simulator

One codebase, two build targets. Your application code (`src/app.*`) is written
once against `Adafruit_GFX` plus three small hardware interfaces, and runs
unchanged on:

- **`native`** — a desktop SDL2 window that compiles the *real* `Adafruit_GFX`
  against a tiny Arduino shim, so text/line/bitmap rendering is pixel-identical
  to hardware. Fast iteration, keyboard-driven GPS, mouse-driven touch.
- **`esp32`** — the Feather ESP32 V2 + 2.4" TFT FeatherWing (ILI9341 + STMPE610)
  + UART GPS + SD tiles.

## Layout

```
platformio.ini            two [env] blocks; src filters pick target files
include/hal/               IGps / ITouch / ITileStore / ITimeTableStore — the seams
src/app.{h,cpp}            SHARED app: map view; slippy-map math + tile blitting
src/timetable.{h,cpp}      SHARED timetable data model + CSV parsers
include/ClockTime.h        wall-clock time-of-day type (wraps at midnight)
src/main_native.cpp        desktop entry (SDL loop)      [native only]
src/main_esp32.cpp         hardware entry (real drivers) [esp32 only]
src/native/                PCDisplay glue, mocks, Arduino shim [native only]
lib/pcdisplay/              PCDisplay: Adafruit_GFX -> SDL2 window
tools/tile_calc.py         lat/lon -> z/x/y tile coords, for picking a ribbon
tools/mbtiles_to_png.py    unpack a MOBAC .mbtiles atlas -> z/x/y.png tree
tools/png_to_bin.py        z/x/y.png tree -> z/x/y.bin RGB565 tiles
assets/tracks/demo.csv     sample GPS replay track
assets/Lennakatten.mbtiles source atlas for the current demo tile ribbon
assets/tiles/<z>/<x>/<y>.bin   pre-converted raster tiles
```

## Build & run — simulator

Install SDL2 dev headers first:

- Ubuntu/Debian: `sudo apt install libsdl2-dev pkg-config`
- macOS: `brew install sdl2 pkg-config`
- Fedora: `sudo dnf install SDL2-devel`

Then:

```
pio run -e native
./.pio/build/native/program
```

Controls: **arrow keys** drive the GPS position, **R** toggles replay of
`assets/tracks/demo.csv`, **Esc** quits.

## Build & flash — hardware

```
pio run -e esp32 -t upload
pio device monitor        # 115200 baud
```

### Editor
- **VS Code**: install the official *PlatformIO IDE* extension for one-click
  build / upload / monitor buttons.
- **Zed**: no PlatformIO extension, so drive it from the integrated terminal
  with the `pio` commands above. Run `pio run -t compiledb` once to generate
  `compile_commands.json` so Zed's clangd gives you completion + diagnostics.

## Tiles

Both targets read 256×256 tiles as raw little-endian RGB565 (`.bin`, 131072 B) at
`<z>/<x>/<y>.bin`, so neither needs a PNG decoder. `ITileStore::loadTile()` is
the read side (`FolderTileStore` on desktop, an SD card on hardware); blitting
tiles into the map view is app logic, not harness.

To produce a tile set: export a MOBAC atlas in "MBTiles" format for the thin
ribbon of tiles your traced route crosses at your chosen zoom (not a whole
region), then:

```
python3 tools/mbtiles_to_png.py <atlas.mbtiles> <png_tile_dir>
python3 tools/png_to_bin.py <png_tile_dir> assets/tiles
```

`tools/tile_calc.py` is a scratch helper for working out which z/x/y tiles a
list of lat/lon waypoints falls into, e.g. to sanity-check the MOBAC atlas
covers the intended ribbon. The checked-in `assets/tiles/` (zoom 13, from
`assets/Lennakatten.mbtiles`) is the demo route used with `demo.csv`.

## Timetable data

Timetable data is stored as plain CSV — human-readable and hand-editable on the
SD card — one file per unique train. `ITimeTableStore::readFile()` is the read
side (a folder on desktop, an SD card on hardware); it just returns raw file
bytes. Parsing into the data model (`Stop`, `Train`, `Timetable`, `Shift`,
`SeasonalTimetableRule`) lives in `src/timetable.{h,cpp}` and is shared by both
targets — pure C++, no HAL or Adafruit dependency.

The device only ever loads the *one* category running on a given day: pick a
shift → its category → load that category's directory into RAM (a season's
worth is tens of KB, well within budget). A category holds ~18 trains at most,
so lookups are simple linear scans.

### Directory layout

```
timetables/
  seasons.csv          which category runs on which dates / weekdays
  shifts.csv           the shift catalog across all categories
  A/                   one directory per traffic category (A, B, D, ...)
    trains.csv         roster: every train in this category
    70.csv             one file per train, named <number>.csv
    91Y.csv
    ...
  B/
    ...
```

`trains.csv` is the authoritative index of which per-train files exist; the
loader opens each `<number>.csv` it lists rather than scanning the directory, so
a roster/file mismatch surfaces as a clear error.

### Column schemas

Each row is one record; columns are comma-separated. There is **no header row** —
label columns with a leading `#` comment instead (blank and `#` lines are
skipped, matching `assets/tracks/demo.csv`). CRLF and LF line endings are both
accepted. Sub-lists within a field (train lists, meets) are `;`-separated.

```
seasons.csv   validFrom,validTo,dayOfWeek,category
              # ISO dates (inclusive); dayOfWeek 1-7 (Mon-Sun)
              2026-06-01,2026-08-31,3,A

shifts.csv    number,category,trainNumbers
              # number's tens digit -> role: 1x Förare, 3x Tågbefälhavare,
              # 4x Konduktör. trainNumbers is ;-separated, in work order.
              31,A,70;71;74;75

trains.csv    number,vehicleType,direction,nextNumber
              # nextNumber = train continuing on the same set (may be empty)
              70,BigSteam,Down,71

<number>.csv  station,arrival,departure,stopType,exchangeType,staffed,meets,remark
              # one row per station called at, in order
              Uö,08:00,08:05,Mandatory,Embarking,true,62;91Y,"Meet 62 here"
```

Accepted field tokens (matched case-insensitively):

- `vehicleType`: `BigSteam` `SmallSteam` `Diesel` `Railbus`
- `direction`: `Up` (toward Uppsala, increasing km) · `Down` (toward Faringe)
- `stopType`: `Mandatory` `Optional`
- `exchangeType`: `Embarking` `Disembarking` `EmbarkingAndDisembarking` `None`
- `staffed`: `true`/`false` (also `1`/`0`, `yes`/`no`)
- times: `H:MM` or `HH:MM`, validated (hour < 24, minute < 60). `arrival` and
  `departure` are each optional — leave the cell **empty** for a station with
  no such time (some list only a departure, some neither). An empty cell parses
  to an absent time; a non-empty but malformed one is still an error.

A malformed row (wrong column count, bad token, out-of-range time) fails the
whole file's parse — the prep step should validate on the desktop before the
card ever reaches the device.

> **Open item:** the `loadCategory` assembly step (roster + per-train stops →
> `Timetable`) isn't written yet.

## `src/app.{h,cpp}`

`App::begin()` brings the peripherals up (`ITileStore::begin()`,
`ITouch::begin()`, `IGps::begin()`, rotation, a cleared screen). `App::tick()`
pumps `IGps::update()`, polls `ITouch::get()`, and — throttled to 1 Hz —
redraws the map.

Currently implemented:
- **`drawMap()`** — a 120×120 map square pinned to the top-right corner,
  centered on the live GPS fix (Web Mercator projection, see
  `lonLatToWorldPx()`). The user stays fixed at the center of the square and
  the map scrolls beneath them as tiles are blitted in and clipped to the
  panel; a filled circle marks the user's position.

Still a stub:
- **`drawLine()`** — declared in `app.h` but not yet implemented. Intended to
  show the user's progress along the railway line: last/current/next stops as
  dots, with the completed segment drawn in green.

Touch input (`ITouch::get()`) is polled each tick but not yet acted on.

## Note on the native GFX build

The `native` env compiles `Adafruit_GFX` against the shim in
`src/native/arduino_compat/`. That shim covers what the base GFX class needs
(`Print`, `PROGMEM`/`pgm_read_*`, a minimal `String`).

`Adafruit_GFX` isn't pulled from the registry on this target: newer versions of
the library unconditionally `#include <Adafruit_I2CDevice.h>` /
`<Adafruit_SPIDevice.h>` (for the `Adafruit_SPITFT`/`Adafruit_GrayOLED`
subclasses it ships alongside), which drags in BusIO/Wire/SPI just to compile
the base class — none of which exist on a plain desktop build. `lib/adafruit_gfx_core/`
is a trimmed local copy of upstream `Adafruit_GFX.{h,cpp}` + `glcdfont.c` with
those two includes dropped, so PCDisplay only ever needs the base class.
`[env:native]` also sets `lib_compat_mode = off`, since PlatformIO's LDF
otherwise flags this vendored copy (and the registry package, if you ever
switch back) as "framework incompatible" under the framework-less `native`
platform. If you re-vendor from a newer `Adafruit_GFX` release, diff against
upstream to see whether those two includes still need dropping.
</content>
