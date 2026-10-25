// Modern button pictures: the prompts drawn as today's Xbox pad marks them.
//
// The game draws its button prompts from one 128x128 DXT3 atlas: B X A Y as coloured ovals, the triggers as dark
// boxes with an arrow, START and BACK as a small dot over the word, the WHITE and BLACK buttons as small ovals, the
// d-pads and the sticks. When it is bound (GameInfo::texture_bound) and still as the disc has it (its bytes' FNV-1a),
// each icon is drawn again inside its own artwork's bounds, which every rectangle the game draws the icon with holds:
// A B X Y as coloured letters on a dark disc, LT and RT as labelled triggers, LB and RB as labelled bumpers, START as
// the Menu button (three lines), BACK as the View button (two squares). The d-pads and sticks look as they did. The
// atlas is decoded, drawn on, and the blocks drawn on are encoded again (two end colours a block, the pair furthest
// apart, each texel the nearest of the four).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace eon {
bool modern_buttons = true;

namespace {

constexpr uint32_t kDxt3 = 0x0E;
constexpr int kSize = 128;                              // texels across and down
constexpr uint64_t kOriginal = 0x09076852FE367BA4ull;  // FNV-1a of the atlas's 16384 bytes as the disc has them

struct Rgba {
    uint8_t r, g, b, a;
};
using Image = Rgba[kSize][kSize];

constexpr Rgba kClear{0, 0, 0, 0};
constexpr Rgba kDark{30, 30, 30, 255};
constexpr Rgba kEdge{150, 150, 150, 255};
constexpr Rgba kWhite{255, 255, 255, 255};

// DXT3: 64 bits of 4 bit alpha, then two RGB565 end colours and 2 bit indices (c0, c1, 2/3 c0 + 1/3 c1, the reverse)
Rgba From565(uint16_t c) {
    return {uint8_t((c >> 11) * 255 / 31), uint8_t((c >> 5 & 63) * 255 / 63), uint8_t((c & 31) * 255 / 31), 255};
}
uint16_t To565(Rgba c) { return uint16_t((c.r * 31 + 127) / 255 << 11 | (c.g * 63 + 127) / 255 << 5 | (c.b * 31 + 127) / 255); }

void Decode(const uint8_t* dxt, Image& out) {
    for (int by = 0; by < kSize / 4; by++)
        for (int bx = 0; bx < kSize / 4; bx++) {
            const uint8_t* b = dxt + (by * (kSize / 4) + bx) * 16;
            uint64_t alpha;
            uint16_t c0, c1;
            uint32_t idx;
            std::memcpy(&alpha, b, 8);
            std::memcpy(&c0, b + 8, 2);
            std::memcpy(&c1, b + 10, 2);
            std::memcpy(&idx, b + 12, 4);
            Rgba p0 = From565(c0), p1 = From565(c1), pal[4] = {p0, p1, p0, p0};
            auto mix = [](Rgba x, Rgba y) { return Rgba{uint8_t((2 * x.r + y.r) / 3), uint8_t((2 * x.g + y.g) / 3), uint8_t((2 * x.b + y.b) / 3), 255}; };
            pal[2] = mix(p0, p1);
            pal[3] = mix(p1, p0);
            for (int k = 0; k < 16; k++) {
                Rgba c = pal[idx >> (2 * k) & 3];
                c.a = uint8_t((alpha >> (4 * k) & 15) * 17);
                out[by * 4 + k / 4][bx * 4 + k % 4] = c;
            }
        }
}

int Dist(Rgba x, Rgba y) { return (x.r - y.r) * (x.r - y.r) + (x.g - y.g) * (x.g - y.g) + (x.b - y.b) * (x.b - y.b); }

void EncodeBlock(const Image& img, int bx, int by, uint8_t* b) {
    Rgba px[16];
    for (int k = 0; k < 16; k++) px[k] = img[by * 4 + k / 4][bx * 4 + k % 4];
    // the end colours: the two seen texels furthest apart
    Rgba e0 = kDark, e1 = kDark;
    int best = -1;
    for (int i = 0; i < 16; i++)
        for (int j = i; j < 16; j++)
            if (px[i].a && px[j].a && Dist(px[i], px[j]) > best) {
                best = Dist(px[i], px[j]);
                e0 = px[i];
                e1 = px[j];
            }
    uint16_t c0 = To565(e0), c1 = To565(e1);
    if (c0 < c1) std::swap(c0, c1);  // four colours whatever the order in DXT3; kept as DXT1's for tidiness
    Rgba p0 = From565(c0), p1 = From565(c1);
    Rgba pal[4] = {p0, p1, {uint8_t((2 * p0.r + p1.r) / 3), uint8_t((2 * p0.g + p1.g) / 3), uint8_t((2 * p0.b + p1.b) / 3), 255},
                   {uint8_t((p0.r + 2 * p1.r) / 3), uint8_t((p0.g + 2 * p1.g) / 3), uint8_t((p0.b + 2 * p1.b) / 3), 255}};
    uint64_t alpha = 0;
    uint32_t idx = 0;
    for (int k = 0; k < 16; k++) {
        alpha |= uint64_t((px[k].a * 15 + 127) / 255) << (4 * k);
        int pick = 0;
        for (int i = 1; i < 4; i++)
            if (Dist(px[k], pal[i]) < Dist(px[k], pal[pick])) pick = i;
        idx |= uint32_t(pick) << (2 * k);
    }
    std::memcpy(b, &alpha, 8);
    std::memcpy(b + 8, &c0, 2);
    std::memcpy(b + 10, &c1, 2);
    std::memcpy(b + 12, &idx, 4);
}

// drawing: a box of the atlas is cleared, then shapes and letters go into it
struct Box {
    int x0, y0, x1, y1;  // inclusive
};

void Clear(Image& img, Box b) {
    for (int y = b.y0; y <= b.y1; y++)
        for (int x = b.x0; x <= b.x1; x++) img[y][x] = kClear;
}

// a disc with soft edges (coverage of 4x4 samples a texel), centred in the box
void Disc(Image& img, Box b, float radius, Rgba body, Rgba rim) {
    float cx = (b.x0 + b.x1 + 1) / 2.0f, cy = (b.y0 + b.y1 + 1) / 2.0f;
    for (int y = b.y0; y <= b.y1; y++)
        for (int x = b.x0; x <= b.x1; x++) {
            int in = 0, inner = 0;
            for (int s = 0; s < 16; s++) {
                float dx = x + (s % 4 + 0.5f) / 4 - cx, dy = y + (s / 4 + 0.5f) / 4 - cy, d = std::sqrt(dx * dx + dy * dy);
                in += d <= radius;
                inner += d <= radius - 1;
            }
            if (!in) continue;
            Rgba c = inner >= 8 ? body : rim;
            c.a = uint8_t(in * 255 / 16);
            img[y][x] = c;
        }
}

// a rounded box: body inside an edge of one texel, corners cut by `cut` texels at the top and one at the bottom
void Plate(Image& img, Box b, int cut) {
    for (int y = b.y0; y <= b.y1; y++)
        for (int x = b.x0; x <= b.x1; x++) {
            int from_top = y - b.y0, from_side = std::min(x - b.x0, b.x1 - x);
            if (from_top < cut && from_side < cut - from_top) continue;
            if (y == b.y1 && from_side == 0) continue;
            bool edge = from_side == 0 || y == b.y0 || y == b.y1 || (from_top < cut && from_side == cut - from_top);
            img[y][x] = edge ? kEdge : kDark;
        }
}

// letters, a row a string (up to nine), their width the strings' length
struct Glyph {
    const char* rows[9];
};
constexpr Glyph kBigA{{"..###..", ".##.##.", "##...##", "##...##", "#######", "##...##", "##...##", "##...##", "##...##"}};
constexpr Glyph kBigB{{"######.", "##...##", "##...##", "##...##", "######.", "##...##", "##...##", "##...##", "######."}};
constexpr Glyph kBigX{{"##...##", "##...##", ".##.##.", "..###..", "...#...", "..###..", ".##.##.", "##...##", "##...##"}};
constexpr Glyph kBigY{{"##...##", "##...##", ".##.##.", "..###..", "..###..", "..###..", "..###..", "..###..", "..###.."}};
constexpr Glyph kL{{"#..", "#..", "#..", "#..", "#..", "#..", "###"}};
constexpr Glyph kR{{"##.", "#.#", "#.#", "##.", "#.#", "#.#", "#.#"}};
constexpr Glyph kB{{"##.", "#.#", "#.#", "##.", "#.#", "#.#", "##."}};
constexpr Glyph kT{{"###", ".#.", ".#.", ".#.", ".#.", ".#.", ".#."}};

void Letter(Image& img, const Glyph& g, int x, int y, Rgba c) {
    for (int r = 0; r < 9 && g.rows[r]; r++)
        for (int k = 0; g.rows[r][k]; k++)
            if (g.rows[r][k] == '#') img[y + r][x + k] = c;
}

// two small letters centred in a box
void Label(Image& img, Box b, const Glyph& first, const Glyph& second) {
    int w = 3 + 1 + 3, x = b.x0 + (b.x1 - b.x0 + 1 - w) / 2, y = b.y0 + (b.y1 - b.y0 + 1 - 7) / 2;
    Letter(img, first, x, y, kWhite);
    Letter(img, second, x + 4, y, kWhite);
}

// A B X Y: a dark disc with the letter in the button's colour
void FaceButton(Image& img, Box b, const Glyph& g, Rgba colour) {
    Clear(img, b);
    float r = std::min(b.x1 - b.x0 + 1, b.y1 - b.y0 + 1) / 2.0f;
    Disc(img, b, r, kDark, kEdge);
    int x = b.x0 + (b.x1 - b.x0 + 1 - 7) / 2, y = b.y0 + (b.y1 - b.y0 + 1 - 9) / 2;
    Letter(img, g, x, y, colour);
}

// LT RT LB RB: a plate with the label, a trigger rounder at the top than a bumper
void Shoulder(Image& img, Box b, Box plate, int cut, const Glyph& first, const Glyph& second) {
    Clear(img, b);
    Plate(img, plate, cut);
    Label(img, plate, first, second);
}

// Menu (three lines) and View (two overlapping squares) on a dark disc in the middle of the dot and word's box
void Menu(Image& img, Box b) {
    Clear(img, b);
    int cx = (b.x0 + b.x1 + 1) / 2, cy = (b.y0 + b.y1 + 1) / 2;
    Box d{cx - 7, cy - 7, cx + 6, cy + 6};
    Disc(img, d, 7, kDark, kEdge);
    for (int line = -1; line <= 1; line++)
        for (int x = cx - 3; x <= cx + 2; x++) img[cy - 1 + line * 3][x] = kWhite;
}

void View(Image& img, Box b) {
    Clear(img, b);
    int cx = (b.x0 + b.x1 + 1) / 2, cy = (b.y0 + b.y1 + 1) / 2;
    Box d{cx - 7, cy - 7, cx + 6, cy + 6};
    Disc(img, d, 7, kDark, kEdge);
    auto square = [&](int x0, int y0) {
        for (int i = 0; i < 5; i++) {
            img[y0][x0 + i] = img[y0 + 4][x0 + i] = kWhite;
            img[y0 + i][x0] = img[y0 + i][x0 + 4] = kWhite;
        }
    };
    square(cx - 4, cy - 4);
    square(cx - 1, cy - 1);
}

// each icon's artwork bounds in the atlas
constexpr Box kFaceB{2, 3, 19, 24}, kFaceX{23, 3, 40, 24}, kFaceA{43, 3, 59, 24}, kFaceY{69, 3, 86, 24};
constexpr Box kLeftTrigger{95, 5, 113, 25}, kRightTrigger{95, 28, 113, 48};
constexpr Box kStart{29, 27, 59, 42}, kBack{30, 83, 56, 98};
constexpr Box kWhiteButton{32, 64, 43, 77}, kBlackButton{48, 64, 59, 77};

void Draw(Image& img) {
    FaceButton(img, kFaceA, kBigA, {100, 200, 60, 255});
    FaceButton(img, kFaceB, kBigB, {235, 60, 60, 255});
    FaceButton(img, kFaceX, kBigX, {50, 140, 245, 255});
    FaceButton(img, kFaceY, kBigY, {250, 200, 40, 255});
    auto inset = [](Box b, int dx, int dy) { return Box{b.x0 + dx, b.y0 + dy, b.x1 - dx, b.y1 - dy}; };
    Shoulder(img, kLeftTrigger, inset(kLeftTrigger, 2, 2), 3, kL, kT);
    Shoulder(img, kRightTrigger, inset(kRightTrigger, 2, 2), 3, kR, kT);
    Shoulder(img, kWhiteButton, {kWhiteButton.x0, kWhiteButton.y0 + 2, kWhiteButton.x1, kWhiteButton.y1 - 2}, 1, kL, kB);
    Shoulder(img, kBlackButton, {kBlackButton.x0, kBlackButton.y0 + 2, kBlackButton.x1, kBlackButton.y1 - 2}, 1, kR, kB);
    Menu(img, kStart);
    View(img, kBack);
}

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

}  // namespace

// GameInfo::texture_bound: D3DBaseTexture Data (+4, physical) and Format (+12: format bits 8-15, the width's and
// height's log2 in bits 20-23 and 24-27)
bool ButtonPictures(uint32_t header) {
    if (!modern_buttons) return false;
    uint32_t format = kernel::R32(header + 12);
    if ((format >> 8 & 0xFF) != kDxt3 || (format >> 20 & 15) != 7 || (format >> 24 & 15) != 7) return false;
    uint32_t data = mem::VirtualFromPhysical(kernel::R32(header + 4));
    constexpr uint32_t kBytes = kSize * kSize;  // DXT3: a byte a texel
    if (!mem::IsCommitted(data) || !mem::IsCommitted(data + kBytes - 1)) return false;
    auto* atlas = static_cast<uint8_t*>(kernel::HostPtr(data));
    if (Fnv(atlas, kBytes) != kOriginal) return false;
    static Image before, after;
    Decode(atlas, before);
    std::memcpy(after, before, sizeof(Image));
    Draw(after);
    for (int by = 0; by < kSize / 4; by++)
        for (int bx = 0; bx < kSize / 4; bx++) {
            bool changed = false;
            for (int y = by * 4; y < by * 4 + 4 && !changed; y++)
                changed = std::memcmp(before[y] + bx * 4, after[y] + bx * 4, 4 * sizeof(Rgba)) != 0;
            if (changed) EncodeBlock(after, bx, by, atlas + (by * (kSize / 4) + bx) * 16);
        }
    LOGR("eon: the button atlas %08X drawn as a modern pad's", data);
    return true;
}
}  // namespace eon
