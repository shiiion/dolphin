#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

class ElfModLoader : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {}
  bool init_mod(Game game, Region region) override {
    return true;
  }
  void on_state_change(ModState old_state) override {}

  GEN_NAME(ElfModLoader)
};

} // namespace prime
