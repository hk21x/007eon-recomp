// Widescreen and ultrawide in the driving missions.
//
// driving.xbe's 3D views are 4:3 whatever the widescreen flag its launch data carries (the same frame with 4:3 and
// widescreen launch data is the same; default.xbe's own views widen), so on a wider screen they stretched. A view's
// projection is built in 0x2111E0 (thiscall: field of view, aspect, near, far): a square projection (0x263B76), its
// x scale times +0x140 (+0x40), its y scale times +0x144 and the aspect (+0x54), then the culling planes: left and right
// from the half angle's tangent times +0x178 and +0x17C, top and bottom from +0x180 and +0x184 over the aspect. Widened
// by k (the frame's aspect over 4:3): the x scale is divided by k after, and +0x178 and +0x17C are k times larger
// while it runs (put back after), so the sides show more and are not culled; the height is as the game made it.
// What the scene draws is chosen apart from that: the visibility pass 0x14C460 walks the scene's quadtree (0x34360)
// testing each node's sphere against the frustum at 0x32A8C0 (0x147300: view matrix +0x10, near +0xB0, far +0xB4,
// side planes as (x, z) normals at +0x70/+0x78 and +0x80/+0x88, top and bottom at +0x94.. and +0xA4..), whose sides
// are the game's 16:9 (with the widescreen flag 0x35ED54 +0x4C, else 4:3) whatever the frame: far terrain chunks past
// those edges popped in as they came inside. At the pass's start its sides are widened by the frame's aspect over the
// game's, in tangent (a plane's x over z), once for each frustum the game writes, and 15% more at every aspect: the
// console's edges were hidden by a TV's overscan.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
float GetF(uint32_t va) {
    float f;
    std::memcpy(&f, &kernel::G<uint32_t>(va), 4);
    return f;
}
void SetF(uint32_t va, float f) { std::memcpy(&kernel::G<uint32_t>(va), &f, 4); }
}  // namespace

X86_EXTERN_FUNC(__imp__sub_002111E0);
X86_FUNC(sub_002111E0) {
    float k = app::Widescreen() ? app::FrameAspect() / (4.0f / 3.0f) : 1.0f;
    if (k <= 1.0f + 1e-3f) return __imp__sub_002111E0(ctx, base);
    uint32_t view = ctx.ecx;
    float left = GetF(view + 0x178), right = GetF(view + 0x17C);
    SetF(view + 0x178, left * k);
    SetF(view + 0x17C, right * k);
    __imp__sub_002111E0(ctx, base);
    SetF(view + 0x178, left);
    SetF(view + 0x17C, right);
    SetF(view + 0x40, GetF(view + 0x40) / k);
    static bool said = false;
    if (!said) {
        said = true;
        LOGR("eon: driving views widened %.3f times for the %.3f:1 frame", k, app::FrameAspect());
    }
}

// the visibility pass (once a frame): the culling frustum's sides as wide as the frame
X86_EXTERN_FUNC(__imp__sub_0014C460);
X86_FUNC(sub_0014C460) {
    constexpr uint32_t kFrustum = 0x32A8C0, kLeft = kFrustum + 0x70, kRight = kFrustum + 0x80;
    constexpr uint32_t kGame = 0x35ED54, kWideFlag = 0x4C;
    // the console's sides lie on the frame's edges, which a TV's overscan hides: shown whole here, buildings and
    // lights popped in at the edges as the car turned (at every aspect); culled with a margin past them
    constexpr float kMargin = 1.15f;
    static float written[4] = {};  // left x, z, right x, z as last widened
    {
        float game = kernel::G<uint8_t>(kernel::R32(kGame) + kWideFlag) ? 16.0f / 9.0f : 4.0f / 3.0f;
        float r = (app::Widescreen() ? std::max(app::FrameAspect() / game, 1.0f) : 1.0f) * kMargin;
        float now[4] = {GetF(kLeft), GetF(kLeft + 8), GetF(kRight), GetF(kRight + 8)};
        if (std::memcmp(now, written, sizeof now) != 0 && now[1] > 0) {
            // a side plane (x, z) keeps x / z = z-ward tangent: that tangent r times larger, the normal unit length
            float t = now[1] / std::fabs(now[0]) * r, a = 1 / std::sqrt(1 + t * t), c = t * a;
            float out[4] = {std::copysign(a, now[0]), c, std::copysign(a, now[2]), c};
            SetF(kLeft, out[0]);
            SetF(kLeft + 8, out[1]);
            SetF(kRight, out[2]);
            SetF(kRight + 8, out[3]);
            std::memcpy(written, out, sizeof out);
            static bool said = false;
            if (!said) {
                said = true;
                LOGR("eon: driving culling %.3f times wider (the game's %.3f:1 for the %.3f:1 frame, a %.0f%% margin)",
                     r, game, app::FrameAspect(), (kMargin - 1) * 100);
            }
        }
    }
    __imp__sub_0014C460(ctx, base);
}
