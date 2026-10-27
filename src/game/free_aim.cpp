// Free aim (work in progress): Bond's shots with no lock go down the middle of the view, as the scope's do.
//
// A gun's shot (0x180CB0) takes its start from its owner's muzzle point (character vtable +0x1A4) and its direction
// from the owner's aim (+0x1A8, an orientation as the camera's angles, with its matrix). Bond's class (vtable
// 0x2E33B8) has its own: the aim 0x153CD0 is the camera's angles (the camera info's +0x10, 0x8D960) in the scope's
// view, the locked target (0x14E480) while locked on (0x151EC0), else his aim pose (+0x12B0); the muzzle 0x153BE0 is
// the camera's position in the scope's view, else his chest (0x162350). His camera is the camera manager's
// (0x3210C0: 0x8D960 with the player index at +0x1130): position +0xC0, pitch and yaw in degrees +0xD0 and +0xD4.
// Here, with no lock, the aim is the camera's as in the scope, and the shot starts on the line through the middle of
// the view, level with Bond, so nothing between the camera and him is hit.
#include <cmath>
#include <cstdlib>

#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {

constexpr uint32_t kCameras = 0x3210C0;
constexpr uint32_t kCameraBytes = 0x240;
constexpr uint32_t kPlayerIndex = 0x1130;
constexpr uint32_t kPosition = 0xB0;
constexpr uint32_t kLockedOn = 0x151EC0;   // thiscall on Bond
constexpr uint32_t kAnglesToMatrix = 0xF0A90;  // cdecl (angles, matrix out)

uint32_t Camera(uint32_t bond) {
    uint32_t cams = kernel::R32(kCameras);
    uint32_t list = cams ? kernel::R32(cams + 4) : 0;
    return list ? list + kernel::R32(bond + kPlayerIndex) * kCameraBytes : 0;
}

// work in progress: off unless EON_FREE_AIM=1 (the shots do not follow the camera yet)
bool FreeAim(X86Context& ctx, uint32_t bond) {
    static const bool on = [] {
        const char* e = std::getenv("EON_FREE_AIM");
        return e && *e == '1';
    }();
    if (!on || !Camera(bond)) return false;
    ctx.ecx = bond;
    bool locked = (kernel::CallGuest(ctx, kLockedOn, {}) & 0xFF) != 0;
    ctx.ecx = bond;
    return !locked;
}

}  // namespace

// thiscall (angles out, matrix out or 0)
X86_EXTERN_FUNC(__imp__sub_00153CD0);
X86_FUNC(sub_00153CD0) {
    uint32_t bond = ctx.ecx, angles = kernel::R32(ctx.esp + 4), matrix = kernel::R32(ctx.esp + 8);
    if (!FreeAim(ctx, bond)) return __imp__sub_00153CD0(ctx, base);
    // as the scope's view: the camera's angles, and their matrix
    uint32_t cam = Camera(bond);
    for (uint32_t i = 0; i < 16; i += 4) kernel::W32(angles + i, kernel::R32(cam + 0xD0 + i));
    if (matrix) kernel::CallGuest(ctx, kAnglesToMatrix, {angles, matrix});
    kernel::Return(ctx, 0, 2);
}

// thiscall (point out)
X86_EXTERN_FUNC(__imp__sub_00153BE0);
X86_FUNC(sub_00153BE0) {
    uint32_t bond = ctx.ecx, out = kernel::R32(ctx.esp + 4);
    if (!FreeAim(ctx, bond)) return __imp__sub_00153BE0(ctx, base);
    uint32_t cam = Camera(bond);
    constexpr float kRad = 3.14159265f / 180;
    float pitch = kernel::G<float>(cam + 0xD0) * kRad, yaw = kernel::G<float>(cam + 0xD4) * kRad;
    float f[3] = {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), -std::sin(pitch)};
    float c[3] = {kernel::G<float>(cam + 0xC0), kernel::G<float>(cam + 0xC4), kernel::G<float>(cam + 0xC8)};
    float b[3] = {kernel::G<float>(bond + kPosition), kernel::G<float>(bond + kPosition + 4),
                  kernel::G<float>(bond + kPosition + 8)};
    float along = (b[0] - c[0]) * f[0] + (b[1] - c[1]) * f[1] + (b[2] - c[2]) * f[2];
    for (int i = 0; i < 3; i++) kernel::G<float>(out + 4 * i) = c[i] + f[i] * along;
    kernel::G<float>(out + 12) = 1;
    kernel::Return(ctx, 0, 1);
}

// thiscall: Bond has an aim for his shots unless his flags +0x1550 hold 0x100, 0x800 or 0x1000 (0x800 while he
// stands or runs without a lock): in free aim always
X86_EXTERN_FUNC(__imp__sub_001493A0);
X86_FUNC(sub_001493A0) {
    uint32_t bond = ctx.ecx;
    if (!FreeAim(ctx, bond)) return __imp__sub_001493A0(ctx, base);
    kernel::Return(ctx, 1, 0);
}
