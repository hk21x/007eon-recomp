// The language setting, for both exes (the game's own choice unless the player made one): src/common/language.cpp
#pragma once
#include <cstdint>

#include "cpu/x86_ops.h"

namespace eon {
extern uint32_t language;                     // the setting: an index of kLanguageChoices, 0 the game's choice
extern const char* const kLanguageChoices[];  // the settings window's names, ended by nullptr
// the entry of the disc's language list (language.locale) for the setting, a guest string; 0 if the setting is the
// game's choice or the disc has no such language. count: thiscall (), name: thiscall (index), both on the game's UI
// object in ctx.ecx
uint32_t ChosenLanguage(X86Context& ctx, uint32_t count, uint32_t name);
}
