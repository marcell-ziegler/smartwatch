// ===========================================================================
//  Desktop simulator entry point.
//
//  Controls:
//    W A S D      d-pad Up / Left / Down / Right     (emulated device buttons)
//    Enter        Select / press                     (emulated device button)
//    Arrow keys   nudge the GPS position (manual driving)
//    R            toggle replay of assets/tracks/demo.csv
//    Esc / close  quit
// ===========================================================================
#include <SDL2/SDL.h>
#include "PCDisplay.h"
#include "native/native_hal.h"
#include "app.h"

static constexpr int SCALE = 2;
static constexpr double STEP = 0.0004;   // ~40 m per key press

int main(int, char**) {
    PCDisplay display(320, 240, SCALE);
    if (!display.begin("Railway watch (sim)")) {
        SDL_Log("display init failed");
        return 1;
    }

    SimGps               gps("assets/tracks/demo.csv");
    SdlTouch             touch(SCALE);
    FolderTileStore      tiles("assets/tiles");
    SdlButtons           buttons;
    FolderTimetableStore timetables("assets/timetables");

    App app(display, gps, touch, tiles, buttons, timetables);
    app.begin();

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                // Emulated device buttons: ignore auto-repeat so a held key is
                // one press, matching the edge-triggered IButtons contract.
                const bool repeat = e.key.repeat != 0;
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    // WASD d-pad + Enter = Select
                    case SDLK_w: if (!repeat) buttons.pushDown(Button::Up);     break;
                    case SDLK_s: if (!repeat) buttons.pushDown(Button::Down);   break;
                    case SDLK_a: if (!repeat) buttons.pushDown(Button::Left);   break;
                    case SDLK_d: if (!repeat) buttons.pushDown(Button::Right);  break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER: if (!repeat) buttons.pushDown(Button::Select); break;
                    // Arrow keys still drive the simulated GPS position.
                    case SDLK_UP:    gps.nudge(+STEP, 0); break;
                    case SDLK_DOWN:  gps.nudge(-STEP, 0); break;
                    case SDLK_LEFT:  gps.nudge(0, -STEP); break;
                    case SDLK_RIGHT: gps.nudge(0, +STEP); break;
                    case SDLK_r:     gps.toggleReplay(); break;
                    default: break;
                }
            }
        }

        app.tick(SDL_GetTicks());
        display.present();
        SDL_Delay(16);   // ~60 fps window refresh; app throttles its own redraws
    }
    return 0;
}
