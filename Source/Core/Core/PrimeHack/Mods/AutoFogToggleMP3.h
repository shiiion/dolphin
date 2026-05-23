#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/HackConfig.h"

namespace prime {

// MOD PURPOSE: Automatically toggle dolphin's fog setting when scan visor is on
class AutoFogToggleMP3 : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState) override {}

  bool is_cheat() const override { return false; }
  GEN_NAME(AutoFogToggleMP3)
};

} // namespace prime
