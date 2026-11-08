// Bond on foot in a driving mission (Train Chase, on the train): W and S walk him, as A and D already sidestep.
//
// driving.xbe's character (vtable 0x2C8948, made only in a mission with an on-foot part; traced by a research agent)
// reads his own queue of the driving actions each frame in 0x1878E0 (fastcall, the character): the left stick's x
// (STEER) and y (STEERVERTICAL) are his walk, so the driving keys' W and S (the triggers: gas and brake) never reached
// him. His state +0x2C is 0x1B to 0x27 in a vehicle (0x18A9C0 tests it so), anything else on foot. While he is on foot
// the keys bound to gas and brake also push the stick up and down (src/common/menu_mouse.cpp).
#include <atomic>
#include <chrono>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern std::atomic<int64_t> driving_on_foot_ms;  // src/common/menu_mouse.cpp
}

X86_EXTERN_FUNC(EON_ORIG(001878E0, 00187840));
X86_FUNC(EON_FN(001878E0, 00187840)) {
    uint32_t state = kernel::R32(ctx.ecx + 0x2C);
    if (state < 0x1B || state > 0x27) {
        using namespace std::chrono;
        eon::driving_on_foot_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    EON_ORIG(001878E0, 00187840)(ctx, base);
}
