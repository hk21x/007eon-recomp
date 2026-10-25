#include <array>
#include <cctype>
#include <cstdlib>
#include <string>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"

namespace eon {
bool skip_intro = false;  // no logo movies at start (HideFile)
bool driving_60fps = true;   // the driving missions at 60 frames a second (src/driving/sixty.cpp)
uint32_t fov_percent = 100;  // the on-foot views' field of view, percent of the game's (src/game/fov.cpp)
extern bool modern_buttons;           // src/common/buttons.cpp
bool ButtonPictures(uint32_t header, uint32_t palette);  // GameInfo::texture_bound
}

namespace {
// EON_SKIP_INTRO=1, EON_MODERN_BUTTONS and EON_DRIVING60 (0 or 1) and EON_FOV (percent) set those options without a
// settings file (the headless harness has none)
void Init() {
    if (const char* e = std::getenv("EON_SKIP_INTRO"); e && *e == '1') eon::skip_intro = true;
    if (const char* e = std::getenv("EON_MODERN_BUTTONS")) eon::modern_buttons = *e == '1';
    if (const char* e = std::getenv("EON_DRIVING60")) eon::driving_60fps = *e == '1';
    if (const char* e = std::getenv("EON_FOV")) eon::fov_percent = uint32_t(std::atoi(e));
    if (eon::skip_intro) LOGR("eon: the intro logos are skipped (skip_intro)");
}

// skip_intro: the boot logos (THX, MGM, the EA logo and its language versions eagdut, eagita, eagswe) reported
// missing, which the game passes over as it did the EA logo when it asked for an NTSC file
bool HideFile(const std::string& path) {
    if (!eon::skip_intro) return false;
    std::string n;
    for (char c : path) n += char(std::tolower(static_cast<unsigned char>(c)));
    size_t slash = n.rfind('\\');
    if (n.find("\\movies\\") == std::string::npos || slash == std::string::npos) return false;
    std::string file = n.substr(slash + 1);
    for (const char* logo : {"thxlogo", "mgm_", "eag"})
        if (file.compare(0, std::char_traits<char>::length(logo), logo) == 0) return true;
    return false;
}

// keyboard and mouse, as a shooter plays (Windows virtual key codes: this file builds for the headless harness too)
constexpr uint16_t kMouseLeft = 0x01, kMouseRight = 0x02, kMouseMiddle = 0x04, kTab = 0x09, kEnter = 0x0D,
                   kEscape = 0x1B, kSpace = 0x20, kArrowLeft = 0x25, kArrowUp = 0x26, kArrowRight = 0x27, kArrowDown = 0x28,
                   kLeftShift = 0xA0, kLeftCtrl = 0xA2;
std::array<uint16_t, 2> ShooterKeys(int c) {
    using namespace input;
    switch (c) {
        case kLSUp: return {'W', 0};
        case kLSDown: return {'S', 0};
        case kLSLeft: return {'A', 0};
        case kLSRight: return {'D', 0};
        case kRT: return {kMouseLeft, 0};    // fire
        case kLT: return {kMouseRight, 0};   // lock on
        case kA: return {kSpace, kEnter};
        case kB: return {kLeftCtrl, 0};
        case kX: return {'Q', 0};            // punch left
        case kY: return {'E', 0};            // punch right
        case kBlack: return {'C', 0};        // wall cover
        case kWhite: return {'F', 0};
        case kUp: return {kArrowUp, '1'};         // the d-pad: weapons, gadgets and the menus
        case kDown: return {kArrowDown, '2'};
        case kLeft: return {kArrowLeft, '3'};
        case kRight: return {kArrowRight, '4'};
        case kStart: return {kEscape, 'P'};
        case kBack: return {kTab, 0};
        case kLThumb: return {'V', kLeftShift};
        case kRThumb: return {kMouseMiddle, 0};
        default: return {0, 0};
    }
}
}  // namespace

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"007: Everything or Nothing",  // window and splash
    .short_title = L"EoN",                // message boxes
    .name = L"EoN",                       // window classes, crash dumps, handover event
    .log = "eon",                         // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "EoN"}, {"driving.xbe", "EoNDriving"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"skip_intro", &eon::skip_intro, nullptr, "Skip intro movies",
                 "The THX, MGM and EA logos; at the next start."},
                {"modern_buttons", &eon::modern_buttons, nullptr, "Modern button pictures",
                 "Today's Xbox pad in the prompts (the keys and mouse while playing on the keyboard either way)."},
                {"fov_percent", nullptr, &eon::fov_percent, "Field of view (%)",
                 "On foot, 50 to 150; 100 is the game's. Cutscenes widen too."},
                {"driving_60fps", &eon::driving_60fps, nullptr, "Driving missions at 60 frames a second",
                 "Same simulation, drawn after every tick instead of every second one; from the next driving mission."}},  // kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = Init,                         // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = nullptr,                       // changes a polled pad
    .rumble = nullptr,                    // changes a rumble before it is set
    .squeeze_screen_programs = true,      // ultrawide: the HUD and menus are drawn by a pass-through program
    .pal_console = true,                  // a PAL disc's movies are _pal only: a PAL 60 Hz console for it
    .hide_file = HideFile,                // skip_intro
    .mouse_look = true,                   // the mouse aims
    .mouse_direct = true,                 // straight into the game's look input (src/game/mouse.cpp)
    .default_keys = ShooterKeys,          // a shooter's keys and mouse buttons
    .texture_bound = eon::ButtonPictures, // modern_buttons
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
