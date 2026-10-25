// Mouse aiming without the stick's limits.
//
// The game reads controller 1's sticks in 0x1BC90 (port and slot on the stack, the output in esi: four floats, left
// x and y, right x and y, each scaled to -1..1, squared by the response curve option 0x2FD334, pushed out on the
// diagonals and clamped), from its pad state through 0x5D210; 0x1C330 maps them to the game's actions. The mouse's
// movement (input::TakeMouse, a rate in full deflections) is added to the right stick there, after the curve and the
// clamp: the turn follows the mouse in proportion, a fast move turning further than the stick's full deflection.
// The game flips the stick's y (up is negative here).
#include "app/app.h"
#include "core/input.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

#include <cstring>

X86_EXTERN_FUNC(__imp__sub_0001BC90);
X86_FUNC(sub_0001BC90) {
    uint32_t port = kernel::R32(ctx.esp + 4), out = ctx.esi;
    __imp__sub_0001BC90(ctx, base);
    float x, y;
    if (port != 0 || !input::TakeMouse(x, y)) return;
    float rx, ry;
    std::memcpy(&rx, &kernel::G<uint32_t>(out + 8), 4);
    std::memcpy(&ry, &kernel::G<uint32_t>(out + 12), 4);
    rx += x;
    ry -= y;
    std::memcpy(&kernel::G<uint32_t>(out + 8), &rx, 4);
    std::memcpy(&kernel::G<uint32_t>(out + 12), &ry, 4);
}
