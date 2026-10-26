// Ultrawide: the title screen ("Press START") pillarboxed in black. Its 4:3 art is four 256x256 tiles (DXT, format
// 0x0E) drawn over the menus' looping movie (a YUY2 frame, format 0x24, drawn over the whole frame): the art is
// squeezed into the centre like any 2D, the movie behind it reached the screen's sides. On the console the art covers
// it (and the movie is drawn as a plain quad, not as 2D the squeeze would take). While the art's tiles are being drawn
// (bound, switched in or replayed), the movie's draws (its YUY2 frames, two in turn) become a black clear
// (gpu::State::blank_format; the title clears nothing itself): the sides are black, as the console never shows the
// movie; in the other menus it fills the screen as before.
#include <chrono>
#include <cstdint>

#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/d3d8.h"

namespace eon {

namespace {
constexpr uint32_t kDxt = 0x0E, kYuy2 = 0x24;
// the first kilobyte of each of the art's tiles (FNV-1a), as the disc has them
constexpr uint64_t kTiles[] = {0x6884A866C8AD4583ull, 0x4C0FA4917E62D9FBull, 0x368064622D52B7F2ull};

bool TitleTile(uint32_t header, uint32_t format) {
    if ((format >> 8 & 0xFF) != kDxt || (format >> 20 & 15) != 8 || (format >> 24 & 15) != 8) return false;
    uint32_t data = mem::VirtualFromPhysical(kernel::R32(header + 4));
    if (!mem::IsCommitted(data) || !mem::IsCommitted(data + 1023)) return false;
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < 1024; i++) h = (h ^ kernel::G<uint8_t>(data + i)) * 1099511628211ull;
    for (uint64_t t : kTiles)
        if (h == t) return true;
    return false;
}
}  // namespace

// each texture the title binds (GameInfo::texture_bound, through src/common/game_info.cpp; replayed draws' too): the
// movie's draw is left out while the art has been drawn in the last 200 ms
void TitleScreen(uint32_t header) {
    using clock = std::chrono::steady_clock;
    static clock::time_point art_seen{};
    if (TitleTile(header, kernel::R32(header + 12))) art_seen = clock::now();
    d3d::State().blank_format = clock::now() - art_seen < std::chrono::milliseconds(200) ? kYuy2 : 0;
}

}  // namespace eon
