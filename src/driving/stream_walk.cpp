// driving.xbe's streams ('STRM', 0x4D525453): 0x24AC20 takes a reader's next packet. It updates the counts inside the
// stream's critical section (0x252840 and 0x252850 enter and leave it), then walks the packet chain to the reader's
// next one outside it, while the writer fills packets and other readers free theirs. On the console's one CPU that
// walk was never cut into; here a player's driving mission stopped 2 s in on a read of 0x8410921E, a packet size half
// written. The whole call runs inside the section (it is recursive: the original's own enter and leave nest inside).
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"
#include "common/stream_next.h"

namespace {
constexpr uint32_t kEnterStream = EON_VA(0x252840, 0x252800);  // (stream + 4): RtlEnterCriticalSection(stream + 8)
constexpr uint32_t kLeaveStream = EON_VA(0x252850, 0x252810);  // (stream + 4): RtlLeaveCriticalSection(stream + 8)
}  // namespace

X86_FUNC(EON_FN(0024AC20, 0024ABE0)) {
    eon::TakeNextPacket(ctx, kEnterStream, kLeaveStream);
}
