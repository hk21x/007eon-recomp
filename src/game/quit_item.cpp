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
#include "common/release.h"
#include "game/fng.h"
#include "game/pc_settings.h"

namespace {

using namespace eon::fng;

constexpr uint32_t kQuitLabel = 0x0E0E0001, kQuitHelp = 0x0E0E0002, kQuitMessage = 0x0E0E0003;  // made up, unused
constexpr uint32_t kOnlineButton = 0x3BD5E, kOnlinePlain = 0x3BD60, kOnlineLit = 0x3BD5F, kOnlineHelp = 0x40978;
constexpr uint32_t kOnlineString = 0xD1E4FF7B, kOnlineHelpString = 0x99A6A723;
// messages the menu's objects answer (from the other items' responses)
constexpr uint32_t kGainFocus = 0xABC08912, kAccept = 0x0C407210, kLoseFocus = 0x55D1E635;
constexpr uint32_t kShowLit = 0xDAE72A78, kShowPlain = 0xE926791B, kHideAll = 0x378C187A;
constexpr uint32_t kFocusSound = 0x44615BAF, kSelectSound = 0x5BA5B1DF;

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

}  // namespace

// the package parser: frontend_spmp.fng with its fourth item made Quit Game
X86_EXTERN_FUNC(EON_ORIG(00138900, 00138200));
X86_FUNC(EON_FN(00138900, 00138200)) {
    uint32_t pkg = kernel::R32(ctx.esp + 4);
    if (pkg && kernel::R32(pkg) == Tag("FEn\xe7") && kernel::R32(pkg + 8) == Tag("PkHd")) {
        const std::string name = static_cast<const char*>(kernel::HostPtr(pkg + 0x14));
        bool main_menu = name == "frontend_spmp.fng", options = name == "frontend_options.fng";
        if (main_menu || options) {
            struct Copy {
                uint32_t from = 0, edited = 0;
            };
            static Copy copies[2];
            Copy& c = copies[options];
            if (pkg != c.from) {
                c = {pkg, 0};
                uint32_t size = kernel::R32(pkg + 4);
                std::vector<Chunk> top = Parse(static_cast<const uint8_t*>(kernel::HostPtr(pkg + 8)), size);
                if (options ? eon::pc::EditOptions(top) : Edit(top)) {
                    Chunk whole{Tag("FEn\xe7"), {}, std::move(top)};
                    std::vector<uint8_t> bytes;
                    Write(whole, bytes);
                    c.edited = mem::AllocSystem(uint32_t(bytes.size()));
                    if (c.edited) std::memcpy(kernel::HostPtr(c.edited), bytes.data(), bytes.size());
                    LOGR("eon: %s: %s (%u bytes, was %u)", name.c_str(), options ? "the unused row made PC Settings" : "Online made Quit Game",
                         uint32_t(bytes.size()), size + 8);
                } else {
                    LOGW("eon: %s: not as expected, left as it is", name.c_str());
                }
            }
            if (c.edited) kernel::W32(ctx.esp + 4, c.edited);
        }
    }
    EON_ORIG(00138900, 00138200)(ctx, base);
}

// the string table lookup (thiscall: the table, a string's hash): the Quit item's words
X86_EXTERN_FUNC(EON_ORIG(00142F60, 001427D0));
X86_FUNC(EON_FN(00142F60, 001427D0)) {
    uint32_t hash = kernel::R32(ctx.esp + 4);
    if (uint32_t text = eon::pc::Text(hash)) {
        ctx.eax = text;
        ctx.esp += 8;  // ret 4
        return;
    }
    if (hash != kQuitLabel && hash != kQuitHelp) return EON_ORIG(00142F60, 001427D0)(ctx, base);
    if (!g_label) {
        // the game's own Single Player, for its language
        uint32_t ret = kernel::R32(ctx.esp), table = ctx.ecx;
        kernel::W32(ctx.esp + 4, 0x1375CD24);
        EON_ORIG(00142F60, 001427D0)(ctx, base);
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
        eon::pc::Words(single.c_str());
        g_label = GuestString(w->label);
        g_help = GuestString(w->help);
    }
    ctx.eax = hash == kQuitLabel ? g_label : g_help;
    ctx.esp += 8;  // ret 4
}

// a message to the game's code (response type 2): Quit Game's closes the game
X86_EXTERN_FUNC(EON_ORIG(001305A0, 0012FEA0));
X86_FUNC(EON_FN(001305A0, 0012FEA0)) {
    if (kernel::R32(ctx.esp + 4) == kQuitMessage) {
        LOGR("eon: Quit Game chosen in the main menu");
        app::Quit();
    }
    eon::pc::Message(kernel::R32(ctx.esp + 4));
    EON_ORIG(001305A0, 0012FEA0)(ctx, base);
}
