#pragma once
// PC Settings in the game's own menus (pc_settings.cpp)
#include <cstdint>
#include <vector>

#include "game/fng.h"

namespace eon::pc {

bool EditOptions(std::vector<fng::Chunk>& top);  // frontend_options.fng: the unused row becomes PC Settings
bool EditPauseOptions(std::vector<fng::Chunk>& top);  // pm_options.fng: a row that opens it
uint32_t Text(uint32_t hash);                    // a string of ours (guest address), 0 for any other hash
bool Message(uint32_t hash);                     // a message of ours: acted on
void Words(const char* single_player);           // the language the game shows, by its words for Single Player

}  // namespace eon::pc
