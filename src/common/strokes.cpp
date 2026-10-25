#include "strokes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace eon {
namespace {

constexpr float kGlyphWidth = 4, kGlyphHeight = 6, kGap = 1.6f;

// each glyph a path in a 4 x 6 box, y down: M x y moves, L x y draws a line, E cx cy rx ry a0 a1 an elliptical arc
// from a0 to a1 degrees (0 right, 90 down) joined to the stroke before it, e the same starting a new one
struct Glyph {
    char c;
    const char* path;
};
constexpr Glyph kGlyphs[] = {
    {'A', "M0 6 L2 0 L4 6 M0.7 4 L3.3 4"},
    {'B', "M0 3 L2.4 3 E2.4 1.5 1.5 1.5 90 -90 L0 0 L0 6 L2.5 6 E2.5 4.5 1.5 1.5 90 -90"},
    {'C', "e2.2 3 2 3 -40 -320"},
    {'D', "M0 0 L0 6 L1.4 6 E1.4 3 2.6 3 90 -90 L0 0"},
    {'E', "M4 0 L0 0 L0 6 L4 6 M0 3 L3 3"},
    {'F', "M4 0 L0 0 L0 6 M0 3 L3 3"},
    {'G', "e2.1 3 2 3 -40 -360 L2.3 3"},
    {'H', "M0 0 L0 6 M4 0 L4 6 M0 3 L4 3"},
    {'I', "M1 0 L3 0 M2 0 L2 6 M1 6 L3 6"},
    {'J', "M4 0 L4 4.2 E2.2 4.2 1.8 1.8 0 180"},
    {'K', "M0 0 L0 6 M4 0 L0 3.8 M1.5 2.6 L4 6"},
    {'L', "M0 0 L0 6 L3.6 6"},
    {'M', "M0 6 L0 0 L2 3.6 L4 0 L4 6"},
    {'N', "M0 6 L0 0 L4 6 L4 0"},
    {'O', "e2 3 2 3 0 360"},
    {'P', "M0 6 L0 0 L2.4 0 E2.4 1.6 1.6 1.6 -90 90 L0 3.2"},
    {'Q', "e2 3 2 3 0 360 M2.6 4.4 L4 6"},
    {'R', "M0 6 L0 0 L2.4 0 E2.4 1.6 1.6 1.6 -90 90 L0 3.2 M2 3.2 L4 6"},
    {'S', "e2 1.5 1.9 1.5 -20 -270 E2 4.5 2 1.5 -90 160"},
    {'T', "M0 0 L4 0 M2 0 L2 6"},
    {'U', "M0 0 L0 4 E2 4 2 2 180 0 L4 0"},
    {'V', "M0 0 L2 6 L4 0"},
    {'W', "M0 0 L1 6 L2 2 L3 6 L4 0"},
    {'X', "M0 0 L4 6 M4 0 L0 6"},
    {'Y', "M0 0 L2 3 L4 0 M2 3 L2 6"},
    {'Z', "M0 0 L4 0 L0 6 L4 6"},
    {'0', "e2 3 1.9 3 0 360"},
    {'1', "M0.8 1.2 L2.2 0 L2.2 6 M0.8 6 L3.6 6"},
    {'2', "e2 1.9 1.9 1.9 -165 15 L0 6 L4 6"},
    {'3', "e2 1.5 1.8 1.5 -150 90 E2 4.5 2 1.5 -90 150"},
    {'4', "M3 6 L3 0 L0 4.2 L4 4.2"},
    {'5', "M3.8 0 L0.5 0 L0.2 2.8 E2 4.1 2 1.9 -125 150"},
    {'6', "M3.2 0 L0.5 3.3 e2 4.2 1.9 1.8 0 360"},
    {'7', "M0 0 L4 0 L1.4 6"},
    {'8', "e2 1.5 1.6 1.5 0 360 e2 4.5 2 1.5 0 360"},
    {'9', "e2 1.8 1.9 1.8 0 360 M3.9 2.2 L0.8 6"},
    {'-', "M0.8 3 L3.2 3"},
    {'+', "M0.4 3 L3.6 3 M2 1.4 L2 4.6"},
    {'/', "M3.6 0 L0.4 6"},
    {'.', "M2 5.8 L2 5.9"},
    {',', "M2.2 5.4 L1.6 6.6"},
    {'\'', "M2 0 L2 1.6"},
    {'[', "M3 0 L1.4 0 L1.4 6 L3 6"},
    {']', "M1 0 L2.6 0 L2.6 6 L1 6"},
    {';', "M2 2 L2 2.1 M2.2 5.4 L1.6 6.6"},
    {'=', "M0.6 2.2 L3.4 2.2 M0.6 3.8 L3.4 3.8"},
    {'`', "M1.4 0 L2.4 1.2"},
    {'\\', "M0.4 0 L3.6 6"},
};

const char* Path(char c) {
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    for (const Glyph& g : kGlyphs)
        if (g.c == c) return g.path;
    return nullptr;  // a space, or a sign it does not have
}

// a glyph's strokes in its own box
void GlyphStrokes(const char* path, std::vector<Segment>& out) {
    Pt pen{0, 0};
    bool down = false;
    const char* p = path;
    auto num = [&] {
        char* end;
        float v = std::strtof(p, &end);
        p = end;
        return v;
    };
    while (*p) {
        char op = *p++;
        if (op == ' ') continue;
        if (op == 'M') {
            float x = num(), y = num();
            pen = {x, y};
            down = true;
        } else if (op == 'L') {
            float x = num(), y = num();
            out.push_back({pen, {x, y}});
            pen = {x, y};
        } else if (op == 'E' || op == 'e') {
            float cx = num(), cy = num(), rx = num(), ry = num(), a0 = num(), a1 = num();
            int steps = std::max(4, int(std::abs(a1 - a0) / 10));
            for (int i = 0; i <= steps; i++) {
                float a = (a0 + (a1 - a0) * i / steps) * 3.14159265f / 180;
                Pt q{cx + rx * std::cos(a), cy + ry * std::sin(a)};
                if (i > 0 || (op == 'E' && down)) out.push_back({pen, q});
                pen = q;
            }
            down = true;
        }
    }
}

}  // namespace

float TextWidth(const char* text, float height) {
    size_t n = std::char_traits<char>::length(text);
    return n ? (n * kGlyphWidth + (n - 1) * kGap) * height / kGlyphHeight : 0;
}

std::vector<Segment> TextStrokes(const char* text, float cx, float cy, float height, float* width) {
    float k = height / kGlyphHeight, w = TextWidth(text, height);
    if (width) *width = w;
    float x0 = cx - w / 2, y0 = cy - height / 2;
    std::vector<Segment> out, glyph;
    for (size_t i = 0; text[i]; i++) {
        const char* path = Path(text[i]);
        if (!path) continue;
        glyph.clear();
        GlyphStrokes(path, glyph);
        float gx = x0 + i * (kGlyphWidth + kGap) * k;
        for (const Segment& s : glyph)
            out.push_back({{gx + s.a.x * k, y0 + s.a.y * k}, {gx + s.b.x * k, y0 + s.b.y * k}});
    }
    return out;
}

float CircleDistance(float x, float y, float cx, float cy, float r) { return std::hypot(x - cx, y - cy) - r; }

float BoxDistance(float x, float y, Area b, float top, float bottom) {
    float r = y < (b.y0 + b.y1) / 2 ? top : bottom;
    float hx = (b.x1 - b.x0) / 2 - r, hy = (b.y1 - b.y0) / 2 - r;
    float qx = std::abs(x - (b.x0 + b.x1) / 2) - hx, qy = std::abs(y - (b.y0 + b.y1) / 2) - hy;
    return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}

float EllipseDistance(float x, float y, Area a) {
    float rx = (a.x1 - a.x0) / 2, ry = (a.y1 - a.y0) / 2;
    return (std::hypot((x - (a.x0 + a.x1) / 2) / rx, (y - (a.y0 + a.y1) / 2) / ry) - 1) * std::min(rx, ry);
}

float PolygonDistance(float x, float y, const std::vector<Pt>& p) {
    float d = -1e9f;
    for (size_t i = 0; i < p.size(); i++) {
        Pt a = p[i], b = p[(i + 1) % p.size()];
        float ex = b.x - a.x, ey = b.y - a.y, len = std::hypot(ex, ey);
        d = std::max(d, ((x - a.x) * ey - (y - a.y) * ex) / len);  // outside is to the left of a clockwise edge
    }
    return d;
}

float StrokeDistance(float x, float y, const std::vector<Segment>& strokes, float half) {
    float best = 1e9f;
    for (const Segment& s : strokes) {
        float dx = s.b.x - s.a.x, dy = s.b.y - s.a.y, len = dx * dx + dy * dy;
        float t = len > 0 ? std::clamp(((x - s.a.x) * dx + (y - s.a.y) * dy) / len, 0.0f, 1.0f) : 0;
        best = std::min(best, std::hypot(x - s.a.x - t * dx, y - s.a.y - t * dy));
    }
    return best - half;
}

void Text(Canvas& c, const char* text, float cx, float cy, float height, float stroke, Colour colour) {
    float w;
    std::vector<Segment> s = TextStrokes(text, cx, cy, height, &w);
    float m = stroke + 1;
    Fill(c, {cx - w / 2 - m, cy - height / 2 - m, cx + w / 2 + m, cy + height / 2 + m}, colour,
         [&](float x, float y) { return StrokeDistance(x, y, s, stroke / 2); });
}

}  // namespace eon
