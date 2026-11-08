// The menus' pointer: in the menus the Windows arrow shows, the item under it takes the focus, and a click selects it.
//
// The menus' engine (EA FEng) has a mouse path of its own that EoN leaves off (traced by a research agent): its manager
// (the object 0x131610 runs on) has a byte at +2, cleared at its start (0x130B94) and set nowhere; set, each package's
// input (0x132000, per package from 0x132519) reads the mouse through the interface's vtable +0x48 (0x121E20 here: a
// bare ret 4) into the manager's +0xC (x, y, the wheel and the buttons, as shorts), and asks the interface's vtable
// +0x4C (0xD24C0: answers false) whether the point is on a button. When the mouse moves off the button in focus onto
// another it gives that one the focus as the d-pad does (0x132C80: lose and gain, the focus sound); the left button's
// press accepts the button under it, or plays A on the page (the same messages as A, 0x131610).
//
// The positions: the package's buttons (+0x8C, their count +0x90; the packages listed from the manager's +0xE4, the
// next at +4) are placed by 0x13DD50 (cdecl: the object, the interface, two floats out) in a 640x480 frame with its
// middle at 0, y down; an object's data (+0x2C) holds its size at +0x38/+0x3C; flag 0x4000000 (+0x1C) disables it.
// Most buttons are a small square at the right of their row (x about 280) with the row's text elsewhere, so a button
// takes its row: up to 15 above and below (half the gap to the next button when closer), from the screen's left to its
// right edge, a row shared with others split at the midpoints between them (Controller and PC Settings, a grid). A big
// button and a text one (type 2, +0x18) take their own rectangle; a page with one button takes the whole screen.
//
// The menus' 2D is laid out for 16:9 and squeezed into a 16:9 middle on a wider screen (GameInfo::hud_aspect).
// A left click on the focused row's arrow (a value's left and right) plays the d-pad's left or right instead.
// Only default.xbe's menus: the driving exe has its own copy of the engine.
// EON_POINTER (headless): a file named pointer in the run folder holds "x y buttons", the 640x480 frame's, kept.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#include "common/release.h"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eon {
int64_t NowMs();
extern std::atomic<int64_t> pointer_ms;  // when the menus last took the pointer (src/common/menu_mouse.cpp)
extern std::atomic<int> pointer_arrow;      // a click on a row's arrow: -1 left, 1 right (src/common/menu_mouse.cpp)
extern std::atomic<int64_t> pointer_arrow_ms;
std::atomic<uint32_t> menu_manager{0};   // the menus' manager (src/game/menu_input.cpp)
}

namespace {

constexpr uint32_t kPlace = EON_VA(0x13DD50, 0x13D650);
constexpr uint32_t kDisabled = 0x4000000;
constexpr float kRowHalf = 15, kRowLeft = 640, kTextHalf = 38;  // a row reaches the screen's left
// two buttons at one x less than kStacked apart are one row: the pause menu's Controller and the PC Settings clone
// beside it, whose button is 3 lower for the focus's order (pc_settings.cpp EditPauseOptions); split kSplit left of x
constexpr float kStacked = 6, kSplit = 300;
// a value row's arrows, the frame's x with its middle at 0 (the pause menu's Options and PC Settings, measured: 344
// and 498 of 640), and how near a click is on one
constexpr float kArrowLeft = 24, kArrowRight = 178, kArrowNear = 20, kArrowRow = 13;

// the pointer in the 640x480 frame (0 to 640, 0 to 480), and the left button; false when there is none
bool PointerAt(float& x, float& y, bool& left) {
    static const bool test = std::getenv("EON_POINTER") != nullptr;
    if (test) {
        std::FILE* f = std::fopen("pointer", "rb");
        if (!f) return false;
        int b = 0;
        bool ok = std::fscanf(f, "%f %f %d", &x, &y, &b) == 3;
        std::fclose(f);
        left = b & 1;
        return ok;
    }
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(app::Window());
    POINT c;
    RECT r;
    if (!input::UsingKeyboard() || GetForegroundWindow() != hwnd || !GetCursorPos(&c) || !ScreenToClient(hwnd, &c) ||
        !GetClientRect(hwnd, &r) || r.right <= 0 || r.bottom <= 0)
        return false;
    float wide = std::max(1.0f, app::FrameAspect() / (16.0f / 9.0f));  // the 2D's 16:9 in the frame's middle
    x = 640 * (0.5f + (float(c.x) / r.right - 0.5f) * wide);
    y = 480 * float(c.y) / r.bottom;
    left = input::MouseButtons() & 1;
    return true;
#else
    return false;
#endif
}

struct Place {
    float x, y, w, h;
    bool on;
};

Place PlaceOf(X86Context& ctx, uint32_t obj, uint32_t iface) {
    Place p{};
    uint32_t data = kernel::R32(obj + 0x2C);
    if (!data) return p;
    uint32_t keep_esp = ctx.esp, keep_ecx = ctx.ecx;
    ctx.esp -= 16;
    uint32_t out = ctx.esp;
    kernel::CallGuest(ctx, kPlace, {obj, iface, out});
    p.x = kernel::G<float>(out);
    p.y = kernel::G<float>(out + 4);
    ctx.esp = keep_esp;
    ctx.ecx = keep_ecx;
    p.w = std::fabs(kernel::G<float>(data + 0x38));
    p.h = std::fabs(kernel::G<float>(data + 0x3C));
    p.on = !(kernel::R32(obj + 0x1C) & kDisabled);
    return p;
}

// the package whose buttons hold obj, 0 if none
uint32_t PackageOf(uint32_t mgr, uint32_t obj) {
    uint32_t pkg = kernel::R32(mgr + 0xE4);
    for (int i = 0; pkg && i < 64; i++, pkg = kernel::R32(pkg + 4)) {
        uint32_t list = kernel::R32(pkg + 0x8C), count = kernel::R32(pkg + 0x90);
        for (uint32_t j = 0; list && j < count && j < 128; j++)
            if (kernel::R32(list + 4 * j) == obj) return pkg;
    }
    return 0;
}

bool Hit(X86Context& ctx, uint32_t mgr, uint32_t iface, uint32_t obj, float x, float y) {
    uint32_t pkg = PackageOf(mgr, obj);
    if (!pkg) return false;
    Place me = PlaceOf(ctx, obj, iface);
    if (!me.on) return false;
    uint32_t list = kernel::R32(pkg + 0x8C), count = kernel::R32(pkg + 0x90);
    int enabled = 0;
    float gap = 1e9f, left = me.x - kRowLeft, right = me.x + me.w / 2 + 10;
    bool stacked = false, upper = false;
    float row_y = me.y;
    for (uint32_t j = 0; j < count && j < 128; j++) {
        uint32_t b = kernel::R32(list + 4 * j);
        if (!b) continue;
        Place o = PlaceOf(ctx, b, iface);
        if (!o.on) continue;
        enabled++;
        if (b == obj) continue;
        float dy = std::fabs(o.y - me.y);
        if (dy >= 2 && dy < kStacked && std::fabs(o.x - me.x) < 2) {  // stacked: one row, the upper one's (below)
            stacked = true;
            row_y = std::min(o.y, me.y);
            upper = me.y < o.y;
        } else if (dy >= 2) {
            gap = std::min(gap, dy);
        } else if (o.x < me.x) {  // a row shared (Controller and PC Settings, a grid): split at the midpoints
            left = std::max(left, (o.x + me.x) / 2);
        } else {
            right = std::min(right, (o.x + me.x) / 2);
        }
    }
    if (enabled == 1) return true;  // one button: a click anywhere is it
    bool text = kernel::R32(obj + 0x18) == 2;
    if (text || me.w > 60 || me.h > 40) {
        float hw = text && me.w < 1 ? kTextHalf : std::max(me.w / 2, 12.0f);
        float hh = std::max(me.h / 2, 10.0f);
        return std::fabs(x - me.x) <= hw && std::fabs(y - me.y) <= hh;
    }
    float half = std::min(kRowHalf, gap / 2);
    if (stacked) {  // Controller's row with PC Settings beside it (src/game/pc_settings.cpp): left and right of kSplit
        if (std::fabs(y - row_y) > half) return false;
        return upper ? x >= me.x - kRowLeft && x < me.x - kSplit : x >= me.x - kSplit && x <= right;
    }
    return std::fabs(y - me.y) <= half && x >= left && x <= right;
}

// a left press on an arrow of the row in focus: -1 the left one, 1 the right, 0 none
int ArrowAt(X86Context& ctx, float x, float y) {
    uint32_t mgr = eon::menu_manager;
    if (!mgr || !mem::IsCommitted(mgr + 0x10C)) return 0;  // kept from a menu since gone
    uint32_t iface = mgr ? kernel::R32(mgr + 0x10C) : 0;
    if (!iface) return 0;
    uint32_t pkg = kernel::R32(mgr + 0xE4);
    for (int i = 0; pkg && i < 64; i++, pkg = kernel::R32(pkg + 4)) {
        uint32_t focus = kernel::R32(pkg + 0x84);
        if (!focus || !(kernel::R32(pkg + 0x20) & 1)) continue;
        Place b = PlaceOf(ctx, focus, iface);
        if (!b.on || std::fabs(y - b.y) > kArrowRow) continue;
        if (std::fabs(x - kArrowLeft) <= kArrowNear) return -1;
        if (std::fabs(x - kArrowRight) <= kArrowNear) return 1;
    }
    return 0;
}

}  // namespace

// the interface's mouse (thiscall: shorts out: x, y, the wheel, the buttons): the pointer in the frame's 640x480 with
// its middle at 0, as the buttons are placed; the wheel stays with src/common/menu_mouse.cpp (the d-pad)
X86_EXTERN_FUNC(EON_ORIG(00121E20, 00121720));
X86_FUNC(EON_FN(00121E20, 00121720)) {
    uint32_t out = kernel::R32(ctx.esp + 4);
    float x, y;
    bool left;
    if (out && PointerAt(x, y, left)) {
        // a click on the focused row's arrow plays the d-pad's left or right (menu_mouse.cpp), not the engine's accept
        static bool left_was = false, on_arrow = false;
        if (left && !left_was) {
            int arrow = ArrowAt(ctx, x - 320, y - 240);
            on_arrow = arrow != 0;
            if (on_arrow) {
                eon::pointer_arrow = arrow;
                eon::pointer_arrow_ms = eon::NowMs();
            }
        }
        left_was = left;
        if (on_arrow) left = false;
        kernel::G<uint16_t>(out) = uint16_t(int16_t(std::lround(x - 320)));
        kernel::G<uint16_t>(out + 2) = uint16_t(int16_t(std::lround(y - 240)));
        kernel::G<uint16_t>(out + 4) = 0;
        kernel::G<uint16_t>(out + 6) = left ? 1 : 0;
        eon::pointer_ms = eon::NowMs();
    }
    EON_ORIG(00121E20, 00121720)(ctx, base);
}

// the interface's hit test (thiscall: x, y as floats, the object): whether the point is on the button
X86_EXTERN_FUNC(EON_ORIG(000D24C0, 000D2040));
X86_FUNC(EON_FN(000D24C0, 000D2040)) {
    uint32_t mgr = eon::menu_manager;
    uint32_t iface = mgr && mem::IsCommitted(mgr + 0x10C) ? kernel::R32(mgr + 0x10C) : 0;
    if (!iface || ctx.ecx != iface) return EON_ORIG(000D24C0, 000D2040)(ctx, base);
    float x, y;
    std::memcpy(&x, &kernel::G<uint32_t>(ctx.esp + 4), 4);
    std::memcpy(&y, &kernel::G<uint32_t>(ctx.esp + 8), 4);
    uint32_t obj = kernel::R32(ctx.esp + 12);
    bool hit = obj && Hit(ctx, mgr, iface, obj, x, y);
    kernel::Return(ctx, hit ? 1 : 0, 3);
}
