// When a menu takes the controller's input, for the mouse in the menus (src/common/menu_mouse.cpp).
//
// The menus' engine (EA FEng) gives each package the controllers' input through 0x131610 (thiscall: the package, the
// port); a package that takes port 1 (its mask +0x20) with a button in focus (+0x84) is a menu in use. Play has one
// too, boot_ingame_pov.fng, taking every port with no button in focus, so focus is asked for. Mission Select
// (frontend_mission_screen.fng, every port) turns its missions with no button in focus either, so it is named: with a
// pad seen on two ports (issue #32) the menus' guard (src/common/menu_mouse.cpp) was off there, and each tap moved two
// missions (measured headless: four Rights, eight missions on; one port, four). A package's name is at +0xC.
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "core/input.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern std::atomic<int64_t> g_menu_input;
extern std::atomic<uint32_t> menu_manager;  // src/game/pointer.cpp
int64_t NowMs();
}  // namespace eon

namespace {
bool MissionSelect(uint32_t pkg) {
    static constexpr char kName[] = "frontend_mission_screen.fng";
    uint32_t name = kernel::R32(pkg + 0xC);
    return name && mem::IsCommitted(name) && mem::IsCommitted(name + sizeof kName - 1) &&
           std::memcmp(kernel::HostPtr(name), kName, sizeof kName) == 0;
}
}  // namespace

X86_EXTERN_FUNC(EON_ORIG(00131610, 00130F10));
X86_FUNC(EON_FN(00131610, 00130F10)) {
    uint32_t pkg = kernel::R32(ctx.esp + 4), port = kernel::R32(ctx.esp + 8);
    // the engine's own mouse path (its manager, ecx, +2), on with the keyboard and mouse: the pointer (src/game/pointer.cpp)
    eon::menu_manager = ctx.ecx;
    kernel::G<uint8_t>(ctx.ecx + 2) = input::UsingKeyboard() || std::getenv("EON_POINTER") ? 1 : 0;
    if (pkg && port == 0 && (kernel::R32(pkg + 0x20) & 1) && (kernel::R32(pkg + 0x84) || MissionSelect(pkg)))
        eon::g_menu_input = eon::NowMs();
    EON_ORIG(00131610, 00130F10)(ctx, base);
}
