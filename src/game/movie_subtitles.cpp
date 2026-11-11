// A movie's subtitles at the movie's speed (issue #5: twice as fast as the speech).
//
// Traced by a research agent (2026-11-11): the movie's subtitle callback 0xC45F0 steps the subtitle track by the change
// in the movie's clock 0x245EF0 (thiscall, the movie: int [+0x24] / float [+0x28]; its only caller). The counter goes up
// once a turn of the movie's loop (0x2462E0 from 0x246490) and the rate is 1 / (2 * the vertical blank's period
// [0x2FE988]) = 29.97: the game takes a turn for two blanks, as its present interval 2 made it on the console. Here the
// loop turns once a blank (60 a second on foot), so the track ran at twice the movie's speed (measured headless, the
// intro in1: 96 s of lines over in 58.5 s under gdb's slowing, the movie 110.6 s; picture and sound are paced by the
// decoder and are right; a level's own subtitles take the measured step and are right).
// The clock here is real time counted while the loop turns: each turn adds the time since the last, at most 0.1 s (a
// pause, a stall), so it holds whatever the loop's rate is (a frame cap, a slow machine).
#include <chrono>
#include <unordered_map>

#include "kernel/kernel.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace {
struct Clock {
    int32_t turns = 0;
    double seconds = 0;
    std::chrono::steady_clock::time_point at{};
};
}  // namespace

X86_FUNC(EON_FN(00245EF0, 00245720)) {
    static std::unordered_map<uint32_t, Clock> clocks;
    Clock& c = clocks[ctx.ecx];
    const int32_t turns = int32_t(kernel::R32(ctx.ecx + 0x24));
    const auto now = std::chrono::steady_clock::now();
    if (turns < c.turns || turns <= 1) {  // a new movie
        c.seconds = 0;
    } else if (turns != c.turns) {
        const double dt = std::chrono::duration<double>(now - c.at).count();
        c.seconds += dt < 0.1 ? dt : 0.1;
    }
    if (turns != c.turns) c.at = now;
    c.turns = turns;
    x87_push(ctx, c.seconds);
    ctx.esp += 4;  // the return address
}
