// The game's Subtitles option kept from one start to the next (Harry: turned on, off again after a restart).
//
// Traced by a research agent (2026-11-11): the option is bit 9 of the profile's options ([[0x31E8CC] +0x44] +0xC0),
// copied to the subtitle manager's on flag (byte [[0x321230] +4], what a movie's subtitles ask: 0xC4480) by 0x23F10 at
// boot and at each level's start. It lives on disc only inside a save game (0x18750 packs it, 0x189C0 unpacks it at
// Load Game), and the front end starts from the defaults (0x1F790: off), so every start it read Off; a Load Game put
// the save's value over the menu's choice, and left the manager's flag as it was until a level started.
// Choosing a language (0x115240, 0x115280, at boot too) sets the bit (the setter 0x19C50) and the manager's flag (its
// setter 0xC3030) to the language's default, on only for Dutch, Italian and Swedish: found with a watchpoint, the kept
// On gone again between boot's two 0x23F10.
// Here the player's choice is kept in the port's settings (subtitles: 0 never chosen, 1 off, 2 on): put back after the
// defaults and a language's default (the setter's other callers), before 0x23F10 and after a Load Game (with the
// manager's flag); the Options menu's own change is the new choice, saved.
#include <cstdint>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace eon {
extern uint32_t subtitles;  // the setting (src/common/game_info.cpp)
}

namespace {
constexpr uint32_t kProfile = EON_VA(0x31E8CC, 0x31DE6C), kManager = EON_VA(0x321230, 0x320728);
constexpr uint32_t kBit = 1u << 9;

// the options word's address, 0 before the profile is there
uint32_t Options() {
    uint32_t p = kernel::R32(kProfile);
    if (!p || !mem::IsCommitted(p + 0x48)) return 0;
    uint32_t q = kernel::R32(p + 0x44);
    return q && mem::IsCommitted(q + 0xC4) ? q + 0xC0 : 0;
}

// the kept choice into the profile, and into the manager's flag when asked
void Apply(bool manager) {
    uint32_t at = Options();
    if (!at) return;
    if (eon::subtitles == 1 || eon::subtitles == 2) {
        uint32_t v = kernel::R32(at);
        kernel::W32(at, eon::subtitles == 2 ? v | kBit : v & ~kBit);
    }
    if (!manager) return;
    uint32_t m = kernel::R32(kManager);
    if (m && mem::IsCommitted(m + 8)) kernel::G<uint8_t>(m + 4) = (kernel::R32(at) & kBit) ? 1 : 0;
}
}  // namespace

// boot and each level's start: the profile's bit to the manager
X86_EXTERN_FUNC(EON_ORIG(00023F10, 00023CB0));
X86_FUNC(EON_FN(00023F10, 00023CB0)) {
    Apply(false);
    EON_ORIG(00023F10, 00023CB0)(ctx, base);
    static int told = 0;
    if (eon::subtitles && told++ < 2) {
        uint32_t m = kernel::R32(kManager);
        LOGR("eon: subtitles: the kept choice (%s) put into the game's options; its subtitles are %s",
             eon::subtitles == 2 ? "on" : "off", m && mem::IsCommitted(m + 8) && kernel::G<uint8_t>(m + 4) ? "on" : "off");
    }
}

// Load Game: the save's options over the profile's
X86_EXTERN_FUNC(EON_ORIG(000189C0, 00018780));
X86_FUNC(EON_FN(000189C0, 00018780)) {
    EON_ORIG(000189C0, 00018780)(ctx, base);
    Apply(true);
}

// the bit's setter. From the Options menu's Subtitles row (0x11EBF0, both the front end's and the pause menu's:
// measured headless, the return address each time) it is the player's choice, kept at once (the menu sets the manager's
// flag next). From anywhere else (the defaults, a language's default, a loaded save) the kept choice is put back
X86_EXTERN_FUNC(EON_ORIG(00019C50, 00019A10));
X86_FUNC(EON_FN(00019C50, 00019A10)) {
    constexpr uint32_t kFromMenu = EON_VA(0x11EC20, 0x11E520);
    const uint32_t from = kernel::R32(ctx.esp);
    EON_ORIG(00019C50, 00019A10)(ctx, base);
    const uint32_t at = Options();
    if (from == kFromMenu && at) {
        const uint32_t now = (kernel::R32(at) & kBit) ? 2 : 1;
        if (now != eon::subtitles) {
            eon::subtitles = now;
            app::Save();
            LOGR("eon: subtitles turned %s in the game's options: kept for the next start", now == 2 ? "on" : "off");
        }
    } else {
        Apply(false);
    }
}

// the manager's flag's setter: a kept choice stands whatever it is given
X86_EXTERN_FUNC(EON_ORIG(000C3030, 000C2BD0));
X86_FUNC(EON_FN(000C3030, 000C2BD0)) {
    EON_ORIG(000C3030, 000C2BD0)(ctx, base);
    if (eon::subtitles == 1 || eon::subtitles == 2) {
        uint32_t m = kernel::R32(kManager);
        if (m && mem::IsCommitted(m + 8)) kernel::G<uint8_t>(m + 4) = eon::subtitles == 2 ? 1 : 0;
    }
}
