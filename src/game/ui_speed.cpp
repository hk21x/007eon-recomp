// The HUD's own animations at the console's speed (issue #28: Bond Sense's red rings too fast at 60).
//
// On foot the game asks for present interval 2 (its device setup 0x5FEC0: 0x35B7C0 = 2), so on the console it drew
// at most 29.97 frames a second; here it draws 60. Movement, the cameras, the menus' FEng packages (0x1323D0, given the
// frame's seconds through 0xC6BE0) and the front end's turning models follow the frame's time (0x1DAC0 measures it into
// 0x320990) and are right. Two parts of the HUD's widget library step by a constant each time they are drawn instead,
// tuned for 29.97 draws a second (the library knows the rate: the lock reticle's own turn, 0x260050, is seconds *
// 29.97 [0x2C3D00]):
// - 0x25FBD0 (fastcall, the widget): Bond Sense's rings round a point of interest. It draws rings from the size +0xCC
//   down by +0xD0, then adds +0xD4 to +0xCC, back by the spacing past +0xC8. Measured headless in the rappel (the
//   inventory open, so Bond Sense on): +0xD4 0.05 a draw, +0xD0 0.7, 60 draws a second, 4.0 to 4.5 rings started a
//   second (one each 14 draws); the console's 29.97 draws start 2.1.
// - 0xCA3A0 (fastcall, the widget): a widget's animation (state +0x78: fades, pulses back and forth, loops, moves) adds
//   its speed +0x74 to its progress +0x80 once a frame, also through the reticle 0x260050 and the widgets' updates.
//   State 1 takes +0x74 as a level, not a step, and is left alone.
// Each is given its step times the frame's seconds * 29.97 for the call (0x320990: real time, 1/60 to 0.1 s), and the
// step put back after (a pulse turning round negates it: the negated step is put back negated); so 30 frames a second
// (the frame rate cap) is the game's own step, 60 half of it. Measured with it, at 60: the rings 2.0 to 2.5 started a
// second, a pulsing HUD widget (vtable 0x2C49A0, state 3, speed 0.05) 1.51 progress a second (3.02 before; the
// console's 1.50); at a 30 cap the pulse 1.45. The front end's background movie is paced by XMV and was right.
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace {
constexpr uint32_t kFrameSeconds = EON_VA(0x320990, 0x31FE88);  // 0x1DAC0: the frame's real time, 1/60 to 0.1 s

// the step's share of the console's frame for this frame
float FrameShare() {
    const float dt = kernel::G<float>(kFrameSeconds);
    return dt > 0 && dt <= 0.1f ? dt * 29.97f : 1.0f;
}

// runs body with the float at va scaled for the frame, then puts the step back (negated if body negated it)
template <class F>
void Scaled(uint32_t va, F&& body) {
    const float kept = kernel::G<float>(va), scaled = kept * FrameShare();
    kernel::G<float>(va) = scaled;
    body();
    const float now = kernel::G<float>(va);
    if (now == scaled) kernel::G<float>(va) = kept;
    else if (now == -scaled) kernel::G<float>(va) = -kept;
}
}  // namespace

X86_EXTERN_FUNC(EON_ORIG(0025FBD0, 0025F400));
X86_FUNC(EON_FN(0025FBD0, 0025F400)) {
    Scaled(ctx.ecx + 0xD4, [&] { EON_ORIG(0025FBD0, 0025F400)(ctx, base); });
}

X86_EXTERN_FUNC(EON_ORIG(000CA3A0, 000C9F20));
X86_FUNC(EON_FN(000CA3A0, 000C9F20)) {
    const uint32_t state = kernel::R32(ctx.ecx + 0x78);
    if (state < 2 || state > 8) return EON_ORIG(000CA3A0, 000C9F20)(ctx, base);
    Scaled(ctx.ecx + 0x74, [&] { EON_ORIG(000CA3A0, 000C9F20)(ctx, base); });
}
