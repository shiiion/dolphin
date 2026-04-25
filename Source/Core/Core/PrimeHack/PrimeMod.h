#pragma once

#include <stdint.h>
#include <map>
#include <string_view>
#include <tuple>
#include <vector>

#include "Core/Core.h"

#define GEN_NAME(cname) std::string_view mod_name() override { return #cname; }

namespace prime {

struct CodeChange {
  uint32_t address, var;
  CodeChange() : CodeChange(0, 0) {}
  CodeChange(uint32_t a, uint32_t v) : address(a), var(v) {}
};

enum class Game : int {
  INVALID_GAME = -1,
  MENU = 0,
  // Wii Games
  PRIME_1 = 3,
  PRIME_2 = 4,
  PRIME_3 = 5,
  PRIME_3_STANDALONE = 6,
  // GC Games
  PRIME_1_GCN = 7,
  PRIME_2_GCN = 8,
  PRIME_1_GCN_R1 = 9,
  PRIME_1_GCN_R2 = 10,
  MAX_VAL = PRIME_1_GCN_R2,
};

enum class Region : int {
  INVALID_REGION = -1,
  NTSC_U = 0,
  PAL = 1,
  MAX_VAL = PAL,
};

enum class ModState {
  // not running, no active instruction changes
  DISABLED,
  // running, no active instruction changes
  CODE_DISABLED,
  // running, active instruction changes
  ENABLED,
};

class HackManager;
class AddressDB;

// Skeleton for a game mod
class PrimeMod {
public:
  virtual ~PrimeMod() {};

  // Run the mod, called each time ActionReplay is hit
  virtual void run_mod(Game game, Region region) = 0;
  // Init the mod, called when a new game / new region is loaded
  // Should NOT do any modifying of the game!!!
  virtual bool init_mod(Game game, Region region) = 0;
  virtual void on_state_change(ModState old_state) = 0;
  virtual void on_reset() {}
  virtual std::string_view mod_name() = 0;

  virtual bool should_apply_changes() const;
  void apply_instruction_changes(bool invalidate = true);
  // Gets the corresponding list of code changes to apply per-frame
  const std::vector<CodeChange>& get_changes_to_apply() const;
  void add_code_change(u32 addr, u32 code, std::string_view group = "");
  void add_asm_patch(std::string_view patch, std::string_view group = "");
  void set_code_change(u32 address, u32 var);
  void update_original_instructions();
  std::vector<CodeChange>& get_code_changes() { return code_changes; }
  std::vector<CodeChange>& get_original_instructions() { return original_instructions; }

  bool has_saved_instructions() const { return !original_instructions.empty(); }
  bool is_initialized() const { return initialized; }
  void mark_initialized() { initialized = true; }
  void reset_mod();

  ModState mod_state() const { return state; }
  void set_state(ModState new_state);
  void set_state_no_notify(ModState new_state);
  void set_code_group_state(const std::string& group_name, ModState new_state);

  void set_temporary_cpu_guard(Core::CPUThreadGuard const* guard) { active_guard = guard; }

  static void set_address_database(const AddressDB* db_ptr) { addr_db = db_ptr; }

protected:
  mutable Core::CPUThreadGuard const* active_guard = nullptr;

  inline static const AddressDB* addr_db = nullptr;

  static u32 lookup_address(std::string_view name);
  u32 lookup_dynamic_address(std::string_view name) const;

  // Because of the new lovely restrictions to memory writes, these now have to be given a CPUThreadGuard
  u8 read8(u32 addr) const;
  u16 read16(u32 addr) const;
  u32 read32(u32 addr) const;
  u32 readi(u32 addr) const;
  u64 read64(u32 addr) const;
  float readf32(u32 addr) const;
  double readf64(u32 addr) const;
  void write8(u8 var, u32 addr) const;
  void write16(u16 var, u32 addr) const;
  void write32(u32 var, u32 addr) const;
  void write64(u64 var, u32 addr) const;
  void writef32(float var, u32 addr) const;
  void writef64(double var, u32 addr) const;

private:
  using group_change = std::tuple<std::vector<size_t>, ModState>;

  bool initialized = false;

  std::vector<CodeChange> original_instructions;
  std::vector<CodeChange> code_changes;
  std::vector<u32> pending_change_backups;
  std::map<std::string, group_change> code_groups;
  ModState state = ModState::DISABLED;

  std::vector<CodeChange> current_active_changes;
};

// Lookup addressdb by "name", bind to name
#define LOOKUP(name) const u32 name = lookup_address(#name)
// Lookup addressdb dynamics by "name", bind to name
#define LOOKUP_DYN(name) const u32 name = lookup_dynamic_address(#name)

} // namespace prime
