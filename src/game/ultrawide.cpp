// Ultrawide: the 3D view widened to the frame's shape instead of stretched.
//
// With the console's widescreen flag (0x5AA59C, from the video flags at 0x12046) the game widens its views by the
// factor at 0x5AA550: 1 at 4:3, 4/3 (0x2C51C8, 0x2C56E4) at 16:9, halved for a split screen's halves; the projection
// (0x61080) divides its x scale by it. 0xF67A0 sets it each frame and draws the views through 0xF6660; 0xF4B60 sets
// it from its argument for 0x12F610's view. On a frame wider than 16:9 the factor is multiplied by the frame's width
// over 16:9 while 0xF6660 draws (put back after: 0xF67A0 sets it before each call) and after 0xF4B60. The HUD and
// menus, laid out for 16:9 and drawn through a pass-through program, are kept 16:9 by the renderers
// (GameInfo::squeeze_screen_programs). The factor also goes into every frustum the game culls with, outside those two:
// 0x61080 builds each projection with it as an argument (0xF5C70's culling planes, 0xF6170's visibility tests), so
// there a factor the game set and these did not widen is widened too (eon::WidenedFactor, from src/game/fov.cpp's
// hook): else whatever lies past the 16:9 edges is culled and pops in as it comes inside.
#include <algorithm>
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {

constexpr uint32_t kWidescreen = 0x5AA59C;
constexpr uint32_t kWiden = 0x5AA550;

float Factor() {
    float f;
    std::memcpy(&f, &kernel::G<uint32_t>(kWiden), 4);
    return f;
}

float g_written = 0;  // the widened factor these hooks last wrote

void SetFactor(float f) {
    std::memcpy(&kernel::G<uint32_t>(kWiden), &f, 4);
    g_written = f;
}

// how much wider than 16:9 the frame is, 1 without the widescreen flag
float Extra() {
    if (!kernel::G<uint8_t>(kWidescreen)) return 1.0f;
    float k = std::max(app::FrameAspect(), 16.0f / 9.0f) / (16.0f / 9.0f);
    static bool said = false;
    if (k > 1.0f + 1e-3f && !said) {
        said = true;
        LOGR("eon: 3D views widened %.3f times for the %.3f:1 frame", k, app::FrameAspect());
    }
    return k;
}

}  // namespace

namespace eon {
// a projection's widen factor (0x61080's argument): widened unless it is the one these hooks wrote
float WidenedFactor(float f) {
    float k = Extra();
    if (k <= 1.0f + 1e-3f || (f == g_written && Factor() == g_written)) return f;
    return f * k;
}
}  // namespace eon

X86_EXTERN_FUNC(__imp__sub_000F6660);
X86_FUNC(sub_000F6660) {
    float was = Factor(), k = Extra();
    if (k > 1.0f + 1e-3f) SetFactor(was * k);
    __imp__sub_000F6660(ctx, base);
    if (k > 1.0f + 1e-3f) SetFactor(was);
}

X86_EXTERN_FUNC(__imp__sub_000F4B60);
X86_FUNC(sub_000F4B60) {
    __imp__sub_000F4B60(ctx, base);
    float k = Extra();
    if (k > 1.0f + 1e-3f) SetFactor(Factor() * k);
}
