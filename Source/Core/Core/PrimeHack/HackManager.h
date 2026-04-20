#pragma once

#include <concepts>
#include <memory>
#include <map>

#include "Core/Core.h"
#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

template <typename T>
concept IsMod = std::derived_from<T, PrimeMod>;

template <IsMod T>
T* GetMod();

template <IsMod T>
void EnableMod(bool notify = true) {
  if (notify) {
    GetMod<T>()->set_state(ModState::ENABLED);
  } else {
    GetMod<T>()->set_state_no_notify(ModState::ENABLED);
  }
}
template <IsMod T>
void DisableMod(bool notify = true) {
  if (notify) {
    GetMod<T>()->set_state(ModState::DISABLED);
  } else {
    GetMod<T>()->set_state_no_notify(ModState::DISABLED);
  }
}
template <IsMod T>
void SetModEnabled(bool enabled) {
  if (enabled) {
    EnableMod<T>();
  } else {
    DisableMod<T>();
  }
}
template <IsMod T>
bool IsModActive() {
  return GetMod<T>()->mod_state() != ModState::DISABLED;
}
template <IsMod T>
void ResetMod() {
  return GetMod<T>()->reset_mod();
}
void RunActiveMods(const Core::CPUThreadGuard& cpu_guard);
Game GetActiveGame();
Region GetActiveRegion();
void Shutdown();

} // namespace prime
