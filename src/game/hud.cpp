// hide_hud on foot: the game's 2D is drawn by UI managers, each through 0xC15D0 (fastcall, the manager in ecx): three
// layers, each on while its byte at +0x100.. is set, each a list of elements (+0x18, 12 bytes a layer). In a level the
// HUD's manager (vtable 0x2C3F80) has them all: the ammo and weapon box, the health bar, the tutorial and objective
// text; the pause screens and menus are other managers'. Its layers are switched off while it draws, the game's
// values put back after (the game switches them off itself while paused).
#include <atomic>
#include <chrono>
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
std::atomic<int64_t> hud_drawn_ms{0};  // when the HUD last drew (free aim asks: src/game/free_aim.cpp)
extern bool hide_hud;  // the setting (src/common/game_info.cpp)
}

X86_EXTERN_FUNC(EON_ORIG(000C15D0, 000C1170));
X86_FUNC(EON_FN(000C15D0, 000C1170)) {
    constexpr uint32_t kHudManager = EON_VA(0x2C3F80, 0x2C34F0), kLayersOn = 0x100, kLayers = 3;
    const uint32_t m = ctx.ecx;
    if (kernel::R32(m) == kHudManager) {
        using namespace std::chrono;
        eon::hud_drawn_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    if (!eon::hide_hud || kernel::R32(m) != kHudManager) return EON_ORIG(000C15D0, 000C1170)(ctx, base);
    uint8_t kept[kLayers];
    for (uint32_t i = 0; i < kLayers; i++) {
        kept[i] = kernel::G<uint8_t>(m + kLayersOn + i);
        kernel::G<uint8_t>(m + kLayersOn + i) = 0;
    }
    EON_ORIG(000C15D0, 000C1170)(ctx, base);
    for (uint32_t i = 0; i < kLayers; i++) kernel::G<uint8_t>(m + kLayersOn + i) = kept[i];
}
