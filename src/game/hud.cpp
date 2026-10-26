// hide_hud on foot: the game's 2D is drawn by UI managers, each through 0xC15D0 (fastcall, the manager in ecx): three
// layers, each on while its byte at +0x100.. is set, each a list of elements (+0x18, 12 bytes a layer). In a level the
// HUD's manager (vtable 0x2C3F80) has them all: the ammo and weapon box, the health bar, the tutorial and objective
// text; the pause screens and menus are other managers'. Its layers are switched off while it draws, the game's
// values put back after (the game switches them off itself while paused).
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace eon {
extern bool hide_hud;  // the setting (src/common/game_info.cpp)
}

X86_EXTERN_FUNC(__imp__sub_000C15D0);
X86_FUNC(sub_000C15D0) {
    constexpr uint32_t kHudManager = 0x2C3F80, kLayersOn = 0x100, kLayers = 3;
    const uint32_t m = ctx.ecx;
    if (!eon::hide_hud || kernel::R32(m) != kHudManager) return __imp__sub_000C15D0(ctx, base);
    uint8_t kept[kLayers];
    for (uint32_t i = 0; i < kLayers; i++) {
        kept[i] = kernel::G<uint8_t>(m + kLayersOn + i);
        kernel::G<uint8_t>(m + kLayersOn + i) = 0;
    }
    __imp__sub_000C15D0(ctx, base);
    for (uint32_t i = 0; i < kLayers; i++) kernel::G<uint8_t>(m + kLayersOn + i) = kept[i];
}
