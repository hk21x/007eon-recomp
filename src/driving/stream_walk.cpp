// driving.xbe's streams ('STRM', 0x4D525453): 0x24AC20 takes a reader's next packet. It updates the counts inside the
// stream's critical section (0x252840 and 0x252850 enter and leave it), then walks the packet chain to the reader's
// next one outside it, while the writer fills packets and other readers free theirs. On the console's one CPU that
// walk was never cut into; here a player's driving mission stopped 2 s in on a read of 0x8410921E, a packet size half
// written. The whole call runs inside the section (it is recursive: the original's own enter and leave nest inside).
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kStreamMagic = 0x4D525453;  // 'STRM'
constexpr uint32_t kEnterStream = 0x252840;    // (stream + 4): RtlEnterCriticalSection(stream + 8)
constexpr uint32_t kLeaveStream = 0x252850;    // (stream + 4): RtlLeaveCriticalSection(stream + 8)
}  // namespace

X86_EXTERN_FUNC(__imp__sub_0024AC20);
X86_FUNC(sub_0024AC20) {
    // __cdecl (reader): the reader's first word is its stream
    uint32_t reader = kernel::R32(ctx.esp + 4);
    uint32_t stream = reader ? kernel::R32(reader) : 0;
    bool locked = stream && kernel::R32(stream) == kStreamMagic;
    if (locked) kernel::CallGuest(ctx, kEnterStream, {stream + 4});
    __imp__sub_0024AC20(ctx, base);
    if (locked) kernel::CallGuest(ctx, kLeaveStream, {stream + 4});
}
