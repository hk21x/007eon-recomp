#pragma once
// A 'STRM' stream's "take a reader's next packet" (default.xbe 0x7C960, driving.xbe 0x24AC20), the game's code run whole
// inside the stream's critical section, each packet's address checked. Other code changes the chain outside that section
// (the flush 0x24B1D0 frees packets with no lock), so a header can be half written or stale, and its size sends the walk
// out of memory: two of a player's driving missions stopped so, reading 0x8412C31C and 0x84B6BE2B. A bad packet ends the
// reader's stream (the reader has nothing left) in place of the crash.
#include <cstdint>

#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace eon {

inline bool PacketOk(uint32_t packet) {
    return mem::IsCommitted(packet) && mem::IsCommitted(packet + 7);
}

// __cdecl (reader): returns the packet taken, 0 for none
inline void TakeNextPacket(X86Context& ctx, uint32_t enter, uint32_t leave) {
    constexpr uint32_t kMagic = 0x4D525453;  // 'STRM'
    constexpr int kMaxPackets = 1 << 20;     // a chain that never reaches the reader's kind
    uint32_t reader = kernel::R32(ctx.esp + 4);
    uint32_t stream = reader && mem::IsCommitted(reader) ? kernel::R32(reader) : 0;
    if (!stream || !mem::IsCommitted(stream) || kernel::R32(stream) != kMagic || !kernel::R32(reader + 8)) {
        ctx.eax = 0;
        ctx.esp += 4;
        return;
    }
    auto bad = [&](uint32_t packet) {
        static bool told = false;
        if (!told) LOGR("eon: a stream's packet chain is damaged at %08X: that reader's stream ends", packet);
        told = true;
        kernel::W32(reader + 8, 0);
    };
    kernel::CallGuest(ctx, enter, {stream + 4});
    uint32_t taken = kernel::R32(reader + 12), result = taken;
    uint32_t size = PacketOk(taken) ? kernel::R32(taken + 4) & 0xFFFFFF : 0;
    if (!size || size > kernel::R32(reader + 8) || !PacketOk(taken + size)) {
        // not to be handed on: what frees a packet follows its size too
        bad(taken);
        result = 0;
    } else {
        kernel::W32(taken + 4, size);
        int32_t left = int32_t(kernel::R32(reader + 8) - size);
        kernel::W32(reader + 8, uint32_t(left));
        if (left > 0) {
            uint32_t want = kernel::R32(reader + 4) << 24, packet = taken + size;
            int walked = 0;
            for (;; walked++) {
                if (walked == kMaxPackets || !PacketOk(packet)) {
                    bad(packet);
                    break;
                }
                uint32_t head = kernel::R32(packet + 4);
                if ((head & 0xFF000000) == want) {
                    kernel::W32(reader + 12, packet);
                    break;
                }
                packet = kernel::R32(packet) == 0xFFFFFFFF ? kernel::R32(stream + 0x40) : packet + (head & 0xFFFFFF);
            }
        }
    }
    kernel::CallGuest(ctx, leave, {stream + 4});
    ctx.eax = result;
    ctx.esp += 4;
}

}  // namespace eon
