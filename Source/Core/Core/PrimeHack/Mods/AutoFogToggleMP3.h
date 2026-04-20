#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/HackConfig.h"

namespace prime {

class AutoFogToggleMP3 : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState) override {}

  GEN_NAME(AutoFogToggleMP3)
};

} // namespace prime
