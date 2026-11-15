// Small round lock-on dot (issue #27): the lock's precision aim dot drawn as a small anti-aliased circle.
//
// While locked on (LT) the right stick or the mouse moves a red dot over the target for head shots. It is part of the
// lock's crosshair item (UI_HUD_CROSSHAIRITEM_*, 0x119A40..0x119B90, updated by 0x11B020 each frame), drawn by its draw
// method 0x25F290 (thiscall, the item in ecx, the 2D renderer as its argument; in the item vtables 0x2DB9DC,
// 0x2DBA2C, 0x2DBA7C): after the item's own picture (+0x2C, and +0x39C while +0x398), when +0x428 is set (0x25F0C0,
// from 0x11B35D: Bond's aim point 0x151F20 answered) it draws the dot at +0x42C (0x25F0F0: four floats, +4 x and +8 y
// across the 2D frame, 0 to 1: the game's dot centred at x*640, y*480 in captures, measured) through 0xCCC30: the
// element +0x164 when +0xC4 is set and +0xCD clear (0x25F0D0, the aim point's flag from 0x151F20; +0xCD from
// 0x119B90, the lock's miss item), else +0xD8. Both are one red texture
// (1,0,0,1), +0xD8 4 units of the 480 high frame (scale +0x80/+0x84 4, measured), +0x164 50 (a faint red disc with
// the dot at its middle). Both are scaled with the frame like the rest of the HUD, so at 4K the dot was an 18 pixel
// red square over what it aims at.
//
// With the option (on by default) the item draws with +0x428 cleared for the call (only that method reads it), and
// the dot is drawn here with ImGui at the same place: a red circle of radius 2 at 720 lines (scaled by the window's
// height, as free aim's crosshair), a thin dark outline round it, and for +0x164 a faint disc of the game's size
// round it. The 2D frame is squeezed into the window's middle at GameInfo::hud_aspect on a wider screen (the
// renderers' ultrawide squeeze), so x maps into that width. Windows only: headless the game's dot is gone and the
// position is logged.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"
#ifdef _WIN32
#include "imgui.h"
#endif

namespace eon {
extern bool small_lock_dot;           // the setting (src/common/game_info.cpp)
extern void (*lock_dot_ui)();         // drawn each frame (src/common/game_info.cpp)
}

namespace {

constexpr uint32_t kDotOn = 0x428, kDotPos = 0x42C, kOnTarget = 0xC4, kMiss = 0xCD;
constexpr float kGlow = 25.0f / 480;  // +0x164's half size, of the frame's height

std::atomic<float> g_x{0}, g_y{0};
std::atomic<bool> g_glow{false};
std::atomic<int64_t> g_drawn_ms{0};

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

float F(uint32_t a) {
    float f;
    std::memcpy(&f, &kernel::G<uint32_t>(a), 4);
    return f;
}

void DrawDot() {
#ifdef _WIN32
    if (NowMs() - g_drawn_ms > 100) return;  // the game did not draw it lately (paused, unlocked)
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float h = vp->WorkSize.y, w = std::min(vp->WorkSize.x, h * app::HudAspect());
    const ImVec2 c(vp->WorkPos.x + vp->WorkSize.x * 0.5f + (g_x - 0.5f) * w, vp->WorkPos.y + g_y * h);
    const float k = h / 720, r = 2.0f * k;
    ImDrawList* d = ImGui::GetForegroundDrawList();
    if (g_glow) d->AddCircleFilled(c, kGlow * h, IM_COL32(255, 0, 0, 40), 48);
    d->AddCircleFilled(c, r + 1.0f * k, IM_COL32(0, 0, 0, 170), 24);
    d->AddCircleFilled(c, r, IM_COL32(255, 20, 20, 255), 24);
#endif
}

}  // namespace

X86_EXTERN_FUNC(EON_ORIG(0025F290, 0025EAC0));
X86_FUNC(EON_FN(0025F290, 0025EAC0)) {
    const uint32_t item = ctx.ecx;
    const uint8_t on = kernel::G<uint8_t>(item + kDotOn);
    if (!eon::small_lock_dot || !on) return EON_ORIG(0025F290, 0025EAC0)(ctx, base);
    const float x = F(item + kDotPos + 4), y = F(item + kDotPos + 8);
    const bool glow = kernel::G<uint8_t>(item + kOnTarget) && !kernel::G<uint8_t>(item + kMiss);
    g_x = x;
    g_y = y;
    g_glow = glow;
    g_drawn_ms = NowMs();
    eon::lock_dot_ui = DrawDot;
    static int said = 0;
    if (said++ % 600 == 0)
        LOGR("eon: lock dot drawn round at %.3f %.3f of the 2D frame (%.0f %.0f of 640x480)%s", x, y, x * 640, y * 480,
             glow ? ", on target" : "");
    kernel::G<uint8_t>(item + kDotOn) = 0;  // the game's square left out
    EON_ORIG(0025F290, 0025EAC0)(ctx, base);
    kernel::G<uint8_t>(item + kDotOn) = on;
}
