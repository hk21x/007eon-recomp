// fov_percent: the 3D views' field of view, scaled.
//
// Every view's projection comes from 0x61080 (cdecl: x scale out, y scale out, the vertical field of view in radians,
// the widen factors 0x5AA550 and 0x5AA554): each scale is 1 over tan(fov / 2) (the x scale over the aspect and the
// widen factor too). Its callers make the view's culling planes from the same scales (0xF5C70, 0xF6170), so the angle
// is scaled where it comes in and what is drawn and what is culled agree. The game's zooms (the sniper scope, the
// cameras' own changes) stay in proportion; the cutscenes drawn in the engine widen with it.
#include <algorithm>
#include <cstring>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace eon {
extern uint32_t fov_percent;  // the setting (src/common/game_info.cpp; EON_FOV headless)
}

X86_EXTERN_FUNC(__imp__sub_00061080);
X86_FUNC(sub_00061080) {
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
    __imp__sub_00061080(ctx, base);
}
