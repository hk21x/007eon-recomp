// Modern button pictures: the controller's BLACK and WHITE buttons are RB and LB on today's pads.
//
// The game draws its button prompts from one 128x128 DXT3 atlas (B X A Y, the triggers, the d-pads, START, BACK, the
// sticks, and the WHITE and BLACK buttons as small white and black ovals in the 16x16 cells at 32,64 and 48,64). When
// it is bound (GameInfo::texture_bound) the two cells are drawn over in guest memory as LB and RB bumpers: white
// letters on a dark rounded body with a grey edge, three colours a DXT block holds exactly (white, dark and the
// blend two thirds of the way to white). Recognised by the cells' original bytes, so a patched atlas is left alone.
#include <cstdint>
#include <cstring>

#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace eon {
bool modern_buttons = true;  // LB and RB in place of WHITE and BLACK

namespace {

constexpr uint32_t kDxt3 = 0x0E;
constexpr uint32_t kAtlasLog2 = 7;               // 128 texels across and down
constexpr uint32_t kBlocksPerRow = 128 / 4;
constexpr uint32_t kCellY = 64, kWhiteX = 32, kBlackX = 48;
constexpr uint64_t kOriginalCells = 0xD26D60581BC4099Bull;  // FNV-1a of both cells' 32 blocks as the disc has them

// 3x7 letters, a row a string
constexpr const char* kL[7] = {"#..", "#..", "#..", "#..", "#..", "#..", "###"};
constexpr const char* kB[7] = {"##.", "#.#", "#.#", "##.", "#.#", "#.#", "##."};
constexpr const char* kR[7] = {"##.", "#.#", "#.#", "##.", "#.#", "#.#", "#.#"};

// a cell's texels: 0 clear, 1 white, 2 dark, 3 grey. Inside the original oval's texels (columns 0 to 11, rows 0 to
// 13 of the cell), which every rectangle the game draws the icon with holds
void DrawBumper(const char* const* left, const char* const* right, uint8_t cell[16][16]) {
    std::memset(cell, 0, 256);
    constexpr int x0 = 0, x1 = 11, y0 = 2, y1 = 11;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            bool corner = (x == x0 || x == x1) && (y == y0 || y == y1);
            bool edge = x == x0 || x == x1 || y == y0 || y == y1;
            if (!corner) cell[y][x] = edge ? 3 : 2;
        }
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 3; x++) {
            if (left[y][x] == '#') cell[3 + y][2 + x] = 1;
            if (right[y][x] == '#') cell[3 + y][6 + x] = 1;
        }
}

// DXT3 blocks: explicit 4 bit alpha, then colours white (c0) and dark (c1); index 2 is two thirds white, the grey
void WriteCell(uint8_t* atlas, uint32_t cx, const uint8_t cell[16][16]) {
    constexpr uint16_t kWhite = 0xFFFF, kDark = 0x2104;  // RGB565: 255,255,255 and 32,32,32
    constexpr uint8_t kIndex[4] = {0, 0, 1, 2};          // clear (any), white, dark, grey
    for (uint32_t by = 0; by < 4; by++)
        for (uint32_t bx = 0; bx < 4; bx++) {
            uint8_t* b = atlas + ((kCellY / 4 + by) * kBlocksPerRow + cx / 4 + bx) * 16;
            uint64_t alpha = 0;
            uint32_t indices = 0;
            for (int k = 0; k < 16; k++) {
                uint8_t t = cell[by * 4 + k / 4][bx * 4 + k % 4];
                if (t) alpha |= uint64_t(15) << (4 * k);
                indices |= uint32_t(kIndex[t]) << (2 * k);
            }
            std::memcpy(b, &alpha, 8);
            std::memcpy(b + 8, &kWhite, 2);
            std::memcpy(b + 10, &kDark, 2);
            std::memcpy(b + 12, &indices, 4);
        }
}

uint64_t CellsHash(const uint8_t* atlas) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint32_t by = kCellY / 4; by < kCellY / 4 + 4; by++)
        for (uint32_t bx = kWhiteX / 4; bx < kBlackX / 4 + 4; bx++)
            for (uint32_t i = 0; i < 16; i++) h = (h ^ atlas[(by * kBlocksPerRow + bx) * 16 + i]) * 0x100000001B3ull;
    return h;
}

}  // namespace

// GameInfo::texture_bound: D3DBaseTexture Data (+4, physical) and Format (+12: format bits 8-15, the width's and
// height's log2 in bits 20-23 and 24-27)
bool ButtonPictures(uint32_t header) {
    if (!modern_buttons) return false;
    uint32_t format = kernel::R32(header + 12);
    if ((format >> 8 & 0xFF) != kDxt3 || (format >> 20 & 15) != kAtlasLog2 || (format >> 24 & 15) != kAtlasLog2)
        return false;
    uint32_t data = mem::VirtualFromPhysical(kernel::R32(header + 4));
    if (!mem::IsCommitted(data) || !mem::IsCommitted(data + 128 * 128 - 1)) return false;
    auto* atlas = static_cast<uint8_t*>(kernel::HostPtr(data));
    if (CellsHash(atlas) != kOriginalCells) return false;
    uint8_t cell[16][16];
    DrawBumper(kL, kB, cell);
    WriteCell(atlas, kWhiteX, cell);
    DrawBumper(kR, kB, cell);
    WriteCell(atlas, kBlackX, cell);
    LOGR("eon: the button atlas %08X: WHITE and BLACK drawn as LB and RB", data);
    return true;
}
}  // namespace eon
