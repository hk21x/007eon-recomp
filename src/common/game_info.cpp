#include <array>
#include <cctype>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <toml++/toml.hpp>
#endif

#include "app/app.h"
#include "core/achievements.h"
#include "core/input.h"
#include "core/log.h"
#include "common/language.h"
#include "common/release.h"

namespace eon {
extern const bool kDriving;  // driving.xbe's exe (src/driving/hooks.cpp), default.xbe's false (src/game/hooks.cpp)
bool skip_intro = false;  // no logo movies at start (HideFile)
bool driving_60fps = true;   // the driving missions at 60 frames a second (src/driving/sixty.cpp)
bool hide_hud = false;       // no HUD, for screenshots (src/driving/hud.cpp, src/game/hud.cpp)
uint32_t fov_percent = 100;  // the on-foot views' field of view, percent of the game's (src/game/fov.cpp)
extern bool modern_buttons;           // src/common/buttons.cpp
bool ButtonPictures(uint32_t header, uint32_t palette);  // GameInfo::texture_bound
void TitleScreen(uint32_t header);                       // src/common/title_screen.cpp (default.xbe's)
void MenuMouse(int port, input::Pad& p);                 // GameInfo::pad: the mouse in the menus
std::vector<achievements::Def> AchievementSet();         // src/common/achievement_set.cpp
}

namespace {
// EON_SKIP_INTRO=1, EON_HIDE_HUD=1, EON_MODERN_BUTTONS and EON_DRIVING60 (0 or 1) and EON_FOV (percent) set those options without a
// settings file (the headless harness has none)
void Init() {
    if (const char* e = std::getenv("EON_SKIP_INTRO"); e && *e == '1') eon::skip_intro = true;
    if (const char* e = std::getenv("EON_MODERN_BUTTONS")) eon::modern_buttons = *e == '1';
    if (const char* e = std::getenv("EON_DRIVING60")) eon::driving_60fps = *e == '1';
    if (const char* e = std::getenv("EON_HIDE_HUD"); e && *e == '1') eon::hide_hud = true;
    if (const char* e = std::getenv("EON_FOV")) eon::fov_percent = uint32_t(std::atoi(e));
    if (const char* e = std::getenv("EON_LANGUAGE")) eon::language = uint32_t(std::atoi(e));
    if (eon::skip_intro) LOGR("eon: the intro logos are skipped (skip_intro)");
    // every achievement's condition parses (a typo would leave it never met)
    for (const achievements::Def& d : app::kGame.achievements)
        if (std::string e = achievements::Check(d.condition); !e.empty()) LOGE("eon: achievement %s: %s", d.id, e.c_str());
}

// skip_intro: the boot logos (THX, MGM, the EA logo and its language versions eagdut, eagita, eagswe) reported
// missing, which the game passes over as it did the EA logo when it asked for an NTSC file
// the xbes with this build's code. Europe: the media check patched (0x63C73 and 0x2661E8 JMP for JGE, after
// XapiVerifyMediaInDrive, which the runtime answers 0), the En/Es/It/Nl/Sv disc, the Fr/De disc (version 4, its
// certificate and signature differ). USA: its one disc. default.xbe then driving.xbe each
std::vector<const char*> SameCode() {
#if EON_USA
    return {"1f4db3b7d3f5102ec7a2351e521f545ef4972be6", "ef1f8404d459feda02fbbe1ea8641569015280d7"};
#else
    return {"62298fe4e618d9d81863080fb1b00c1667d871c9", "da2b27d66d4b5f23af47421b9572fcad06669d22",
            "65fe0c47889bae5732b93e36215b564c94df9e9f", "93390d82f2a7dd5371fd6d7b725577e806e4f09e",
            "0d2acd66ea75949254916f1f91f7bf31beac9bbe", "08a873cfff115a77164405f7f93890e04ad395f0"};
#endif
}

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
                   kLeftShift = 0xA0, kLeftCtrl = 0xA2, kBackspace = 0x08;
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

// the driving missions, as their own control map has them (driving\misc.viv: RT gas, LT brake and reverse, B handbrake,
// A the weapon, X the gadget, Y action, BLACK and the d-pad's left and right the next weapon, WHITE the full screen map,
// the right stick down to look back and its click to centre the camera, the d-pad's up the camera, BACK Bond sense):
// a racing game's keys, the mouse buttons firing (the mouse does not steer)
std::array<uint16_t, 2> DrivingKeys(int c) {
    using namespace input;
    switch (c) {
        case kRT: return {'W', 0};                   // gas
        case kLT: return {'S', 0};                   // brake, reverse
        case kLSLeft: return {'A', kArrowLeft};      // steer
        case kLSRight: return {'D', kArrowRight};
        case kLSUp: return {kArrowUp, 0};
        case kLSDown: return {kArrowDown, 0};
        case kB: return {kSpace, kBackspace};        // handbrake; back in the menus
        case kA: return {kMouseLeft, kEnter};        // the weapon; select in the menus
        case kX: return {kMouseRight, 'E'};          // the gadget
        case kY: return {'F', 0};                    // action
        case kBlack: return {'R', 0};                // the next weapon
        case kWhite: return {'M', 0};                // the full screen map
        case kRSDown: return {'C', 0};               // look back
        case kRThumb: return {'V', kMouseMiddle};    // the camera behind the car
        case kUp: return {'1', 0};                   // the camera
        case kDown: return {'2', 0};
        case kLeft: return {'3', 0};                 // the weapons
        case kRight: return {'4', 0};
        case kStart: return {kEscape, 'P'};
        case kBack: return {kTab, 0};                // Bond sense
        default: return {0, 0};
    }
}

std::array<uint16_t, 2> Keys(int c) { return eon::kDriving ? DrivingKeys(c) : ShooterKeys(c); }

#ifdef _WIN32  // the settings file (the headless harness has none)
// One settings file for both exes, each with its own keys: the runtime reads and writes [keys] as this exe's (only
// what differs from its defaults); the driving exe's are kept as [driving_keys] instead, and each exe writes the
// other's table back as it read it.
constexpr const char* kOtherKeys[2] = {"driving_keys", "keys"};  // by kDriving: the table this exe keeps unread
toml::table g_other_keys;

void LoadKeys(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    const toml::table* other = t[kOtherKeys[eon::kDriving]].as_table();
    g_other_keys = other ? *other : toml::table{};
    if (!eon::kDriving) return;
    // the runtime took [keys], default.xbe's, as these: the driving defaults and [driving_keys] instead
    app::Settings& s = app::Get();
    for (int c = 0; c < input::kControls && c < int(s.keys.size()); c++) s.keys[c] = DrivingKeys(c);
    if (const toml::table* mine = t["driving_keys"].as_table())
        for (auto& [name, v] : *mine) {
            int c = input::ControlByName(std::string(name.str()).c_str());
            const toml::array* a = v.as_array();
            if (c < 0 || c >= int(s.keys.size()) || !a) continue;
            s.keys[c] = {0, 0};
            for (size_t i = 0; i < 2 && i < a->size(); i++) s.keys[c][i] = app::KeyFromName((*a)[i].value_or(std::string()));
        }
}

void SaveKeys(void* table) {
    toml::table& t = *static_cast<toml::table*>(table);
    if (eon::kDriving) {
        // the runtime wrote this exe's as [keys]
        if (toml::table* mine = t["keys"].as_table()) {
            toml::table copy = *mine;
            t.erase("keys");
            t.insert("driving_keys", copy);
        }
    }
    if (!g_other_keys.empty()) t.insert_or_assign(kOtherKeys[eon::kDriving], g_other_keys);
}
#else
constexpr void (*LoadKeys)(const void*) = nullptr;
constexpr void (*SaveKeys)(void*) = nullptr;
#endif
// GameInfo::texture_bound: the button pictures (both exes), the title screen (default.xbe)
bool TextureBound(uint32_t header, uint32_t palette) {
    if (!eon::kDriving) eon::TitleScreen(header);
    return eon::ButtonPictures(header, palette);
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
                {"language", nullptr, &eon::language, "Language",
                 "At the next start. Your disc's languages: a language it does not have is ignored.", eon::kLanguageChoices},
                {"fov_percent", nullptr, &eon::fov_percent, "Field of view (%)",
                 "On foot, 50 to 150; 100 is the game's. Cutscenes widen too."},
                {"hide_hud", &eon::hide_hud, nullptr, "Hide the HUD", "For screenshots; at once, on foot and driving."},
                {"driving_60fps", &eon::driving_60fps, nullptr, "Driving missions at 60 frames a second",
                 "Same simulation, drawn after every tick instead of every second one; from the next driving mission."}},  // kept in the settings file
    .load = LoadKeys,                     // the driving exe's keys apart (DrivingKeys)
    .save = SaveKeys,
    .init = Init,                         // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = eon::MenuMouse,                // changes a polled pad: the mouse in the menus
    .rumble = nullptr,                    // changes a rumble before it is set
    .achievements = eon::AchievementSet(),  // conditions on the profile, checked each frame (ACHIEVEMENTS.md)
    .squeeze_screen_programs = true,      // ultrawide: the HUD and menus are drawn by a pass-through program
    .sharpen_2d = true,                   // text and HUD pictures enlarged sharp
    .pal_console = !EON_USA,              // a PAL disc's movies are _pal only: a PAL 60 Hz console for it; the
                                          // USA disc's an NTSC console in North America (src/common/release.h)
    .disc_rate = 6,                       // disc reads at the console drive's speed: instant ones stall ND1b's streaming
    .hide_file = HideFile,                // skip_intro
    .mouse_look = true,                   // the mouse aims
    .mouse_direct = true,                 // straight into the game's look input (src/game/mouse.cpp)
    .default_keys = Keys,                 // a shooter's keys and mouse buttons on foot, a racing game's driving
    .texture_bound = TextureBound,         // modern_buttons, the title screen on ultrawide
    .same_code = SameCode(),              // the other copies of this release's code
    // optional, after these: .wheel (what a steering wheel's force feedback follows)
};
