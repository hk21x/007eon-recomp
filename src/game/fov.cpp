// fov_percent: the 3D views' field of view, scaled.
//
// Every view's projection comes from 0x61080 (cdecl: x scale out, y scale out, the vertical field of view in radians,
// the widen factors 0x5AA550 and 0x5AA554, the first widened for ultrawide here too: src/game/ultrawide.cpp): each
// scale is 1 over tan(fov / 2) (the x scale over the aspect and the widen factor too). Its callers make the view's
// culling planes from the same scales (0xF5C70, 0xF6170), so the angle is scaled where it comes in and what is drawn
// and what is culled agree. The game's zooms (the sniper scope, the cameras' own changes) stay in proportion; the
// cutscenes drawn in the engine widen with it.
#include <algorithm>
#include <cstring>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern uint32_t fov_percent;  // the setting (src/common/game_info.cpp; EON_FOV headless)
float WidenedFactor(float f);  // ultrawide: the widen factor every frustum gets (src/game/ultrawide.cpp)
}

X86_EXTERN_FUNC(EON_ORIG(00061080, 00060DB0));
X86_FUNC(EON_FN(00061080, 00060DB0)) {
    // ultrawide: the frustums the game culls with as wide as the views it draws (the widen factor, the 4th argument)
    float kx;
    std::memcpy(&kx, &kernel::G<uint32_t>(ctx.esp + 16), 4);
    kx = eon::WidenedFactor(kx);
    std::memcpy(&kernel::G<uint32_t>(ctx.esp + 16), &kx, 4);
    uint32_t pct = std::clamp<uint32_t>(eon::fov_percent, 50, 150);
    if (pct != 100) {
        float fov;
        std::memcpy(&fov, &kernel::G<uint32_t>(ctx.esp + 12), 4);
        float wider = std::min(fov * float(pct) / 100, 2.9f);  // short of 180 degrees
        std::memcpy(&kernel::G<uint32_t>(ctx.esp + 12), &wider, 4);
        static uint32_t said = 100;
        if (said != pct) {
            said = pct;
            LOGR("eon: field of view %u%% (%.1f degrees for the game's %.1f)", pct, wider * 57.29578f, fov * 57.29578f);
        }
    }
    EON_ORIG(00061080, 00060DB0)(ctx, base);
}
