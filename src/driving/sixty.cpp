// Driving at 60 frames a second.
//
// driving.xbe simulates in ticks of 1/60 s (0x3165F8 = 60, 0x3165FC = 1/60, 0x316600 the tick count, one tick
// 0x7A6B0), run by the scheduler 0x15F4D0 against the vertical blank count 0x35FBB4 (the blank callback 0x15CD30).
// It presents every second blank (0x214CB0(2), from the device setup 0x14D290), and the scheduler waits for two blanks
// and runs two ticks (0x15F5B3: last + 2) unless its object's byte +0x1C or +0x1E is set; then it runs one tick for
// each blank gone by (the blanks since the last run times the speed +0x20, up to 11). The drawing task 0x72410 draws
// (0x14EBF0) only when the callback's second count, bumped every other blank (0x35FBB8), differs from the one it last
// drew at (0x326F30). So for 60: the interval asked for is 1, +0x1E is set while the scheduler runs (put back after:
// nothing else reads it here), and the drawing task sees a new count at each blank. The game's speed stays real time:
// the ticks follow the blanks either way.
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

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
X86_EXTERN_FUNC(EON_ORIG(00214CB0, 00214C70));
X86_FUNC(EON_FN(00214CB0, 00214C70)) {
    if (On() && kernel::R32(ctx.esp + 4) == 2) kernel::W32(ctx.esp + 4, 1);
    EON_ORIG(00214CB0, 00214C70)(ctx, base);
}

// fastcall (the scheduler object in ecx)
X86_EXTERN_FUNC(EON_ORIG(0015F4D0, 0015F430));
X86_FUNC(EON_FN(0015F4D0, 0015F430)) {
    if (!On()) return EON_ORIG(0015F4D0, 0015F430)(ctx, base);
    uint32_t sched = ctx.ecx;
    uint8_t was = kernel::G<uint8_t>(sched + 0x1E);
    kernel::G<uint8_t>(sched + 0x1E) = 1;
    EON_ORIG(0015F4D0, 0015F430)(ctx, base);
    kernel::G<uint8_t>(sched + 0x1E) = was;
}

// fastcall (its task object in ecx): a blank gone by since it last ran looks like a new half rate count to it
X86_EXTERN_FUNC(EON_ORIG(00072410, 000722B0));
X86_FUNC(EON_FN(00072410, 000722B0)) {
    if (On()) {
        constexpr uint32_t kBlanks = EON_VA(0x35FBB4, 0x35FB94), kHalfBlanks = EON_VA(0x35FBB8, 0x35FB98);
        constexpr uint32_t kDrawnAt = EON_VA(0x326F30, 0x326F10);
        static uint32_t last = 0;
        uint32_t blanks = kernel::R32(kBlanks);
        if (blanks != last && kernel::R32(kDrawnAt) == kernel::R32(kHalfBlanks))
            kernel::W32(kDrawnAt, kernel::R32(kHalfBlanks) - 1);
        last = blanks;
    }
    EON_ORIG(00072410, 000722B0)(ctx, base);
}
