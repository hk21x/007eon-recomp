// A mission's intro movie can be skipped the first time too (issue #24: only once seen before).
//
// Traced by a research agent (2026-11-11): the level script's play movie command 0x1D020 (cdecl, the command; [cmd +4]
// its flags, [cmd +8] the movie's name) makes a mission movie (flags & 0x3000) unskippable while the profile says the
// mission was not played yet (0x88010 on [0x31E8D8]), unless its flags' bit 31 is set, the game's own skippable anyway;
// the skip (A, the movie's poll 0x245290) is then ignored. Set for the call and put back after, so the script's data and
// the save are unchanged. The boot logos carry bit 14, never skippable, and stay so (skip_intro hides them instead);
// the menus' and pause's background loops and the attract movies do not come here. Measured headless (first boot, the
// bit set in gdb): the new game's intro in1 ended at the next A, the level loaded after it.
#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

X86_EXTERN_FUNC(EON_ORIG(0001D020, 0001CDE0));
X86_FUNC(EON_FN(0001D020, 0001CDE0)) {
    const uint32_t cmd = kernel::R32(ctx.esp + 4);
    const uint32_t flags = cmd ? kernel::R32(cmd + 4) : 0;
    if (cmd) kernel::W32(cmd + 4, flags | 0x80000000u);
    EON_ORIG(0001D020, 0001CDE0)(ctx, base);
    if (cmd) kernel::W32(cmd + 4, flags);
}
