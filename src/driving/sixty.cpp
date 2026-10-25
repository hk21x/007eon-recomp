// Driving at 60 frames a second.
//
// driving.xbe simulates in ticks of 1/60 s (0x3165F8 = 60, 0x3165FC = 1/60, 0x316600 the tick count, one tick
// 0x7A6B0), run by the scheduler 0x15F4D0 against the vertical blank count 0x35FBB4 (the blank callback 0x15CD30).
// It presents every second blank (0x214CB0(2), from the device setup 0x14D290), and the scheduler waits for two blanks
// and runs two ticks (0x15F5B3: last + 2) unless its object's byte +0x1C or +0x1E is set; then it runs one tick for
// each blank gone by (the blanks since the last run times the speed +0x20, up to 11). So for 60: the interval asked
// for is 1, and +0x1E is set while the scheduler runs (put back after: nothing else reads it here). The game's speed
// stays real time: the ticks follow the blanks either way.
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace eon {
extern bool driving_60fps;  // the setting (src/common/game_info.cpp; EON_DRIVING60 headless)
}

namespace {
bool On() {
    static const bool on = [] {
        if (eon::driving_60fps) LOGR("eon: driving at 60 frames a second");
        return eon::driving_60fps;
    }();
    return on;
}
}  // namespace

// thiscall (interval): 1 every blank, 2 every second blank, 3 every fourth
X86_EXTERN_FUNC(__imp__sub_00214CB0);
X86_FUNC(sub_00214CB0) {
    if (On() && kernel::R32(ctx.esp + 4) == 2) kernel::W32(ctx.esp + 4, 1);
    __imp__sub_00214CB0(ctx, base);
}

// fastcall (the scheduler object in ecx)
X86_EXTERN_FUNC(__imp__sub_0015F4D0);
X86_FUNC(sub_0015F4D0) {
    if (!On()) return __imp__sub_0015F4D0(ctx, base);
    uint32_t sched = ctx.ecx;
    uint8_t was = kernel::G<uint8_t>(sched + 0x1E);
    kernel::G<uint8_t>(sched + 0x1E) = 1;
    __imp__sub_0015F4D0(ctx, base);
    kernel::G<uint8_t>(sched + 0x1E) = was;
}
