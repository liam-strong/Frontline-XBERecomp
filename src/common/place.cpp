#include "common/place.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <imgui.h>
#include <toml++/toml.hpp>

#include "app/app.h"
#include "kernel/kernel.h"

namespace game {

namespace {

// The body matrix is at +0x210. Its translation is the world position, and the update copies that row to +0xB0.
constexpr uint32_t kPos = 0x240;
constexpr uint32_t kPosCopy = 0xB0;
constexpr uint32_t kVel = 0x80;
constexpr uint32_t kShove = 0x90;
constexpr uint32_t kYaw = 0x3E8;
constexpr uint32_t kPitch = 0x3EC;
constexpr int kKeep = 8;
constexpr int kSettle = 4;  // frames to hold the spot, so the next step does not carry the old speed

struct Spot {
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
};

std::vector<Spot> g_spots;
bool g_loaded = false;
bool g_hooked = false;
bool g_live = false;
Spot g_here;
int g_back = -1;
int g_hold = 0;
std::string g_note;

std::filesystem::path File() { return app::ExeDir() / "places.toml"; }

bool Finite(float v) { return std::isfinite(v) && std::fabs(v) < 100000.0f; }

bool Sane(const Spot& s) { return Finite(s.x) && Finite(s.y) && Finite(s.z) && Finite(s.yaw) && Finite(s.pitch); }

void WriteFile() {
    toml::array rows;
    for (const Spot& s : g_spots) {
        rows.push_back(toml::table{{"x", double(s.x)},
                                    {"y", double(s.y)},
                                    {"z", double(s.z)},
                                    {"yaw", double(s.yaw)},
                                    {"pitch", double(s.pitch)}});
    }
    std::ofstream out(File());
    out << toml::table{{"place", std::move(rows)}} << "\n";
}

void WriteAt(uint32_t player, const Spot& s) {
    kernel::G<float>(player + kPos) = s.x;
    kernel::G<float>(player + kPos + 4) = s.y;
    kernel::G<float>(player + kPos + 8) = s.z;
    kernel::G<float>(player + kPosCopy) = s.x;
    kernel::G<float>(player + kPosCopy + 4) = s.y;
    kernel::G<float>(player + kPosCopy + 8) = s.z;
    kernel::G<float>(player + kYaw) = s.yaw;
    kernel::G<float>(player + kPitch) = s.pitch;
    for (uint32_t i = 0; i < 12; i += 4) kernel::G<float>(player + kVel + i) = 0;
    for (uint32_t i = 0; i < 12; i += 4) kernel::G<float>(player + kShove + i) = 0;
}

}  // namespace

void LoadPlaces() {
    if (g_loaded) return;
    g_loaded = true;
    std::error_code ec;
    if (!std::filesystem::exists(File(), ec)) return;
    toml::table table;
    try {
        table = toml::parse_file(File().string());
    } catch (const toml::parse_error&) {
        return;
    }
    const toml::array* rows = table["place"].as_array();
    if (!rows) return;
    for (const toml::node& node : *rows) {
        const toml::table* row = node.as_table();
        if (!row) continue;
        Spot s;
        s.x = float((*row)["x"].value_or(0.0));
        s.y = float((*row)["y"].value_or(0.0));
        s.z = float((*row)["z"].value_or(0.0));
        s.yaw = float((*row)["yaw"].value_or(0.0));
        s.pitch = float((*row)["pitch"].value_or(0.0));
        if (Sane(s) && int(g_spots.size()) < kKeep) g_spots.push_back(s);
    }
}

void PlacesFrame() {
    g_live = g_hooked;
    g_hooked = false;
}

void NotePlayer(uint32_t player) {
    if (!player) return;
    uint32_t kind = kernel::R32(player);
    if (kind < 0x12D260u || kind >= 0x143200u) return;
    Spot here;
    here.x = kernel::G<float>(player + kPos);
    here.y = kernel::G<float>(player + kPos + 4);
    here.z = kernel::G<float>(player + kPos + 8);
    here.yaw = kernel::G<float>(player + kYaw);
    here.pitch = kernel::G<float>(player + kPitch);
    if (!Sane(here)) return;
    g_here = here;
    g_hooked = true;
    if (g_back < 0 || g_back >= int(g_spots.size())) return;
    WriteAt(player, g_spots[size_t(g_back)]);
    if (--g_hold <= 0) g_back = -1;
}

void DrawPlaces() {
    if (!g_loaded) LoadPlaces();
    ImGui::Separator();
    ImGui::TextUnformatted("Saved places");
    if (g_live) ImGui::Text("Here:  %.0f,  %.0f,  %.0f", g_here.x, g_here.y, g_here.z);
    else ImGui::TextUnformatted("Here:  not on foot");
    ImGui::BeginDisabled(!g_live);
    if (ImGui::Button("Save this spot")) {
        g_spots.insert(g_spots.begin(), g_here);
        if (int(g_spots.size()) > kKeep) g_spots.pop_back();
        WriteFile();
        g_note = "Saved.";
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Return puts you back on that ground, facing the same way. Load the mission you saved it in first.");
    if (!g_note.empty()) ImGui::TextUnformatted(g_note.c_str());
    for (int i = 0; i < int(g_spots.size()); i++) {
        const Spot& s = g_spots[size_t(i)];
        ImGui::PushID(i);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%.0f,  %.0f,  %.0f", s.x, s.y, s.z);
        ImGui::SameLine();
        ImGui::BeginDisabled(!g_live);
        if (ImGui::SmallButton("Return")) {
            g_back = i;
            g_hold = kSettle;
            g_note = "Returning.";
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            if (g_back == i) g_back = -1;
            else if (g_back > i) g_back--;
            g_spots.erase(g_spots.begin() + i);
            WriteFile();
            g_note.clear();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
}

}  // namespace game
