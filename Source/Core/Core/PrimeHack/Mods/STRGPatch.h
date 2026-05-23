#pragma once

#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/PrimeMod.h"

#include <map>

namespace prime {

// Generic STRG table patcher to allow PrimeHack to dynamically change named string table entries
// Currently used for
//   - Modifying "Nunchuk Required" message
//   - Modifying "Shake Wiimote" message for Gandrayda fight
class STRGPatch : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState) override {}
  bool is_cheat() const override { return false; }
  GEN_NAME(STRGPatch)

  void patch_strg_entry_vmc_common(PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu, u32 strg_header, u32 key_ptr);
  std::map<std::string, std::pair<u32, std::string>> const& get_table() const {
    return replace_tbl;
  }
  void add_table_entry(std::string key, std::string val);
  void clear_table();

private:
  void run_mod_common(u32 tbl_address);
  void recompute_tbl_off();

  u32 guest_table_addr;
  u32 current_tbl_off;
  std::map<std::string, std::pair<u32, std::string>> replace_tbl;
};

} // namespace prime
