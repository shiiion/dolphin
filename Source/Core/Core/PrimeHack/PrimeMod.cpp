#include "Core/PrimeHack/PrimeMod.h"

#include "Common/Assembler/GekkoAssembler.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/AddressDB.h"
#include "Core/PrimeHack/HackManager.h"
#include "Core/System.h"

namespace prime {

bool PrimeMod::should_apply_changes() const {
  if (is_cheat() && CachedHardcoreEnabled()) {
    return false;
  }
  std::vector<CodeChange> const& cc_vec = get_changes_to_apply();

  for (CodeChange const& change : cc_vec) {
    if (read32(change.address) != change.var) {
      return true;
    }
  }
  return false;
}

void PrimeMod::apply_instruction_changes(bool invalidate)  {
  if (is_cheat() && CachedHardcoreEnabled()) {
    return;
  }

  auto active_changes = get_changes_to_apply();
  for (CodeChange const& change : active_changes) {
    write32(change.var, change.address);
    if (invalidate) {
      Core::System::GetInstance().GetPowerPC().ScheduleInvalidateCacheThreadSafe(change.address);
    }
  }
}

void PrimeMod::apply_original_instructions(bool invalidate) {
  if (is_cheat() && CachedHardcoreEnabled()) {
    return;
  }

  for (CodeChange const& change : original_instructions) {
    write32(change.var, change.address);
    if (invalidate) {
      Core::System::GetInstance().GetPowerPC().ScheduleInvalidateCacheThreadSafe(change.address);
    }
  }
}

const std::vector<CodeChange>& PrimeMod::get_changes_to_apply() const {
  if (state == ModState::DISABLED || patches_disabled) {
    return original_instructions;
  } else {
    return current_active_changes;
  }
}

void PrimeMod::set_code_group_state(std::string_view group_name, ModState new_state) {
  auto cg_it = code_groups.find(group_name);
  if (cg_it == code_groups.end()) {
    return;
  }

  group_change& cg = cg_it->second;
  if (std::get<1>(cg) == new_state) {
    return; // Can't not do anything UwU! (Shio)
    // Mangler LARPing as a furry ^
  }

  std::get<1>(cg) = new_state;
  std::vector<CodeChange> const& from_vec = (new_state == ModState::ENABLED ? code_changes : original_instructions);
  for (auto const& code_idx : std::get<0>(cg)) {
    current_active_changes[code_idx] = from_vec[code_idx];
  }
}

void PrimeMod::overlay_disable() {
  stashed_state = state;
  set_state(ModState::DISABLED);
  apply_instruction_changes(true);
}

void PrimeMod::lift_overlay() {
  if (stashed_state) {
    set_state(*stashed_state);
    apply_instruction_changes(true);
  }
  stashed_state = std::nullopt;
}

void PrimeMod::reset_mod() {
  code_changes.clear();
  original_instructions.clear();
  pending_change_backups.clear();
  code_groups.clear();
  current_active_changes.clear();
  initialized = false;
  patches_disabled = false;
  on_reset();
}

void PrimeMod::set_state(ModState new_state) {
  ModState original = this->state;
  if (is_cheat() && CachedHardcoreEnabled()) {
    return;
  }

  this->state = new_state;
  if (original != new_state) {
    on_state_change(original);
  }
}

void PrimeMod::set_state_no_notify(ModState new_state) {
  this->state = new_state;
}

void PrimeMod::add_code_change(u32 addr, u32 code, std::string_view group) {
  if (group != "") {
    group_change& cg = code_groups[std::string(group)];
    std::get<0>(cg).push_back(code_changes.size());
    std::get<1>(cg) = ModState::ENABLED;
  }
  pending_change_backups.emplace_back(addr);
  code_changes.emplace_back(addr, code);
  current_active_changes.emplace_back(addr, code);
}

void PrimeMod::add_asm_patch(std::string_view asm_patch, std::string_view group) {
  using namespace Common::GekkoAssembler;
  auto result = Assemble(asm_patch, 0);
  if (IsFailure(result)) {
    fprintf(stderr, "Assembler failure: %s\n", GetFailure(result).FormatError().c_str());
    fflush(stderr);
    ASSERT(false);
  }
  std::vector<CodeBlock> const& code_changes_blocks = GetT(result);
  for (auto const& block : code_changes_blocks) {
    for (u32 i = 0; i < block.instructions.size(); i += 4) {
      add_code_change(block.block_address + i, Common::swap32(&block.instructions[i]), group);
    }
  }
}

void PrimeMod::add_module_code_change(u32 reladdr, u32 code, std::string_view module) {
  auto mc_it = pending_dyna_changes.find(module);
  if (mc_it == pending_dyna_changes.end()) {
    pending_dyna_changes.emplace(std::string(module), std::vector<CodeChange> {});
    mc_it = pending_dyna_changes.find(module);
  }

  mc_it->second.emplace_back(reladdr, code);
}

void PrimeMod::set_code_change(u32 address, u32 var) {
  code_changes[address].var = var;
  current_active_changes[address].var = var;
}

void PrimeMod::update_original_instructions() {
  for (u32 addr : pending_change_backups) {
    original_instructions.emplace_back(addr, readi(addr));
  }
  pending_change_backups.clear();
}

std::vector<CodeChange> const* PrimeMod::get_pending_dyna_changes(std::string_view mod_name) {
  auto mc_it = pending_dyna_changes.find(mod_name);
  if (mc_it == pending_dyna_changes.end()) {
    return nullptr;
  }
  return &mc_it->second;
}

u32 PrimeMod::lookup_address(std::string_view name) {
  return addr_db->lookup_address(GetActiveGame(), GetActiveRegion(), name);
}

u32 PrimeMod::lookup_dynamic_address(std::string_view name) const {
  return addr_db->lookup_dynamic_address(*active_guard, GetActiveGame(), GetActiveRegion(), name);
}

u8 PrimeMod::read8(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead<u8>(*active_guard, addr);
}

u16 PrimeMod::read16(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead<u16>(*active_guard, addr);
}

u32 PrimeMod::read32(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead<u32>(*active_guard, addr);
}

u32 PrimeMod::readi(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead_Instruction(*active_guard, addr);
}

u64 PrimeMod::read64(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead<u64>(*active_guard, addr);
}

float PrimeMod::readf32(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return std::bit_cast<float>(read32(addr));
}

double PrimeMod::readf64(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return std::bit_cast<double>(read64(addr));
}

void PrimeMod::write8(u8 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite<u8>(*active_guard, var, addr);
}

void PrimeMod::write16(u16 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite<u16>(*active_guard, var, addr);
}

void PrimeMod::write32(u32 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite<u32>(*active_guard, var, addr);
}

void PrimeMod::write64(u64 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite<u64>(*active_guard, var, addr);
}

void PrimeMod::writef32(float var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite<float>(*active_guard, var, addr);
}

void PrimeMod::writef64(double var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite<double>(*active_guard, var, addr);
}
}
