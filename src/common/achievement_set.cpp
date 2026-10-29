// EoN's achievements (ACHIEVEMENTS.md has the set and the memory map). Conditions are in RetroAchievements' syntax.
//
// The profile (GAME.DAT's image) is held by default.xbe at 0x31EC50 once a save is loaded (from +0x6C on; 0x652154
// is the load buffer it comes through, whole), and by driving.xbe in the launch data it hands back (0x36CAD8,
// "LOADER PRESENT" at +8, the profile at +0x1F0: 0x36CCC8). Both hold +0x78, a dword that differs from save to save
// and is never 0 in one. Mission k's record is 0x24 bytes at
// profile + 0x84 + 0x24 k (Mission Select shows them in that order; the accessors 0x16A00..0x16E60): its first byte
// bit 0 completed (0x16A30), bits 1-3 the medal (0x16A00: 1 bronze, 2 silver, 3 gold, 4 platinum), bits 4-5 the
// difficulty (0x16A10: 0 Operative, 1 Agent, 2 00 Agent); +6 a 16 bit mask of the Bond moments found (0x16C00 counts
// it). Each exe checks its own copy, so a driving mission's achievement comes as it ends in driving.xbe.
//
// Loading a save fills the whole profile at once, and an achievement only has to be seen unmet once before (from
// boot, with the profile still empty): each condition takes a change this frame (a record's bit going from 0 to 1, a
// sum growing) and a guard that the profile was already there last frame (+0x78 not 0 then and now), so loading a
// finished save awards nothing.
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "core/achievements.h"
#include "common/release.h"

namespace eon {
extern const bool kDriving;  // driving.xbe's exe (src/driving/hooks.cpp)

namespace {

struct Mission {
    int record;
    const char* id;
    const char* name;
    bool medals;  // the mission is scored (MI6 Training and the interludes are not)
};
// records 0-13 checked against Mission Select with Harry's save; the rest in the order of the game's text, to be
// confirmed with a later save (record 12 is not shown by name yet: Leap of Faith by the text)
constexpr Mission kMissions[] = {
    {0, "ground_zero", "Ground Zero", true},
    {1, "mi6_training", "MI6 Training", false},
    {2, "long_way_down", "A Long Way Down", true},
    {3, "train_chase", "Train Chase", true},
    {4, "old_friend", "An Old Friend", true},
    {5, "sand_storm", "Sand Storm", true},
    {7, "serena", "Serena St. Germaine", true},
    {8, "vertigo", "Vertigo", true},
    {9, "ruined_tower", "The Ruined Tower", true},
    {10, "death_agent", "Death of an Agent", true},
    {11, "show_of_force", "A Show of Force", true},
    {12, "leap_of_faith", "Leap of Faith", false},
    {13, "mardi_gras", "Mardi Gras Mayhem", true},
    {14, "kiss_kiss", "The Kiss Kiss Club", true},
    {15, "underworld", "Underworld", true},
    {16, "deaths_door", "Death's Door", true},
    {17, "big_easy", "Battle in the Big Easy", true},
    {18, "faded_splendor", "Faded Splendor", true},
    {19, "machinery", "The Machinery of Evil", true},
    {20, "pontchartrain", "The Pontchartrain Bridge", true},
    {21, "simple_exchange", "A Simple Exchange", true},
    {22, "red_line", "Red Line", true},
    {23, "ambushed", "Ambushed", true},
    {24, "high_road", "The High Road", true},
    {25, "diavolo_plan", "Diavolo's Plan", true},
    {26, "platinum_war", "The Platinum War", true},
    {27, "dangerous_descent", "Dangerous Descent", true},
    {28, "red_underground", "Red Underground", true},
    {29, "final_card", "The Final Card", true},
    {30, "everything", "Everything or Nothing", true},
};
constexpr int kFinal = 30;

struct Copy {
    uint32_t profile;
    const char* guard;  // the profile was there last frame and is now (+0x78)
};
const Copy& This() {
    // the European addresses, then the USA ones (src/common/release.h)
    static const Copy game = {EON_VA(0x31EC50, 0x31E148),
                              EON_VA("d0xX31ecc8!=0_0xX31ecc8!=0_", "d0xX31e1c0!=0_0xX31e1c0!=0_")};
    static const Copy driving = {EON_VA(0x36CCC8, 0x36CCA8),
                                 EON_VA("d0xX36cd40!=0_0xX36cd40!=0_", "d0xX36cd20!=0_0xX36cd20!=0_")};
    return kDriving ? driving : game;
}
uint32_t Record(int k) { return This().profile + 0x84 + 0x24 * uint32_t(k); }

const char* Keep(std::string s) {
    static std::deque<std::string> kept;
    return kept.emplace_back(std::move(s)).c_str();
}
std::string Hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%x", v);
    return b;
}

// mission k completed this frame: the record's bit 0 from 0 to 1
const char* Completed(int k, const char* also = "") {
    std::string r = Hex(Record(k));
    return Keep(This().guard + std::string(also) + "d0xM" + r + "=0_0xM" + r + "=1");
}

// an add/sub source chain ending in a comparison: each term "A:x" adds, "B:x" subtracts; the last must add
std::string Chain(std::vector<std::string> terms, const std::string& compare) {
    std::string last = terms.back().substr(2);
    terms.pop_back();
    std::string s;
    for (const std::string& t : terms) s += t + "_";
    return s + last + compare;
}
// operands over the scored missions' records: the size letters with the record's address (plus a byte offset)
std::vector<std::string> Over(const char* flag, const char* size, uint32_t offset = 0, bool every = false) {
    std::vector<std::string> v;
    for (const Mission& m : kMissions)
        if (m.medals || every) v.push_back(std::string(flag) + size + Hex(Record(m.record) + offset));
    return v;
}
std::vector<std::string> Join(std::vector<std::string> a, const std::vector<std::string>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
// grown this frame (last frame's terms subtracted first, then the current ones) and now at least n
const char* GrownToAtLeast(const std::vector<std::string>& now, const std::vector<std::string>& before, int n) {
    std::string grown = Chain(Join(before, now), ">0");
    std::string total = Chain(now, ">=" + std::to_string(n));
    return Keep(This().guard + grown + "_" + total);
}
// the medals' bits 2 and 3: silver (2), gold (3) and platinum (4) set one of them, bronze (1) neither
const char* MedalsAtLeast(int n) {
    return GrownToAtLeast(Join(Over("A:", "0xO"), Over("A:", "0xP")), Join(Over("B:", "d0xO"), Over("B:", "d0xP")), n);
}
// Bond moments found over every mission: each record's mask at +6, both bytes counted
const char* BondMomentsAtLeast(int n) {
    return GrownToAtLeast(Join(Over("A:", "0xK", 6, true), Over("A:", "0xK", 7, true)),
                          Join(Over("B:", "d0xK", 6, true), Over("B:", "d0xK", 7, true)), n);
}
// any scored mission's medal reaching at least gold (3) or platinum (4) this frame: one alternative group each
const char* AnyMedalReached(int medal) {
    // the low nibble is the medal times 2 plus the completed bit: at least 2 medal + 1
    const int nibble = medal * 2 + 1;
    std::string c = This().guard;
    c.pop_back();
    for (const Mission& m : kMissions) {
        if (!m.medals) continue;
        std::string r = Hex(Record(m.record));
        c += "Sd0xL" + r + "<" + std::to_string(nibble) + "_0xL" + r + ">=" + std::to_string(nibble);
    }
    return Keep(c);
}
// a mission's difficulty reaching 00 Agent (2: bit 5 set, bit 4 clear) this frame, completed
std::string On00Agent(int k) {
    std::string r = Hex(Record(k));
    return "d0xR" + r + "=0_0xR" + r + "=1_0xQ" + r + "=0_0xM" + r + "=1";
}
// any scored mission: one alternative group each
const char* Any00Agent() {
    std::string c = This().guard;
    c.pop_back();
    for (const Mission& m : kMissions)
        if (m.medals) c += "S" + On00Agent(m.record);
    return Keep(c);
}

}  // namespace

std::vector<achievements::Def> AchievementSet() {
    std::vector<achievements::Def> set;
    for (const Mission& m : kMissions) {
        if (m.record == kFinal) continue;
        static std::deque<std::string> text;
        const char* id = text.emplace_back(std::string("mission_") + m.id).c_str();
        const char* title = text.emplace_back(m.name).c_str();
        const char* what = text.emplace_back(std::string("Complete ") + m.name).c_str();
        set.push_back({id, title, what, m.record == 0 ? 3u : 5u, Completed(m.record)});
    }
    set.insert(set.end(), {
        {"mission_everything", "Everything or Nothing", "Complete the final mission", 25, Completed(kFinal)},
        {"final_00", "Licence Renewed", "Complete Everything or Nothing on 00 Agent", 50,
         Keep(This().guard + On00Agent(kFinal))},
        {"first_00", "Double-0 Status", "Complete a mission on 00 Agent", 10, Any00Agent()},
        {"first_gold", "Goldfinger", "Earn a gold medal", 5, AnyMedalReached(3)},
        {"first_platinum", "Platinum Standard", "Earn a platinum medal", 10, AnyMedalReached(4)},
        {"medals_10", "Decorated Agent", "Earn silver or better in 10 missions", 10, MedalsAtLeast(10)},
        {"medals_20", "For Services Rendered", "Earn silver or better in 20 missions", 25, MedalsAtLeast(20)},
        {"bond_moments_10", "Bond Moment", "Find 10 Bond moments", 5, BondMomentsAtLeast(10)},
        {"bond_moments_30", "Style and Substance", "Find 30 Bond moments", 10, BondMomentsAtLeast(30)},
        {"bond_moments_60", "The Name's Bond", "Find 60 Bond moments", 25, BondMomentsAtLeast(60)},
    });
    return set;
}

}  // namespace eon
