// The mouse wheel zooms a scope, or a mounted gun, as Agent Under Fire's does (there it changed weapon).
//
// The zoom (a weapon's, or a mounted gun's; 0x14B640, thiscall on Bond, answers the one in use): +0 its factor, +4 the
// least (1), +8 the most, +0xC the rate, +0x10 a byte, scoped in. Each scoped frame 0x153810 passes the zoom button's
// analog (-1 to 1) to 0x17FB30 (thiscall on the zoom, one float), which sets factor * (1 + rate * a) within the
// least and the most and flags the change, from which the zoom's sounds play (0x180BC0) and the scope's camera takes
// its field of view (60 / factor, 0x1AC560). Here a notch of the wheel waiting (src/common/menu_mouse.cpp sends them
// while zoom_wheel holds) adds to that analog what makes the factor kNotch times as much, or less.
#include <atomic>
#include <cmath>
#include <cstring>

#include "core/input.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern std::atomic<bool> zoom_wheel;
extern std::atomic<int> zoom_notches;
}

namespace {
constexpr float kNotch = 1.25f;
std::atomic<uint32_t> g_zoom{0};  // player 1's zoom while the wheel zooms it
}

namespace eon {
// from Bond's update (src/game/free_aim.cpp), player 1's: the zoom in use (0 none) and whether he is on a mounted gun
void ZoomWheel(uint32_t zoom, bool mounted) {
    bool on = zoom && input::UsingKeyboard() && (mounted || kernel::G<uint8_t>(zoom + 0x10));
    g_zoom = on ? zoom : 0;
    eon::zoom_wheel = on;
    if (!on) eon::zoom_notches = 0;
}
}  // namespace eon

X86_EXTERN_FUNC(EON_ORIG(0017FB30, 0017F3A0));
X86_FUNC(EON_FN(0017FB30, 0017F3A0)) {
    uint32_t zoom = ctx.ecx;
    if (zoom && zoom == g_zoom) {
        int n = eon::zoom_notches.exchange(0);
        float rate = kernel::G<float>(zoom + 0xC);
        if (n && rate > 0) {
            float a;
            std::memcpy(&a, &kernel::G<uint32_t>(ctx.esp + 4), 4);
            a += (std::pow(kNotch, float(n)) - 1) / rate;
            std::memcpy(&kernel::G<uint32_t>(ctx.esp + 4), &a, 4);
        }
    }
    EON_ORIG(0017FB30, 0017F3A0)(ctx, base);
}
