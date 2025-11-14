#include "Core/PrimeHack/PrimeMod.h"

#include "Core/PrimeHack/AddressDB.h"
#include "Core/PrimeHack/HackManager.h"

#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"

namespace prime {

bool PrimeMod::should_apply_changes() const {
  std::vector<CodeChange> const& cc_vec = get_changes_to_apply();

  for (CodeChange const& change : cc_vec) {
    if (read32(change.address) != change.var) {
      return true;
    }
  }
  return false;
}

void PrimeMod::apply_instruction_changes(bool invalidate)  {
  auto active_changes = get_changes_to_apply();
  for (CodeChange const& change : active_changes) {
    write32(change.var, change.address);
    if (invalidate) {
      Core::System::GetInstance().GetPowerPC().ScheduleInvalidateCacheThreadSafe(change.address);
    }
  }
}

const std::vector<CodeChange>& PrimeMod::get_changes_to_apply() const {
  if (state == ModState::CODE_DISABLED ||
      state == ModState::DISABLED) {
    return original_instructions;
  }
  else {
    return current_active_changes;
  }
}

void PrimeMod::set_code_group_state(const std::string& group_name, ModState new_state) {
  if (code_groups.find(group_name) == code_groups.end()) {
    return;
  }

  group_change& cg = code_groups[std::string(group_name)];
  if (std::get<1>(cg) == new_state) {
    return; // Can't not do anything UwU! (Shio)
  }

  std::get<1>(cg) = new_state;
  std::vector<CodeChange> const& from_vec = (new_state == ModState::ENABLED ? code_changes : original_instructions);
  for (auto const& code_idx : std::get<0>(cg)) {
    current_active_changes[code_idx] = from_vec[code_idx];
  }
}

void PrimeMod::reset_mod() {
  code_changes.clear();
  original_instructions.clear();
  pending_change_backups.clear();
  current_active_changes.clear();
  initialized = false;
  on_reset();
}

void PrimeMod::set_state(ModState new_state) {
  ModState original = this->state;
  this->state = new_state;
  on_state_change(original);
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

void PrimeMod::set_code_change(u32 address, u32 var) {
  code_changes[address].var = var;
  current_active_changes[address].var = var;
}

void PrimeMod::update_original_instructions() {
  for (u32 addr : pending_change_backups) {
    original_instructions.emplace_back(addr, readi(addr));
  }
  pending_change_backups.clear();
  for (auto [addr, inst] : original_instructions) {
    INFO_LOG_FMT(ACTIONREPLAY,
                 "Saved original instruction @ 0x{:08x} with value 0x{:08x}",
                 addr, inst);
   }
}

u32 PrimeMod::lookup_address(std::string_view name) {
  return addr_db->lookup_address(hack_mgr->get_active_game(), hack_mgr->get_active_region(), name);
}

u32 PrimeMod::lookup_dynamic_address(std::string_view name) const {
  return addr_db->lookup_dynamic_address(*active_guard, hack_mgr->get_active_game(), hack_mgr->get_active_region(), name);
}

u8 PrimeMod::read8(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead_U8(*active_guard, addr);
}

u16 PrimeMod::read16(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead_U16(*active_guard, addr);
}

u32 PrimeMod::read32(u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return 0;
  }
  return PowerPC::MMU::HostRead_U32(*active_guard, addr);
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
  return PowerPC::MMU::HostRead_U64(*active_guard, addr);
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
  PowerPC::MMU::HostWrite_U8(*active_guard, var, addr);
}

void PrimeMod::write16(u16 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite_U16(*active_guard, var, addr);
}

void PrimeMod::write32(u32 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite_U32(*active_guard, var, addr);
}

void PrimeMod::write64(u64 var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite_U64(*active_guard, var, addr);
}

void PrimeMod::writef32(float var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite_F32(*active_guard, var, addr);
}

void PrimeMod::writef64(double var, u32 addr) const {
  if (active_guard == nullptr) {
    WARN_LOG_FMT(POWERPC, "Attempted active mod code outside of critical section");
    return;
  }
  PowerPC::MMU::HostWrite_F64(*active_guard, var, addr);
}
}
