#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "common/fov.h"

// MOHRS settings kept in the settings file beside the exe, beyond the SDK's own
namespace game {

struct Options {
    bool skip_intros = false;     // the EA logo and the opening Pearl Harbor movie at boot
    bool skip_cutscenes = false;  // each level's intro and outro movies
    bool load_last_save = true;   // boot into the newest save; the menu stays when there is none
    bool fps_counter = false;     // frames a second in the picture's top left corner
    bool widescreen = true;       // 16:9 on a widescreen output; the 4:3 frame is shown across the screen
    bool fps60 = false;           // the Xbox frame pacing, until Frontline's frame wait is hooked
    bool unlocked_fps = false;    // experimental elapsed-time updates; fps_cap/vsync set the presentation rate
    uint32_t fov = kDefaultFov;    // horizontal degrees on a 4:3 frame; 70 original, widescreen adds Hor+
    // keyboard and mouse (src/common/mouse_keyboard.cpp)
    float mouse_sensitivity = 1.0f;  // horizontal, 0.05 degrees a count at 1
    float mouse_vertical_sensitivity = 1.0f;
    bool invert_mouse = false;  // the Y axis
    bool invert_mouse_x = false;
    uint32_t walk_speed = 50;  // percent of running, while the walk key is held
    std::map<std::string, std::string> keys;  // action id to key names, absent is the default
};

Options& Opt();

// F1 opens the graphics page, F2 the controls page. While it is open, keys and mouse look stay off the pad.
bool SettingsOpen();
void DrawPcSettings();
bool SettingsMessage(uint32_t msg, uintptr_t wp, intptr_t lp);

}  // namespace game
