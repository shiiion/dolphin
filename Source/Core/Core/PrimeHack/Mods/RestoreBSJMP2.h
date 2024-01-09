#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
  class RestoreBSJMP2 : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override {}
    bool init_mod(Game game, Region region) override {
      switch (game) {
      case Game::PRIME_2:
        if (region == Region::NTSC_U)
        {
          // when instant unmorph occurs set to falling instead of apply jump
          add_code_change(0x801436f0, 0x38800004);
        }
        else if (region == Region::NTSC_J)
        {
          // when instant unmorph occurs set to falling instead of apply jump
          add_code_change(0x80142d0c, 0x38800004);
        }
        else if (region == Region::PAL)
        {
          // when instant unmorph occurs set to falling instead of apply jump
          add_code_change(0x80144e64, 0x38800004);
        }
        break;
      }

      return true;
    }
    void on_state_change(ModState old_state) override {}
  };
}  // namespace prime
