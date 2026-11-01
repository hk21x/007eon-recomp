// default.xbe's streams ('STRM', 0x4D525453): 0x7C960 takes a reader's next packet, the walk to the next one outside the
// stream's critical section (0x7F560 and 0x7F570 enter and leave it). The same code as driving.xbe's 0x24AC20, where it
// crashed a player's driving mission (src/driving/stream_walk.cpp has the account): the whole call runs inside it.
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"
#include "common/stream_next.h"

namespace {
constexpr uint32_t kEnterStream = EON_VA(0x7F560, 0x7F290);  // (stream + 4): RtlEnterCriticalSection(stream + 8)
constexpr uint32_t kLeaveStream = EON_VA(0x7F570, 0x7F2A0);  // (stream + 4): RtlLeaveCriticalSection(stream + 8)
}  // namespace

X86_FUNC(EON_FN(0007C960, 0007C690)) {
    eon::TakeNextPacket(ctx, kEnterStream, kLeaveStream);
}
