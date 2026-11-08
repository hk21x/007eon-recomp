// Mouse aiming without the stick's limits.
//
// The game reads controller 1's sticks in 0x1BC90 (port and slot on the stack, the output in esi: four floats, left
// x and y, right x and y, each scaled to -1..1, squared by the response curve option 0x2FD334, pushed out on the
// diagonals and clamped), from its pad state through 0x5D210; 0x1C330 maps them to the game's actions.
//
// While the camera takes the mouse (src/game/camera.cpp: on foot, not locked on), the mouse's counts turn it directly:
// kDegreesPerCount at 100% sensitivity. Otherwise (the lock's dot, the scope, mounted guns) its movement
// (input::TakeMouse, a rate in full deflections) is added to the right stick there, after the curve and the clamp,
// scaled by the aim sensitivity. While free aim aims (the right button) the camera's turn is scaled by its own setting. The game flips the stick's y (up is negative here).
#include "app/app.h"
#include "core/input.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include "common/release.h"

namespace eon {
extern uint32_t aim_sensitivity;                     // the setting, percent (src/common/game_info.cpp)
extern uint32_t free_aim_sensitivity;                // the camera's while free aim aims, percent (the same)
bool FreeAimAiming();                                // src/game/free_aim.cpp: the right button aims
extern std::atomic<bool> scoped;                     // src/game/free_aim.cpp: a scope is up
extern std::atomic<float> mouse_turn, mouse_tilt;   // src/game/camera.cpp
extern std::atomic<int64_t> camera_takes_mouse_ms;
}

namespace {

constexpr float kDegreesPerCount = 0.04f;  // at 100%: an 800 dpi mouse turns the view 32 degrees an inch
constexpr float kFullRate = 2400;          // input::TakeMouse's counts a second for a full deflection (sdk)

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// this poll's mouse: counts moved (the sensitivity in, up positive) and the same as a stick rate; false when it is not
// playing
bool Take(float& cx, float& cy, float& rx, float& ry) {
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::clamp(std::chrono::duration<float>(now - last).count(), 0.001f, 0.1f);
    last = now;
    if (!input::TakeMouse(rx, ry)) return false;
    cx = rx * kFullRate * dt;
    cy = ry * kFullRate * dt;
    return true;
}

}  // namespace

X86_EXTERN_FUNC(EON_ORIG(0001BC90, 0001BA50));
X86_FUNC(EON_FN(0001BC90, 0001BA50)) {
    uint32_t port = kernel::R32(ctx.esp + 4), out = ctx.esi;
    EON_ORIG(0001BC90, 0001BA50)(ctx, base);
    float cx, cy, x, y;
    if (port != 0 || !Take(cx, cy, x, y)) return;
    if (NowMs() - eon::camera_takes_mouse_ms < 250) {
        // while free aim aims, its own share of the sensitivity (Sensitivity while aiming)
        float k = eon::FreeAimAiming() ? float(eon::free_aim_sensitivity) / 100 : 1;
        eon::mouse_turn = eon::mouse_turn - cx * kDegreesPerCount * k;  // right lowers the heading, as the stick's turn does
        eon::mouse_tilt = eon::mouse_tilt + cy * kDegreesPerCount * k;
        return;
    }
    eon::mouse_turn = 0;  // nothing left over for when the camera takes it again
    eon::mouse_tilt = 0;
    float k = float(eon::aim_sensitivity) / 100;
    float rx, ry;
    std::memcpy(&rx, &kernel::G<uint32_t>(out + 8), 4);
    std::memcpy(&ry, &kernel::G<uint32_t>(out + 12), 4);
    rx += x * k;
    // in a scope the mouse's up and down the other way round: fed as elsewhere it was "inverted compared to normal
    // aiming" (Harry)
    ry += eon::scoped ? y * k : -y * k;
    std::memcpy(&kernel::G<uint32_t>(out + 8), &rx, 4);
    std::memcpy(&kernel::G<uint32_t>(out + 12), &ry, 4);
}
