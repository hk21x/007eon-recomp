// language: the game's language, from the settings window.
//
// Each exe has one place the language is set, by name (default.xbe 0xC18C0, driving.xbe 0x1F3310): at boot with the name
// saved in the profile, else what the console's language picks from the disc's list (language.locale: ENG DUT ITA SPA
// SWE on the Europe disc, FRE GER on the other, where 2 languages go by the console's alone, and the runtime's console
// is English, so German could not be had). The hooks there put the setting's name in its place when the disc has it.
#include "common/language.h"

#include <cctype>
#include <string>

#include "core/log.h"
#include "kernel/kernel.h"

namespace eon {

uint32_t language = 0;  // the setting: an index of kLanguageChoices, 0 the game's choice

// the settings window's names; the first is the game's own choice
const char* const kLanguageChoices[] = {"The game's choice", "English", "German", "French", "Spanish", "Italian", "Dutch",
                                        "Swedish", nullptr};

uint32_t ChosenLanguage(X86Context& ctx, uint32_t count, uint32_t name) {
    static const char* const kNames[] = {"", "ENG", "GER", "FRE", "SPA", "ITA", "DUT", "SWE"};
    if (language == 0 || language >= sizeof(kNames) / sizeof(kNames[0])) return 0;
    uint32_t n = kernel::CallGuest(ctx, count, {});
    for (uint32_t i = 0; i < n && i < 16; i++) {
        uint32_t s = kernel::CallGuest(ctx, name, {i});
        if (!s) continue;
        std::string entry;
        for (uint32_t k = 0; k < 15; k++) {
            char c = char(kernel::G<uint8_t>(s + k));
            if (!c) break;
            entry += char(std::toupper(static_cast<unsigned char>(c)));
        }
        if (entry == kNames[language]) return s;
    }
    static bool said = false;
    if (!said) LOGR("eon: the setting's language (%s) is not on this disc: the game's own choice", kNames[language]);
    said = true;
    return 0;
}

}  // namespace eon
