// A stroke font and shapes drawn by coverage, at any size: the button pictures (src/common/buttons.cpp) are drawn with
// them once at the atlas's own size and once four times larger (gpu::SetReplacement).
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace eon {

struct Pt {
    float x, y;
};
struct Segment {
    Pt a, b;
};

// a string's strokes (A-Z, 0-9, a few signs), placed with its cap height `height` and centred on (cx, cy); one glyph
// is 4 units of 6 tall and they are 1.6 apart. The width taken comes back in width.
std::vector<Segment> TextStrokes(const char* text, float cx, float cy, float height, float* width = nullptr);
float TextWidth(const char* text, float height);

// premultiplied colours, `scale` pixels a texel across and down; shapes are given in texels
struct Canvas {
    int scale = 1, size = 0;  // pixels across and down
    std::vector<std::array<float, 4>> px;
};

struct Colour {
    uint8_t r, g, b, a;
};

struct Area {
    float x0, y0, x1, y1;  // texels, x1 and y1 past the end
};

// coverage of 4x4 samples a pixel inside `area`; a sample is in where the shape's signed distance (texels) is below 0
template <class Shape>
void Fill(Canvas& c, Area area, Colour colour, Shape distance) {
    int px0 = std::max(0, int(area.x0 * c.scale)), py0 = std::max(0, int(area.y0 * c.scale));
    int px1 = std::min(c.size, int(area.x1 * c.scale + 0.999f)), py1 = std::min(c.size, int(area.y1 * c.scale + 0.999f));
    for (int y = py0; y < py1; y++)
        for (int x = px0; x < px1; x++) {
            int in = 0;
            for (int s = 0; s < 16; s++)
                in += distance((x + (s % 4 + 0.5f) / 4) / c.scale, (y + (s / 4 + 0.5f) / 4) / c.scale) < 0;
            if (!in) continue;
            float a = in / 16.0f * colour.a / 255.0f;
            auto& p = c.px[size_t(y) * c.size + x];
            p = {colour.r / 255.0f * a + p[0] * (1 - a), colour.g / 255.0f * a + p[1] * (1 - a),
                 colour.b / 255.0f * a + p[2] * (1 - a), a + p[3] * (1 - a)};
        }
}

// signed distances
float CircleDistance(float x, float y, float cx, float cy, float r);
// a box with rounded corners, `top` the radius of the upper two and `bottom` of the lower
float BoxDistance(float x, float y, Area b, float top, float bottom);
// an ellipse filling the area (near enough a distance for coverage)
float EllipseDistance(float x, float y, Area a);
// a convex polygon, its corners clockwise on screen (y down)
float PolygonDistance(float x, float y, const std::vector<Pt>& corners);
// strokes `half` thick each side, round ends
float StrokeDistance(float x, float y, const std::vector<Segment>& strokes, float half);

void Text(Canvas& c, const char* text, float cx, float cy, float height, float stroke, Colour colour);

}  // namespace eon
