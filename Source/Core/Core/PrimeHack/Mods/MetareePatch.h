#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
class MetareePatch : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override {
      if ((game == Game::PRIME_1_GCN && region == Region::NTSC_U) ||  
          (game == Game::PRIME_1_GCN_R1 && region == Region::NTSC_U) ||  
          (game == Game::PRIME_1_GCN_R2 && region == Region::NTSC_U) ||  
          (game == Game::PRIME_1_GCN && region == Region::PAL) ||  
          (game == Game::PRIME_1 && region == Region::NTSC_U) ||  
          (game == Game::PRIME_1 && region == Region::PAL)) {
        LOOKUP_DYN(world);
        if (world == 0) {
          return;
        }
        LOOKUP_DYN(world_id);
        if (read32(world_id) != 0xa8be6291) {
          return;
        }
        LOOKUP_DYN(world_layers_arr);
        if (world_layers_arr == 0) {
          return;
        }
        constexpr u32 kAreaId = 9;
        constexpr u32 kStride = 16;
        u32 active_layers = read32(world_layers_arr + 0xc + kAreaId * kStride);
        if ((active_layers & 0b100) == 0) {
          active_layers |= 0b100;
          write32(active_layers, world_layers_arr + 0xc + kAreaId * kStride);
        }
      }
    }
    bool init_mod(Game game, Region region) override {
      return true;
    }
    void on_state_change(ModState old_state) override {}
    private:
  };
}
