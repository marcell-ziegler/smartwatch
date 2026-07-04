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
include/hal/              IGps / ITouch / ITileStore  — the three seams
src/app.{h,cpp}           SHARED app: layout, slippy-map math, overlay logic
src/main_native.cpp       desktop entry (SDL loop)      [native only]
src/main_esp32.cpp        hardware entry (real drivers) [esp32 only]
src/native/               PCDisplay glue, mocks, Arduino shim [native only]
lib/pcdisplay/            PCDisplay: Adafruit_GFX -> SDL2 window
tools/png_to_bin.py       OSM PNG tiles -> RGB565 .bin (offline)
assets/tracks/demo.csv    sample GPS replay track
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
`<z>/<x>/<y>.bin`, so neither needs a PNG decoder. Convert offline with
`tools/png_to_bin.py`. For a fixed heritage line, only convert the thin ribbon of
tiles your traced route crosses at your chosen zoom — not a whole region.
`ITileStore::loadTile()` is the read side (`FolderTileStore` on desktop, an SD
card on hardware); blitting tiles into a map view is app logic, not harness.

## `src/app.{h,cpp}`

This is intentionally a bare stub: `App::begin()` brings the peripherals up
(`ITileStore::begin()`, `ITouch::begin()`, `IGps::begin()`, rotation, a
cleared screen) and `App::tick()` just pumps `IGps::update()` and polls
`ITouch::get()`. Everything else — layout, the slippy-map math, timetable
display, route/station overlays — is the actual application and is left for
you to build on top of the three HAL interfaces in `include/hal/`.

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
