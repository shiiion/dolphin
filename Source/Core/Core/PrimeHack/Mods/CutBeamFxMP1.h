#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

// MOD PURPOSE: Restores effects from prime 1 GC that were removed in Trilogy
class CutBeamFxMP1 : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {}
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState) override {}
  bool is_cheat() const override { return false; }
  GEN_NAME(CutBeamFxMP1)
};

}
