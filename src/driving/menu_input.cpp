// When a driving mission's menu takes the controller's input, for the mouse in the menus (src/common/menu_mouse.cpp);
// default.xbe's is src/game/menu_input.cpp (the same engine, 0x131610 there).
//
// The menus' engine (EA FEng) gives each package the controllers' input through 0x13C2D0 (thiscall: the package, the
// port); a package that takes port 1 (its mask +0x20) with a button in focus (+0x84) is a menu in use.
#include <atomic>
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern std::atomic<int64_t> g_menu_input;
int64_t NowMs();
}  // namespace eon

X86_EXTERN_FUNC(EON_ORIG(0013C2D0, 0013C180));
X86_FUNC(EON_FN(0013C2D0, 0013C180)) {
    uint32_t pkg = kernel::R32(ctx.esp + 4), port = kernel::R32(ctx.esp + 8);
    if (pkg && port == 0 && (kernel::R32(pkg + 0x20) & 1) && kernel::R32(pkg + 0x84)) eon::g_menu_input = eon::NowMs();
    EON_ORIG(0013C2D0, 0013C180)(ctx, base);
}
