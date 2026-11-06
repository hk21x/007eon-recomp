// fov_percent: the 3D views' field of view, scaled.
//
// Every view on foot and its culling start from the camera info 0x8D960 makes (position, then pitch, yaw, roll, and at
// +0x30 the field of view, a factor of the game's angle: 1 unless a zoom changes it): 0xF6170 builds a frustum from that value and from the scales 0x61080 makes of it, so the
// angle is scaled there, once, and what is drawn and what is culled agree. Scaling 0x61080's argument instead (as
// until 1.1.0) changed only some culling, not the picture. The game's zooms (the sniper scope, the cameras' own changes)
// stay in proportion. 0x61080 is still hooked for ultrawide's widen factor (src/game/ultrawide.cpp).
#include <algorithm>
#include <cstring>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern uint32_t fov_percent;  // the setting (src/common/game_info.cpp; EON_FOV headless)
float WidenedFactor(float f);  // ultrawide: the widen factor every frustum gets (src/game/ultrawide.cpp)
void FreeAimInfo(uint32_t player, uint32_t out);  // free aim's zoom and shoulder (src/game/free_aim.cpp)
}

X86_EXTERN_FUNC(EON_ORIG(00061080, 00060DB0));
X86_FUNC(EON_FN(00061080, 00060DB0)) {
    // ultrawide: the frustums the game culls with as wide as the views it draws (the widen factor, the 4th argument)
    float kx;
    std::memcpy(&kx, &kernel::G<uint32_t>(ctx.esp + 16), 4);
    kx = eon::WidenedFactor(kx);
    std::memcpy(&kernel::G<uint32_t>(ctx.esp + 16), &kx, 4);
    EON_ORIG(00061080, 00060DB0)(ctx, base);
}

X86_EXTERN_FUNC(EON_ORIG(0008D960, 0008D6D0));
X86_FUNC(EON_FN(0008D960, 0008D6D0)) {
    uint32_t player = kernel::R32(ctx.esp + 4), out = kernel::R32(ctx.esp + 8);
    EON_ORIG(0008D960, 0008D6D0)(ctx, base);
    uint32_t pct = std::clamp<uint32_t>(eon::fov_percent, 50, 150);
    if (pct != 100) {
        float fov = kernel::G<float>(out + 0x30), wider = fov * float(pct) / 100;
        kernel::G<float>(out + 0x30) = wider;
        static uint32_t said = 100;
        if (said != pct) {
            said = pct;
            LOGR("eon: field of view %u%% (the camera's factor %.2f for the game's %.2f)", pct, wider, fov);
        }
    }
    eon::FreeAimInfo(player, out);
}
