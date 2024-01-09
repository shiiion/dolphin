#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include <Core/PrimeHack/HackConfig.h>

namespace prime {
  class JPToEng : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override {
      if (region == Region::NTSC_J) {
        // switch from STRG_IntroLevelLoadSubtitles to STRG_IntroLevelLoad_0
        // for some reasons english subtitles make the game crashing
        // when starting a new save file
        if (game == Game::PRIME_2) {
          write32(0x66CDE50C, 0x80838184);
        } else if (game == Game::PRIME_2_GCN) {
          write32(0x66CDE50C, 0x806FBF6C);
        }
      }
    }
    bool init_mod(Game game, Region region) override {
      if (region == Region::NTSC_J)
      {
        switch (game)
        {
        case Game::MENU_PRIME_1:
          add_code_change(0x805398e8, 0x454e474c); // JAPN -> ENGL
          break;
        case Game::MENU_PRIME_2:
          add_code_change(0x80539708, 0x454e474c); // JAPN -> ENGL
          break;
        case Game::PRIME_1:
          add_code_change(0x8047bf50, 0x454e474c); // JAPN -> ENGL
          break;
        // MP1 GC JP version is known to have "Select language" which
        // shouldn't show up (minor glitch)
        case Game::PRIME_1_GCN:
          add_code_change(0x803c4748, 0x454e474c); // JAPN -> ENGL
          break;
        case Game::PRIME_2:
          add_code_change(0x804ace08, 0x454e474c); // JAPN -> ENGL
          break;
        case Game::PRIME_2_GCN:
          add_code_change(0x803b2f40, 0x454e474c); // JAPN -> ENGL
          break;
        case Game::PRIME_3_STANDALONE:
          add_code_change(0x8058ba68, 0x454e474c); // JAPN -> ENGL
          break;
        }
      }
      return true;
    }
    void on_state_change(ModState old_state) override {}
  };
}
