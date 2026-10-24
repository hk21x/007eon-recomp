#include <cctype>
#include <cstdlib>
#include <string>

#include "app/app.h"
#include "core/log.h"

namespace eon {
bool skip_intro = false;  // no logo movies at start (HideFile)
}

namespace {
// EON_SKIP_INTRO=1 sets skip_intro without a settings file (the headless harness has none)
void Init() {
    if (const char* e = std::getenv("EON_SKIP_INTRO"); e && *e == '1') eon::skip_intro = true;
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
                 "The THX, MGM and EA logos; at the next start."}},  // game settings kept in the settings file
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
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
