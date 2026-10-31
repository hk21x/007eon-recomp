// The language setting in the driving missions (src/common/language.cpp): the same set, by name, as on foot.
#include "common/language.h"
#include "common/release.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

// thiscall (name, 0 the first on the disc's list)
X86_EXTERN_FUNC(EON_ORIG(001F3310, 001F3270));
X86_FUNC(EON_FN(001F3310, 001F3270)) {
    constexpr uint32_t kCount = EON_VA(0x1ECAD0, 0X1ECA30), kName = EON_VA(0x1E8810, 0X1E8770);
    if (uint32_t chosen = eon::ChosenLanguage(ctx, kCount, kName)) kernel::W32(ctx.esp + 4, chosen);
    EON_ORIG(001F3310, 001F3270)(ctx, base);
}
