// Widescreen and ultrawide in the driving missions.
//
// driving.xbe's 3D views are 4:3 whatever the widescreen flag its launch data carries (the same frame with 4:3 and
// widescreen launch data is the same; default.xbe's own views widen), so on a wider screen they stretched. A view's
// projection is built in 0x2111E0 (thiscall: field of view, aspect, near, far): a square projection (0x263B76), its
// x scale times +0x140 (+0x40), its y scale times +0x144 and the aspect (+0x54), then the culling planes: left and right
// from the half angle's tangent times +0x178 and +0x17C, top and bottom from +0x180 and +0x184 over the aspect. Widened
// by k (the frame's aspect over 4:3): the x scale is divided by k after, and +0x178 and +0x17C are k times larger
// while it runs (put back after), so the sides show more and are not culled; the height is as the game made it.
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
