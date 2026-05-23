#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

// MOD PURPOSE: Disabled hud popups when acquiring items, among other things
class DisableHudMemoPopup : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState) override {}
  bool is_cheat() const override { return true; }
  GEN_NAME(DisableHudMemoPopup)

private:
  void init_mod_mp1(Region region);
  void init_mod_mp1gc(Region region);
  void init_mod_mp1gc_r1();
  void init_mod_mp1gc_r2();

  void init_mod_mp3(Game game, Region region);
};

} // namespace prime
