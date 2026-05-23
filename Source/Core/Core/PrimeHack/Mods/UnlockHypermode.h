#pragma once

#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/System.h"

namespace prime {

// MOD PURPOSE: Unlock hypermode difficulty for trilogy
class UnlockHypermode : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {
    if (game != Game::MENU) {
      return;
    }

    u32 base = read32(Core::System::GetInstance().GetPPCState().gpr[13] - r13_offset);
    write8(1, base + 0x68);
    write8(1, base + 0xa0);
    write8(1, base + 0xd8);
  }

  bool init_mod(Game game, Region region) override {
    if (region == Region::NTSC_U) {
      r13_offset = 0x2f94;
    } else if (region == Region::PAL) {
      r13_offset = 0x2f34;
    }

    return true;
  }

  void on_state_change(ModState) override {}

  bool is_cheat() const override { return true; }
  GEN_NAME(UnlockHypermode)

private:
  u32 r13_offset;
};

} // namespace prime
