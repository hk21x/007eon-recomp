// Modern button pictures: the prompts drawn as today's Xbox pad marks them, sharp at any resolution.
//
// The game draws its button prompts from one 128x128 DXT3 atlas: B X A Y as coloured ovals, the triggers as dark
// boxes with an arrow, START and BACK as a small dot over the word, the WHITE and BLACK buttons as small ovals, the
// d-pads with red marks, the sticks and the menus' arrows. When it is bound (GameInfo::texture_bound) and still as the
// disc has it (its bytes' FNV-1a), each icon is drawn again inside its own artwork's bounds, which every rectangle the
// game draws the icon with holds: A B X Y as coloured letters on a dark disc, LT and RT as labelled triggers, LB and RB
// as labelled bumpers, START as the Menu button (three lines), BACK as the View button (two squares), the d-pads with
// the arms meant lit white, the sticks and arrows plainer. They are shapes and a stroke font (src/common/strokes.h)
// drawn by coverage, so twice: at the atlas's size into guest memory (the blocks drawn on encoded again: two end
// colours a block, the pair furthest apart, each texel the nearest of the four), and four times larger for the
// renderers to draw instead (gpu::SetReplacement). driving.xbe has the same atlas as P8 (swizzled indices into a
// palette bound with it): told by its look (each 16x16 cell's mean colour), drawn on the same way, each texel changed
// the palette's nearest colour.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "gpu/texture.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "strokes.h"

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

// drawing: each icon's box is cleared, then shapes and letters go into it (src/common/strokes.h), on a canvas the
// atlas's size or larger
struct Box {
    int x0, y0, x1, y1;  // inclusive
};

Area Inside(Box b) { return {float(b.x0), float(b.y0), float(b.x1 + 1), float(b.y1 + 1)}; }
Colour C(Rgba c) { return {c.r, c.g, c.b, c.a}; }

void Clear(Canvas& c, Box b) {
    for (int y = b.y0 * c.scale; y < (b.y1 + 1) * c.scale; y++)
        for (int x = b.x0 * c.scale; x < (b.x1 + 1) * c.scale; x++) c.px[size_t(y) * c.size + x] = {};
}

// a dark disc with a lighter rim
void Disc(Canvas& c, float cx, float cy, float r) {
    Area a{cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1};
    Fill(c, a, C(kEdge), [&](float x, float y) { return CircleDistance(x, y, cx, cy, r); });
    Fill(c, a, C(kDark), [&](float x, float y) { return CircleDistance(x, y, cx, cy, r - 0.8f); });
}

// A B X Y: the letter in the button's colour on a dark disc
void FaceButton(Canvas& c, Box b, const char* letter, Rgba colour) {
    Clear(c, b);
    Area a = Inside(b);
    float cx = (a.x0 + a.x1) / 2, cy = (a.y0 + a.y1) / 2, r = std::min(a.x1 - a.x0, a.y1 - a.y0) / 2;
    Disc(c, cx, cy, r);
    Text(c, letter, cx, cy, 9, 2.0f, C(colour));
}

// LT RT LB RB: a plate with the label, a trigger rounder at the top than a bumper
void Shoulder(Canvas& c, Box b, Area plate, float top, float bottom, const char* label, float height = 6.5f) {
    Clear(c, b);
    Fill(c, plate, C(kEdge), [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom); });
    Fill(c, plate, C(kDark), [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom) + 0.8f; });
    Text(c, label, (plate.x0 + plate.x1) / 2, (plate.y0 + plate.y1) / 2, height, 1.2f, C(kWhite));
}

// Menu (three lines) and View (two overlapping squares) on a dark disc in the middle of the dot and word's box
void Menu(Canvas& c, Box b) {
    Clear(c, b);
    float cx = (b.x0 + b.x1 + 1) / 2, cy = (b.y0 + b.y1 + 1) / 2;
    Disc(c, cx, cy, 7);
    std::vector<Segment> lines;
    for (int i = -1; i <= 1; i++) lines.push_back({{cx - 2.6f, cy + i * 2.8f}, {cx + 2.6f, cy + i * 2.8f}});
    Fill(c, {cx - 5, cy - 5, cx + 5, cy + 5}, C(kWhite), [&](float x, float y) { return StrokeDistance(x, y, lines, 0.6f); });
}

void View(Canvas& c, Box b) {
    Clear(c, b);
    float cx = (b.x0 + b.x1 + 1) / 2, cy = (b.y0 + b.y1 + 1) / 2;
    Disc(c, cx, cy, 7);
    auto square = [&](float x0, float y0, bool front) {
        Area s{x0, y0, x0 + 4.4f, y0 + 4.4f};
        std::vector<Segment> edges = {{{s.x0, s.y0}, {s.x1, s.y0}}, {{s.x1, s.y0}, {s.x1, s.y1}},
                                      {{s.x1, s.y1}, {s.x0, s.y1}}, {{s.x0, s.y1}, {s.x0, s.y0}}};
        Area around{s.x0 - 1, s.y0 - 1, s.x1 + 1, s.y1 + 1};
        if (front) Fill(c, around, C(kDark), [&](float x, float y) { return BoxDistance(x, y, s, 0.5f, 0.5f); });
        Fill(c, around, C(kWhite), [&](float x, float y) { return StrokeDistance(x, y, edges, 0.55f); });
    };
    square(cx - 3.6f, cy - 3.6f, false);
    square(cx - 0.8f, cy - 0.8f, true);
}

// the d-pads: a dark disc with the cross, the arms named lit (the game's red marks) white
enum : int { kArmUp = 1, kArmDown = 2, kArmLeft = 4, kArmRight = 8 };
void DPad(Canvas& c, Box b, int lit) {
    Clear(c, b);
    Area a = Inside(b);
    Fill(c, a, C(kEdge), [&](float x, float y) { return EllipseDistance(x, y, a); });
    Area in{a.x0 + 0.8f, a.y0 + 0.8f, a.x1 - 0.8f, a.y1 - 0.8f};
    Fill(c, a, C(kDark), [&](float x, float y) { return EllipseDistance(x, y, in); });
    float cx = (a.x0 + a.x1) / 2, cy = (a.y0 + a.y1) / 2, w = 2.6f;
    float lx = (a.x1 - a.x0) / 2 - 2.6f, ly = (a.y1 - a.y0) / 2 - 3.2f;
    Area arms[4] = {{cx - w, cy - ly, cx + w, cy}, {cx - w, cy, cx + w, cy + ly}, {cx - lx, cy - w, cx, cy + w},
                    {cx, cy - w, cx + lx, cy + w}};
    constexpr Colour kArm{110, 110, 110, 255};
    for (int i = 0; i < 4; i++)
        Fill(c, arms[i], kArm, [&](float x, float y) { return BoxDistance(x, y, arms[i], 0.8f, 0.8f); });
    for (int i = 0; i < 4; i++) {
        if (!(lit >> i & 1)) continue;
        Area arm = arms[i];  // its outer part, the middle left grey
        if (i == 0) arm.y1 -= w;
        if (i == 1) arm.y0 += w;
        if (i == 2) arm.x1 -= w;
        if (i == 3) arm.x0 += w;
        Fill(c, arm, C(kWhite), [&](float x, float y) { return BoxDistance(x, y, arm, 0.8f, 0.8f); });
    }
}

// a thumbstick from above: a dark cap, its dished top a ring lighter
void Stick(Canvas& c, Box b) {
    Clear(c, b);
    Area a = Inside(b);
    Fill(c, a, C(kEdge), [&](float x, float y) { return EllipseDistance(x, y, a); });
    Area in{a.x0 + 0.8f, a.y0 + 0.8f, a.x1 - 0.8f, a.y1 - 0.8f};
    Fill(c, a, C(kDark), [&](float x, float y) { return EllipseDistance(x, y, in); });
    float kx = (a.x1 - a.x0) * 0.2f, ky = (a.y1 - a.y0) * 0.2f;
    Area dish{a.x0 + kx, a.y0 + ky, a.x1 - kx, a.y1 - ky};
    constexpr Colour kRing{80, 80, 80, 255};
    Fill(c, a, kRing, [&](float x, float y) { return std::abs(EllipseDistance(x, y, dish)) - 0.6f; });
}

// the menus' left and right arrows: light grey heads with a darker edge
void Arrow(Canvas& c, Box b, bool left) {
    Clear(c, b);
    Area a = Inside(b);
    float cy = (a.y0 + a.y1) / 2, tip = left ? a.x0 + 0.5f : a.x1 - 0.5f, back = left ? a.x1 - 1.5f : a.x0 + 1.5f;
    std::vector<Pt> head = left ? std::vector<Pt>{{tip, cy}, {back, a.y0 + 0.5f}, {back, a.y1 - 0.5f}}
                                : std::vector<Pt>{{tip, cy}, {back, a.y1 - 0.5f}, {back, a.y0 + 0.5f}};
    constexpr Colour kRim{110, 110, 110, 255}, kHead{215, 215, 215, 255};
    Fill(c, a, kRim, [&](float x, float y) { return PolygonDistance(x, y, head); });
    Fill(c, a, kHead, [&](float x, float y) { return PolygonDistance(x, y, head) + 0.9f; });
}

// each icon's artwork bounds in the atlas
constexpr Box kFaceB{2, 3, 19, 24}, kFaceX{23, 3, 40, 24}, kFaceA{43, 3, 59, 24}, kFaceY{69, 3, 86, 24};
constexpr Box kLeftTrigger{95, 5, 113, 25}, kRightTrigger{95, 28, 113, 48};
constexpr Box kStart{28, 26, 62, 44}, kBack{27, 82, 62, 100};  // past the art: the P8 copy's faint halo
constexpr Box kWhiteButton{32, 64, 43, 77}, kBlackButton{48, 64, 59, 77};
constexpr Box kPadUpDown{5, 28, 24, 52}, kPadLeftRight{5, 57, 24, 81}, kPadAll{5, 86, 24, 110};
constexpr Box kPadUp{31, 102, 50, 126}, kPadRight{56, 102, 75, 126}, kPadDown{81, 102, 100, 126}, kPadLeft{106, 102, 125, 126};
constexpr Box kStick1{66, 56, 90, 84}, kStick2{93, 56, 117, 84};
constexpr Box kArrowLeft{33, 45, 46, 58}, kArrowRight{48, 45, 61, 58};
constexpr Box kDrawn[] = {kFaceB,    kFaceX,    kFaceA,        kFaceY, kLeftTrigger, kRightTrigger, kStart,
                          kBack,     kWhiteButton, kBlackButton, kPadUpDown, kPadLeftRight, kPadAll, kPadUp,
                          kPadRight, kPadDown,  kPadLeft,      kStick1, kStick2,     kArrowLeft,    kArrowRight};

void Draw(Canvas& c) {
    FaceButton(c, kFaceA, "A", {100, 200, 60, 255});
    FaceButton(c, kFaceB, "B", {235, 60, 60, 255});
    FaceButton(c, kFaceX, "X", {50, 140, 245, 255});
    FaceButton(c, kFaceY, "Y", {250, 200, 40, 255});
    auto inset = [](Box b, float dx, float dy) { return Area{b.x0 + dx, b.y0 + dy, b.x1 + 1 - dx, b.y1 + 1 - dy}; };
    Shoulder(c, kLeftTrigger, inset(kLeftTrigger, 2, 2), 4, 1.5f, "LT");
    Shoulder(c, kRightTrigger, inset(kRightTrigger, 2, 2), 4, 1.5f, "RT");
    Shoulder(c, kWhiteButton, inset(kWhiteButton, 0, 2), 2, 2, "LB", 5.2f);
    Shoulder(c, kBlackButton, inset(kBlackButton, 0, 2), 2, 2, "RB", 5.2f);
    Menu(c, kStart);
    View(c, kBack);
    DPad(c, kPadUpDown, kArmUp | kArmDown);
    DPad(c, kPadLeftRight, kArmLeft | kArmRight);
    DPad(c, kPadAll, kArmUp | kArmDown | kArmLeft | kArmRight);
    DPad(c, kPadUp, kArmUp);
    DPad(c, kPadRight, kArmRight);
    DPad(c, kPadDown, kArmDown);
    DPad(c, kPadLeft, kArmLeft);
    Stick(c, kStick1);
    Stick(c, kStick2);
    Arrow(c, kArrowLeft, true);
    Arrow(c, kArrowRight, false);
}

// the atlas onto a canvas `scale` times its size (bilinear, premultiplied), and back
Canvas Upscale(const Image& img, int scale) {
    Canvas c;
    c.scale = scale;
    c.size = kSize * scale;
    c.px.resize(size_t(c.size) * c.size);
    auto at = [&](int x, int y) {
        Rgba t = img[std::clamp(y, 0, kSize - 1)][std::clamp(x, 0, kSize - 1)];
        float a = t.a / 255.0f;
        return std::array<float, 4>{t.r / 255.0f * a, t.g / 255.0f * a, t.b / 255.0f * a, a};
    };
    for (int y = 0; y < c.size; y++)
        for (int x = 0; x < c.size; x++) {
            float u = (x + 0.5f) / scale - 0.5f, v = (y + 0.5f) / scale - 0.5f;
            int x0 = int(std::floor(u)), y0 = int(std::floor(v));
            float fx = u - x0, fy = v - y0;
            auto p00 = at(x0, y0), p10 = at(x0 + 1, y0), p01 = at(x0, y0 + 1), p11 = at(x0 + 1, y0 + 1);
            auto& o = c.px[size_t(y) * c.size + x];
            for (int i = 0; i < 4; i++)
                o[i] = (p00[i] * (1 - fx) + p10[i] * fx) * (1 - fy) + (p01[i] * (1 - fx) + p11[i] * fx) * fy;
        }
    return c;
}

Rgba Straight(const std::array<float, 4>& p) {
    if (p[3] <= 0) return kClear;
    auto ch = [&](float v) { return uint8_t(std::clamp(v / p[3] * 255 + 0.5f, 0.0f, 255.0f)); };
    return {ch(p[0]), ch(p[1]), ch(p[2]), uint8_t(std::clamp(p[3] * 255 + 0.5f, 0.0f, 255.0f))};
}

// the atlas drawn on: `after` as the console gets it, and gpu::SetReplacement's picture four times larger
constexpr int kSharp = 4;
std::vector<uint32_t> Redraw(const Image& before, Image& after) {
    // the old art gone first, so none of it is smeared past the boxes when enlarged
    static Image cleared;
    std::memcpy(cleared, before, sizeof(Image));
    for (Box b : kDrawn)
        for (int y = b.y0; y <= b.y1; y++)
            for (int x = b.x0; x <= b.x1; x++) cleared[y][x] = kClear;
    Canvas c = Upscale(cleared, 1);
    Draw(c);
    for (int y = 0; y < kSize; y++)
        for (int x = 0; x < kSize; x++) after[y][x] = Straight(c.px[size_t(y) * kSize + x]);
    // where the drawing did not reach, the texels as they were (the conversion's rounding aside)
    for (int y = 0; y < kSize; y++)
        for (int x = 0; x < kSize; x++) {
            Rgba a = after[y][x], b = before[y][x];
            if (std::abs(a.r - b.r) <= 1 && std::abs(a.g - b.g) <= 1 && std::abs(a.b - b.b) <= 1 && std::abs(a.a - b.a) <= 1)
                after[y][x] = b;
        }
    Canvas sharp = Upscale(cleared, kSharp);
    Draw(sharp);
    std::vector<uint32_t> bgra(sharp.px.size());
    for (size_t i = 0; i < bgra.size(); i++) {
        Rgba p = Straight(sharp.px[i]);
        bgra[i] = uint32_t(p.b) | uint32_t(p.g) << 8 | uint32_t(p.r) << 16 | uint32_t(p.a) << 24;
    }
    return bgra;
}

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

// driving.xbe's copy is P8 (swizzled indices into the stage's palette), recognised by its look: the atlas's mean
// colour in each 16x16 cell (premultiplied by alpha), within kLookSlack of the disc's (both copies agree within 4)
constexpr Rgba kLook[64] = {
    {88, 66, 67, 146}, {42, 37, 60, 121}, {32, 40, 54, 132}, {43, 64, 43, 131}, {71, 62, 42, 113}, {26, 21, 9, 74},
    {39, 40, 40, 175}, {0, 0, 0, 21},     {43, 30, 30, 110}, {15, 13, 26, 80},  {18, 21, 30, 109}, {12, 26, 12, 83},
    {28, 23, 9, 71},   {11, 8, 2, 51},    {25, 25, 25, 223}, {0, 0, 0, 27},     {69, 64, 64, 160}, {53, 53, 54, 131},
    {40, 40, 40, 69},  {31, 31, 31, 40},  {0, 0, 0, 0},      {0, 0, 0, 15},     {43, 43, 43, 255}, {0, 0, 0, 31},
    {28, 26, 27, 71},  {14, 14, 14, 47},  {36, 36, 36, 69},  {36, 36, 36, 69},  {16, 16, 16, 68},  {10, 10, 10, 49},
    {22, 23, 23, 116}, {2, 2, 2, 15},     {66, 62, 63, 156}, {44, 40, 41, 117}, {70, 70, 70, 118}, {23, 23, 23, 127},
    {41, 41, 42, 208}, {34, 34, 35, 193}, {46, 46, 47, 254}, {12, 12, 12, 80},  {32, 28, 28, 79},  {19, 19, 20, 58},
    {27, 27, 27, 74},  {10, 10, 10, 12},  {5, 5, 5, 34},     {2, 2, 2, 21},     {7, 7, 7, 53},     {0, 0, 0, 2},
    {57, 49, 50, 131}, {41, 37, 38, 101}, {65, 62, 62, 143}, {27, 27, 27, 67},  {30, 30, 30, 79},  {41, 41, 41, 107},
    {14, 14, 14, 50},  {35, 36, 37, 96},  {0, 0, 0, 0},      {1, 1, 1, 5},      {84, 84, 85, 203}, {40, 40, 41, 108},
    {59, 55, 56, 139}, {77, 74, 74, 185}, {39, 36, 36, 104}, {68, 68, 70, 166}};
constexpr int kLookSlack = 12;

bool LooksLikeAtlas(const Image& img) {
    for (int g = 0; g < 64; g++) {
        int s[4] = {};
        for (int y = g / 8 * 16; y < g / 8 * 16 + 16; y++)
            for (int x = g % 8 * 16; x < g % 8 * 16 + 16; x++) {
                Rgba c = img[y][x];
                s[0] += c.r * c.a / 255;
                s[1] += c.g * c.a / 255;
                s[2] += c.b * c.a / 255;
                s[3] += c.a;
            }
        const Rgba& w = kLook[g];
        if (std::abs(s[0] / 256 - w.r) > kLookSlack || std::abs(s[1] / 256 - w.g) > kLookSlack ||
            std::abs(s[2] / 256 - w.b) > kLookSlack || std::abs(s[3] / 256 - w.a) > kLookSlack)
            return false;
    }
    return true;
}

// a swizzled texel's offset in a 128x128 texture: x and y bits interleaved, x first
uint32_t Swizzle(int x, int y) {
    uint32_t o = 0;
    for (int i = 0; i < 7; i++) o |= uint32_t((x >> i & 1) << (2 * i) | (y >> i & 1) << (2 * i + 1));
    return o;
}

int Dist4(Rgba x, Rgba y) { return Dist(x, y) + (x.a - y.a) * (x.a - y.a); }

// the DXT3 copy: its bytes as the disc has them, the changed blocks encoded again
bool PatchDxt3(uint8_t* atlas, std::vector<uint32_t>& sharp) {
    if (Fnv(atlas, kSize * kSize) != kOriginal) return false;
    static Image before, after;
    Decode(atlas, before);
    sharp = Redraw(before, after);
    for (int by = 0; by < kSize / 4; by++)
        for (int bx = 0; bx < kSize / 4; bx++) {
            bool changed = false;
            for (int y = by * 4; y < by * 4 + 4 && !changed; y++)
                changed = std::memcmp(before[y] + bx * 4, after[y] + bx * 4, 4 * sizeof(Rgba)) != 0;
            if (changed) EncodeBlock(after, bx, by, atlas + (by * (kSize / 4) + bx) * 16);
        }
    return true;
}

// the P8 copy: decoded through its palette (BGRA, 256 entries fewer by the size code), each changed texel the
// palette's nearest colour
bool PatchP8(uint8_t* atlas, const uint8_t* bgra, int colours, std::vector<uint32_t>& sharp) {
    static Image before, after;
    Rgba pal[256];
    for (int i = 0; i < colours; i++) pal[i] = {bgra[i * 4 + 2], bgra[i * 4 + 1], bgra[i * 4], bgra[i * 4 + 3]};
    for (int y = 0; y < kSize; y++)
        for (int x = 0; x < kSize; x++) {
            uint8_t i = atlas[Swizzle(x, y)];
            before[y][x] = i < colours ? pal[i] : kClear;
        }
    if (!LooksLikeAtlas(before)) return false;
    sharp = Redraw(before, after);
    int clear = -1;  // a fully clear colour, for the texels cleared
    for (int i = 0; i < colours && clear < 0; i++)
        if (pal[i].a == 0) clear = i;
    for (int y = 0; y < kSize; y++)
        for (int x = 0; x < kSize; x++) {
            Rgba c = after[y][x];
            if (std::memcmp(&c, &before[y][x], sizeof(Rgba)) == 0) continue;
            if (c.a == 0 && clear >= 0) {
                atlas[Swizzle(x, y)] = uint8_t(clear);
                continue;
            }
            int best = 0;
            for (int i = 1; i < colours; i++)
                if (Dist4(c, pal[i]) < Dist4(c, pal[best])) best = i;
            atlas[Swizzle(x, y)] = uint8_t(best);
        }
    return true;
}

}  // namespace

// GameInfo::texture_bound: D3DBaseTexture Data (+4, physical) and Format (+12: format bits 8-15, the width's and
// height's log2 in bits 20-23 and 24-27); D3DPalette Common (+0, its size code in the top two bits) and Data (+4)
bool ButtonPictures(uint32_t header, uint32_t palette) {
    constexpr uint32_t kP8 = 0x0B;
    if (!modern_buttons) return false;
    uint32_t format = kernel::R32(header + 12), kind = format >> 8 & 0xFF;
    if ((kind != kDxt3 && kind != kP8) || (format >> 20 & 15) != 7 || (format >> 24 & 15) != 7) return false;
    uint32_t data = mem::VirtualFromPhysical(kernel::R32(header + 4));
    constexpr uint32_t kBytes = kSize * kSize;  // DXT3 and P8: a byte a texel
    if (!mem::IsCommitted(data) || !mem::IsCommitted(data + kBytes - 1)) return false;
    auto* atlas = static_cast<uint8_t*>(kernel::HostPtr(data));
    bool done;
    std::vector<uint32_t> sharp;
    if (kind == kDxt3) {
        done = PatchDxt3(atlas, sharp);
    } else {
        if (!palette) return false;
        uint32_t colours = 256u >> (kernel::R32(palette) >> 30), pal = mem::VirtualFromPhysical(kernel::R32(palette + 4));
        if (!mem::IsCommitted(pal) || !mem::IsCommitted(pal + colours * 4 - 1)) return false;
        done = PatchP8(atlas, static_cast<const uint8_t*>(kernel::HostPtr(pal)), int(colours), sharp);
    }
    if (!done) return false;
    gpu::SetReplacement(data, kBytes, kSize * kSharp, kSize * kSharp, std::move(sharp));
    LOGR("eon: the button atlas %08X (%s) drawn as a modern pad's, and %dx%d for the renderers", data,
         kind == kDxt3 ? "DXT3" : "P8", kSize * kSharp, kSize * kSharp);
    return done;
}
}  // namespace eon
