// High frame rate on foot: default.xbe times its frames by the clock. Its frame timer 0x5E780 takes the time since the
// last frame from QueryPerformanceCounter (0x62977, through 0x5DF60) into seconds at 0x2FE98C (1/60 to start) and keeps
// a 16 frame rate at 0x35B824, so the world moves by the time that passed. Only the pacing held it at 60: the game asks
// for present interval 0, one blank, and a blank is 1/60 s. With the setting the blanks it waits for are counted at the
// frame rate cap. Measured headless at 120: frames of 0.014 to 0.018 s and the game's own rate 68 to 75 (the CPU drawing),
// at 60 0.0163 s and 60. driving.xbe steps in ticks of 1/60 instead (src/driving/sixty.cpp) and stays at 60.
#include <algorithm>

#include "app/app.h"

namespace eon {
extern const bool kDriving;  // src/driving/hooks.cpp, src/game/hooks.cpp
extern bool foot_high_fps;   // the setting (src/common/game_info.cpp; EON_FOOT_HIGH_FPS headless)

// each frame (GameInfo::pad): the cap may change in the settings window
void FootPace() {
    if (kDriving) return;
    const uint32_t cap = app::Get().fps_cap;
    app::g_pace_hz = foot_high_fps && cap > 60 ? cap : 60;
}
}  // namespace eon
