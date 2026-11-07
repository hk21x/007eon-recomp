// PC Settings in the game's own menus: Options, then a screen of the settings the F1 window has (not the key bindings,
// the wheel or the monitor).
//
// Options (frontend_options.fng) has a third row this release hides: its button 0x1BD72 checks a condition and hands
// the focus on, and the code's message 0xFA75E903 disables it and hides the row. The row's two labels stay, without their hide response,
// named PC Settings; its value and arrows stay hidden; its button's accept opens the new screen as Audio Settings' does
// (response 0x200, the package's path). That path goes to the package loader, the first function of the object at
// +0x10C of the FEng manager (0x121680), which is asked for Audio Settings' package and answers with an edited copy: the
// title and each row's texts are strings of ours, which the lookup asks for every frame, so a value shows as soon as
// it changes; the arrows send messages of ours (response type 2, see quit_item.cpp); two more rows are clones of the
// lowest and the five are spaced to fit. Back is Audio Settings' (its package response opens Options). The first row
// turns the page, the other four are that page's settings.
#include "game/pc_settings.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

#include "app/app.h"
#include "common/language.h"
#include "common/release.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eon {
extern bool skip_intro, modern_buttons, driving_60fps, hide_hud, slow_motion_blend, camera_follow;
extern uint32_t fov_percent, aim_sensitivity, aim_raise, free_aim_sensitivity;
}

namespace {

using namespace eon::fng;
namespace app = ::app;

constexpr uint32_t kLabel = 0x0E0E0010, kTitle = 0x0E0E0011;   // made up, unused
constexpr uint32_t kRowLabel = 0x0E0E0020, kRowValue = 0x0E0E0030, kRowMessage = 0x0E0E0040;  // + row, + row * 2 + right
constexpr int kRows = 5;
constexpr int kPauseRowCount = 8;
constexpr uint32_t kBack = 0x0E0E0012, kBlank = 0x0E0E0013;
constexpr uint32_t kPauseLabel = 0x0E0E0060, kPauseValue = 0x0E0E0070, kPauseMessage = 0x0E0E0080;
constexpr const char* kPath = "\\Bond3\\assets\\frontend\\frontend_options_pc.fng";
constexpr const char* kAudioPath = "\\Bond3\\assets\\frontend\\frontend_options_audio.fng";
// the same screen from the pause menu: Back returns there
constexpr const char* kPausePath = "\\bond3\\assets\\common\\ui\\hud\\pm_options2.fng";
constexpr const char* kPauseBack = "\\bond3\\assets\\common\\ui\\hud\\pm_options.fng";

// settings: a name, its value's words, a step left (-1) or right (+1)
struct Row {
    const char* label;
    std::string (*text)();
    void (*step)(int);
};
std::string OnOff(bool b) { return b ? "On" : "Off"; }
void Toggle(bool& b) {
    b = !b;
    app::Save();
}
template <typename T>
int Cycle(T v, const T* list, int n, int d) {  // the index after v's
    int i = 0;
    for (int k = 0; k < n; k++)
        if (list[k] == v) i = k;
    return (i + d + n) % n;
}

#define FLAG(name, expr)                                                     \
    {                                                                        \
        name, [] { return OnOff(expr); }, [](int) { Toggle(expr); }          \
    }

struct Mode {
    uint32_t w, h;
    bool operator==(const Mode& o) const { return w == o.w && h == o.h; }
};
std::vector<Mode> Modes() {
    std::vector<Mode> list{{0, 0}};  // Desktop
#if defined(_WIN32)
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
        Mode m{dm.dmPelsWidth, dm.dmPelsHeight};
        if (dm.dmBitsPerPel >= 32 && dm.dmPelsHeight >= 480 && std::find(list.begin(), list.end(), m) == list.end())
            list.push_back(m);
    }
    std::sort(list.begin() + 1, list.end(), [](const Mode& a, const Mode& b) { return a.w != b.w ? a.w > b.w : a.h > b.h; });
#endif
    return list;
}

const char* const kModes[] = {"Fullscreen", "Borderless", "Window"};
const char* const kAspects[] = {"Auto", "4:3", "16:9", "Fill"};
const char* const kMsaaNames[] = {"Off", "2x", "4x", "8x"};
constexpr uint32_t kAnisos[] = {0, 2, 4, 8, 16};
constexpr uint32_t kCaps[] = {30, 60, 120, 144, 240, 0};

struct Page {
    const char* name;
    Row rows[kRows - 1];
};
const Page kPages[] = {
    {"Game",
     {{"Language (restart)", [] { return std::string(eon::language ? eon::kLanguageChoices[eon::language] : "Default"); },
       [](int d) {
           int n = 0;
           while (eon::kLanguageChoices[n]) n++;
           eon::language = uint32_t((int(eon::language) + d + n) % n);
           app::Save();
       }},
      {"Field of view", [] { return std::to_string(eon::fov_percent) + "%"; },
       [](int d) {
           eon::fov_percent = uint32_t(std::clamp(int(eon::fov_percent) + d * 5, 50, 150));
           app::Save();
       }},
      FLAG("Skip intro movies", eon::skip_intro),
      FLAG("Modern buttons", eon::modern_buttons)}},
    {"Driving and HUD",
     {FLAG("Driving at 60 FPS", eon::driving_60fps),
      FLAG("Hide the HUD", eon::hide_hud),
      FLAG("Pause in background", app::Get().pause_unfocused),
      FLAG("Smooth slow motion", eon::slow_motion_blend)}},
    {"Display",
     {{"Display mode", [] { return std::string(kModes[uint32_t(app::Get().display_mode) % 3]); },
       [](int d) {
           auto& s = app::Get();
           s.display_mode = app::DisplayMode((int(s.display_mode) + d + 3) % 3);
           app::Save();
           app::ApplyDisplay();
       }},
      {"Resolution",
       [] {
           auto& s = app::Get();
           return s.width && s.height ? std::to_string(s.width) + " x " + std::to_string(s.height) : std::string("Desktop");
       },
       [](int d) {
           auto& s = app::Get();
           static const std::vector<Mode> list = Modes();
           Mode m = list[size_t(Cycle(Mode{s.width, s.height}, list.data(), int(list.size()), d))];
           s.width = m.w;
           s.height = m.h;
           app::Save();
           app::ApplyDisplay();
       }},
      FLAG("Vertical sync", app::Get().vsync),
      {"Aspect (restart)", [] { return std::string(kAspects[app::Get().aspect % 4]); },
       [](int d) {
           auto& s = app::Get();
           s.aspect = (s.aspect % 4 + uint32_t(d + 4)) % 4;
           app::Save();
       }}}},
    {"Graphics",
     {{"Render scale", [] { return std::to_string(app::Get().render_percent) + "%"; },
       [](int d) {
           auto& s = app::Get();
           s.render_percent = uint32_t(std::clamp(int(s.render_percent) + d * 25, 25, 200));
           app::Save();
       }},
      {"MSAA", [] { return std::string(kMsaaNames[std::min<uint32_t>(app::Get().msaa, 3)]); },
       [](int d) {
           auto& s = app::Get();
           s.msaa = (std::min<uint32_t>(s.msaa, 3) + uint32_t(d + 4)) % 4;
           app::Save();
       }},
      {"Edge smoothing",
       [] {
           auto& s = app::Get();
           return std::string(s.smaa ? "SMAA" : s.fxaa ? "FXAA" : "Off");
       },
       [](int d) {
           // off, FXAA, SMAA in turn: SMAA wins over FXAA, so one is on at a time
           auto& s = app::Get();
           int at = (s.smaa ? 2 : s.fxaa ? 1 : 0);
           at = (at + d + 3) % 3;
           s.fxaa = at == 1;
           s.smaa = at == 2;
           app::Save();
       }},
      {"Anisotropic filter",
       [] {
           uint32_t a = app::Get().aniso;
           return a ? std::to_string(a) + "x" : std::string("Default");
       },
       [](int d) {
           auto& s = app::Get();
           s.aniso = kAnisos[Cycle(s.aniso, kAnisos, 5, d)];
           app::Save();
       }}}},
    {"Graphics 2",
     {{"Frame rate cap",
       [] {
           uint32_t c = app::Get().fps_cap;
           return c ? std::to_string(c) : std::string("Unlimited");
       },
       [](int d) {
           auto& s = app::Get();
           s.fps_cap = kCaps[Cycle(s.fps_cap, kCaps, 6, d)];
           app::Save();
       }},
      FLAG("Show FPS", app::Get().show_fps),
      FLAG("Sharp text and HUD", app::Get().sharp_2d),
      {"Renderer (restart)", [] { return std::string(app::Get().renderer % 2 ? "D3D11" : "D3D12"); },
       [](int) {
           auto& s = app::Get();
           s.renderer = (s.renderer + 1) % 2;
           app::Save();
       }}}},
    {"Mouse",
     {FLAG("Keyboard play", app::Get().keyboard),
      FLAG("Mouse look", app::Get().mouse_look),
      FLAG("Invert mouse", app::Get().mouse_invert_y),
      {"Mouse sensitivity", [] { return std::to_string(app::Get().mouse_sensitivity) + "%"; },
       [](int d) {
           auto& s = app::Get();
           s.mouse_sensitivity = uint32_t(std::clamp(int(s.mouse_sensitivity) + d * 10, 10, 1000));
           app::Save();
       }}}},
    {"Mouse 2",
     {{"Aim sensitivity", [] { return std::to_string(eon::aim_sensitivity) + "%"; },
       [](int d) {
           eon::aim_sensitivity = uint32_t(std::clamp(int(eon::aim_sensitivity) + d * 10, 10, 1000));
           app::Save();
       }},
      {"Sensitivity while aiming", [] { return std::to_string(eon::free_aim_sensitivity) + "%"; },
       [](int d) {
           eon::free_aim_sensitivity = uint32_t(std::clamp(int(eon::free_aim_sensitivity) + d * 10, 10, 200));
           app::Save();
       }},
      FLAG("Camera follows movement", eon::camera_follow),
      {"Aim height", [] { return std::to_string(eon::aim_raise); },
       [](int d) {
           eon::aim_raise = uint32_t(std::clamp(int(eon::aim_raise) + d, 0, 20));
           app::Save();
       }}}},
};
constexpr int kPageCount = int(sizeof(kPages) / sizeof(kPages[0]));
int g_page = 0;

// the words the game's own language wants
struct Words {
    const char* single;
    const char* label;
};
constexpr Words kWords[] = {
    {"Single", "PC Settings"},       {"Einzel", "PC-Einstellungen"}, {"Un joueur", "Options PC"}, {"Solo", "Options PC"},
    {"Un jugador", "Opciones de PC"}, {"Giocatore", "Opzioni PC"},    {"En spelare", "PC-installningar"},
    {"E", "PC-instellingen"},
};
const char* g_words = kWords[0].label;

// a guest buffer per hash, rewritten as the lookup asks
uint32_t Buffer(uint32_t hash, const std::string& text) {
    static std::map<uint32_t, uint32_t> buffers;
    uint32_t& va = buffers[hash];
    if (!va) va = mem::AllocSystem(80);
    if (va) std::snprintf(static_cast<char*>(kernel::HostPtr(va)), 80, "%s", text.c_str());
    return va;
}

// the records of an ObjD, MsgR, PkgR, Targ or script body: two letters, a 16 bit length, the value
template <typename F>
void Records(std::vector<uint8_t>& b, F f) {
    size_t p = 0;
    while (p + 4 <= b.size()) {
        size_t len = b[p + 2] | b[p + 3] << 8;
        f(char(b[p]), char(b[p + 1]), p + 4, len);
        p += 4 + len;
    }
}
void PutU32(std::vector<uint8_t>& b, size_t at, uint32_t v) { std::memcpy(&b[at], &v, 4); }

// Targ: the guids each message goes to
using Targets = std::vector<std::pair<uint32_t, std::vector<uint32_t>>>;
Targets ReadTargets(Chunk& targ) {
    Targets t;
    Records(targ.body, [&](char a, char b, size_t at, size_t len) {
        if (a != 'M' || b != 't' || len < 4) return;
        t.push_back({Rd(targ.body, at), {}});
        for (size_t k = 4; k + 4 <= len; k += 4) t.back().second.push_back(Rd(targ.body, at + k));
    });
    return t;
}
void WriteTargets(Chunk& targ, const Targets& t) {
    std::vector<uint8_t> out;
    Record(out, "Tc", uint32_t(t.size()));
    for (const auto& [message, guids] : t) {
        out.push_back('M');
        out.push_back('t');
        uint32_t len = 4 + uint32_t(guids.size()) * 4;
        out.push_back(uint8_t(len));
        out.push_back(uint8_t(len >> 8));
        Put(out, message);
        for (uint32_t g : guids) Put(out, g);
    }
    targ.body = std::move(out);
}
std::vector<uint32_t>* TargetsOf(Targets& t, uint32_t message) {
    for (auto& e : t)
        if (e.first == message) return &e.second;
    return nullptr;
}

Chunk* Find(std::vector<Chunk>& list, const char* tag) {
    for (Chunk& c : list)
        if (c.tag == Tag(tag)) return &c;
    return nullptr;
}
Chunk* ObjD(Chunk& ob) { return ob.kids.empty() || ob.kids[0].tag != Tag("ObjD") ? nullptr : &ob.kids[0]; }
uint32_t Guid(Chunk& ob) {
    Chunk* d = ObjD(ob);
    if (!d) return 0;
    size_t at = OtWord(*d, 2);
    return at != SIZE_MAX && at + 4 <= d->body.size() ? Rd(d->body, at) : 0;
}
void SetString(Chunk& ob, uint32_t hash) {
    Chunk* d = ObjD(ob);
    size_t at = d ? OtWord(*d, 7) : SIZE_MAX;
    if (at != SIZE_MAX && at + 4 <= d->body.size()) PutU32(d->body, at, hash);
}

// a MsgR's messages without one of them
std::vector<uint8_t> Without(std::vector<uint8_t>& msgr, uint32_t message) {
    std::vector<uint8_t> out;
    size_t p = 0;
    while (p + 16 <= msgr.size()) {
        size_t start = p;
        uint32_t m = Rd(msgr, p + 4), count = Rd(msgr, p + 12);
        p += 16;
        for (uint32_t i = 0; i < count * 3 && p + 4 <= msgr.size(); i++) p += 4 + (msgr[p + 2] | msgr[p + 3] << 8);
        if (m != message) out.insert(out.end(), msgr.begin() + long(start), msgr.begin() + long(p));
    }
    return out;
}

// an object's place: its scripts' position keys, or its description's where it has none
void Move(Chunk& ob, float dx, float dy) {
    auto add = [&](std::vector<uint8_t>& b, size_t at) {
        float xy[2];
        std::memcpy(xy, &b[at], 8);
        xy[0] += dx;
        xy[1] += dy;
        std::memcpy(&b[at], xy, 8);
    };
    int moved = 0;
    for (Chunk& k : ob.kids) {
        if (k.tag != Tag("Scrp")) continue;
        Records(k.body, [&](char a, char c, size_t at, size_t len) {
            if (a != 'S' || c != 'n' || len < 12) return;
            uint32_t tracks = Rd(k.body, at + 8);
            size_t p = at + 12, end = at + len;
            for (uint32_t t = 0; t < tracks && p + 12 <= end; t++) {
                uint8_t size = k.body[p + 1], field = k.body[p + 6];
                uint32_t keys = Rd(k.body, p + 8);
                p += 12;
                for (uint32_t i = 0; i < keys && p + 4 + size <= end; i++) {
                    // the first key (time -1) is the position, later ones are moves from it
                    if (size == 12 && field == 7 && Rd(k.body, p) == 0xFFFFFFFF) add(k.body, p + 4), moved++;
                    p += 4 + size;
                }
            }
        });
    }
    if (Chunk* d = ObjD(ob); d && !moved)
        Records(d->body, [&](char a, char c, size_t at, size_t len) {
            if (a == 'S' && c == 'A' && len >= 12) add(d->body, at + 4);
        });
}

// a text's size: the description's scale
void Scale(Chunk& ob, float scale) {
    Chunk* d = ObjD(ob);
    if (!d) return;
    Records(d->body, [&](char a, char c, size_t at, size_t len) {
        if (a != 'S' || c != 'A' || len < 24) return;
        std::memcpy(&d->body[at + 16], &scale, 4);
        std::memcpy(&d->body[at + 20], &scale, 4);
    });
}

// one row's objects and messages in Audio Settings
struct Group {
    uint32_t button, lit, arrow_a, arrow_b, value, label;
    uint32_t gain, lose, left, right;
    float lit_y;
};
constexpr Group kGroups[3] = {
    {0x4280C, 0x4281C, 0x4280B, 0x4280A, 0x42809, 0x42808, 0x63E73D95, 0xFF43C16C, 0xA69E76CF, 0x9936EC7D, -25},
    {0x42811, 0x4281D, 0x42810, 0x4280F, 0x4280E, 0x4280D, 0x4749B068, 0x5558F74B, 0x3E36916E, 0x38C32A9C, 21},
    {0x4281B, 0x4281F, 0x4281A, 0x42819, 0x42818, 0x42817, 0x6876F5D8, 0xB6776C5B, 0x4928CE1E, 0x8ECFF98C, 67},
};
// the page row is its value alone, under the title; the settings at Audio Settings' own spacing
constexpr float kPageY = -40, kFirstY = -2, kPitch = 30, kArrowWiden = 62;
constexpr float kArrowRaise = 20;   // to the label's line: a row is one line, "name: value"
constexpr float kTextScale = 0.8f;
constexpr uint32_t kCloneGuid = 0x0E100000;

uint32_t CloneGuid(uint32_t guid, int clone) { return kCloneGuid + uint32_t(clone) * 0x10000 + (guid & 0xFFFF); }
uint32_t CloneMessage(uint32_t message, int clone) { return message ^ (0x01010100u * uint32_t(clone)); }

// a row's texts and arrows, wherever its objects came from
void WireRow(std::vector<Chunk*>& objs, const Group& g, int row) {
    for (Chunk* ob : objs) {
        uint32_t id = Guid(*ob) & 0xFFFF;
        if (id == (g.lit & 0xFFFF) || id == (g.label & 0xFFFF)) SetString(*ob, kRowLabel + uint32_t(row));
        if (id != (g.value & 0xFFFF)) continue;
        SetString(*ob, kRowValue + uint32_t(row));
        // a name of its own: the game's audio code finds these rows' values by name and writes its volumes in them
        if (Chunk* d = ObjD(*ob)) {
            PutU32(d->body, OtWord(*d, 1), kRowValue + 0x100 + uint32_t(row));
            PutU32(d->body, OtWord(*d, 3), kRowValue + 0x100 + uint32_t(row));
        }
        // its two responses of type 2: the game's decrease and increase, here ours
        for (Chunk& k : ob->kids) {
            if (k.tag != Tag("MsgR")) continue;
            Records(k.body, [&](char a, char b, size_t at, size_t) {
                if (a != 'R' || b != 'u') return;
                uint32_t v = Rd(k.body, at);
                if (v == 0xAF065097) PutU32(k.body, at, kRowMessage + uint32_t(row) * 2);
                if (v == 0x43C50185) PutU32(k.body, at, kRowMessage + uint32_t(row) * 2 + 1);
            });
        }
    }
}

// the volume rows' sounds, the volume's sample at each step: the ordinary click
void PlainSounds(Chunk& button) {
    for (Chunk& k : button.kids) {
        if (k.tag != Tag("MsgR")) continue;
        Records(k.body, [&](char a, char b, size_t at, size_t) {
            if (a != 'R' || b != 'u') return;
            uint32_t v = Rd(k.body, at);
            if (v == 0x90A98729 || v == 0xE2819A4A || v == 0x88ECEFDA) PutU32(k.body, at, 0x44615BAF);
        });
    }
}

// the accept message of a row's button: a step right
void AcceptSteps(Chunk& button, uint32_t right) {
    for (Chunk& k : button.kids) {
        if (k.tag != Tag("MsgR")) continue;
        Record(k.body, "Mi", 0x0C407210);
        Record(k.body, "MC", 1);
        Record(k.body, "Ri", 1);
        Record(k.body, "Ru", right);
        Record(k.body, "Rt", 0);
    }
}

bool EditAudio(std::vector<Chunk>& top) {
    Chunk* objs = Find(top, "Obj\xcc");
    Chunk* targ = Find(top, "Targ");
    Chunk* head = Find(top, "PkHd");
    if (!objs || !targ || !head || head->body.size() < 4 + 27) return false;
    std::memset(&head->body[4], 0, head->body.size() - 4);
    std::memcpy(&head->body[4], "frontend_options_pc.fng", 24);
    Targets targets = ReadTargets(*targ);

    // accept steps a row right too: the mouse has no left or right, a click is accept
    for (const Group& g : kGroups) {
        for (Chunk& ob : objs->kids) {
            if (ob.tag != Tag("FOb\xea") || Guid(ob) != g.button) continue;
            AcceptSteps(ob, g.right);
            PlainSounds(ob);
        }
        std::vector<uint32_t>* t = TargetsOf(targets, 0x0C407210);
        if (!t) return false;
        t->push_back(g.button);
    }

    auto member = [](const Group& g, uint32_t guid) {
        return guid == g.button || guid == g.lit || guid == g.arrow_a || guid == g.arrow_b || guid == g.value ||
               guid == g.label;
    };
    // two more rows out of the lowest
    const Group& low = kGroups[2];
    std::vector<Chunk> clones[2];
    for (Chunk& ob : objs->kids)
        if (ob.tag == Tag("FOb\xea") && member(low, Guid(ob)))
            for (int c = 0; c < 2; c++) clones[c].push_back(ob);
    if (clones[0].size() != 6) return false;
    for (int c = 0; c < 2; c++) {
        for (Chunk& ob : clones[c]) {
            Chunk* d = ObjD(ob);
            size_t guid_at = OtWord(*d, 2);
            uint32_t guid = Rd(d->body, guid_at);
            PutU32(d->body, guid_at, CloneGuid(guid, c + 1));
            // the object's name, words 1 and 3, as unique as its guid
            uint32_t name = Rd(d->body, OtWord(*d, 1)) ^ (0x00010000u * uint32_t(c + 1));
            PutU32(d->body, OtWord(*d, 1), name);
            PutU32(d->body, OtWord(*d, 3), name);
            for (Chunk& k : ob.kids) {
                if (k.tag != Tag("MsgR")) continue;
                Records(k.body, [&](char a, char b, size_t at, size_t) {
                    if ((a == 'M' && b == 'i') || (a == 'R' && b == 'u')) {
                        uint32_t v = Rd(k.body, at);
                        if (v == low.gain || v == low.lose || v == low.left || v == low.right)
                            PutU32(k.body, at, CloneMessage(v, c + 1));
                    }
                });
            }
        }
        for (uint32_t m : {low.gain, low.lose, low.left, low.right}) {
            std::vector<uint32_t>* t = TargetsOf(targets, m);
            if (!t) return false;
            std::vector<uint32_t> copy;
            for (uint32_t g : *t) copy.push_back(CloneGuid(g, c + 1));
            targets.push_back({CloneMessage(m, c + 1), copy});
        }
        for (uint32_t m : {0xABC08912u, 0x55D1E635u, 0x9120409Eu, 0xB5971BF1u, 0x0C407210u}) {
            std::vector<uint32_t>* t = TargetsOf(targets, m);
            if (!t) return false;
            t->push_back(CloneGuid(low.button, c + 1));
        }
    }
    for (int c = 0; c < 2; c++)
        for (Chunk& ob : clones[c]) objs->kids.push_back(ob);
    if (Chunk* butn = Find(objs->kids, "Butn"); butn && butn->body.size() == 4) PutU32(butn->body, 0, 5);

    // the rows' places, texts and arrows
    std::vector<Chunk*> all;
    for (Chunk& ob : objs->kids)
        if (ob.tag == Tag("FOb\xea")) all.push_back(&ob);
    int wired = 0;
    for (int row = 0; row < kRows; row++) {
        const Group& g = kGroups[std::min(row, 2)];
        int clone = row >= 3 ? row - 2 : 0;
        std::vector<Chunk*> group;
        for (Chunk* ob : all) {
            uint32_t guid = Guid(*ob);
            bool in = clone ? guid >= kCloneGuid + uint32_t(clone) * 0x10000 && guid < kCloneGuid + uint32_t(clone + 1) * 0x10000
                            : member(g, guid);
            if (in) group.push_back(ob);
        }
        if (group.size() != 6) return false;
        for (Chunk* ob : group) {
            // the arrows wider apart: the values are longer than a volume
            uint32_t id = Guid(*ob) & 0xFFFF;
            float dx = id == (g.arrow_a & 0xFFFF) ? kArrowWiden : id == (g.arrow_b & 0xFFFF) ? -kArrowWiden : 0;
            float raise = id == (g.arrow_a & 0xFFFF) || id == (g.arrow_b & 0xFFFF) ? kArrowRaise : 0;
            Move(*ob, dx, (row ? kFirstY + kPitch * float(row - 1) : kPageY) - g.lit_y - raise);
            if (id == (g.lit & 0xFFFF) || id == (g.label & 0xFFFF)) Scale(*ob, kTextScale);
        }
        WireRow(group, g, row);
        wired++;
    }
    for (Chunk* ob : all)
        if (Guid(*ob) == 0x42B94) SetString(*ob, kTitle);
    WriteTargets(*targ, targets);
    return wired == kRows;
}

uint32_t g_packages[2] = {};  // the edited copies: from the main menu, from the pause menu

// the pause menu's rows in pm_options.fng, top to bottom
struct PauseRow {
    uint32_t button, value, right;
    uint32_t labels[4], helps[2];
};
constexpr PauseRow kPauseRows[kPauseRowCount] = {
    {0x43BE1, 0x43BDB, 0x8D5372D0, {0x43BDC, 0x43BDA}, {0x444D0}},
    {0x3F0A1, 0x3F0A3, 0x0ABB13A7, {0x3F0A4, 0x3F0A2}, {0x3F0F0}},
    {0x3BDE0, 0x3BDED, 0xECDF0889, {0x3BE2B, 0x3BDE3}, {0x3BDE1}},
    {0x4268E, 0x4268B, 0x96D96B39, {0x4268F, 0x4268A, 0x42D33, 0x42CCD}, {0x42D4F, 0x3BDDE}},
    {0x42717, 0x426E8, 0x51B5338A, {0x426EB, 0x426E7}, {0x43224}},
    {0x4170E, 0x416FC, 0x9936EC7D, {0x416FF, 0x416FB}, {0x3BDEA}},
    {0x425C3, 0x425C0, 0x38C32A9C, {0x425C4, 0x425BF}, {0x3BDEB}},
    {0x42651, 0x4264E, 0x8ECFF98C, {0x42652, 0x4264D}, {0x427B0}},
};

// every setting, in order: the pause screen's seven rows a page
const std::vector<const Row*>& AllRows() {
    static const std::vector<const Row*> rows = [] {
        std::vector<const Row*> r;
        for (const Page& p : kPages)
            for (const Row& row : p.rows)
                if (row.label) r.push_back(&row);
        return r;
    }();
    return rows;
}
int PausePages() { return (int(AllRows().size()) + kPauseRowCount - 2) / (kPauseRowCount - 1); }
int g_pause_page = 0;
const Row* PauseRowOf(int row) {
    size_t i = size_t(g_pause_page) * size_t(kPauseRowCount - 1) + size_t(row - 1);
    return row > 0 && i < AllRows().size() ? AllRows()[i] : nullptr;
}

// the PC Settings screen from the pause menu: a copy of pm_options.fng, its rows the settings, Controller's Back
bool EditPausePc(std::vector<Chunk>& top) {
    Chunk* objs = Find(top, "Obj\xcc");
    Chunk* targ = Find(top, "Targ");
    Chunk* head = Find(top, "PkHd");
    if (!objs || !targ || !head || head->body.size() < 4 + 16) return false;
    std::memset(&head->body[4], 0, head->body.size() - 4);
    std::memcpy(&head->body[4], "pm_options2.fng", 16);
    Targets t = ReadTargets(*targ);
    auto* accept = TargetsOf(t, 0x0C407210);
    if (!accept) return false;
    int found = 0;
    for (Chunk& ob : objs->kids) {
        if (ob.tag != Tag("FOb\xea")) continue;
        uint32_t guid = Guid(ob);
        if (guid == 0x430E1 || guid == 0x430C6) SetString(ob, kBack);
        if (guid == 0x3BDE4) SetString(ob, kBlank);
        if (guid == 0x430BE)
            for (Chunk& k : ob.kids)
                if (k.tag == Tag("MsgR")) {
                    std::vector<uint8_t> out;
                    Records(k.body, [&](char a, char b, size_t at, size_t len) {
                        out.push_back(uint8_t(a));
                        out.push_back(uint8_t(b));
                        if (a == 'R' && b == 's') {
                            std::string v(kPauseBack);
                            v.resize((v.size() + 4) & ~size_t(3));
                            out.push_back(uint8_t(v.size()));
                            out.push_back(uint8_t(v.size() >> 8));
                            out.insert(out.end(), v.begin(), v.end());
                        } else {
                            out.push_back(uint8_t(len));
                            out.push_back(uint8_t(len >> 8));
                            out.insert(out.end(), k.body.begin() + long(at), k.body.begin() + long(at + len));
                        }
                    });
                    k.body = std::move(out);
                }
        for (int row = 0; row < kPauseRowCount; row++) {
            const PauseRow& r = kPauseRows[row];
            for (uint32_t l : r.labels)
                if (l && guid == l) SetString(ob, kPauseLabel + uint32_t(row)), found++;
            for (uint32_t h : r.helps)
                if (h && guid == h) SetString(ob, kBlank);
            if (guid == r.button) {
                // the code's own switches for the row off
                for (Chunk& k : ob.kids)
                    if (k.tag == Tag("MsgR")) k.body = Without(k.body, 0xA5654E4E), k.body = Without(k.body, 0x520F1A50);
                AcceptSteps(ob, r.right);
                PlainSounds(ob);
                accept->push_back(guid);
                found++;
            }
            if (guid == r.value) {
                SetString(ob, kPauseValue + uint32_t(row));
                Chunk* d = ObjD(ob);
                PutU32(d->body, OtWord(*d, 1), kPauseValue + 0x100 + uint32_t(row));
                PutU32(d->body, OtWord(*d, 3), kPauseValue + 0x100 + uint32_t(row));
                for (Chunk& k : ob.kids) {
                    if (k.tag != Tag("MsgR")) continue;
                    Records(k.body, [&](char a, char b, size_t at, size_t) {
                        if (a != 'R' || b != 'u') return;
                        uint32_t v = Rd(k.body, at);
                        if (v == 0xAF065097) PutU32(k.body, at, kPauseMessage + uint32_t(row) * 2);
                        if (v == 0x43C50185) PutU32(k.body, at, kPauseMessage + uint32_t(row) * 2 + 1);
                    });
                }
                found++;
            }
        }
    }
    WriteTargets(*targ, t);
    return found == 2 * kPauseRowCount + 2 * kPauseRowCount + 2;  // 2 labels a row, 2 more on one, a button and a value
}

}  // namespace

namespace eon::pc {

void Words(const char* single_player) {
    for (const auto& w : kWords)
        if (std::string(single_player).rfind(w.single, 0) == 0) {
            g_words = w.label;
            return;
        }
}

bool EditOptions(std::vector<Chunk>& top) {
    constexpr uint32_t kButton = 0x1BD72, kLit = 0x1C482, kPlain = 0x1C486;
    constexpr uint32_t kShow = 0xDF14C4C0, kHide = 0xAF23C563, kHideRow = 0xC05F1E96;
    Chunk* objs = Find(top, "Obj\xcc");
    Chunk* targ = Find(top, "Targ");
    if (!objs || !targ) return false;
    int done = 0;
    // the button's own scripts end in an event that disables it: Audio Settings' scripts instead
    std::vector<Chunk> scripts;
    for (Chunk& ob : objs->kids)
        if (ob.tag == Tag("FOb\xea") && Guid(ob) == 0x4285D)
            for (Chunk& k : ob.kids)
                if (k.tag == Tag("Scrp")) scripts.push_back(k);
    for (Chunk& ob : objs->kids) {
        if (ob.tag != Tag("FOb\xea")) continue;
        uint32_t guid = Guid(ob);
        if (guid == 0x42D09 || guid == 0x42D01 || guid == 0x390FC) Move(ob, 0, 2000);  // the row's value and arrows: not used
        if (guid == kButton) {
            std::vector<Chunk> kids;
            for (Chunk& k : ob.kids)
                if (k.tag != Tag("Scrp")) kids.push_back(k);
            kids.insert(kids.begin() + 1, scripts.begin(), scripts.end());
            ob.kids = std::move(kids);
        }
        if (guid == kLit || guid == kPlain) {
            SetString(ob, kLabel);
            for (Chunk& k : ob.kids)
                if (k.tag == Tag("MsgR")) k.body = Without(k.body, kHideRow);
            done++;
        } else if (guid == kButton) {
            for (Chunk& k : ob.kids) {
                if (k.tag != Tag("MsgR")) continue;
                std::vector<uint8_t> b;
                auto msg = [&](uint32_t m, uint32_t n) {
                    Record(b, "Mi", m);
                    Record(b, "MC", n);
                };
                auto resp = [&](uint32_t kind, uint32_t ru) {
                    Record(b, "Ri", kind);
                    Record(b, "Ru", ru);
                    Record(b, "Rt", 0);
                };
                constexpr uint32_t kFocusSound = 0x44615BAF;
                msg(0xABC08912, 2);
                resp(1, kShow);
                resp(3, kFocusSound);
                msg(0x55D1E635, 1);
                resp(1, kHide);
                msg(0x0C407210, 2);
                resp(3, kFocusSound);
                Record(b, "Ri", 0x200);
                std::string path(kPath);
                path.resize((path.size() + 4) & ~size_t(3));
                b.push_back('R');
                b.push_back('s');
                b.push_back(uint8_t(path.size()));
                b.push_back(uint8_t(path.size() >> 8));
                b.insert(b.end(), path.begin(), path.end());
                Record(b, "Rt", 0);
                msg(0xFA75E903, 1);  // the code's hiding of the row: its value and arrows, the button stays usable
                resp(1, kHideRow);
                k.body = std::move(b);
                done++;
            }
        }
    }
    if (done != 3) return false;
    Targets t = ReadTargets(*targ);
    for (uint32_t m : {kShow, kHide})
        if (auto* g = TargetsOf(t, m)) *g = {kLit, kPlain};
    if (auto* g = TargetsOf(t, 0x0C407210)) g->push_back(kButton);
    WriteTargets(*targ, t);
    return true;
}

bool EditPauseOptions(std::vector<Chunk>& top) {
    // a row below the volumes, a clone of Controller's (a label lit, a label plain, a button)
    constexpr uint32_t kButton = 0x430BE, kLit = 0x430E1, kPlain = 0x430C6, kNew = 0x0E200000;
    constexpr uint32_t kShowLit = 0x75C4820E, kShowPlain = 0xCC5C9467, kFocusSound = 0x44615BAF;
    // on Controller's line, to the right; its button between that line and the next, for the focus's order
    constexpr float kRight = 260, kButtonDown = 14;
    Chunk* objs = Find(top, "Obj\xcc");
    Chunk* targ = Find(top, "Targ");
    Chunk* butn = objs ? Find(objs->kids, "Butn") : nullptr;
    if (!objs || !targ || !butn || butn->body.size() != 4) return false;
    std::vector<Chunk> clones;
    for (Chunk& ob : objs->kids)
        if (ob.tag == Tag("FOb\xea") && (Guid(ob) == kButton || Guid(ob) == kLit || Guid(ob) == kPlain)) clones.push_back(ob);
    if (clones.size() != 3) return false;
    for (Chunk& ob : clones) {
        uint32_t guid = Guid(ob);
        Chunk* d = ObjD(ob);
        PutU32(d->body, OtWord(*d, 2), kNew + (guid & 0xFFFF));
        uint32_t name = kNew + 0x100000 + (guid & 0xFFFF);
        PutU32(d->body, OtWord(*d, 1), name);
        PutU32(d->body, OtWord(*d, 3), name);
        if (guid == kButton) Move(ob, 0, kButtonDown);
        else Move(ob, kRight, 0);
        if (guid != kButton) {
            SetString(ob, kLabel);
            continue;
        }
        for (Chunk& k : ob.kids) {
            if (k.tag != Tag("MsgR")) continue;
            std::vector<uint8_t> b;
            auto msg = [&](uint32_t m, uint32_t n) {
                Record(b, "Mi", m);
                Record(b, "MC", n);
            };
            auto resp = [&](uint32_t kind, uint32_t ru, uint32_t rt) {
                Record(b, "Ri", kind);
                Record(b, "Ru", ru);
                Record(b, "Rt", rt);
            };
            msg(0xABC08912, 3);
            resp(3, kFocusSound, 0);
            resp(1, kShowLit, kNew + (kLit & 0xFFFF));
            resp(1, kShowPlain, kNew + (kPlain & 0xFFFF));
            msg(0x55D1E635, 2);
            resp(1, kShowPlain, kNew + (kLit & 0xFFFF));
            resp(1, kShowLit, kNew + (kPlain & 0xFFFF));
            msg(0x0C407210, 2);
            resp(3, kFocusSound, 0);
            Record(b, "Ri", 0x200);
            std::string path(kPausePath);
            path.resize((path.size() + 4) & ~size_t(3));
            b.push_back('R');
            b.push_back('s');
            b.push_back(uint8_t(path.size()));
            b.push_back(uint8_t(path.size() >> 8));
            b.insert(b.end(), path.begin(), path.end());
            Record(b, "Rt", 0);
            k.body = std::move(b);
        }
    }
    for (Chunk& ob : clones) objs->kids.push_back(ob);
    PutU32(butn->body, 0, Rd(butn->body, 0) + 1);
    Targets t = ReadTargets(*targ);
    for (uint32_t m : {kShowLit, kShowPlain}) {
        auto* g = TargetsOf(t, m);
        if (!g) return false;
        g->push_back(kNew + (kLit & 0xFFFF));
        g->push_back(kNew + (kPlain & 0xFFFF));
    }
    for (uint32_t m : {0xABC08912u, 0x55D1E635u, 0x0C407210u}) {
        auto* g = TargetsOf(t, m);
        if (!g) return false;
        g->push_back(kNew + (kButton & 0xFFFF));
    }
    WriteTargets(*targ, t);
    return true;
}

uint32_t Text(uint32_t hash) {
    if (hash == kLabel || hash == kTitle) return Buffer(hash, g_words);
    if (hash == kBack) return Buffer(hash, "Back");
    if (hash == kBlank) return Buffer(hash, "");
    if (hash >= kPauseLabel && hash < kPauseLabel + kPauseRowCount) {
        int row = int(hash - kPauseLabel);
        if (row == 0) return Buffer(hash, g_words);
        const Row* r = PauseRowOf(row);
        return Buffer(hash, r ? r->label : "");
    }
    if (hash >= kPauseValue && hash < kPauseValue + kPauseRowCount) {
        int row = int(hash - kPauseValue);
        if (row == 0) return Buffer(hash, std::to_string(g_pause_page + 1) + " of " + std::to_string(PausePages()));
        const Row* r = PauseRowOf(row);
        return Buffer(hash, r ? r->text() : "");
    }
    if (hash >= kRowLabel && hash < kRowLabel + kRows) {
        int row = int(hash - kRowLabel);
        if (row == 0) return Buffer(hash, kPages[g_page].name);
        const Row& r = kPages[g_page].rows[row - 1];
        return Buffer(hash, r.label ? std::string(r.label) + ": " + r.text() : std::string());
    }
    if (hash >= kRowValue && hash < kRowValue + kRows) {
        return Buffer(hash, "");  // the row's value is in its label
    }
    return 0;
}

bool Message(uint32_t hash) {
    if (hash >= kPauseMessage && hash < kPauseMessage + kPauseRowCount * 2) {
        int row = int(hash - kPauseMessage) / 2, d = (hash - kPauseMessage) % 2 ? 1 : -1;
        if (row == 0) {
            g_pause_page = (g_pause_page + d + PausePages()) % PausePages();
        } else if (const Row* r = PauseRowOf(row)) {
            r->step(d);
            LOGR("eon: PC Settings: %s: %s", r->label, r->text().c_str());
        }
        return true;
    }
    if (hash < kRowMessage || hash >= kRowMessage + kRows * 2) return false;
    int row = int(hash - kRowMessage) / 2, d = (hash - kRowMessage) % 2 ? 1 : -1;
    if (row == 0) {
        g_page = (g_page + d + kPageCount) % kPageCount;
    } else if (const Row& r = kPages[g_page].rows[row - 1]; r.step) {
        r.step(d);
        LOGR("eon: PC Settings: %s: %s", r.label, r.text().c_str());
    }
    return true;
}

}  // namespace eon::pc

// the FEng package loader (thiscall: a path, two out values; ret 12): PC Settings is an edited Audio Settings, or
// from the pause menu an edited pm_options.fng (Audio Settings is not loaded in a mission)
X86_EXTERN_FUNC(EON_ORIG(00121680, 00120F80));
X86_FUNC(EON_FN(00121680, 00120F80)) {
    uint32_t path = kernel::R32(ctx.esp + 4);
    const char* name = path ? static_cast<const char*>(kernel::HostPtr(path)) : "";
    bool pause = std::strcmp(name, kPausePath) == 0, ours = pause || std::strcmp(name, kPath) == 0;
    if (ours) {
        static uint32_t audio = GuestString(kAudioPath), options = GuestString(kPauseBack);
        kernel::W32(ctx.esp + 4, pause ? options : audio);
    }
    EON_ORIG(00121680, 00120F80)(ctx, base);
    if (!ours) return;
    if (!ctx.eax) {
        LOGW("eon: PC Settings: its base screen did not load");
        return;
    }
    uint32_t& package = g_packages[pause];
    if (!package) {
        uint32_t pkg = ctx.eax, size = kernel::R32(pkg + 4);
        std::vector<Chunk> top = Parse(static_cast<const uint8_t*>(kernel::HostPtr(pkg + 8)), size);
        if (pause ? EditPausePc(top) : EditAudio(top)) {
            Chunk whole{Tag("FEn\xe7"), {}, std::move(top)};
            std::vector<uint8_t> bytes;
            Write(whole, bytes);
            package = mem::AllocSystem(uint32_t(bytes.size()));
            if (package) std::memcpy(kernel::HostPtr(package), bytes.data(), bytes.size());
            LOGR("eon: PC Settings screen%s: %u bytes, was %u", pause ? " (pause)" : "", uint32_t(bytes.size()), size + 8);
        } else {
            LOGW("eon: PC Settings: its base screen is not as expected");
        }
    }
    if (package) ctx.eax = package;
}
