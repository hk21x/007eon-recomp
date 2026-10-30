// The mouse in the menus: the wheel scrolls (d-pad up and down), the left button selects (A), the right goes back (B).
//
// While a menu takes the controller's input (src/game/menu_input.cpp and src/driving/menu_input.cpp mark it), the
// mouse's buttons stop playing what they are bound to in play (fire, lock on) and
// play A and B instead; each notch of the wheel is a press of the d-pad, one after another.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>

#include "core/input.h"
#include "core/log.h"

namespace eon {

std::atomic<int64_t> g_menu_input{-1000};  // when a menu last took the input, ms (src/game/menu_input.cpp)
std::atomic<bool> aim_trigger{false};  // free aim holds the fire input (src/game/free_aim.cpp)
std::atomic<int64_t> aim_release{0};   // let go until then, ms (polls outrun frames: a press needs the game to see it up)

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
    static bool was = false;
    if (menu != was) LOGI("eon: %s", menu ? "a menu takes the input: the mouse plays A, B and the d-pad" : "out of the menus");
    was = menu;
    if (!menu) {
        pending = 0;
        // free aim's right button: the right trigger (src/game/free_aim.cpp), let go for a punch (X, Y), which it turned
        // into moving the gun, and for the left button's press
        if (now < eon::aim_release) {
            p.analog[7] = 0;
        } else if (eon::aim_trigger && !p.analog[2] && !p.analog[3]) {
            p.analog[7] = 255;
        }
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
    if (held & 1) p.analog[0] = 255;  // A
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
