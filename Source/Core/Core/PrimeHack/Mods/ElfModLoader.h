#pragma once

#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PrimeHack/ElfModLoaderInterface.h"
#include "Core/PrimeHack/PrimeMod.h"

#include <vector>
#include <utility>

namespace prime {

enum class State : u8 {
  ACTIVE,      // Currently linked and running
  INIT,        // First time loading a mod
  REINIT,      // Initializing a mod which was unloaded
  UNLOAD_REQ,  // Mod has been requested to be unloaded
  UNLOAD_PEND, // Waiting for the mod to finish any cleanup
  UNLOADED,    // Mod has been fully unloaded
  NOT_FOUND,   // Mod not supported for the current game/region
};

// NOTE: All LiveMod data exists only during emulation being active, when the ElfMod/ModPack data is
// effectively locked down from any user changes. It is safe to have direct pointers to this data
// following these guarantees
struct LiveMod {
  std::string pack_name;
  State state;
  u32 state_tbl_idx;
  u32 load_slide;

  // Only valid when state == State::ACTIVE
  struct {
    ElfMod const* base;
    // var_addr_list is in lockstep with corresponding ElfMod's CVarList
    std::vector<u32> var_addr_list;
    std::vector<std::pair<u32, u32>> vt_hooks;
    std::vector<std::pair<u32, u32>> bl_hooks;
    std::vector<std::pair<u32, u32>> trampolines;
  } linked;
};

class ElfModLoader : public PrimeMod {
public:
  inline static constexpr u32 kMaxMods = 64;

  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}
  void on_reset() override;
  GEN_NAME(ElfModLoader)

private:
  std::vector<LiveMod> active_mods;
  PPCSymbolDB symbol_db;
  u32 debug_output_addr = 0;
  u32 next_load_slide = 0;

  struct CallgateData {
    u32 state_free_idx;
    u32 cg_free_idx;
    u32 dp_free_idx;
    u32 tr_free_idx;

    u32 sentinel_base;
    u32 dispatcher_base;
    u32 fini_nullsub_base;
    u32 state_table_base;
    u32 cg_table_base;
    u32 dp_table_base;
    u32 tr_table_base;
    bool valid;

    void reset() { valid = false; }
    void remap();
  } cg;

private:
  void sync_mod_states();
  void update_bat_regs();

  bool load_mod(LiveMod& mod, Game, Region);

  // NOTE: Patches to the callgate region is not included in the CodeChanges vector
  u32 add_callgate_entry(u32 hook_target, u32 vfte_addr, u32 mod_index);
  u32 add_trampoline_restore_entry(u32 original_addr);
  void create_vthook_callgated(u32 hook_target, u32 original_addr, u32 mod_index);
  void create_blhook_callgated(u32 hook_target, u32 bl_addr, u32 mod_index);
  void create_trampoline_callgated(u32 hook_target, u32 func_start, u32 mod_index);
  bool create_cleanup_hook(Game game, Region region, u32 mod_index);

  void write_cvar_val(CVarVal val, u32 addr);
  void read_cvar(CVar& var);
};

} // namespace prime
