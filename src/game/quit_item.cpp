// Quit Game in the main menu.
//
// The menus are EA FEng packages (chunks: FEn container, PkHd name, Obj container of FOb objects, each an ObjD
// description, Scrp animations and MsgR message responses), all resident in one blob and parsed by 0x138900 (thiscall,
// the package's bytes the first argument) when a screen opens. The main menu, frontend_spmp.fng, has a fourth item
// under Options that this release never shows: Online (its button 0x3BD5E, its two label states 0x3BD60 and 0x3BD5F,
// string 0xD1E4FF7B, its help line 0x40978). Its button's focus response checks for Xbox Live and hands the focus back
// to Options (responses 0x300 and 0x100), and its 0x1E977C1F response hides the labels. A copy of the package with that
// item made Quit Game is parsed instead: the labels and help line show new strings (the string lookup 0x142F60 answers
// their hashes), and the button's responses are the other items' (focus: show the highlighted label, hide the plain one;
// accept: the select sound and a message of its own; leaving: back). Response type 2 sends a message to the game's code
// through 0x1305A0 (the message's hash the first argument); that message closes the game as closing its window does.
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace {

constexpr uint32_t kQuitLabel = 0x0E0E0001, kQuitHelp = 0x0E0E0002, kQuitMessage = 0x0E0E0003;  // made up, unused
constexpr uint32_t kOnlineButton = 0x3BD5E, kOnlinePlain = 0x3BD60, kOnlineLit = 0x3BD5F, kOnlineHelp = 0x40978;
constexpr uint32_t kOnlineString = 0xD1E4FF7B, kOnlineHelpString = 0x99A6A723;
// messages the menu's objects answer (from the other items' responses)
constexpr uint32_t kGainFocus = 0xABC08912, kAccept = 0x0C407210, kLoseFocus = 0x55D1E635;
constexpr uint32_t kShowLit = 0xDAE72A78, kShowPlain = 0xE926791B, kHideAll = 0x378C187A;
constexpr uint32_t kFocusSound = 0x44615BAF, kSelectSound = 0x5BA5B1DF;

uint32_t Tag(const char* t) {
    uint32_t v;
    std::memcpy(&v, t, 4);
    return v;
}
uint32_t Rd(const std::vector<uint8_t>& b, size_t at) {
    uint32_t v;
    std::memcpy(&v, &b[at], 4);
    return v;
}
void Put(std::vector<uint8_t>& out, uint32_t v) {
    uint8_t b[4];
    std::memcpy(b, &v, 4);
    out.insert(out.end(), b, b + 4);
}

// a record of an ObjD or MsgR body: two letters, a 16 bit length, the value
void Record(std::vector<uint8_t>& out, const char* name, uint32_t v) {
    out.push_back(uint8_t(name[0]));
    out.push_back(uint8_t(name[1]));
    out.push_back(4);
    out.push_back(0);
    Put(out, v);
}

struct Response {
    uint32_t kind, message, target;
};
void Message(std::vector<uint8_t>& out, uint32_t message, std::initializer_list<Response> rs) {
    Record(out, "Mi", message);
    Record(out, "MC", uint32_t(rs.size()));
    for (const Response& r : rs) {
        Record(out, "Ri", r.kind);
        Record(out, "Ru", r.message);
        Record(out, "Rt", r.target);
    }
}

// the Quit button's responses, as Options' are
std::vector<uint8_t> QuitResponses() {
    std::vector<uint8_t> b;
    Message(b, kGainFocus, {{1, kShowLit, kOnlinePlain}, {1, kShowLit, kOnlineLit}, {3, kFocusSound, 0}});
    Message(b, kAccept, {{2, kSelectSound, 0}, {2, kQuitMessage, 0}});
    Message(b, kLoseFocus, {{1, kShowPlain, kOnlinePlain}, {1, kShowPlain, kOnlineLit}});
    return b;
}

// a label's responses without the one that hides it for want of Xbox Live
std::vector<uint8_t> WithoutHide(const std::vector<uint8_t>& msgr) {
    std::vector<uint8_t> out;
    size_t p = 0;
    while (p + 12 <= msgr.size()) {
        // Mi, MC, then MC times Ri Ru|Rs Rt
        size_t start = p;
        uint32_t message = Rd(msgr, p + 4);
        uint32_t count = Rd(msgr, p + 12);
        p += 16;
        for (uint32_t i = 0; i < count * 3 && p + 4 <= msgr.size(); i++) p += 4 + (msgr[p + 2] | msgr[p + 3] << 8);
        if (message != kHideAll) out.insert(out.end(), msgr.begin() + long(start), msgr.begin() + long(p));
    }
    return out;
}

// FEng chunks: a tag, a length, the body; a tag whose last byte is 0x80 or more holds chunks
struct Chunk {
    uint32_t tag;
    std::vector<uint8_t> body;
    std::vector<Chunk> kids;
    bool container() const { return (tag >> 24) >= 0x80; }
};
std::vector<Chunk> Parse(const uint8_t* p, size_t n) {
    std::vector<Chunk> out;
    size_t at = 0;
    while (at + 8 <= n) {
        Chunk c;
        uint32_t size;
        std::memcpy(&c.tag, p + at, 4);
        std::memcpy(&size, p + at + 4, 4);
        if (at + 8 + size > n) break;
        if (c.container())
            c.kids = Parse(p + at + 8, size);
        else
            c.body.assign(p + at + 8, p + at + 8 + size);
        out.push_back(std::move(c));
        at += 8 + size;
    }
    return out;
}
void Write(const Chunk& c, std::vector<uint8_t>& out) {
    std::vector<uint8_t> body;
    if (c.container())
        for (const Chunk& k : c.kids) Write(k, body);
    else
        body = c.body;
    Put(out, c.tag);
    Put(out, uint32_t(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

// an object's ObjD: its Ot record's words (type, name, guid, name, flags, order, parent, string, font for text)
size_t OtWord(const Chunk& objd, int word) {
    size_t p = 0;
    while (p + 4 <= objd.body.size()) {
        size_t len = objd.body[p + 2] | objd.body[p + 3] << 8;
        if (objd.body[p] == 'O' && objd.body[p + 1] == 't') return p + 4 + size_t(word) * 4;
        p += 4 + len;
    }
    return SIZE_MAX;
}

bool Edit(std::vector<Chunk>& top) {
    int done = 0;
    for (Chunk& objs : top) {
        if (objs.tag != Tag("Obj\xcc")) continue;
        for (Chunk& ob : objs.kids) {
            if (ob.tag != Tag("FOb\xea") || ob.kids.empty() || ob.kids[0].tag != Tag("ObjD")) continue;
            Chunk& objd = ob.kids[0];
            size_t guid_at = OtWord(objd, 2), string_at = OtWord(objd, 7);
            if (guid_at + 4 > objd.body.size()) continue;
            uint32_t guid = Rd(objd.body, guid_at);
            auto set_string = [&](uint32_t from, uint32_t to) {
                if (string_at + 4 <= objd.body.size() && Rd(objd.body, string_at) == from) {
                    std::memcpy(&objd.body[string_at], &to, 4);
                    done++;
                }
            };
            if (guid == kOnlinePlain || guid == kOnlineLit) {
                set_string(kOnlineString, kQuitLabel);
                for (Chunk& k : ob.kids)
                    if (k.tag == Tag("MsgR")) k.body = WithoutHide(k.body);
            } else if (guid == kOnlineHelp) {
                set_string(kOnlineHelpString, kQuitHelp);
            } else if (guid == kOnlineButton) {
                for (Chunk& k : ob.kids)
                    if (k.tag == Tag("MsgR")) k.body = QuitResponses(), done++;
            }
        }
    }
    return done == 4;
}

// the language the game shows, told by its own words for Single Player
struct Words {
    const char* single;  // how Single Player starts
    const char* label;
    const char* help;
};
constexpr Words kWords[] = {
    {"Single", "Quit Game", "Close the game and return\nto Windows."},
    {"Einzel", "Spiel beenden", "Das Spiel schliessen."},
    {"Un joueur", "Quitter le jeu", "Fermer le jeu."},
    {"Solo", "Quitter le jeu", "Fermer le jeu."},
    {"Un jugador", "Salir del juego", "Cerrar el juego."},
    {"Giocatore", "Esci dal gioco", "Chiudi il gioco."},
    {"En spelare", "Avsluta spelet", "Stang spelet."},
    {"E", "Spel afsluiten", "Het spel sluiten."},
};

uint32_t g_label = 0, g_help = 0;  // the strings in guest memory

uint32_t GuestString(const char* s) {
    size_t n = std::strlen(s) + 1;
    uint32_t va = mem::AllocSystem(uint32_t(n));
    if (va) std::memcpy(kernel::HostPtr(va), s, n);
    return va;
}

}  // namespace

// the package parser: frontend_spmp.fng with its fourth item made Quit Game
X86_EXTERN_FUNC(__imp__sub_00138900);
X86_FUNC(sub_00138900) {
    uint32_t pkg = kernel::R32(ctx.esp + 4);
    static uint32_t from = 0, edited = 0;
    if (pkg && kernel::R32(pkg) == Tag("FEn\xe7") && kernel::R32(pkg + 8) == Tag("PkHd") &&
        std::strcmp(static_cast<const char*>(kernel::HostPtr(pkg + 0x14)), "frontend_spmp.fng") == 0) {
        if (pkg != from) {
            from = pkg;
            edited = 0;
            uint32_t size = kernel::R32(pkg + 4);
            std::vector<Chunk> top = Parse(static_cast<const uint8_t*>(kernel::HostPtr(pkg + 8)), size);
            if (Edit(top)) {
                Chunk whole{Tag("FEn\xe7"), {}, std::move(top)};
                std::vector<uint8_t> bytes;
                Write(whole, bytes);
                edited = mem::AllocSystem(uint32_t(bytes.size()));
                if (edited) std::memcpy(kernel::HostPtr(edited), bytes.data(), bytes.size());
                LOGR("eon: main menu: Online made Quit Game (%u bytes, was %u)", uint32_t(bytes.size()), size + 8);
            } else {
                LOGW("eon: main menu: the Online item is not as expected, no Quit Game");
            }
        }
        if (edited) kernel::W32(ctx.esp + 4, edited);
    }
    __imp__sub_00138900(ctx, base);
}

// the string table lookup (thiscall: the table, a string's hash): the Quit item's words
X86_EXTERN_FUNC(__imp__sub_00142F60);
X86_FUNC(sub_00142F60) {
    uint32_t hash = kernel::R32(ctx.esp + 4);
    if (hash != kQuitLabel && hash != kQuitHelp) return __imp__sub_00142F60(ctx, base);
    if (!g_label) {
        // the game's own Single Player, for its language
        uint32_t ret = kernel::R32(ctx.esp), table = ctx.ecx;
        kernel::W32(ctx.esp + 4, 0x1375CD24);
        __imp__sub_00142F60(ctx, base);
        ctx.esp -= 8;  // it returned (ret 4): the stack back as it was
        kernel::W32(ctx.esp, ret);
        kernel::W32(ctx.esp + 4, hash);
        ctx.ecx = table;
        std::string single = ctx.eax ? static_cast<const char*>(kernel::HostPtr(ctx.eax)) : "";
        const Words* w = &kWords[0];
        for (const Words& x : kWords)
            if (single.rfind(x.single, 0) == 0) {
                w = &x;
                break;
            }
        g_label = GuestString(w->label);
        g_help = GuestString(w->help);
    }
    ctx.eax = hash == kQuitLabel ? g_label : g_help;
    ctx.esp += 8;  // ret 4
}

// a message to the game's code (response type 2): Quit Game's closes the game
X86_EXTERN_FUNC(__imp__sub_001305A0);
X86_FUNC(sub_001305A0) {
    if (kernel::R32(ctx.esp + 4) == kQuitMessage) {
        LOGR("eon: Quit Game chosen in the main menu");
        app::Quit();
    }
    __imp__sub_001305A0(ctx, base);
}
