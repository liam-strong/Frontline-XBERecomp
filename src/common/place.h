#pragma once
#include <cstdint>

// A spot in the mission, kept in places.toml beside the executable.
namespace game {

void LoadPlaces();
void PlacesFrame();           // once a frame, before the settings panel draws
void NotePlayer(uint32_t player);  // the on-foot player, each time the look update runs
void DrawPlaces();

}  // namespace game
