// Player 1's controller is never a second copy of controller 1 (issue #35: the pause menu took no input, pad or keys).
//
// The game's pad layer (0x5D8F0, each port's state after XInputGetState) hands a port whose input changed to 0x5D860
// (esi the port), which makes that port the active one (its slot in the table at 0x35B6F4, 12 bytes a slot, the active
// slot's index at 0x35B6E8); 0x5DCD0 then puts the active slot first: player 1, its port at 0x35B6EC. The pause menu's
// package takes only player 1's input (its mask +0x20 is 1; the menus' engine reads a player's pad through 0x121970,
// which finds the player's port with 0x5D4F0), where the front end's take every player (mask 0x7FFFFFFF).
// A pad seen on two ports (issue #32) changes both ports in the same poll, the later port last: player 1 became the
// copy on port 2. The menus' guard (src/common/menu_mouse.cpp) lets go of a later port holding just what port 1 holds,
// so in the pause menu player 1's port read nothing, and the keyboard, which is controller 1, was never player 1's.
// Measured headless (a harness pad script given to ports 1 and 2): player 1's port 2 after the title, and the pause
// menu took no tab, no Start and no keys.
// Here a later port holding just what controller 1 holds does not become the active controller; controller 1 stays
// player 1. Two players' pads differ and are taken as before.
#include <cstdint>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
bool SameAsFirstPort(int port);  // src/common/menu_mouse.cpp
}

namespace {
constexpr uint32_t kPlayerPort = EON_VA(0x35B6EC, 0x35ABEC);
}

X86_EXTERN_FUNC(EON_ORIG(0005D860, 0005D590));
X86_FUNC(EON_FN(0005D860, 0005D590)) {
    const int port = int(ctx.esi);
    if (eon::SameAsFirstPort(port)) {
        static int told = 0;
        if (told++ < 3) LOGR("eon: controller %d holds exactly what controller 1 does: not made the active controller", port + 1);
        ctx.esp += 4;  // the return address
        return;
    }
    EON_ORIG(0005D860, 0005D590)(ctx, base);
}

// player 1's port, in the log when it changes (for reports)
X86_EXTERN_FUNC(EON_ORIG(0005DCD0, 0005DA00));
X86_FUNC(EON_FN(0005DCD0, 0005DA00)) {
    EON_ORIG(0005DCD0, 0005DA00)(ctx, base);
    static uint32_t last = 0xFFFFFFFE;
    const uint32_t now = kernel::R32(kPlayerPort);
    if (now != last && now < 4) LOGR("eon: player 1 is controller %u", now + 1);
    last = now;
}
