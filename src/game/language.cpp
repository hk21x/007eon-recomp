// The language setting on foot (src/common/language.cpp): where the language is set, and the game's question at first
// boot (0x1C9D0: which language, and whether to ask with its language screen) answered with no screen.
#include "common/language.h"
#include "common/release.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kCount = EON_VA(0xC11A0, 0xC0D40), kName = EON_VA(0xC11C0, 0xC0D60);
constexpr uint32_t kUi = EON_VA(0xC0800, 0XC03A0);  // the UI object, as 0x1C9D0 gets it
}

// thiscall (name, 0 the first on the disc's list)
X86_EXTERN_FUNC(EON_ORIG(000C18C0, 000C1460));
X86_FUNC(EON_FN(000C18C0, 000C1460)) {
    if (uint32_t chosen = eon::ChosenLanguage(ctx, kCount, kName)) kernel::W32(ctx.esp + 4, chosen);
    EON_ORIG(000C18C0, 000C1460)(ctx, base);
}

// cdecl (name out): answers whether to ask
X86_EXTERN_FUNC(EON_ORIG(0001C9D0, 0001C790));
X86_FUNC(EON_FN(0001C9D0, 0001C790)) {
    uint32_t keep = ctx.ecx;
    ctx.ecx = kernel::CallGuest(ctx, kUi, {});
    uint32_t chosen = eon::ChosenLanguage(ctx, kCount, kName);
    ctx.ecx = keep;
    if (!chosen) return EON_ORIG(0001C9D0, 0001C790)(ctx, base);
    kernel::W32(kernel::R32(ctx.esp + 4), chosen);
    kernel::Return(ctx, 0, 0);
}
