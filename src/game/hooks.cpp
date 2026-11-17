// Hooks for Frontline's default.xbe.
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/app.h"
#include "common/fov.h"
#include "common/mouse_keyboard.h"
#include "common/options.h"
#include "common/place.h"
#include "common/widescreen.h"
#include "core/log.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"

namespace {

constexpr uint32_t kYaw = 0x3E8;
constexpr uint32_t kPitch = 0x3EC;
// a mounted gun's aim, clamped by the game to the limits at +0x520/+0x524 after the stick rates are added
constexpr uint32_t kGunYaw = 0x528;
constexpr uint32_t kGunPitch = 0x52C;
// 0.05 degrees a count at sensitivity 1, the same scale as Rising Sun
constexpr float kRadians = 0.05f * 3.14159265f / 180.0f;

// the shell object, its save manager, and the loader blob that relaunches a mission
constexpr uint32_t kShell = 0x2FA430;
constexpr uint32_t kManager = 0x1910;
constexpr uint32_t kLoaderReady = 0x2FA320;
constexpr uint32_t kLoader = 0x2FA324;
constexpr uint32_t kLoaderResume = 0x2218;
constexpr uint32_t kEnumSaves = 0x77B10;  // thiscall, ecx = manager, eax = how many slots were filled
constexpr uint32_t kLoadSlot = 0x7DEE0;   // thiscall, ecx = shell, arg0 = device, arg1 = slot
constexpr uint32_t kSlotStride = 0x208;
constexpr uint32_t kSlotName = 0x104;
constexpr uint32_t kSlotUsed = 0x204;
constexpr uint32_t kSlotMax = 120;

struct Flags {
    uint32_t cf, zf, sf, of, pf;
};

Flags TakeFlags(const X86Context& ctx) { return {ctx.cf, ctx.zf, ctx.sf, ctx.of, ctx.pf}; }

void PutFlags(X86Context& ctx, Flags f) {
    ctx.cf = f.cf;
    ctx.zf = f.zf;
    ctx.sf = f.sf;
    ctx.of = f.of;
    ctx.pf = f.pf;
}

bool Guest(uint32_t p) { return p >= 0x10000 && p < 0x7FFE0000 && (p & 3) == 0; }

bool g_busy = false;
bool g_playing = false;     // the player is in a level, so a save list there is the pause menu
bool g_finished = false;    // this process has continued a save, or there is nothing to continue
bool g_enum_tried = false;  // the save list hook already tried during this frame
int g_loaded = 0;
int g_frames = 0;
int g_since_load = 0;

void NotePlaying() { g_playing = true; }

bool BootDecided() { return kernel::G<uint8_t>(kLoaderReady) != 0; }

// the relaunch that carries a mission in from the menu sets this on the loader object
bool ContinuingLevel() {
    uint32_t loader = kernel::R32(kLoader);
    return Guest(loader) && kernel::G<uint8_t>(loader + kLoaderResume) != 0;
}

uint32_t Manager() {
    uint32_t mgr = kernel::R32(kShell + kManager);
    if (!Guest(mgr)) return 0;
    if (!Guest(kernel::R32(mgr + 0x48)) || !Guest(kernel::R32(mgr + 0x4C))) return 0;
    if (kernel::R32(mgr + 0x30) > 8) return 0;
    return mgr;
}

std::string MetaName(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes.size() >= 2 && uint8_t(bytes[0]) == 0xFF && uint8_t(bytes[1]) == 0xFE) bytes.erase(0, 2);
    std::string text;
    bool wide = bytes.size() >= 4 && bytes[1] == '\0';
    if (wide) {
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
            unsigned char lo = uint8_t(bytes[i]), hi = uint8_t(bytes[i + 1]);
            if (!lo && !hi) break;
            text.push_back(hi ? '?' : char(lo));
        }
    } else {
        text = bytes;
    }
    auto at = text.find("Name=");
    if (at == std::string::npos) return {};
    at += 5;
    size_t end = text.find_first_of("\r\n", at);
    std::string name = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
    while (!name.empty() && (name.back() == ' ' || name.back() == '\0')) name.pop_back();
    return name;
}

// the folder is the save name as UTF-16BE hex, which is how the game writes the directory
std::string FolderOf(const std::string& name) {
    std::string hex;
    hex.reserve(name.size() * 4);
    for (unsigned char c : name) {
        char b[5];
        std::snprintf(b, sizeof b, "%04X", unsigned(c));
        hex += b;
    }
    return hex;
}

bool SameText(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower(uint8_t(a[i])) != std::tolower(uint8_t(b[i]))) return false;
    return true;
}

// the game.xsv with the latest write, and the directory name the slot path contains
std::string NewestSave(std::string& folder) {
    namespace fs = std::filesystem;
    int inst = app::Instance();
    fs::path root = app::ExeDir() / (inst > 1 ? L"hdd" + std::to_wstring(inst) : L"hdd") / L"E" / L"UDATA" / L"4541001a";
    std::error_code ec;
    fs::file_time_type best{};
    bool any = false;
    std::string name;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        fs::path xsv = it->path() / L"game.xsv";
        if (!fs::is_regular_file(xsv, ec)) continue;
        auto written = fs::last_write_time(xsv, ec);
        if (ec || (any && written < best)) continue;
        std::string meta = MetaName(it->path() / L"SaveMeta.xbx");
        any = true;
        best = written;
        name = meta;
        folder = it->path().filename().string();
    }
    return name;
}

int FindSlot(uint32_t mgr, const std::string& name, const std::string& folder) {
    uint32_t base = kernel::R32(mgr + 0x4C);
    if (!Guest(base)) return -1;
    for (uint32_t i = 0; i < kSlotMax; i++) {
        uint32_t slot = base + i * kSlotStride;
        if (kernel::R32(slot + kSlotUsed) != 1) continue;
        std::string shown = kernel::ReadCString(slot + kSlotName, 0x7F);
        std::string path = kernel::ReadCString(slot, 0x100);
        if (!name.empty() && SameText(shown, name)) return int(i);
        if (!folder.empty() && path.find(folder) != std::string::npos) return int(i);
    }
    return -1;
}

// fills the slot list when the caller has not just done it, then reads that save the way the load menu does
bool ContinueSave(X86Context& ctx, uint32_t mgr, bool enumerate) {
    if (g_finished || g_playing || !game::Opt().load_last_save) return false;
    if (!BootDecided() || ContinuingLevel()) return false;
    std::string folder;
    std::string name = NewestSave(folder);
    if (name.empty() && folder.empty()) {
        g_finished = true;
        return false;
    }
    Flags flags = TakeFlags(ctx);
    if (enumerate) {
        ctx.eax = 0;
        ctx.ecx = mgr;
        kernel::CallGuest(ctx, kEnumSaves, {});
    }
    int slot = FindSlot(mgr, name, folder);
    if (slot < 0) {
        PutFlags(ctx, flags);
        return false;
    }
    uint32_t device = kernel::R32(mgr + 0x30);
    ctx.ecx = kShell;
    uint32_t result = kernel::CallGuest(ctx, kLoadSlot, {device, uint32_t(slot)});
    PutFlags(ctx, flags);
    if ((result & 0xFF) == 0) {
        LOGR("last save \"%s\" was not read", name.c_str());
        return false;
    }
    g_loaded++;
    g_since_load = 0;
    LOGR("continuing the last save \"%s\"", name.empty() ? folder.c_str() : name.c_str());
    return true;
}

}  // namespace

// [[hook]] 0x6351A, ebx the player, before fld of the stick rates that are added to yaw and pitch
void MouseLook(X86Context& ctx, uint8_t*) {
    NotePlaying();
    game::NotePlayer(ctx.ebx);
    if (game::SettingsOpen()) return;
    uint32_t player = ctx.ebx;
    if (!player) return;
    float dx, dy;
    if (!input::TakeMouse(dx, dy)) return;
    kernel::G<float>(player + kYaw) -= dx * kRadians;
    kernel::G<float>(player + kPitch) += dy * kRadians;
}

// [[hook]] 0x62A4B, ebx the player on a mounted gun, before fld of the stick rates that are added to the gun's aim.
// Taking the mouse here also keeps the mouse buttons live, so fire works while mounted.
void MountedLook(X86Context& ctx, uint8_t*) {
    NotePlaying();
    if (game::SettingsOpen()) return;
    uint32_t player = ctx.ebx;
    if (!player) return;
    float dx, dy;
    if (!input::TakeMouse(dx, dy)) return;
    kernel::G<float>(player + kGunYaw) -= dx * kRadians;
    kernel::G<float>(player + kGunPitch) += dy * kRadians;
}

// [[hook]] 0x427BB, the ret of SetFov: ecx is the camera, tan(half width) is already at +0x2C and tan(half height) at +0x30
void SetFov(X86Context& ctx, uint8_t*) {
    uint32_t camera = ctx.ecx;
    if (!camera) return;
    float scale = game::FovTangentScale(game::Opt().fov);
    if (scale != 1.0f) {
        kernel::G<float>(camera + 0x2C) *= scale;
        kernel::G<float>(camera + 0x30) *= scale;
    }
    game::WidenView(camera);
}

// [[hook]] 0x77CF4, just after the save manager finishes listing slots. esi is still that manager.
void LoadLastSave(X86Context& ctx, uint8_t*) {
    if (g_busy || g_finished || g_playing || g_loaded || g_enum_tried || !game::Opt().load_last_save) return;
    // the front end can ask for the list many times a frame; one try, then again with the present hook's pace
    if (g_frames > 0 && (g_frames % 90) != 0) return;
    if (!Guest(ctx.esi)) return;
    g_enum_tried = true;
    g_busy = true;
    ContinueSave(ctx, ctx.esi, false);
    g_busy = false;
}

// [[hook]] 0xE20E0, the start of a present. The front end may not list saves until Load Game is opened,
// so this asks for the list once the menu boot is up and continues the newest save from there.
void OfferLastSave(X86Context& ctx, uint8_t*) {
    g_enum_tried = false;
    if (g_busy || !game::Opt().load_last_save) return;
    if (g_playing || ContinuingLevel()) {
        g_finished = true;
        return;
    }
    if (g_finished || !BootDecided()) return;
    g_frames++;
    if (g_loaded > 0) {
        // the shell kept the load: stop. a transition the movie cleared gets one more try
        if (kernel::R32(kShell + 0x21C) == 1) {
            g_finished = true;
            return;
        }
        g_since_load++;
        if (g_since_load < 150) return;
        if (g_loaded >= 2 || g_since_load > 180) {
            g_finished = true;
            return;
        }
    } else if (g_frames < 240 || (g_frames % 90) != 0) {
        if (g_frames > 2400) g_finished = true;
        return;
    }
    uint32_t mgr = Manager();
    if (!mgr) return;
    g_busy = true;
    ContinueSave(ctx, mgr, true);
    g_busy = false;
    if (g_frames > 2400 && g_loaded == 0) g_finished = true;
}
