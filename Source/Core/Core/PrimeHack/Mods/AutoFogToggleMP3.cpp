#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/Mods/AutoFogToggleMP3.h"
#include "Core/PrimeHack/HackConfig.h"

namespace prime {

void AutoFogToggleMP3::run_mod(Game game, Region) {
  if (game != Game::PRIME_3 && game != Game::PRIME_3_STANDALONE) {
    return;
  }

  bool should_use = false;

  LOOKUP_DYN(active_visor);
  should_use = read32(active_visor) == 1u;

  if (GetFogDisabled() != should_use) {
    SetFogDisabled(should_use);
  }
}

bool AutoFogToggleMP3::init_mod(Game, Region) {
  return true;
}

} // namespace prime
