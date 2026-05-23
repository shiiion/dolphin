#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

// MOD PURPOSE: Add a direct control in emulator for activating springball to all games
class SpringballButton : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState) override {}
  bool is_cheat() const override { return false; }
  GEN_NAME(SpringballButton)

private:
  void springball_code_gc(Game game, u32 start_point, u32 bomb_pup_id, u32 has_power_up, u32 bomb_jump);
  void springball_code(u32 start_point);
  void springball_check();
};

} // namespace prime
