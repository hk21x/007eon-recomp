// default.xbe's streams ('STRM', 0x4D525453): 0x7C960 takes a reader's next packet, the walk to the next one outside the
// stream's critical section (0x7F560 and 0x7F570 enter and leave it). The same code as driving.xbe's 0x24AC20, where it
// crashed a player's driving mission (src/driving/stream_walk.cpp has the account): the whole call runs inside it.
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kStreamMagic = 0x4D525453;  // 'STRM'
constexpr uint32_t kEnterStream = 0x7F560;     // (stream + 4): RtlEnterCriticalSection(stream + 8)
constexpr uint32_t kLeaveStream = 0x7F570;     // (stream + 4): RtlLeaveCriticalSection(stream + 8)
}  // namespace

X86_EXTERN_FUNC(__imp__sub_0007C960);
X86_FUNC(sub_0007C960) {
    // __cdecl (reader): the reader's first word is its stream
    uint32_t reader = kernel::R32(ctx.esp + 4);
    uint32_t stream = reader ? kernel::R32(reader) : 0;
    bool locked = stream && kernel::R32(stream) == kStreamMagic;
    if (locked) kernel::CallGuest(ctx, kEnterStream, {stream + 4});
    __imp__sub_0007C960(ctx, base);
    if (locked) kernel::CallGuest(ctx, kLeaveStream, {stream + 4});
}
