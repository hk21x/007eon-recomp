// hide_hud in the driving missions: the game's own "hud" option (its profile's, read by 0x15D070 into the byte
// 0x2FA279, 1 shown), which the frame's HUD pass 0xDFCB0 (and the radar and meters in 0x1F2440 under it) and the
// objective and hint messages (0x1EC000, 0x1EC1E0) test. Cleared while those run, the game's value put back after.
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern bool hide_hud;  // the setting (src/common/game_info.cpp)
}

namespace {
struct HudHidden {
    static constexpr uint32_t kHudShown = EON_VA(0x2FA279, 0x2FA259);
    uint8_t kept = kernel::G<uint8_t>(kHudShown);
    HudHidden() {
        if (eon::hide_hud) kernel::G<uint8_t>(kHudShown) = 0;
    }
    ~HudHidden() { kernel::G<uint8_t>(kHudShown) = kept; }
};
}  // namespace

X86_EXTERN_FUNC(EON_ORIG(000DFCB0, 000DFC20));
X86_FUNC(EON_FN(000DFCB0, 000DFC20)) {
    HudHidden h;
    EON_ORIG(000DFCB0, 000DFC20)(ctx, base);
}

X86_EXTERN_FUNC(EON_ORIG(001EC000, 001EBF60));
X86_FUNC(EON_FN(001EC000, 001EBF60)) {
    HudHidden h;
    EON_ORIG(001EC000, 001EBF60)(ctx, base);
}

X86_EXTERN_FUNC(EON_ORIG(001EC1E0, 001EC140));
X86_FUNC(EON_FN(001EC1E0, 001EC140)) {
    HudHidden h;
    EON_ORIG(001EC1E0, 001EC140)(ctx, base);
}
