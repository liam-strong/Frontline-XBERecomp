#include "common/options.h"
#include "common/place.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <imgui.h>

#include "app/app.h"
#include "common/mouse_keyboard.h"

namespace game {

namespace {

bool g_open = false;
int g_shown = 0;    // the page on screen: 0 graphics, 1 controls
int g_force = -1;   // select this page once, then leave the tab bar alone
int g_bind = -1;    // action index waiting for a key
int g_arm = 0;      // frames to ignore the click that started a bind

const char* Label(const char* id) {
    struct Row {
        const char* id;
        const char* label;
    };
    static constexpr Row kRows[] = {
        {"move_forward", "Move forward"},
        {"move_back", "Move back"},
        {"strafe_left", "Strafe left"},
        {"strafe_right", "Strafe right"},
        {"walk", "Walk"},
        {"fire", "Fire  (right trigger)"},
        {"aim_mode", "Aim  (left trigger)"},
        {"action", "Use  (A)"},
        {"change_weapon", "Next weapon  (Black)"},
        {"grenade_throw", "Previous weapon  (White)"},
        {"jump", "Jump  (Y)"},
        {"reload", "Reload  (X)"},
        {"crouch", "Crouch  (B)"},
        {"center_view", "Center view  (left stick click)"},
        {"melee_attack", "Melee  (right stick click)"},
        {"sniper_zoom_up", "Zoom in  (D-pad up)"},
        {"sniper_zoom_down", "Zoom out  (D-pad down)"},
        {"dpad_left", "D-pad left"},
        {"dpad_right", "D-pad right"},
        {"back", "Back"},
        {"pause", "Pause  (Start)"},
        {"look_up", "Look up"},
        {"look_down", "Look down"},
        {"look_left", "Look left"},
        {"look_right", "Look right"},
    };
    for (const Row& r : kRows)
        if (std::strcmp(r.id, id) == 0) return r.label;
    return id;
}

void Open(int page) {
    if (g_open && g_shown == page) {
        g_open = false;
        g_bind = -1;
        return;
    }
    g_open = true;
    g_shown = page;
    g_force = page;
    g_bind = -1;
    input::SetMenuInput(true);
    input::ReleaseCursor();
}

std::string Binding(const input::Action& a) {
    const auto& keys = Opt().keys;
    auto it = keys.find(a.id);
    if (it != keys.end() && !it->second.empty()) return it->second;
    return a.defaults;
}

void SetBinding(const input::Action& a, const char* key) {
    Opt().keys[a.id] = key;
    input::LoadBindings();
    app::Save();
}

void DrawGraphics(app::Settings& s, Options& o) {
    bool save = false;
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.58f);
    int mode = int(s.display_mode);
    const char* modes[] = {"Fullscreen", "Borderless", "Windowed"};
    if (ImGui::Combo("Display", &mode, modes, 3)) {
        s.display_mode = app::DisplayMode(mode);
        app::ApplyDisplay();
        save = true;
    }

    app::Mode desktop = app::Desktop();
    struct Res {
        uint32_t w, h;
        char label[32];
    };
    Res list[8];
    int n = 0;
    auto add = [&](uint32_t w, uint32_t h, const char* label) {
        if (n >= 8) return;
        if (w && h && (w > desktop.width || h > desktop.height)) return;
        list[n++] = {w, h, {}};
        std::snprintf(list[n - 1].label, sizeof(list[0].label), "%s", label);
    };
    add(0, 0, "Desktop");
    char buf[32];
    auto preset = [&](uint32_t w, uint32_t h) {
        std::snprintf(buf, sizeof(buf), "%u x %u", w, h);
        add(w, h, buf);
    };
    preset(1280, 720);
    preset(1600, 900);
    preset(1920, 1080);
    preset(2560, 1440);
    preset(3840, 2160);
    int which = 0;
    bool listed = false;
    for (int i = 0; i < n; i++) {
        if (list[i].w == s.width && list[i].h == s.height) {
            which = i;
            listed = true;
        }
    }
    if (!listed && n < 8) {
        std::snprintf(buf, sizeof(buf), "%u x %u", s.width, s.height);
        add(s.width, s.height, buf);
        which = n - 1;
    }
    const char* labels[8];
    for (int i = 0; i < n; i++) labels[i] = list[i].label;
    if (ImGui::Combo("Resolution", &which, labels, n)) {
        s.width = list[which].w;
        s.height = list[which].h;
        app::ApplyDisplay();
        save = true;
    }

    int scale = int(std::clamp(s.render_percent, 50u, 200u));
    if (ImGui::SliderInt("Render scale", &scale, 50, 200, "%d%%")) {
        s.render_percent = uint32_t(scale);
        save = true;
    }
    int aniso_i = 0;
    const uint32_t aniso_v[] = {0, 2, 4, 8, 16};
    const char* aniso_l[] = {"As the game asks", "2x", "4x", "8x", "16x"};
    for (int i = 0; i < 5; i++)
        if (aniso_v[i] == s.aniso) aniso_i = i;
    if (ImGui::Combo("Anisotropic filtering", &aniso_i, aniso_l, 5)) {
        s.aniso = aniso_v[aniso_i];
        save = true;
    }
    if (ImGui::Checkbox("VSync", &s.vsync)) save = true;
    const uint32_t caps[] = {0, app::kFpsCapRefresh, 30, 60, 75, 120, 144, 165, 240};
    const char* cap_l[] = {"Unlimited", "Monitor", "30", "60", "75", "120", "144", "165", "240"};
    int cap_i = 3;
    for (int i = 0; i < 9; i++)
        if (caps[i] == s.fps_cap) cap_i = i;
    if (ImGui::Combo("Frame cap", &cap_i, cap_l, 9)) {
        s.fps_cap = caps[cap_i];
        save = true;
    }
    if (ImGui::Checkbox("Widescreen", &o.widescreen)) save = true;
    int fov = int(ClampFov(o.fov));
    if (ImGui::SliderInt("Field of view", &fov, int(kMinFov), int(kMaxFov), "%d deg")) {
        o.fov = uint32_t(fov);
        save = true;
    }
    ImGui::TextUnformatted("Field of view applies the next time the game sets the camera.");
    if (ImGui::Checkbox("Continue the last save", &o.load_last_save)) save = true;
    ImGui::TextUnformatted("Starts the newest save the next time the game launches.");
    if (ImGui::Checkbox("FPS counter", &o.fps_counter)) save = true;
    ImGui::PopItemWidth();
    if (save) app::Save();
    ImGui::Separator();
    ImGui::TextWrapped("Resolution, render scale, filtering, vsync and the frame cap are applied by this renderer on NVIDIA and AMD.");
    DrawPlaces();
}

void DrawControls() {
    size_t count = 0;
    const input::Action* actions = input::Actions(&count);
    if (g_arm > 0) g_arm--;
    if (g_bind >= 0 && g_arm == 0) {
        if (int vk = input::PollBindKey()) {
            if (const char* name = input::KeyName(vk)) SetBinding(actions[g_bind], name);
            g_bind = -1;
        }
    } else {
        input::PollBindKey();
    }
    if (ImGui::Button("Reset all to defaults")) {
        for (size_t i = 0; i < count; i++) Opt().keys[actions[i].id] = actions[i].defaults;
        input::LoadBindings();
        app::Save();
        g_bind = -1;
    }
    ImGui::TextWrapped("Each key presses an Xbox button. The layout chosen in the game's own options changes what that button does.");
    const float reserved = ImGui::GetFrameHeightWithSpacing() * 4.0f + ImGui::GetTextLineHeight();
    float list_h = ImGui::GetContentRegionAvail().y - reserved;
    if (list_h < 220.0f) list_h = 220.0f;
    const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH |
                                        ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("binds", 3, table_flags, ImVec2(0, list_h))) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("##set", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < count; i++) {
            ImGui::TableNextRow();
            ImGui::PushID(int(i));
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Label(actions[i].id));
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            if (int(i) == g_bind) ImGui::TextUnformatted("Press a key...");
            else ImGui::TextUnformatted(Binding(actions[i]).c_str());
            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("Bind")) {
                g_bind = int(i);
                g_arm = 2;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::SliderFloat("Mouse sensitivity", &Opt().mouse_sensitivity, 0.1f, 5.0f, "%.2f");
    ImGui::SliderFloat("Vertical sensitivity", &Opt().mouse_vertical_sensitivity, 0.1f, 5.0f, "%.2f");
    ImGui::Checkbox("Invert mouse Y", &Opt().invert_mouse);
    ImGui::Checkbox("Invert mouse X", &Opt().invert_mouse_x);
    static float sens = -1, vsens = -1;
    static bool iy = false, ix = false;
    Options& o = Opt();
    if (sens != o.mouse_sensitivity || vsens != o.mouse_vertical_sensitivity || iy != o.invert_mouse ||
        ix != o.invert_mouse_x) {
        if (sens >= 0) app::Save();
        sens = o.mouse_sensitivity;
        vsens = o.mouse_vertical_sensitivity;
        iy = o.invert_mouse;
        ix = o.invert_mouse_x;
    }
}

}  // namespace

bool SettingsOpen() { return g_open; }

bool SettingsMessage(uint32_t msg, uintptr_t wp, intptr_t lp) {
    if (msg == WM_MOUSEWHEEL && g_bind >= 0 && g_arm == 0) {
        size_t count = 0;
        const input::Action* actions = input::Actions(&count);
        if (g_bind < int(count)) SetBinding(actions[g_bind], GET_WHEEL_DELTA_WPARAM(wp) > 0 ? "Wheel Up" : "Wheel Down");
        g_bind = -1;
        return true;
    }
    if (msg == WM_KEYDOWN && !(lp & (1 << 30))) {
        if (wp == VK_F1) {
            Open(0);
            return true;
        }
        if (wp == VK_F2) {
            Open(1);
            return true;
        }
    }
    return false;
}

void DrawPcSettings() {
    if (!g_open) return;
    ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 480), ImVec2(1100, 1200));
    if (!ImGui::Begin("PC Settings", &g_open)) {
        ImGui::End();
        return;
    }
    int select = g_force;
    g_force = -1;
    if (ImGui::BeginTabBar("pc")) {
        ImGuiTabItemFlags graphics = select == 0 ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags controls = select == 1 ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Graphics", nullptr, graphics)) {
            g_shown = 0;
            DrawGraphics(app::Get(), Opt());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls", nullptr, controls)) {
            g_shown = 1;
            DrawControls();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::TextUnformatted("F1 graphics and saved places, F2 controls.");
    ImGui::End();
    if (!g_open) g_bind = -1;
}

}  // namespace game
