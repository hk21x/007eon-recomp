// The tanks' gun camera turned by the mouse directly (the rail tank of A Show of Force, the tank), as the camera on foot.
//
// Traced by a research agent: the tanks' aim camera ("AutoDrive", camera.ini's TankSAAutoDriveCam and the like; type
// 0x800) takes the sticks' CAMROTATE actions in 0xC82D0 into a rate (+0x344 across, +0x340 up and down: the stick's up
// stored negated), eases it (+0x324/+0x320, by the ini's inertia) with a ramp on top (+0x254/+0x250), and turns its yaw
// +0x24C and pitch +0x248 (in turns: 1.0 is 360 degrees) by it each frame in 0xCEE70 (fastcall, the camera; the only
// update of that type), which then holds them in the arm's limits and builds the view. The gun aims along the view and
// an aiming cursor's offset (+0x244/+0x240, moved by the stick within a dead zone while LT aims). So the stick's feel:
// a rate that eases and ramps. With the keyboard and mouse the mouse's turn (src/common/menu_mouse.cpp, degrees) is
// added to the yaw and pitch here, the rate, its easing, the ramp and the cursor cleared, and the update keeps the
// limits. The chase camera of every vehicle is another (0xD1590, src/driving/car_camera.cpp): in a car the mouse swings
// it round; the helicopter's mouse is still the right stick.
#include <atomic>

#include "core/input.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern std::atomic<float> drive_turn, drive_tilt;   // degrees waiting, right and up positive (menu_mouse.cpp)
extern std::atomic<int64_t> drive_aim_camera_ms;    // when this camera last took them
int64_t NowMs();
}

X86_EXTERN_FUNC(EON_ORIG(000CEE70, 000CEDE0));
X86_FUNC(EON_FN(000CEE70, 000CEDE0)) {
    uint32_t cam = ctx.ecx;
    if (input::UsingKeyboard()) {
        eon::drive_aim_camera_ms = eon::NowMs();
        float turn = eon::drive_turn.exchange(0), tilt = eon::drive_tilt.exchange(0);
        kernel::G<float>(cam + 0x24C) += turn / 360;  // the stick's right raises the yaw
        kernel::G<float>(cam + 0x248) -= tilt / 360;  // and its up (stored negated) lowers the pitch
        for (uint32_t at : {0x340u, 0x344u, 0x320u, 0x324u, 0x250u, 0x254u, 0x240u, 0x244u}) kernel::G<float>(cam + at) = 0;
    }
    EON_ORIG(000CEE70, 000CEDE0)(ctx, base);
}
