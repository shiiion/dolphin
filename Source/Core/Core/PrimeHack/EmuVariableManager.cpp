#include "Core/PrimeHack/EmuVariableManager.h"

#include "Core/PowerPC/MMU.h"
#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include <cassert>

namespace prime {
void EmuVariableManager::set_variable(Core::CPUThreadGuard const& guard, const std::string& variable, u8 value) {
  auto result = variables_list.find(variable);
  if (result == variables_list.end()) {
    return;
  }

  PowerPC::MMU::HostWrite<u8>(guard, value, result->second);
}

void EmuVariableManager::set_variable(Core::CPUThreadGuard const& guard, const std::string& variable, u32 value) {
  auto result = variables_list.find(variable);
  if (result == variables_list.end()) {
    return;
  }

  PowerPC::MMU::HostWrite<u32>(guard, value, result->second);
}

void EmuVariableManager::set_variable(Core::CPUThreadGuard const& guard, const std::string& variable, float value) {
  auto result = variables_list.find(variable);
  if (result == variables_list.end()) {
    return;
  }

  PowerPC::MMU::HostWrite<float>(guard, value, result->second);
}

u32 EmuVariableManager::get_uint(Core::CPUThreadGuard const& guard, const std::string& variable) const {
  auto result = variables_list.find(variable);
  if (result == variables_list.end()) {
    return 0;
  }

  return PowerPC::MMU::HostRead<u32>(guard, result->second);
}

float EmuVariableManager::get_float(Core::CPUThreadGuard const& guard, const std::string& variable) const {
  auto result = variables_list.find(variable);
  if (result == variables_list.end()) {
    return 0;
  }

  return PowerPC::MMU::HostRead<float>(guard, result->second);
}

u32 EmuVariableManager::get_address(const std::string& variable) const {
  auto result = variables_list.find(variable);
  if (result == variables_list.end()) {
    return 0;
  }

  return result->second;
}

void EmuVariableManager::register_variable(const std::string& name) {
  if (variables_list.find(name) != variables_list.end()) {
    return;
  }

  const u32 address = GuestAllocAligned(4, 2);
  if (address == 0) {
    assert("Guest allocator out of space");
    return;
  }
  variables_list[name] = address;
}

std::pair<u32, u32> EmuVariableManager::make_lis_ori(u32 gpr_num, const std::string& name) {
  const u32 address = get_address(name);
  const u32 lis = gen_lis(gpr_num, static_cast<u16>((address >> 16) & 0xffff));
  const u32 ori = gen_ori(gpr_num, gpr_num, static_cast<u16>(address & 0xffff));

  return std::pair<u32, u32>(lis, ori);
}

void EmuVariableManager::reset_variables() {
  variables_list.clear();
}
}; // namespace prime
