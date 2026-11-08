// Mouse look on foot: the mouse turns the camera itself, as a PC third person game's does.
//
// The camera in play (class 0x2E26BC) orbits Bond in its update 0x1D1C70 (thiscall, the seconds on the stack): his
// position +0x70, the camera's +0x80, its height angle +0xD0 (degrees, clamped there), the view's heading +0x94
// (degrees; turning right lowers it). The right stick sets the speed
// the orbit turns at (+0xCC, the height +0xC8), and each eases toward the stick's, so a turn runs on after the stick is
// let go; fed to the stick, the mouse spun the view. Here a mouse count turns the orbit and the height by a fixed
// angle before the update, and the stick's speeds are left to the controller and keys. The update also swings the
// camera behind Bond as he moves (a timer at +0xC0 starts it, +0xC4 holds it, +0xB8 is the angle left); with
// camera_follow off that is held back while the mouse plays, and the camera moves with Bond (src/game/free_aim.cpp), so
// his sidestep does not turn the view; nor does the update's easing of the height back to rest while he moves. The recentre button's swing (+0xBC) still runs.
//
// The update ignores the stick while Bond is locked on, aiming a gadget and the like (the checks below are its own):
// then the right stick moves the lock's dot, so the mouse goes to the stick (src/game/mouse.cpp) instead.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include "core/input.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern bool camera_follow;                  // the setting (src/common/game_info.cpp)
std::atomic<float> mouse_turn{0}, mouse_tilt{0};  // degrees waiting for the camera (src/game/mouse.cpp)
std::atomic<int64_t> camera_takes_mouse_ms{0};    // when the camera last took the mouse
void FreeAimCamera(uint32_t mode, bool keep);  // src/game/free_aim.cpp: the boom moved with Bond
}

namespace {

constexpr uint32_t kFollowCamera = EON_VA(0x2E26BC, 0x2E1BE4);  // the camera mode's vtable in play
constexpr uint32_t kPlayers = EON_VA(0x3208A8, 0x31FDA0);      // 0x146430's object: a mode's player (+0x44) to Bond
constexpr uint32_t kBondOf = EON_VA(0x146430, 0x145CA0);       // thiscall (player)
constexpr uint32_t kState = EON_VA(0x147BF0, 0x147460);        // thiscall on Bond (0): his state, its +0x24
constexpr uint32_t kAiming = EON_VA(0x148EF0, 0x148760);       // thiscall on Bond
constexpr uint32_t kClock = EON_VA(0x3210B4, 0x3205AC);        // the game's seconds
constexpr uint32_t kPlayerIndex = 0x1130;                      // Bond: 0 for player 1
constexpr uint32_t kTarget = 0x70, kPosition = 0x80, kViewYaw = 0x94, kSwing = 0xB8, kSwingButton = 0xBC,
                   kSwingTimer = 0xC0, kSwinging = 0xC4, kTilt = 0xD0;
constexpr float kRadians = 3.14159265f / 180;
constexpr float kTiltLimit = 70, kTiltBias = 15;  // the game's height limit (0x31C5B0), the correction's

// the height the mouse holds while it plays (below), and the correction for the game's easing back to rest
float g_tilt_want = 0, g_tilt_bias = 0;
bool g_tilt_held = false;

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

uint32_t Ask(X86Context& ctx, uint32_t self, uint32_t fn, std::initializer_list<uint32_t> args) {
    uint32_t keep = ctx.ecx;
    ctx.ecx = self;
    uint32_t r = kernel::CallGuest(ctx, fn, args);
    ctx.ecx = keep;
    return r;
}

// the update's own reasons to ignore the stick (0x1D1D5D on): two states, a third for 2 seconds from the mode's +0x1B0,
// aiming, Bond's +0xD04 and +0x10F3
bool IgnoresStick(X86Context& ctx, uint32_t mode, uint32_t bond) {
    uint32_t state = kernel::R32(Ask(ctx, bond, kState, {0}) + 0x24);
    if (state == 0x3CF || state == 0x3D0) return true;
    if (state == 0x3BD && kernel::G<float>(kClock) - kernel::G<float>(mode + 0x1B0) < 2.0f) return true;
    return (Ask(ctx, bond, kAiming, {}) & 0xFF) || kernel::R32(bond + 0xD04) || kernel::G<uint8_t>(bond + 0x10F3);
}

}  // namespace

X86_EXTERN_FUNC(EON_ORIG(001D1C70, 001D14A0));
X86_FUNC(EON_FN(001D1C70, 001D14A0)) {
    uint32_t mode = ctx.ecx;
    bool keep = false;  // the mouse plays and the camera does not follow Bond's moves
    uint32_t bond = kernel::R32(mode) == kFollowCamera
                        ? Ask(ctx, kernel::R32(kPlayers), kBondOf, {kernel::R32(mode + 0x44)}) : 0;
    if (bond && kernel::R32(bond + kPlayerIndex) == 0 && input::UsingKeyboard() && !IgnoresStick(ctx, mode, bond)) {
        eon::camera_takes_mouse_ms = NowMs();
        keep = !eon::camera_follow;
        float turn = eon::mouse_turn.exchange(0), tilt = eon::mouse_tilt.exchange(0);
        if (turn != 0) {  // the orbit about Bond, as the stick's turn (0x1D2E50) rotates it
            float c = std::cos(turn * kRadians), s = std::sin(turn * kRadians);
            float x = kernel::G<float>(mode + kPosition) - kernel::G<float>(mode + kTarget);
            float y = kernel::G<float>(mode + kPosition + 4) - kernel::G<float>(mode + kTarget + 4);
            kernel::G<float>(mode + kPosition) = kernel::G<float>(mode + kTarget) + x * c - y * s;
            kernel::G<float>(mode + kPosition + 4) = kernel::G<float>(mode + kTarget + 4) + x * s + y * c;
            // and the view's heading with it, as the stick's turn does (0x1D2E9F): left alone it eased after the orbit,
            // so a quick move trailed behind a slow one
            float yaw = kernel::G<float>(mode + kViewYaw) + turn;
            yaw -= 360 * std::floor((yaw + 180) / 360);
            kernel::G<float>(mode + kViewYaw) = yaw;
        }
        // the height: the game eases it back to rest while Bond moves and the stick's height is still (0x1D2DC0),
        // so with the mouse a strafe sank the view (Harry: "almost forces you to shoot their legs"). Held where the
        // mouse puts it: started each update from there plus a correction, which grows by what the update took off
        if (keep) {
            if (!g_tilt_held) {
                g_tilt_want = kernel::G<float>(mode + kTilt);
                g_tilt_bias = 0;
                g_tilt_held = true;
            }
            g_tilt_want = std::clamp(g_tilt_want + tilt, -kTiltLimit, kTiltLimit);
            kernel::G<float>(mode + kTilt) = g_tilt_want + g_tilt_bias;
        } else {
            kernel::G<float>(mode + kTilt) += tilt;
        }
        if (!eon::camera_follow && !kernel::G<uint8_t>(mode + kSwingButton)) {
            kernel::G<float>(mode + kSwingTimer) = 0;
            kernel::G<uint8_t>(mode + kSwinging) = 0;
            kernel::G<float>(mode + kSwing) = 0;
        }
    }
    if (kernel::R32(mode) == kFollowCamera) eon::FreeAimCamera(mode, keep);
    EON_ORIG(001D1C70, 001D14A0)(ctx, base);
    if (keep) {
        float err = g_tilt_want - kernel::G<float>(mode + kTilt);
        g_tilt_bias = std::clamp(g_tilt_bias + err, -kTiltBias, kTiltBias);
    } else if (bond) {
        g_tilt_held = false;
    }
}

// Wall cover's camera (0x1AECD0, thiscall, its mode class 0x2DCF7C): the right stick sets turn speeds (+0xD8 across,
// +0xD4 up and down) that add into offsets from the wall's direction, +0xE0 (degrees, within 80) and +0xDC (within 60),
// unless Bond is locked on (0x148EF0), +0xD04 or +0x10F3. There the mouse sets the offsets directly, as it turns the
// follow camera, and the speeds are held at 0; locked on, it moves the lock's aim (src/game/mouse.cpp)
X86_EXTERN_FUNC(EON_ORIG(001AECD0, 001AE500));
X86_FUNC(EON_FN(001AECD0, 001AE500)) {
    uint32_t mode = ctx.ecx;
    uint32_t bond = Ask(ctx, kernel::R32(kPlayers), kBondOf, {kernel::R32(mode + 0x44)});
    if (bond && kernel::R32(bond + kPlayerIndex) == 0 && input::UsingKeyboard() &&
        !(Ask(ctx, bond, kAiming, {}) & 0xFF) && !kernel::R32(bond + 0xD04) && !kernel::G<uint8_t>(bond + 0x10F3)) {
        eon::camera_takes_mouse_ms = NowMs();
        float turn = eon::mouse_turn.exchange(0), tilt = eon::mouse_tilt.exchange(0);
        kernel::G<float>(mode + 0xE0) = std::clamp(kernel::G<float>(mode + 0xE0) + turn, -80.0f, 80.0f);
        kernel::G<float>(mode + 0xDC) = std::clamp(kernel::G<float>(mode + 0xDC) + tilt, -60.0f, 60.0f);
        kernel::G<float>(mode + 0xD4) = 0;
        kernel::G<float>(mode + 0xD8) = 0;
    }
    EON_ORIG(001AECD0, 001AE500)(ctx, base);
}
