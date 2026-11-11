// A car's chase camera swung round it by the mouse (free look), easing back behind the car once the mouse rests.
//
// Traced by a research agent (2026-11-11): every vehicle's chase camera is camera.ini's "HeliCam" type (kind 2; the car
// of Serena's mission CayenneHeliCam0), updated each frame by 0xD1590 (fastcall, the camera; the ini loader 0xD3400
// stores it in the camera's descriptor). Its arm, the camera's place in the car's own frame (Heli_Sideways, Heli_Height,
// Heli_Distance: x, y, z, the distance negative behind), is read at the update's start from arm +0x20:
//   desc = [0x32A7E0] + (short)[cam +0x170] * 0x24, cfg = [0x32A7DC] + (short)[desc +0xA] * 0x60,
//   arm  = [cfg +0x38] + (char)[cam +0x2F6] * 0x50
// and the game eases its eye offset (cam +0x3A0) toward it at Min_Rate to Max_Rate by speed, the eye (+0xB0) = the car's
// anchor (+0x140) + that, collision checked, the view built from it. So the arm turned before the update (and put back
// after: it is the ini's, shared) swings the camera round the car, the game's own easing smoothing it; measured headless
// by poking the arm (60, -90 and 45 degrees: the camera round the car at each). A turn of t degrees: x' = x cos t -
// z sin t, z' = x sin t + z cos t, t > 0 the camera to the car's left (the view turning right). Pitch turns (z, y) the
// same way, p > 0 raising the camera (the view looking down).
// Look back (the right stick down, 0xBCF00: [[cam +0x404] +0x21] = 1) negates the arm's z at 0xD170D: turned, it came out
// mirrored, so while it is held the free look is let go.
// The tanks' gun camera is another (src/driving/aim_camera.cpp); the mouse reaches this only in a car (menu_mouse.cpp).
//
// EON_CAR_LOOK_TEST (headless): a file named carlook in the run folder holds "yaw pitch" in degrees, held while it is there.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern std::atomic<float> car_look_yaw, car_look_pitch;  // degrees, right and down positive (menu_mouse.cpp)
extern std::atomic<int64_t> car_look_ms;
int64_t NowMs();
}

namespace {
constexpr uint32_t kDescs = EON_VA(0x32A7E0, 0x32A7C0), kCfgs = EON_VA(0x32A7DC, 0x32A7BC);
constexpr int64_t kRest = 1000;   // ms of a resting mouse before the camera eases back
constexpr float kBack = 0.08f;    // of the angle taken off each frame while easing back (about 0.5 s at 60)
constexpr float kPi = 3.14159265f;

// the camera's arm vector (x, y, z at arm +0x20), 0 when it cannot be found
uint32_t Arm(uint32_t cam) {
    uint32_t descs = kernel::R32(kDescs), cfgs = kernel::R32(kCfgs);
    if (!descs || !cfgs) return 0;
    uint32_t desc = descs + uint32_t(int32_t(kernel::G<int16_t>(cam + 0x170)) * 0x24);
    if (!mem::IsCommitted(desc + 0xC)) return 0;
    uint32_t cfg = cfgs + uint32_t(int32_t(kernel::G<int16_t>(desc + 0xA)) * 0x60);
    if (!mem::IsCommitted(cfg + 0x3C)) return 0;
    uint32_t arms = kernel::R32(cfg + 0x38);
    if (!arms) return 0;
    uint32_t arm = arms + uint32_t(int32_t(kernel::G<int8_t>(cam + 0x2F6)) * 0x50);
    return mem::IsCommitted(arm + 0x2C) ? arm + 0x20 : 0;
}

bool LookingBack(uint32_t cam) {
    uint32_t state = kernel::R32(cam + 0x404);
    return state && mem::IsCommitted(state + 0x22) && kernel::G<uint8_t>(state + 0x21) == 1;
}
}  // namespace

X86_EXTERN_FUNC(EON_ORIG(000D1590, 000D1500));
X86_FUNC(EON_FN(000D1590, 000D1500)) {
    uint32_t cam = ctx.ecx;
    const int64_t now = eon::NowMs();
    static const bool test = std::getenv("EON_CAR_LOOK_TEST") != nullptr;
    if (test) {
        if (std::FILE* f = std::fopen("carlook", "rb")) {
            float y = 0, p = 0;
            if (std::fscanf(f, "%f %f", &y, &p) == 2) eon::car_look_yaw = y, eon::car_look_pitch = p, eon::car_look_ms = now;
            std::fclose(f);
        }
    }
    // let go while looking back, and eased back once the mouse rests
    if (LookingBack(cam)) {
        eon::car_look_yaw = 0;
        eon::car_look_pitch = 0;
    } else if (now - eon::car_look_ms.load() > kRest) {
        float y = eon::car_look_yaw.load(), p = eon::car_look_pitch.load();
        eon::car_look_yaw = std::fabs(y) < 0.05f ? 0.0f : y * (1 - kBack);
        eon::car_look_pitch = std::fabs(p) < 0.05f ? 0.0f : p * (1 - kBack);
    }
    const float yaw = eon::car_look_yaw.load(), pitch = eon::car_look_pitch.load();
    uint32_t arm = (yaw != 0 || pitch != 0) ? Arm(cam) : 0;
    float kept[3] = {};
    if (arm) {
        for (int i = 0; i < 3; i++) kept[i] = kernel::G<float>(arm + uint32_t(i) * 4);
        float x = kept[0], y = kept[1], z = kept[2];
        const float p = pitch * kPi / 180, t = yaw * kPi / 180;
        const float z1 = z * std::cos(p) + y * std::sin(p), y1 = y * std::cos(p) - z * std::sin(p);  // raised: looks down
        const float x2 = x * std::cos(t) - z1 * std::sin(t), z2 = x * std::sin(t) + z1 * std::cos(t);  // round the car
        kernel::G<float>(arm) = x2;
        kernel::G<float>(arm + 4) = y1;
        kernel::G<float>(arm + 8) = z2;
    }
    EON_ORIG(000D1590, 000D1500)(ctx, base);
    if (arm)
        for (int i = 0; i < 3; i++) kernel::G<float>(arm + uint32_t(i) * 4) = kept[i];
}
