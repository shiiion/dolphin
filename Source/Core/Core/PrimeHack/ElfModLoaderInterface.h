#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <variant>

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

struct FCC {
  char _arr[4];
  FCC();
  FCC(std::string_view sv);
  FCC(Game game, Region region);
  std::string to_string() const;
  std::pair<Game, Region> to_game_region() const;
};

struct ModVersion {
  int major;
  int minor;

  static std::optional<ModVersion> parse(std::string const& str);
  std::string to_string() const;
};

using CVarVal = std::variant<uint8_t, uint16_t, uint32_t, uint64_t, float, double, bool>;
enum class CVarType {
  INT8,
  INT16,
  INT32,
  INT64,
  FLOAT32,
  FLOAT64,
  BOOLEAN,
};
struct CVar {
  std::string name;
  std::string description;
  CVarType type;
  // Current value
  CVarVal value;
  // Default encoded in .elf
  CVarVal def;

  CVar() = default;
  CVar(std::string&& n, std::string&& d, CVarType t)
    : name(std::move(n)), description(std::move(d)), type(t) {}
};

std::string CVarValString(CVarVal const&);
std::optional<CVarVal> ParseCvarValue(CVarType type, std::string const& val);

struct Presets {
  Game game;
  Region region;
  std::string name;

  std::vector<std::tuple<std::string, CVarType, CVarVal>> vals;

  mutable bool dirty;

  bool is_persistent() const;
};

struct ElfMod {
  Game game;
  Region region;
  std::string pack_name;
  std::string elf_path;
  std::string presets_dir;
  std::vector<Presets> saved_presets;

  std::vector<CVar> var_list;
  std::vector<CodeChange> changes;
  std::vector<std::pair<std::string, u32>> vt_hooks;
  std::vector<std::pair<std::string, u32>> bl_hooks;
  std::vector<std::pair<std::string, u32>> trampolines;

  // Apply the preset to the var_list
  void apply_preset(std::string const& preset_name);
  // Load defaults encoded in the .elf to the var_list
  void load_defaults();
  // Updates or creates a new preset of the given name with the current var list
  void update_or_create_preset(std::string const& name);
  // Flush all dirty presets to disk
  void flush();
};

struct ModPack {
  std::string name;
  ModVersion version;
  std::vector<ElfMod> supported_games;
  std::string root_dir;

  ElfMod* get_mod(Game game, Region region);
  void set_mod_enabled(bool);
  bool is_mod_enabled() const {
    if (!enabled.has_value()) {
      update_cache();
    }
    return *enabled;
  }

private:
  void update_cache() const;
  mutable std::optional<bool> enabled;
};

// Config launch option: select a preset for a mod+game+region to be the initial preset
void AddInitialPreset(std::string const& mod, std::string const& gr, std::string const& file);
// Stupid helper to check if the modloader is enabled by config
bool ModLoaderEnabled();
// Get the list of mods which have been detected in the Dolphin userdata directory, as filled by
// RefreshMods
std::vector<ModPack> const& GetAvailableMods();
// Find a ModPack by name
ModPack* GetPack(std::string const& name);

// Refreshes available ModPack list from the Dolphin userdata directory
void RefreshMods();

// Import a mod into the Dolphin userdata directory. Does not refresh active mod list
// Outputs string-formatted error (or empty if OK)
std::string ImportNewMod(std::string const& path);

} // namespace prime
