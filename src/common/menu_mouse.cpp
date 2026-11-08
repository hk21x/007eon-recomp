// The mouse in the menus: the wheel scrolls (d-pad up and down), the left button selects (A; on foot the item under the
// arrow, src/game/pointer.cpp), the right goes back (B).
//
// While a menu takes the controller's input (src/game/menu_input.cpp and src/driving/menu_input.cpp mark it), the
// mouse's buttons stop playing what they are bound to in play (fire, lock on) and
// play A and B instead; each notch of the wheel is a press of the d-pad, one after another.
#include <algorithm>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <cstdint>

#include "core/input.h"
#include "core/log.h"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app/app.h"
#endif

namespace eon {
extern const bool kDriving;  // driving.xbe's exe (src/driving/hooks.cpp)

std::atomic<int64_t> g_menu_input{-1000};  // when a menu last took the input, ms (src/game/menu_input.cpp)
extern bool free_aim;                 // the setting (src/common/game_info.cpp)
std::atomic<bool> aim_trigger{false};  // free aim holds the fire input (src/game/free_aim.cpp)
std::atomic<int64_t> reload_fire_from{0}, reload_fire_until{0};  // R's trigger pull, ms (src/game/free_aim.cpp)
std::atomic<int64_t> aim_release{0};
std::atomic<int64_t> lock_tap_ms{-1000};  // a tap of the lock, LT: a scope opened or closed (src/game/free_aim.cpp)
std::atomic<int64_t> pointer_ms{-1000};  // when the menus' engine last took the pointer (src/game/pointer.cpp)
std::atomic<int> pointer_arrow{0};        // a click on a value row's arrow: -1 left, 1 right (src/game/pointer.cpp)
std::atomic<int64_t> pointer_arrow_ms{-1000};
std::atomic<bool> zoom_wheel{false};   // scoped in or on a mounted gun: the wheel zooms (src/game/zoom.cpp)
std::atomic<int> zoom_notches{0};      // the wheel's notches waiting for the zoom, in positive   // let go until then, ms (polls outrun frames: a press needs the game to see it up)

#ifdef _WIN32
// the arrow while the menus take the pointer: the runtime hides the cursor while the mouse looks (as Agent Under Fire)
WNDPROC g_proc = nullptr;
std::atomic<bool> g_arrow{false};
LRESULT CALLBACK PointerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT && g_arrow.load() && input::MouseLooking()) {
        SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW)));
        return TRUE;
    }
    return CallWindowProcW(g_proc, hwnd, msg, wp, lp);
}
void ShowArrow(bool show) {
    HWND hwnd = static_cast<HWND>(app::Window());
    if (!hwnd) return;
    if (!g_proc) {
        g_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PointerProc)));
        LOGR("eon: the menus' pointer is ready");
    }
    if (g_arrow.exchange(show) != show) PostMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}
#endif

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void MenuMouse(int port, input::Pad& p) {
    constexpr int64_t kStale = 250, kPress = 70, kGap = 70;  // ms
    constexpr uint16_t kMouseBits[3] = {0x01, 0x02, 0x04};    // VK_LBUTTON VK_RBUTTON VK_MBUTTON
    if (port != 0) return;
    static int pending = 0;  // notches to play, up positive
    static int64_t pulse = 0;
    int notches = input::TakeWheel();  // taken in play too, so none wait for the next menu
    uint32_t held = input::MouseButtons();
    int64_t now = NowMs();
    bool menu = now - g_menu_input.load() <= kStale;
#ifdef _WIN32
    ShowArrow(menu && now - eon::pointer_ms.load() < 250);
#endif
    static bool was = false;
    if (menu != was) LOGI("eon: %s", menu ? "a menu takes the input: the mouse plays A, B and the d-pad" : "out of the menus");
    was = menu;
    // in play with free aim, the wheel: the next weapon a notch (CYCLEWEAPONSFORWARD, the d-pad's down; the game has
    // no previous weapon, so both ways go forward); in the driving missions Black, their next weapon, free aim or not
    static int weapons = 0;
    static int64_t weapon_pulse = 0;
    if (!menu && eon::zoom_wheel && notches) {  // a scope or a mounted gun: the wheel zooms instead
        eon::zoom_notches += notches;
        notches = 0;
    }
    if (!menu && (eon::free_aim || eon::kDriving) && notches) {
        if (!weapons) weapon_pulse = now;
        weapons = std::min(weapons + std::abs(notches), 4);
    }
    if (menu) weapons = 0;
    if (!menu) {
        pending = 0;
        if (weapons) {
            int64_t t = now - weapon_pulse;
            if (t < kPress) {
                if (eon::kDriving) p.analog[input::kBlack - input::kA] = 255;  // driving: Black, its next weapon
                else p.buttons |= uint16_t(1u << input::kDown);
            } else if (t >= kPress + kGap) {
                weapons--;
                weapon_pulse = now;
            }
        }
        // free aim's right button: the right trigger (src/game/free_aim.cpp), let go for a punch (X, Y), which it turned
        // into moving the gun, and for the left button's press
        if (now < eon::aim_release) {
            p.analog[7] = 0;
        } else if (eon::aim_trigger && !p.analog[2] && !p.analog[3]) {
            p.analog[7] = 255;
        }
        if (now >= eon::reload_fire_from && now < eon::reload_fire_until) p.analog[7] = 255;  // R: the empty gun's pull
        if (now - eon::lock_tap_ms.load() < kPress) p.analog[6] = 255;  // the right button's scope: LT tapped
#ifdef _WIN32
        // free aim's number keys pick weapons (src/game/weapon_keys.cpp): the d-pad they are also bound to (1 to 4 by
        // default) is let go when only a number key holds it, so the gadgets do not cycle too
        if (eon::free_aim && input::UsingKeyboard()) {
            for (int c = input::kUp; c <= input::kRight; c++) {
                if (!(p.buttons >> c & 1)) continue;
                bool number = false, other = false;
                for (uint16_t k : input::ControlKeys(c)) {
                    if (!k || !(GetAsyncKeyState(k) & 0x8000)) continue;
                    (k >= '1' && k <= '9' ? number : other) = true;
                }
                if (number && !other) p.buttons = uint16_t(p.buttons & ~(1u << c));
            }
        }
#endif
        return;
    }
    // the buttons out of what they play in play
    for (int c = 0; c < input::kA + 8; c++) {
        auto keys = input::ControlKeys(c);
        bool mouse = false;
        for (int b = 0; b < 3; b++)
            if (held >> b & 1) mouse |= keys[0] == kMouseBits[b] || keys[1] == kMouseBits[b];
        if (!mouse) continue;
        if (c < input::kA) p.buttons = uint16_t(p.buttons & ~(1u << c));
        else p.analog[c - input::kA] = 0;
    }
    // the left button is A, unless the menus' engine takes the pointer: then it accepts what it is over itself
    // (src/game/pointer.cpp), or every click accepted twice
    bool pointer = now - eon::pointer_ms.load() < 250;
    if ((held & 1) && !pointer) p.analog[0] = 255;  // A
    if (now - eon::pointer_arrow_ms.load() < kPress)  // a click on a row's arrow: the d-pad's left or right
        p.buttons |= uint16_t(1u << (eon::pointer_arrow.load() < 0 ? input::kLeft : input::kRight));
    if (held & 2) p.analog[1] = 255;  // B
    // the wheel: a d-pad press a notch
    if (notches && !pending) pulse = now;
    pending = std::clamp(pending + notches, -8, 8);
    if (pending) {
        int64_t t = now - pulse;
        if (t < kPress) {
            p.buttons |= uint16_t(1u << (pending > 0 ? input::kUp : input::kDown));
        } else if (t >= kPress + kGap) {
            pending += pending > 0 ? -1 : 1;
            pulse = now;
        }
    }
}

}  // namespace eon
