// Free aim: on foot with the keyboard and mouse, shots go to the middle of the view, not to what the game picks.
//
// A gun's shot (0x180CB0) takes its start from its owner's muzzle point (character vtable +0x1A4) and its direction
// from the owner's aim (+0x1A8) when that is within 90 degrees of the gun's own forward. Bond's class (vtable 0x2E33B8)
// has its own: the aim 0x153CD0 is the camera's angles in the scope's view, the locked target (0x14E480) while locked
// on (0x151EC0), else his aim pose (+0x12B0); the muzzle 0x153BE0 is the camera's position in the scope's view, else
// his chest. Here, with no lock, the aim is the camera's as in the scope (measured: the bullet's forward at 0x222BF0 is
// the camera's), and the shot starts on the line through the middle of the view, level with Bond, so nothing between
// the camera and him is hit. Holding the lock (R by default) plays as the game does.
//
// Bond stands in the middle of the view: the camera info every view and aim reads (0x8D960: position, then pitch, yaw,
// roll in degrees) is moved to the right, over his shoulder. While a mouse button is held he faces the camera's way:
// his yaw is the angles' +0x64, from which the game rebuilds his matrix (+0x70) each update, so he turns and his shots
// stay in the gun's 90 degrees. The right button also zooms a little, and alone holds the fire input with the shot blocked, for the stance firing gives. The crosshair is drawn here.
//
// EON_FREE_AIM (headless, no mouse): 0 off, 1 on, 2 on with a button held.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>

#include "core/input.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"
#ifdef _WIN32
#include "imgui.h"
#endif

namespace eon {
extern bool free_aim;                 // the setting (src/common/game_info.cpp)
extern void (*frame_ui)(double fps);  // drawn each frame (src/common/game_info.cpp)
extern std::atomic<bool> aim_trigger;  // the fire input held for the stance (src/common/menu_mouse.cpp)
extern std::atomic<int64_t> aim_release;  // the fire input let go until then, ms (src/common/menu_mouse.cpp)
extern std::atomic<int64_t> hud_drawn_ms;  // when the HUD last drew (src/game/hud.cpp): none in movies or the like
}

namespace {

constexpr uint32_t kCameras = EON_VA(0x3210C0, 0x3205B8);
constexpr uint32_t kCameraBytes = 0x240;
constexpr uint32_t kPlayerIndex = 0x1130;
constexpr uint32_t kYaw = 0x64;
constexpr uint32_t kMatrix = 0x70;  // three rows of 16 bytes, forward first
constexpr uint32_t kPosition = 0xB0;
constexpr uint32_t kLockedOn = EON_VA(0x151EC0, 0x151730);       // thiscall on Bond
constexpr uint32_t kMounted = EON_VA(0x1500B0, 0x14F920);        // thiscall on Bond: on a mounted gun
constexpr uint32_t kZoomable = EON_VA(0x14B640, 0x14AEB0);       // thiscall on Bond: a weapon with a scope
constexpr uint32_t kAnglesToMatrix = EON_VA(0xF0A90, 0xF0610);   // cdecl (angles, matrix out)
constexpr uint32_t kFollowCamera = EON_VA(0x2E2694, 0X2E1BBC);      // the camera mode's class in play
constexpr uint32_t kGunOwner = EON_VA(0x1A7A40, 0x1A7270);       // thiscall on a gun
constexpr uint32_t kBodyTable = EON_VA(0x2DD118, 0x2DC640);      // 0x14A2C0's numbers for the upper body
constexpr uint32_t kScopeView = 0xBAA3F02;                       // what the scope's camera mode answers (vtable +8)
constexpr float kRadians = 3.14159265f / 180;
constexpr float kShoulder = 38;       // world units right of Bond
constexpr float kRightButtonZoom = 0.8f;
constexpr float kBlendPerUpdate = 0.12f;

std::atomic<uint32_t> g_bond{0};
std::atomic<float> g_shoulder{0};
std::atomic<float> g_zoom{1};
std::atomic<bool> g_crosshair{false};
std::atomic<bool> g_turned{false};    // play, and a mouse button held
std::atomic<bool> g_aim_only{false};  // play, and the right button held without the left
std::atomic<int64_t> g_updated{0};

int Forced() {
    static const int v = [] {
        const char* e = std::getenv("EON_FREE_AIM");
        return e && *e ? std::atoi(e) : -1;
    }();
    return v;
}

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

uint32_t Camera(uint32_t bond) {
    uint32_t cams = kernel::R32(kCameras);
    uint32_t list = cams ? kernel::R32(cams + 4) : 0;
    return list ? list + kernel::R32(bond + kPlayerIndex) * kCameraBytes : 0;
}

// the camera mode in use (0x8DDD0), 0 if none
uint32_t CameraMode(uint32_t cam) { return cam ? kernel::R32(cam + 8 + kernel::R32(cam + 0x80) * 12) : 0; }

// a thiscall on Bond that answers in al, the registers kept
bool AskBond(X86Context& ctx, uint32_t bond, uint32_t fn) {
    uint32_t keep = ctx.ecx;
    ctx.ecx = bond;
    bool yes = (kernel::CallGuest(ctx, fn, {}) & 0xFF) != 0;
    ctx.ecx = keep;
    return yes;
}

bool ScopeView(X86Context& ctx, uint32_t mode) {
    if (!mode) return false;
    uint32_t keep = ctx.ecx;
    ctx.esp -= 16;  // room on the guest's stack for its answer
    ctx.ecx = mode;
    bool yes = (kernel::CallGuest(ctx, kernel::R32(kernel::R32(mode) + 8), {kScopeView, ctx.esp}) & 0xFF) != 0;
    ctx.esp += 16;
    ctx.ecx = keep;
    return yes;
}

// free aim plays for Bond now: the setting, the keyboard and mouse, his camera, no lock and no mounted gun
bool Plays(X86Context& ctx, uint32_t bond) {
    if (bond != g_bond) return false;  // player 1, whom the mouse plays (his update, below)
    int forced = Forced();
    if (forced == 0 || (forced < 0 && (!eon::free_aim || !input::UsingKeyboard()))) return false;
    return Camera(bond) && !AskBond(ctx, bond, kLockedOn) && !AskBond(ctx, bond, kMounted);
}

void DrawCrosshair(double) {
#ifdef _WIN32
    if (!g_crosshair || NowMs() - g_updated > 150) return;  // a paused game does not update Bond
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 c(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    float k = vp->WorkSize.y / 720, gap = 5 * k, arm = 9 * k;
    ImDrawList* d = ImGui::GetForegroundDrawList();
    auto lines = [&](ImU32 col, float w) {
        d->AddLine(ImVec2(c.x - gap - arm, c.y), ImVec2(c.x - gap, c.y), col, w);
        d->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + arm, c.y), col, w);
        d->AddLine(ImVec2(c.x, c.y - gap - arm), ImVec2(c.x, c.y - gap), col, w);
        d->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + arm), col, w);
    };
    lines(IM_COL32(0, 0, 0, 170), 3.5f * k);
    lines(IM_COL32(255, 255, 255, 235), 1.5f * k);
#endif
}

}  // namespace

// Bond's update, thiscall: whether this is play, and a held button turns him
X86_EXTERN_FUNC(EON_ORIG(00156E50, 001566C0));
X86_FUNC(EON_FN(00156E50, 001566C0)) {
    uint32_t bond = ctx.ecx;
    EON_ORIG(00156E50, 001566C0)(ctx, base);
    if (kernel::R32(bond + kPlayerIndex) != 0) return;
    eon::frame_ui = DrawCrosshair;
    g_bond = bond;
    g_updated = NowMs();
    uint32_t cam = Camera(bond), mode = CameraMode(cam);
    uint32_t kind = mode ? kernel::R32(mode) : 0;
    bool plays = Plays(ctx, bond);
    bool scope = plays && ScopeView(ctx, mode);
    uint32_t buttons = Forced() == 2 ? 3 : input::MouseButtons();
    // play: the follow camera (the lock's and the cutscenes' are other classes) with the HUD up
    bool on = plays && !scope && kind == kFollowCamera && (Forced() >= 0 || NowMs() - eon::hud_drawn_ms < 500);
    g_crosshair = on;
    float shoulder = g_shoulder, want = on ? kShoulder : 0;
    g_shoulder = shoulder + std::clamp(want - shoulder, -kShoulder * kBlendPerUpdate, kShoulder * kBlendPerUpdate);
    float zoom = g_zoom, zoom_want = on && (buttons & 2) ? kRightButtonZoom : 1;
    g_zoom = zoom + std::clamp(zoom_want - zoom, -0.03f, 0.03f);
    g_turned = on && (buttons & 3);
    g_aim_only = on && (buttons & 2) && !(buttons & 1);
    eon::aim_trigger = g_aim_only.load();
    // the left button on top of the right: a gun fired by the press needs one, so the held input lets go first
    static bool left_was = false;
    if ((buttons & 1) && !left_was && (buttons & 2)) eon::aim_release = NowMs() + 70;
    left_was = buttons & 1;
    if (!on || !(buttons & 3)) return;
    float yaw = kernel::G<float>(cam + 0xD4);
    float have = std::atan2(kernel::G<float>(bond + kMatrix + 4), kernel::G<float>(bond + kMatrix));
    float d = yaw * kRadians - have, c = std::cos(d), s = std::sin(d);
    kernel::G<float>(bond + kYaw) = yaw;
    for (uint32_t row = kMatrix; row < kMatrix + 0x30; row += 0x10) {
        float x = kernel::G<float>(bond + row), y = kernel::G<float>(bond + row + 4);
        kernel::G<float>(bond + row) = c * x - s * y;
        kernel::G<float>(bond + row + 4) = s * x + c * y;
    }
}

// A gun's shot, thiscall (mode): the right button alone holds the fire input, for the stance firing has (his action
// state, the kinds 0x161E00 asks for), and Bond's gun does not go off
X86_EXTERN_FUNC(EON_ORIG(00181250, 00180AC0));
X86_FUNC(EON_FN(00181250, 00180AC0)) {
    if (g_aim_only) {
        uint32_t keep = ctx.ecx;
        uint32_t owner = kernel::CallGuest(ctx, kGunOwner, {});
        ctx.ecx = keep;
        if (owner == g_bond) {
            // as the shot ends, or the gun waits for it and fires no more
            kernel::W32(keep + 0x328, kernel::R32(keep + 0x328) | 0x80000);
            return kernel::Return(ctx, 0, 1);
        }
    }
    EON_ORIG(00181250, 00180AC0)(ctx, base);
}

// Bond's upper body to his aim, fastcall: 0x14A740 goes to the game's own, 0x14A2C0, with +0xD70 & 6 set (which is not,
// measured, in this stance) and that needs a target; here the camera's pitch through its numbers.
// What it writes: yaw +0x988 (the shoulders' offset for that pitch), +0x98C, pitch +0x990, each within the limits at
// +0x96C and +0x974; vtable +0x110 and +0x114 then apply them.
X86_EXTERN_FUNC(EON_ORIG(0014A740, 00149FB0));
X86_FUNC(EON_FN(0014A740, 00149FB0)) {
    uint32_t bond = ctx.ecx;
    if (bond != g_bond || !g_turned) return EON_ORIG(0014A740, 00149FB0)(ctx, base);
    uint32_t cam = Camera(bond);
    float pitch = kernel::G<float>(cam + 0xD0);
    if (pitch > 180) pitch -= 360;
    pitch *= kRadians;
    // the table's pairs: pitch scale and offset, then shoulders', each two-handed or not and flipped or not
    uint32_t held = kernel::R32(kernel::R32(bond + 0x274) + 0x10);
    uint32_t grip = held ? kernel::R32(held + 0x370) : 0;
    bool wide = grip >= 0x10000 && grip <= 0x200000;
    bool flipped = (kernel::R32(bond + 0xD70) & 4) != 0;
    auto entry = [](uint32_t at) { return kernel::G<float>(kBodyTable + at); };
    float a = flipped ? pitch * entry(wide ? 0x1C : 0xC) - entry(wide ? 0x18 : 0x8)
                      : pitch * entry(wide ? 0x14 : 0x4) + entry(wide ? 0x10 : 0x0);
    float b = flipped ? pitch * entry(wide ? 0x3C : 0x2C) - entry(wide ? 0x38 : 0x28)
                      : pitch * entry(wide ? 0x34 : 0x24) - entry(wide ? 0x30 : 0x20);
    float yaw_limit = kernel::G<float>(bond + 0x96C), pitch_limit = kernel::G<float>(bond + 0x974);
    kernel::G<float>(bond + 0x988) = std::clamp(-b, -yaw_limit, yaw_limit);
    kernel::W32(bond + 0x98C, 0);
    kernel::G<float>(bond + 0x990) = std::clamp(a, -pitch_limit, pitch_limit);
    // +0xCDC bit 0 lets +0x114 apply them to his spine's bones
    kernel::W32(bond + 0xCDC, kernel::R32(bond + 0xCDC) | 1);
    uint32_t vtable = kernel::R32(bond);
    for (uint32_t slot : {0x110u, 0x114u}) {
        ctx.ecx = bond;
        kernel::CallGuest(ctx, kernel::R32(vtable + slot), {});
    }
    ctx.ecx = bond;
    kernel::Return(ctx, 1, 0);
}

// the camera info, cdecl (player, out): position, then angles at +0x10
X86_EXTERN_FUNC(EON_ORIG(0008D960, 0008D6D0));
X86_FUNC(EON_FN(0008D960, 0008D6D0)) {
    uint32_t player = kernel::R32(ctx.esp + 4), out = kernel::R32(ctx.esp + 8), bond = g_bond;
    EON_ORIG(0008D960, 0008D6D0)(ctx, base);
    if (!bond || player != kernel::R32(bond + kPlayerIndex)) return;
    // the zoom here, in the one field of view every view and frustum starts from: 0xF6170 builds a frustum from this
    // value and from the scales 0x61080 makes of it, so changing it at 0x61080 alone culled what was in view
    kernel::G<float>(out + 0x30) *= g_zoom;
    float shoulder = g_shoulder;
    float yaw = kernel::G<float>(out + 0x14) * kRadians;
    kernel::G<float>(out) += std::sin(yaw) * shoulder;
    kernel::G<float>(out + 4) -= std::cos(yaw) * shoulder;
}

// thiscall (angles out, matrix out or 0)
X86_EXTERN_FUNC(EON_ORIG(00153CD0, 00153540));
X86_FUNC(EON_FN(00153CD0, 00153540)) {
    uint32_t bond = ctx.ecx, angles = kernel::R32(ctx.esp + 4), matrix = kernel::R32(ctx.esp + 8);
    if (!Plays(ctx, bond)) return EON_ORIG(00153CD0, 00153540)(ctx, base);
    // as the scope's view: the camera's angles, and their matrix
    uint32_t cam = Camera(bond);
    for (uint32_t i = 0; i < 16; i += 4) kernel::W32(angles + i, kernel::R32(cam + 0xD0 + i));
    if (matrix) kernel::CallGuest(ctx, kAnglesToMatrix, {angles, matrix});
    kernel::Return(ctx, 0, 2);
}

// thiscall (point out)
X86_EXTERN_FUNC(EON_ORIG(00153BE0, 00153450));
X86_FUNC(EON_FN(00153BE0, 00153450)) {
    uint32_t bond = ctx.ecx, out = kernel::R32(ctx.esp + 4);
    if (!Plays(ctx, bond)) return EON_ORIG(00153BE0, 00153450)(ctx, base);
    uint32_t cam = Camera(bond);
    float pitch = kernel::G<float>(cam + 0xD0) * kRadians, yaw = kernel::G<float>(cam + 0xD4) * kRadians;
    float f[3] = {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), -std::sin(pitch)};
    float shoulder = g_shoulder;
    float c[3] = {kernel::G<float>(cam + 0xC0) + std::sin(yaw) * shoulder,
                  kernel::G<float>(cam + 0xC4) - std::cos(yaw) * shoulder, kernel::G<float>(cam + 0xC8)};
    float b[3] = {kernel::G<float>(bond + kPosition), kernel::G<float>(bond + kPosition + 4),
                  kernel::G<float>(bond + kPosition + 8)};
    float along = (b[0] - c[0]) * f[0] + (b[1] - c[1]) * f[1] + (b[2] - c[2]) * f[2];
    for (int i = 0; i < 3; i++) kernel::G<float>(out + 4 * i) = c[i] + f[i] * along;
    kernel::G<float>(out + 12) = 1;
    kernel::Return(ctx, 0, 1);
}

// thiscall: Bond has an aim for his shots unless his flags +0x1550 hold 0x100, 0x800 or 0x1000 (0x800 while he
// stands or runs without a lock): in free aim always
X86_EXTERN_FUNC(EON_ORIG(001493A0, 00148C10));
X86_FUNC(EON_FN(001493A0, 00148C10)) {
    uint32_t bond = ctx.ecx;
    if (!Plays(ctx, bond)) return EON_ORIG(001493A0, 00148C10)(ctx, base);
    kernel::Return(ctx, 1, 0);
}
