// Streamed loads that stopped for good in the driving missions (New Orleans, ND1b, before its end sequence).
//
// The track streams (data\track\stream\<map>.str, read by the asset manager's SushiLoader 0x47E10) go through the
// game's stream object ("STRM", 0x24A7C0): background reads fill a ring buffer (500 KB) with packets, each a header
// (+0 the block's id or -1 the wrap marker or -2 released, +4 the reader's id << 24 | its size) and its data; each
// reader (a record of the stream, the stream, its id, the bytes it has to read, its next packet) takes packets in turn
// (0x24AC20) and releases them (0x24B140), and the ring's read pointer (+0x60) only moves past released ones. When
// the ring has no room the reads pause (+0x48 = 2) until a release makes room.
// Harry's dumps at the stall: the ring full and paused, the loader's reader with nothing left to read and its next
// packet well past the read pointer, and two packets of that reader behind it never released (blocks 36d0 and 36df
// of an earlier request): the ring could not move past them, the reads never resumed, and the request the end
// sequence waited for (0x5DE) stayed pending. A reader never goes back, so a packet of its own between the read
// pointer and its next packet is abandoned. When a reader finds nothing to read while its stream is paused for room,
// those are released here with the game's own release, which also resumes the reads.
#include <cstdint>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kStreamMagic = 0x4D525453;  // "STRM"
constexpr uint32_t kRelease = 0x24B140;         // cdecl (reader, packet)
constexpr uint32_t kWrap = 0xFFFFFFFF, kReleased = 0xFFFFFFFE;
}  // namespace

X86_EXTERN_FUNC(__imp__sub_0024AC20);
X86_FUNC(sub_0024AC20) {
    const uint32_t reader = kernel::R32(ctx.esp + 4);  // cdecl: the reader's record
    __imp__sub_0024AC20(ctx, base);
    if (ctx.eax || !reader) return;
    const uint32_t stream = kernel::R32(reader);
    if (!stream || kernel::R32(stream) != kStreamMagic || kernel::R32(stream + 0x48) != 2) return;
    const uint32_t id = kernel::R32(reader + 4), next = kernel::R32(reader + 0xC);
    const uint32_t ring = kernel::R32(stream + 0x40), ring_end = kernel::R32(stream + 0x44);
    uint32_t p = kernel::R32(stream + 0x60), released = 0;
    for (int n = 0; n < 4096 && p != next; n++) {
        if (p < ring || p + 8 > ring_end) break;
        const uint32_t head = kernel::R32(p), info = kernel::R32(p + 4), size = info & 0xFFFFFF;
        if (head == kWrap) {
            p = ring;
            continue;
        }
        if (!size) break;
        if (head != kReleased && info >> 24 == id) {
            const uint32_t eax = ctx.eax;
            kernel::CallGuest(ctx, kRelease, {reader, p});
            ctx.eax = eax;
            released++;
        }
        p += size;
    }
    if (released) {
        static uint32_t said = 0;
        if (said++ < 8)
            LOGR("eon: %u abandoned stream packet%s of reader %u released (stream %08X): its reads resume", released,
                 released == 1 ? "" : "s", id, stream);
    }
}
