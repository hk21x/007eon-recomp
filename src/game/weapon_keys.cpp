// Free aim's number keys: 1 to 6 pick Bond's weapons as the weapon cycle orders them, X the one held before.
//
// His inventory ([Bond +0x274]): +0 an array of 12 byte entries, the item first, +4 their count, +0x10 the item held,
// +0x18 the one selected. An item is a weapon with +0x328 bit 0x100 (0x1A8D10); the cycle (CYCLEWEAPONSFORWARD, the
// d-pad's down: 0x152070) walks the array in order, skips an item its vtable +0x12C (the inventory's +0x1C bit 0)
// answers true for, and one sharing the held item's +0x294 (the same weapon in another mode). Its switch, done here
// at once: the selection (0x1AA2B0, thiscall on the inventory), the switch with its animation (0x14D6F0, thiscall on
// Bond (item, 0, 0): state 0x3CB; it refuses while the gun is busy, a reload say), the item and time it keeps
// (+0x1474 through 0x25E40, +0x146C the clock 0x3210B0). Not while the cycle itself would not run (+0x1550 0x400000).
// The d-pad the number keys are also bound to is let go in src/common/menu_mouse.cpp.
//
// EON_WEAPON_TEST (headless): a file named weapon in the run folder holds the key, 1 to 6 or x.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eon {
extern bool free_aim;  // the setting (src/common/game_info.cpp)
}

namespace {

constexpr uint32_t kSelect = EON_VA(0x1AA2B0, 0x1A9AE0);  // thiscall on the inventory (item)
constexpr uint32_t kSwitch = EON_VA(0x14D6F0, 0x14CF60);  // thiscall on Bond (item, 0, 0)
constexpr uint32_t kKeep = EON_VA(0x25E40, 0x25B70);      // thiscall on a kept pointer (item)
constexpr uint32_t kClock = EON_VA(0x3210B0, 0x3205A8);
constexpr int kKeys = 6;

uint32_t Call(X86Context& ctx, uint32_t self, uint32_t fn, std::initializer_list<uint32_t> args) {
    uint32_t keep = ctx.ecx;
    ctx.ecx = self;
    uint32_t r = kernel::CallGuest(ctx, fn, args);
    ctx.ecx = keep;
    return r;
}

// the key pressed now: 1 to kKeys, 0 for the last weapon, -1 none
int Pressed() {
    static const bool test = std::getenv("EON_WEAPON_TEST") != nullptr;
    if (test) {
        std::FILE* f = std::fopen("weapon", "rb");
        if (!f) return -1;
        int c = std::fgetc(f);
        std::fclose(f);
        std::error_code ec;
        std::filesystem::remove("weapon", ec);
        return c == 'x' ? 0 : (c >= '1' && c < '1' + kKeys ? c - '0' : -1);
    }
#ifdef _WIN32
    static bool was[kKeys + 1] = {};
    int got = -1;
    for (int i = 0; i <= kKeys; i++) {
        bool down = (GetAsyncKeyState(i ? '0' + i : 'X') & 0x8000) != 0;
        if (down && !was[i]) got = i;
        was[i] = down;
    }
    return input::MouseLooking() ? got : -1;
#else
    return -1;
#endif
}

// his weapons in the cycle's order, one an item kind
std::vector<uint32_t> Weapons(X86Context& ctx, uint32_t inv) {
    std::vector<uint32_t> out;
    std::vector<uint16_t> kinds;
    uint32_t list = kernel::R32(inv), count = kernel::R32(inv + 4);
    uint32_t off = kernel::R32(inv + 0x1C) & 1;
    for (uint32_t i = 0; i < count && i < 64; i++) {
        uint32_t item = kernel::R32(list + 12 * i);
        if (!item || !(kernel::R32(item + 0x328) & 0x100)) continue;
        if (Call(ctx, item, kernel::R32(kernel::R32(item) + 0x12C), {off}) & 0xFF) continue;
        uint16_t kind = kernel::G<uint16_t>(item + 0x294);
        bool seen = false;
        for (uint16_t k : kinds) seen |= k == kind;
        if (seen) continue;
        kinds.push_back(kind);
        out.push_back(item);
    }
    return out;
}

}  // namespace

namespace eon {
// from Bond's update (src/game/free_aim.cpp), player 1's, after the game's: plays when free aim plays on foot
void WeaponKeys(X86Context& ctx, uint32_t bond, bool plays) {
    static uint32_t held_was = 0, before = 0;
    uint32_t inv = kernel::R32(bond + 0x274);
    if (!inv) return;
    uint32_t held = kernel::R32(inv + 0x10);
    if (held != held_was) {  // the one held before, for X
        if (held_was && (kernel::R32(held_was + 0x328) & 0x100)) before = held_was;
        held_was = held;
    }
    int key = Pressed();
    if (key < 0 || !plays || !eon::free_aim || (kernel::R32(bond + 0x1550) & 0x400000)) return;
    std::vector<uint32_t> weapons = Weapons(ctx, inv);
    uint32_t want = 0;
    if (key == 0) {
        for (uint32_t w : weapons) want = w == before ? w : want;
    } else if (key <= int(weapons.size())) {
        want = weapons[key - 1];
    }
    if (!want || want == held) return;
    Call(ctx, inv, kSelect, {want});
    bool ok = (Call(ctx, bond, kSwitch, {want, 0, 0}) & 0xFF) != 0;
    Call(ctx, bond + 0x1474, kKeep, {want});
    kernel::W32(bond + 0x146C, kernel::R32(kClock));
    static int said = 0;
    if (said++ < 20)
        LOGR("eon: weapon keys: %s to weapon %d of %zu (%08X)%s", key ? "key" : "X", key ? key : -1, weapons.size(), want,
             ok ? "" : ", selected (the switch refused: busy)");
}
}  // namespace eon
