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
#include <cstring>

#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "common/release.h"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app/app.h"
#endif

namespace eon {
extern const bool kDriving;  // driving.xbe's exe (src/driving/hooks.cpp)
extern uint32_t aim_sensitivity;  // the setting, percent (src/common/game_info.cpp)

// driving.xbe: the vehicle's control config (data\control\*.def) is a tank's, the rail tank's or the helicopter's, where
// the right stick turns the camera and the gun with it. The controls manager 0x320E24 (getter 0x28F90): +4 the config in
// use, +0xC its entries, 0x24 bytes each, the name first (0x29F80 picks one by name). The car's is drivecfg, whose
// right stick down looks back, so the mouse is left out there
enum class Vehicle { kOther, kTank, kRailTank, kHeli };
Vehicle DrivingVehicle() {
    constexpr uint32_t kControls = EON_VA(0x320E24, 0x320E04);
    if (!kDriving || !mem::IsCommitted(kControls + 0x10)) return Vehicle::kOther;
    uint32_t index = kernel::R32(kControls + 4), count = kernel::R32(kControls + 8), list = kernel::R32(kControls + 0xC);
    if (!list || index >= count || count > 16 || !mem::IsCommitted(list + index * 0x24 + 0x1F)) return Vehicle::kOther;
    char name[33] = {};
    for (int i = 0; i < 32; i++) name[i] = char(kernel::G<uint8_t>(list + index * 0x24 + i));
    for (char& c : name) c = char(c >= 'A' && c <= 'Z' ? c + 32 : c);
    if (std::strstr(name, "railtank")) return Vehicle::kRailTank;
    if (std::strstr(name, "tank")) return Vehicle::kTank;
    if (std::strstr(name, "heli")) return Vehicle::kHeli;
    return Vehicle::kOther;
}

std::atomic<int64_t> g_menu_input{-1000};  // when a menu last took the input, ms (src/game/menu_input.cpp)
extern bool free_aim;                 // the setting (src/common/game_info.cpp)
std::atomic<bool> aim_trigger{false};  // free aim holds the fire input (src/game/free_aim.cpp)
std::atomic<int64_t> reload_fire_from{0}, reload_fire_until{0};  // R's trigger pull, ms (src/game/free_aim.cpp)
std::atomic<int64_t> aim_release{0};
std::atomic<int64_t> driving_on_foot_ms{-1000};
std::atomic<float> drive_turn{0}, drive_tilt{0};  // the mouse for the tanks' aim camera, degrees (src/driving/aim_camera.cpp)
std::atomic<float> car_look_yaw{0}, car_look_pitch{0};  // a car's free look, degrees, right and down positive (src/driving/car_camera.cpp)
std::atomic<int64_t> car_look_ms{-1000};               // when the mouse last moved it
std::atomic<int64_t> drive_aim_camera_ms{-1000};  // Bond on foot in a driving mission (src/driving/on_foot.cpp)
std::atomic<int64_t> lock_tap_ms{-1000};  // a tap of the lock, LT: a scope opened or closed (src/game/free_aim.cpp)
std::atomic<bool> scope_by_right{false};  // the right button opened the scope (src/game/free_aim.cpp): closed on its let go
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
    // one controller seen on two ports (issue #32: a pad Windows offers through two interfaces; not proven for the
    // reporter's): the menus take every port, each with its own press and repeat, so each tap moved two rows (measured
    // headless with port 1 given port 0's input). In a menu, a later port holding just what port 1's controller holds
    // is let go; two players' pads differ, and in play nothing changes
    static input::Pad first{};
    if (port == 0) first = p;
    if (port != 0) {
        const bool in_menu = NowMs() - g_menu_input.load() <= kStale;
        const auto near = [](int a, int b) { return std::abs(a - b) < 6000; };
        bool pressed = first.buttons != 0 || std::abs(int(first.lx)) > 12000 || std::abs(int(first.ly)) > 12000;
        for (uint8_t a : first.analog) pressed |= a > 64;
        bool same = p.buttons == first.buttons && near(p.lx, first.lx) && near(p.ly, first.ly);
        for (int i = 0; i < 8; i++) same &= (p.analog[i] > 64) == (first.analog[i] > 64);
        if (in_menu && pressed && same) {
            static int told = 0;
            if (told++ < 3) LOGR("eon: menus: controller %d holds exactly what controller 1 does, taken as the same pad", port + 1);
            const bool connected = p.connected;
            p = input::Pad{};
            p.connected = connected;
        }
        return;
    }
    static int pending = 0;  // notches to play, up positive
    static int64_t pulse = 0;
    int notches = input::TakeWheel();  // taken in play too, so none wait for the next menu
    uint32_t held = input::MouseButtons();
    int64_t now = NowMs();
    bool menu = now - g_menu_input.load() <= kStale;
    // a scope the right button opened closes on its let go, a tap of the lock: here, as Bond's update (where it was
    // opened) does not run while the scope is up (Harry's log: none from scoping in to letting go)
    if (eon::scope_by_right && !(held & 2)) {
        eon::scope_by_right = false;
        eon::lock_tap_ms = now;
        LOGR("eon: scope: the right button let go, the lock tapped");
    }
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
        // a driving mission's tank, rail tank or helicopter (A Show of Force): the mouse turns the gun, as the right
        // stick (a rate in full deflections, scaled by Aim sensitivity); the game's invert options apply to it there
        Vehicle vehicle = eon::kDriving && input::UsingKeyboard() ? DrivingVehicle() : Vehicle::kOther;
        float mx, my;
        static auto last_take = std::chrono::steady_clock::now();
        auto take_now = std::chrono::steady_clock::now();
        float dt = std::clamp(std::chrono::duration<float>(take_now - last_take).count(), 0.001f, 0.1f);
        last_take = take_now;
        // a car (Harry): the mouse swings the chase camera round it (src/driving/car_camera.cpp), which eases back behind
        // the car once the mouse rests; up looks up (the camera lowered), as on foot, Invert mouse flipping it
        const bool foot = eon::kDriving && now - eon::driving_on_foot_ms.load() < 250;
        const bool in_car = eon::kDriving && input::UsingKeyboard() && vehicle == Vehicle::kOther && !foot;
        // on foot in a driving mission (Train Chase) the right stick turns the camera, so the mouse plays it, as in the
        // helicopter (Harry); its invert is Invert mouse's (input::TakeMouse) over the game's own
        if (in_car && input::TakeMouse(mx, my)) {
            constexpr float kFullRate = 2400, kDegreesPerCount = 0.04f;  // as the tanks' camera below
            if (mx != 0 || my != 0) {
                eon::car_look_yaw = std::clamp(eon::car_look_yaw + mx * kFullRate * dt * kDegreesPerCount, -150.0f, 150.0f);
                eon::car_look_pitch = std::clamp(eon::car_look_pitch - my * kFullRate * dt * kDegreesPerCount, -10.0f, 35.0f);
                eon::car_look_ms = now;
            }
        } else if ((vehicle != Vehicle::kOther || (foot && input::UsingKeyboard())) && input::TakeMouse(mx, my)) {
            if (vehicle != Vehicle::kHeli && !foot && now - eon::drive_aim_camera_ms.load() < 250) {
                // the tanks' aim camera turns by the mouse directly (src/driving/aim_camera.cpp): its counts (a rate
                // in full deflections, 2400 counts a second) at the camera's 0.04 degrees a count on foot
                constexpr float kFullRate = 2400, kDegreesPerCount = 0.04f;
                eon::drive_turn = eon::drive_turn + mx * kFullRate * dt * kDegreesPerCount;
                eon::drive_tilt = eon::drive_tilt + my * kFullRate * dt * kDegreesPerCount;
            } else {
                // on foot (Train Chase) the stick's camera is slow for a mouse: three times as much there (Harry: too slow)
                constexpr float kFootGain = 3;
                float k = float(eon::aim_sensitivity) / 100 * 32767 * (foot ? kFootGain : 1);
                p.rx = int16_t(std::clamp(float(p.rx) + mx * k, -32767.0f, 32767.0f));
                p.ry = int16_t(std::clamp(float(p.ry) + my * k, -32767.0f, 32767.0f));
            }
        }
        // the rail tank (A Show of Force) fires with RT and aims with LT, where the car's keys have gas and brake (W,
        // S) and the mouse's buttons play A and X, which it does not use: there the left button fires and the right
        // aims, and W and S (no gas on rails) do nothing. The tank and the helicopter fire with A, the left button.
        if (vehicle == Vehicle::kRailTank) {
            p.analog[input::kRT - input::kA] = (held & 1) ? 255 : 0;
            p.analog[input::kLT - input::kA] = (held & 2) ? 255 : 0;
        }
#ifdef _WIN32
        // the helicopter (Sand Storm) climbs and descends on the left stick's y (STEERVERTICAL; the down arrow climbs):
        // E climbs and Q descends (Harry), and E no longer plays the gadget there (X: the right button alone)
        if (vehicle == Vehicle::kHeli) {
            bool up = GetAsyncKeyState('E') & 0x8000, down = GetAsyncKeyState('Q') & 0x8000;
            if (up != down) p.ly = up ? -32767 : 32767;
            p.analog[input::kX - input::kA] = (held & 2) ? 255 : 0;
        }
#endif
#ifdef _WIN32
        // on foot in a driving mission (Train Chase) the gas and brake keys (W, S) also walk him: the left stick's up
        // and down, his walk there, unless the stick is already pushed (the arrows, or a pad). The keys themselves are
        // read, and a pad's stick at rest is a little off centre: with one plugged in, W and S did nothing (Harry)
        constexpr int kStickRest = 8000;  // about a quarter: a resting stick is well inside it
        if (eon::kDriving && now - eon::driving_on_foot_ms.load() < 250 && std::abs(int(p.ly)) < kStickRest) {
            auto held_key = [](int c) {
                for (uint16_t k : input::ControlKeys(c))
                    if (k && (GetAsyncKeyState(k) & 0x8000)) return true;
                return false;
            };
            bool fwd = held_key(input::kRT), back = held_key(input::kLT);
            if (fwd != back) p.ly = fwd ? 32767 : -32767;
        }
#endif
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
