#include "Core/PrimeHack/ElfModLoaderInterface.h"

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/StringUtil.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"
#include "Core/Boot/ElfReader.h"
#include "Core/Config/MainSettings.h"
#include "Core/System.h"

#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>

namespace prime {
namespace {

namespace fs = std::filesystem;

// List of mods discovered from RefreshMods
std::vector<ModPack> sDiscoveredMods;
// List of presets to be loaded on init
std::vector<std::pair<std::string, std::string>> sInitialPresets;

std::optional<CVarType> parse_cvar_type(std::string const& str) {
  if (str == "f32") {
    return CVarType::FLOAT32;
  } else if (str == "bool") {
    return CVarType::BOOLEAN;
  } else if (str == "i32") {
    return CVarType::INT32;
  } else if (str == "i8") {
    return CVarType::INT8;
  } else if (str == "i16") {
    return CVarType::INT16;
  } else if (str == "i64") {
    return CVarType::INT64;
  } else if (str == "f64") {
    return CVarType::FLOAT64;
  }

  return std::nullopt;
}

constexpr std::string_view cvar_type_string(CVarType type) {
  switch (type) {
    case CVarType::INT8:
      return "i8";
    case CVarType::INT16:
      return "i16";
    case CVarType::INT32:
      return "i32";
    case CVarType::INT64:
      return "i64";
    case CVarType::FLOAT32:
      return "f32";
    case CVarType::FLOAT64:
      return "f64";
    case CVarType::BOOLEAN:
      return "bool";
  }

  return "";
}

template <typename T>
std::optional<CVarVal> parse_int(std::string const& val) {
  T parsed;
  if (TryParse(val, &parsed, 10)) {
    return std::make_optional(parsed);
  }
  return std::nullopt;
}

template <typename T>
std::optional<CVarVal> parse_flt(std::string const& val) {
  T parsed;
  if (TryParse(val, &parsed)) {
    return std::make_optional(parsed);
  }
  return std::nullopt;
}

std::optional<std::string> parse_elfpath(std::string const& rel_dir, std::string const& str) {
  fs::path search_name(str);

  if (search_name.is_absolute() && fs::exists(str)) {
    return std::make_optional<std::string>(str);
  }
  search_name = fs::path(rel_dir) / str;

  if (fs::exists(search_name)) {
    auto tmp = search_name.native();
    return std::make_optional<std::string>(tmp.begin(), tmp.end());
  }
  return std::nullopt;
}

std::string game_region_pretty(Game g, Region r) {
  std::string game_str, region_str;
  switch (g) {
    case Game::PRIME_1_GCN:
      game_str = "Metroid Prime 1 GCN Rev. 0";
      break;
    case Game::PRIME_1_GCN_R1:
      game_str = "Metroid Prime 1 GCN Rev. 1";
      break;
    case Game::PRIME_1_GCN_R2:
      game_str = "Metroid Prime 1 GCN Rev. 2";
      break;
    case Game::PRIME_2_GCN:
      game_str = "Metroid Prime 2 GCN";
      break;
    case Game::PRIME_3_STANDALONE:
      game_str = "Metroid Prime 3 Standalone";
      break;
    case Game::PRIME_1:
      game_str = "Metroid Prime 1 Trilogy";
      break;
    case Game::PRIME_2:
      game_str = "Metroid Prime 2 Trilogy";
      break;
    case Game::PRIME_3:
      game_str = "Metroid Prime 3 Trilogy";
      break;
    default:
      game_str = "Invalid";
      break;
  }
  switch (r) {
    case Region::NTSC_U:
      region_str = "NTSC-U";
      break;
    case Region::PAL:
      region_str = "PAL";
      break;
    default:
      region_str = "Invalid";
  }

  return fmt::format("{} {}", game_str, region_str);
}

std::string try_compute_defaults_from_elf(ElfMod& mod) {
  ElfReader reader(mod.elf_path);

  auto sym_map = reader.StaticSymMap();
  for (auto& var : mod.var_list) {
    auto it = sym_map.find(var.name);
    if (it == sym_map.end()) {
      return fmt::format("CVar {} not found", var.name);
    } else if (it->second == nullptr) {
      switch (var.type) {
        case CVarType::INT8:
          var.def = u8{0};
          break;
        case CVarType::INT16:
          var.def = u16{0};
          break;
        case CVarType::INT32:
          var.def = u32{0};
          break;
        case CVarType::INT64:
          var.def = u64{0};
          break;
        case CVarType::FLOAT32:
          var.def = float{0.0f};
          break;
        case CVarType::FLOAT64:
          var.def = double{0.0};
          break;
        case CVarType::BOOLEAN:
          var.def = false;
          break;
      }
    } else {
      switch (var.type) {
        case CVarType::INT8:
          var.def = Common::swap8(*it->second);
          break;
        case CVarType::INT16:
          var.def = Common::swap16(*reinterpret_cast<u16 const*>(it->second));
          break;
        case CVarType::INT32:
          var.def = Common::swap32(*reinterpret_cast<u32 const*>(it->second));
          break;
        case CVarType::INT64:
          var.def = Common::swap64(*reinterpret_cast<u64 const*>(it->second));
          break;
        case CVarType::FLOAT32:
          var.def = std::bit_cast<float>(Common::swap32(*reinterpret_cast<u32 const*>(it->second)));
          break;
        case CVarType::FLOAT64:
          var.def = std::bit_cast<double>(Common::swap64(*reinterpret_cast<u64 const*>(it->second)));
          break;
        case CVarType::BOOLEAN:
          var.def = *it->second != 0;
          break;
      }
    }
  }
  return "";
}

// ELF Mod shape:
// {
//   "binpath": "<rel/abs path to .elf>",
//   "codechanges": [
//     ["<address>", "<value>"],
//     ...
//   ], -- optional
//   "cvars": [
//     {
//       "name": "<cvar name>",
//       "desc": "<cvar description>",
//       "type": "<cvar type>"
//     },
//     ...
//   ], -- optional
//   "hooks": [
//     {
//       "mod_func": "<exported function name>",
//       "addr": "<GC/WII address to hook>",
//       "type": "<vtable, callsite, or trampoline>"
//     },
//     ...
//   ] -- optional
// }
std::expected<ElfMod, std::string>
parse_game_mod(std::string const& root_path, picojson::object const& root, Game g, Region r) {
  ElfMod mod_out;
  mod_out.game = g;
  mod_out.region = r;
  mod_out.presets_dir = (fs::path(root_path) / "presets").string();
  const std::string game_str = game_region_pretty(g, r);

  if (auto const& elf_path = root.find("binpath"); elf_path != root.end() &&
      elf_path->second.is<std::string>()) {
    auto mpath = parse_elfpath(root_path, elf_path->second.get<std::string>());
    if (mpath) {
      mod_out.elf_path = *mpath;
    } else {
      return std::unexpected(fmt::format("'binpath' field not found for game {}", game_str));
    }
  } else {
    return std::unexpected(fmt::format("Missing 'binpath' field for game {}", game_str));
  }

  if (auto const& changes = root.find("codechanges"); changes != root.end() &&
      changes->second.is<picojson::array>()) {
    for (auto const& change : changes->second.get<picojson::array>()) {
      if (!change.is<picojson::array>()) {
        return std::unexpected(fmt::format("Expected 'codechanges' elements to be array for game {}", game_str));
      }

      auto const& change_pair = change.get<picojson::array>();
      if (change_pair.size() != 2 || !change_pair[0].is<std::string>() ||
          !change_pair[1].is<std::string>()) {
        return std::unexpected(fmt::format("Malformed 'codechanges' array for game {}", game_str));
      }

      auto const& addr_s = change_pair[0].get<std::string>();
      auto const& val_s = change_pair[1].get<std::string>();

      mod_out.changes.emplace_back(
        static_cast<uint32_t>(strtoul(addr_s.c_str(), nullptr, 16)),
        static_cast<uint32_t>(strtoul(val_s.c_str(), nullptr, 16))
      );
    }
  } // Changes optional

  if (auto const& cvars = root.find("cvars"); cvars != root.end() &&
      cvars->second.is<picojson::array>()) {
    for (auto const& cvar : cvars->second.get<picojson::array>()) {
      if (!cvar.is<picojson::object>()) {
        return std::unexpected(fmt::format("Expected 'cvars' elements to be object for game {}", game_str));
      }

      CVar new_cvar;
      auto const& cvar_obj = cvar.get<picojson::object>();
      if (auto const& name = cvar_obj.find("name"); name != cvar_obj.end() &&
          name->second.is<std::string>()) {
        new_cvar.name = name->second.get<std::string>();
      } else {
        return std::unexpected(fmt::format("Malformed 'cvars' element missing 'name' field for game {}", game_str));
      }
      if (auto const& desc = cvar_obj.find("desc"); desc != cvar_obj.end() &&
          desc->second.is<std::string>()) {
        new_cvar.description = desc->second.get<std::string>();
      } else {
        return std::unexpected(fmt::format("Malformed 'cvars' element missing 'desc' field for game {}", game_str));
      }
      if (auto const& type = cvar_obj.find("type"); type != cvar_obj.end() &&
          type->second.is<std::string>()) {
        if (auto parsed_type = parse_cvar_type(type->second.get<std::string>()); parsed_type) {
          new_cvar.type = *parsed_type;
        } else {
        return std::unexpected(
            fmt::format("Invalid 'type' field in 'cvars' element for game {}: {}",
                        game_str, type->second.get<std::string>()));
        }
      } else {
        return std::unexpected(fmt::format("Malformed 'cvars' element missing 'type' field for game {}", game_str));
      }

      mod_out.var_list.emplace_back(std::move(new_cvar));
    }

    std::sort(mod_out.var_list.begin(), mod_out.var_list.end(),
              [](CVar const& lhs, CVar const& rhs) { return lhs.name < rhs.name; });
  } // CVars optional

  if (auto const& hooks = root.find("hooks"); hooks != root.end() &&
      hooks->second.is<picojson::array>()) {
    for (auto const& hook : hooks->second.get<picojson::array>()) {
      if (!hook.is<picojson::object>()) {
        return std::unexpected(fmt::format("Expected 'hooks' elements to be object for game {}", game_str));
      }

      auto const& hook_obj = hook.get<picojson::object>();
      std::string mod_func_sym;
      if (auto const& hook_func = hook_obj.find("mod_func"); hook_func != hook_obj.end() &&
          hook_func->second.is<std::string>()) {
        mod_func_sym = hook_func->second.get<std::string>();
      } else {
        return std::unexpected(fmt::format("Malformed 'hooks' element missing 'mod_func' field for game {}", game_str));
      }

      u32 addr;
      if (auto const& hook_addr = hook_obj.find("addr"); hook_addr != hook_obj.end() &&
          hook_addr->second.is<std::string>()) {
        addr = static_cast<uint32_t>(strtoul(hook_addr->second.get<std::string>().c_str(), nullptr, 16));
      } else {
        return std::unexpected(fmt::format("Malformed 'hooks' element missing 'addr' field for game {}", game_str));
      }

      if (auto const& hook_type = hook_obj.find("type"); hook_type != hook_obj.end() &&
          hook_type->second.is<std::string>()) {
        if (hook_type->second.get<std::string>() == "vtable") {
          mod_out.vt_hooks.emplace_back(mod_func_sym, addr);
        } else if (hook_type->second.get<std::string>() == "callsite") {
          mod_out.bl_hooks.emplace_back(mod_func_sym, addr);
        } else if (hook_type->second.get<std::string>() == "trampoline") {
          mod_out.trampolines.emplace_back(mod_func_sym, addr);
        } else {
          return std::unexpected(
            fmt::format("Invalid 'type' field in 'hooks' element for game {}: {}", game_str,
                        hook_type->second.get<std::string>()));
        }
      } else {
        return std::unexpected(fmt::format("Malformed 'hooks' element missing 'type' field for game {}", game_str));
      }
    }
  } // Hooks optional

  if (auto err = try_compute_defaults_from_elf(mod_out); !err.empty()) {
    return std::unexpected(err);
  } else {
    // Initialize the cvar list to default values initially
    mod_out.load_defaults();
    return mod_out;
  }
}

// MPK JSON shape:
// {
//   "name": "<mod name>",
//   "version": "X.Y",
//   "<gameid>": <ELF Mod object> - see parse_game_mod
//   ...
// }
std::expected<ModPack, std::string> parse_mpk(std::string const& path) {
  ModPack result_pack;

  if (!File::Exists(path)) {
    return std::unexpected("Modpack root not found");
  }

  picojson::value root;
  std::string error;
  if (!JsonFromFile(path, &root, &error)) {
    return std::unexpected(fmt::format("Failed to parse json: {}", error));
  }

  if (!root.is<picojson::object>()) {
    return std::unexpected("Invalid json");
  }
  const auto& root_obj = root.get<picojson::object>();

  if (auto name = root_obj.find("name"); name != root_obj.end() &&
      name->second.is<std::string>()) {
    result_pack.name = name->second.get<std::string>();
  } else {
    return std::unexpected("Modpack missing 'name' field");
  }

  if (auto vers = root_obj.find("version"); vers != root_obj.end() &&
      vers->second.is<std::string>()) {
    if (auto vers_parse = ModVersion::parse(vers->second.get<std::string>()); vers_parse) {
      result_pack.version = *vers_parse;
    } else {
      return std::unexpected("Invalid 'version' formatting, expected form X.Y");
    }
  } else {
    return std::unexpected("Modpack missing 'version' field");
  }

  // List of supported games for the elf mod loader system
  const std::array<FCC, 16> supported_games = {
    FCC(Game::PRIME_1_GCN, Region::NTSC_U),
    FCC(Game::PRIME_1_GCN_R1, Region::NTSC_U),
    FCC(Game::PRIME_1_GCN_R2, Region::NTSC_U),
    FCC(Game::PRIME_2_GCN, Region::NTSC_U),
    FCC(Game::PRIME_3_STANDALONE, Region::NTSC_U),
    FCC(Game::PRIME_1, Region::NTSC_U),
    FCC(Game::PRIME_2, Region::NTSC_U),
    FCC(Game::PRIME_3, Region::NTSC_U),

    // Can't support PAL games, since they seem to strongly dislike extending RAM
    // but in case I figure something out in the future there's no harm in leaving this in here
    FCC(Game::PRIME_1_GCN, Region::PAL),
    FCC(Game::PRIME_1_GCN_R1, Region::PAL),
    FCC(Game::PRIME_1_GCN_R2, Region::PAL),
    FCC(Game::PRIME_2_GCN, Region::PAL),
    FCC(Game::PRIME_3_STANDALONE, Region::PAL),
    FCC(Game::PRIME_1, Region::PAL),
    FCC(Game::PRIME_2, Region::PAL),
    FCC(Game::PRIME_3, Region::PAL),
  };

  std::string base_path = fs::path(path).parent_path().string();
  for (auto game_fcc : supported_games) {
    if (auto game_def = root_obj.find(game_fcc.to_string()); game_def != root_obj.end() &&
        game_def->second.is<picojson::object>()) {
      auto [game, region] = game_fcc.to_game_region();
      auto parse_result = parse_game_mod(base_path, game_def->second.get<picojson::object>(), game, region);
      if (parse_result.has_value()) {
        parse_result->pack_name = result_pack.name;
        result_pack.supported_games.emplace_back(std::move(*parse_result));
      } else {
        // Any malformed mods in a modpack should fail the entire modpack
        return std::unexpected(parse_result.error());
      }
    }
  }

  return result_pack;
}

bool extract_zip(void* zip_reader, std::string const& ext_path) {
  mz_zip_reader_goto_first_entry(zip_reader);

  std::vector<u8> file_contents;
  do {
    mz_zip_file* file_info;
    mz_zip_reader_entry_get_info(zip_reader, &file_info);

    // Skip any directories
    if (mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby) == MZ_OK) {
      continue;
    }

    file_contents.resize(file_info->uncompressed_size);
    if (!Common::ReadFileFromZip(zip_reader, file_contents.data(), file_contents.size())) {
      return false;
    }

    std::string full_path = ext_path + "/" + file_info->filename;
    if (!File::CreateFullPath(full_path)) {
      return false;
    }

    std::ofstream unzip_stream;
    File::OpenFStream(unzip_stream, full_path, std::ios_base::out | std::ios::binary);
    if (!unzip_stream.good()) {
      return false;
    }

    unzip_stream.write(reinterpret_cast<char*>(file_contents.data()), file_contents.size());
  } while (mz_zip_reader_goto_next_entry(zip_reader) != MZ_END_OF_LIST);

  return true;
}

// Preset JSON shape:
// {
//   "game": "<gameid>",
//   "presets": [
//     {
//       "name": "<cvar name>",
//       "type": "<cvar type>",
//       "val": "<cvar val>"
//     },
//   ...
//   ]
// }
std::string parse_preset(ModPack& pack, fs::path const& path) {
  Presets result;

  std::string error;
  picojson::value root;
  if (!JsonFromFile(path.string(), &root, &error)) {
    return error;
  }

  if (!root.is<picojson::object>()) {
    return "Invalid JSON";
  }
  const auto& root_obj = root.get<picojson::object>();

  FCC preset_game;
  if (auto gamergn = root_obj.find("game"); gamergn != root_obj.end() &&
      gamergn->second.is<std::string>()) {
    preset_game = FCC(gamergn->second.get<std::string>());
  } else {
    return "Missing 'game' field";
  }

  auto [game, region] = preset_game.to_game_region();
  ElfMod* preset_mod = pack.get_mod(game, region);
  if (preset_mod == nullptr) {
    return "Preset for unsupported game/region";
  }

  result.game = game;
  result.region = region;

  if (auto var_list = root_obj.find("presets"); var_list != root_obj.end() &&
      var_list->second.is<picojson::array>()) {
    for (auto const& var : var_list->second.get<picojson::array>()) {
      if (!var.is<picojson::object>()) {
        return "Invalid presets array";
      }
      auto const& var_obj = var.get<picojson::object>();

      std::string var_name;
      CVarType var_type;
      std::string var_val;

      if (auto const& name = var_obj.find("name"); name != var_obj.end() &&
          name->second.is<std::string>()) {
        var_name = name->second.get<std::string>();
      } else {
        return "Invalid preset var missing 'name' field";
      }

      if (auto const& type = var_obj.find("type"); type != var_obj.end() &&
          type->second.is<std::string>()) {
        if (auto parsed_type = parse_cvar_type(type->second.get<std::string>()); parsed_type) {
          var_type = *parsed_type;
        } else {
          return "Invalid 'type' field in preset var";
        }
      } else {
        return "Invalid preset var missing 'type' field";
      }

      if (auto const& val = var_obj.find("val"); val != var_obj.end() &&
          val->second.is<std::string>()) {
        var_val = val->second.get<std::string>();
      } else {
        return "Invalid preset var missing 'val' field";
      }

      if (auto parsed_val = ParseCvarValue(var_type, var_val); parsed_val) {
        result.vals.emplace_back(std::move(var_name), var_type, *parsed_val);
      } else {
        return "Preset 'val' field could not be parsed";
      }
    }
  }

  result.name = path.stem().string();
  result.dirty = false;
  auto const& res_ref = preset_mod->saved_presets.emplace_back(std::move(result));

  if (res_ref.is_persistent()) {
    // Since this is the initial loading state of this mod, set the active preset to the persist
    // preset on disk
    preset_mod->apply_preset(preset_game.to_string());
  }

  return "";
}

void read_modpack_data(ModPack& pack, fs::path const& root_dir) {
  pack.root_dir = root_dir.string();

  const auto presets_dir = root_dir / "presets";
  // No presets to load
  if (!fs::exists(presets_dir) || !fs::is_directory(presets_dir)) {
    return;
  }

  for (auto it : fs::directory_iterator(presets_dir)) {
    // Configs should only be json files
    if (it.path().extension() != ".json" || !it.is_regular_file()) {
      continue;
    }

    std::string err = parse_preset(pack, it.path());
    if (!err.empty()) {
      ERROR_LOG_FMT(PRIMEHACK, "Failed to parse preset file {}", it.path().string());
    }
  }

  // After parsing, check if we have any initial preset settings
  for (auto const& [modgr, file] : sInitialPresets) {
    for (auto& mod : pack.supported_games) {
      if (modgr == fmt::format("{}.{}", pack.name, FCC(mod.game, mod.region).to_string())) {
        const std::string stripped_filename = fs::path(file).stem().string();
        mod.apply_preset(stripped_filename);
      }
    }
  }
}

void save_preset(Presets const& preset, fs::path const& presets_dir) {
  FCC preset_gamergn = FCC(preset.game, preset.region);
  picojson::object root_obj;
  root_obj.emplace("game", preset_gamergn.to_string());

  picojson::array var_list;
  for (auto const& [var_name, var_type, var_val] : preset.vals) {
    picojson::object var_obj;
    var_obj.emplace("name", var_name);
    var_obj.emplace("type", std::string(cvar_type_string(var_type)));
    var_obj.emplace("val", CVarValString(var_val));

    var_list.emplace_back(std::move(var_obj));
  }

  root_obj.emplace("presets", std::move(var_list));

  // Don't need preset files to be human readable, they should be edited from UI
  const std::string serialized = picojson::value{root_obj}.serialize(false);

  std::string preset_file_path = (presets_dir / (preset.name + ".json")).string();
  File::CreateFullPath(preset_file_path);

  std::ofstream preset_file;
  File::OpenFStream(preset_file, preset_file_path, std::ios_base::out);
  preset_file << serialized;
}

} // namespace

FCC::FCC() {
  _arr[0] = _arr[1] = _arr[2] = _arr[3] = 'X';
}

FCC::FCC(std::string_view sv) : FCC() {
  memcpy(_arr, sv.data(), std::min(sizeof(_arr), sv.length()));
}

FCC::FCC(Game game, Region region) : FCC() {
  switch (game) {
    case Game::PRIME_1_GCN:
      _arr[0] = '1';
      _arr[1] = 'S';
      _arr[2] = '0';
      break;
    case Game::PRIME_1_GCN_R1:
      _arr[0] = '1';
      _arr[1] = 'S';
      _arr[2] = '1';
      break;
    case Game::PRIME_1_GCN_R2:
      _arr[0] = '1';
      _arr[1] = 'S';
      _arr[2] = '2';
      break;
    case Game::PRIME_1:
      _arr[0] = '1';
      _arr[1] = 'T';
      _arr[2] = '0';
      break;
    case Game::PRIME_2_GCN:
      _arr[0] = '2';
      _arr[1] = 'S';
      _arr[2] = '0';
      break;
    case Game::PRIME_2:
      _arr[0] = '2';
      _arr[1] = 'T';
      _arr[2] = '0';
      break;
    case Game::PRIME_3_STANDALONE:
      _arr[0] = '3';
      _arr[1] = 'S';
      _arr[2] = '0';
      break;
    case Game::PRIME_3:
      _arr[0] = '3';
      _arr[1] = 'T';
      _arr[2] = '0';
      break;
    default:
      break;
  }

  if (region == Region::NTSC_U) {
    _arr[3] = 'N';
  } else if (region == Region::PAL) {
    _arr[3] = 'P';
  }
}

std::string FCC::to_string() const {
  return std::string(_arr, 4);
}

std::pair<Game, Region> FCC::to_game_region() const {
  Region region;
  if (_arr[3] == 'N') {
    region = Region::NTSC_U;
  } else if (_arr[3] == 'P') {
    region = Region::PAL;
  } else {
    region = Region::INVALID_REGION;
  }

  if (_arr[0] == '1') {
    if (_arr[1] == 'S') {
      switch (_arr[2]) {
        case '0':
          return std::make_pair(Game::PRIME_1_GCN, region);
        case '1':
          return std::make_pair(Game::PRIME_1_GCN_R1, region);
        case '2':
          return std::make_pair(Game::PRIME_1_GCN_R2, region);
        default:
          break;
      }
    } else if (_arr[1] == 'T') {
      return std::make_pair(Game::PRIME_1, region);
    }
  } else if (_arr[0] == '2' && _arr[2] == '0') {
    if (_arr[1] == 'S') {
      return std::make_pair(Game::PRIME_2_GCN, region);
    } else if (_arr[1] == 'T') {
      return std::make_pair(Game::PRIME_2, region);
    }
  } else if (_arr[0] == '3' && _arr[2] == '0') {
    if (_arr[1] == 'S') {
      return std::make_pair(Game::PRIME_3_STANDALONE, region);
    } else if (_arr[1] == 'T') {
      return std::make_pair(Game::PRIME_3, region);
    }
  }

  return std::make_pair(Game::INVALID_GAME, region);
}

std::string CVarValString(CVarVal const& var) {
  if (uint8_t const* v8 = std::get_if<uint8_t>(&var); v8 != nullptr) {
    return std::to_string(static_cast<u64>(*v8));
  } else if (uint16_t const* v16 = std::get_if<uint16_t>(&var); v16 != nullptr) {
    return std::to_string(static_cast<u64>(*v16));
  } else if (uint32_t const* v32 = std::get_if<uint32_t>(&var); v32 != nullptr) {
    return std::to_string(static_cast<u64>(*v32));
  } else if (uint64_t const* v64 = std::get_if<uint64_t>(&var); v64 != nullptr) {
    return std::to_string(*v64);
  } else if (float const* f32 = std::get_if<float>(&var); f32 != nullptr) {
    return fmt::format("{}", *f32);
  } else if (double const* f64 = std::get_if<double>(&var); f64 != nullptr) {
    return fmt::format("{}", *f64);
  } else if (bool const* b = std::get_if<bool>(&var); b != nullptr) {
    return *b ? "On" : "Off";
  }
  return "";
}

std::optional<CVarVal> ParseCvarValue(CVarType type, std::string const& val) {
  switch (type) {
    case CVarType::INT8:
      return parse_int<u8>(val);
    case CVarType::INT16:
      return parse_int<u16>(val);
    case CVarType::INT32:
      return parse_int<u32>(val);
    case CVarType::INT64:
      return parse_int<u64>(val);
    case CVarType::FLOAT32:
      return parse_flt<float>(val);
    case CVarType::FLOAT64:
      return parse_flt<double>(val);
    case CVarType::BOOLEAN:
      return val == "On" ? std::make_optional(true) : (val == "Off" ? std::make_optional(false) : std::nullopt);
  }
  return std::nullopt;
}

bool Presets::is_persistent() const {
  return name == FCC(game, region).to_string();
}

ElfMod* ModPack::get_mod(Game game, Region region) {
  auto mod_it = std::find_if(supported_games.begin(), supported_games.end(), [game, region](ElfMod const& mod) {
    return mod.game == game && mod.region == region;
  });
  return mod_it != supported_games.end() ? &*mod_it : nullptr;
}

void ModPack::set_mod_enabled(bool en) {
  update_cache();
  enabled = en;
  File::WriteStringToFile(root_dir + "/state.txt", en ? "y" : "n");
}

void ModPack::update_cache() const {
  fs::path state_file = fs::path(root_dir) / "state.txt";
  if (!fs::exists(state_file)) {
    File::CreateEmptyFile(state_file.string());
    File::WriteStringToFile(state_file.string(), "n");
    enabled = false;
  } else {
    std::string contents;
    File::ReadFileToString(state_file.string(), contents);
    if (contents == "y") {
      enabled = true;
    } else if (contents == "n") {
      enabled = false;
    } else {
      enabled = false;
      File::WriteStringToFile(state_file.string(), "n");
    }
  }
}

void AddInitialPreset(std::string const& mod, std::string const& gr, std::string const& file) {
  auto [game, region] = FCC(gr).to_game_region();
  if (game == Game::INVALID_GAME || region == Region::INVALID_REGION) {
    WARN_LOG_FMT(PRIMEHACK,
                 "Invalid GameRegion '{}'. Expected FourCC of format <1/2/3><S/T><0/1/2><N/P>", gr);
    return;
  }
  sInitialPresets.emplace_back(fmt::format("{}.{}", mod, gr), file);
}

bool ModLoaderEnabled() {
  return Config::Get(Config::PRIMEHACK_MODLOADER_ENABLED);
}

std::vector<ModPack> const& GetAvailableMods() {
  return sDiscoveredMods;
}

ModPack* GetPack(std::string const& name) {
  auto pack_it = std::find_if(sDiscoveredMods.begin(), sDiscoveredMods.end(), [&name](ModPack& pack) {
    return pack.name == name;
  });
  return pack_it == sDiscoveredMods.end() ? nullptr : &*pack_it;
}

std::optional<ModVersion> ModVersion::parse(std::string const& str) {
  auto majmin = SplitString(str, '.');
  if (majmin.size() != 2) {
    return std::nullopt;
  }
  ModVersion result;
  if (!TryParse(majmin[0], &result.major)) {
    return std::nullopt;
  }
  if (!TryParse(majmin[1], &result.minor)) {
    return std::nullopt;
  }

  return result;
}

std::string ModVersion::to_string() const {
  return fmt::format("{}.{}", major, minor);
}

void ElfMod::apply_preset(std::string const& preset_name) {
  Presets const* presets = nullptr;
  for (auto const& p : saved_presets) {
    if (p.name == preset_name) {
      presets = &p;
      break;
    }
  }

  if (presets == nullptr) {
    WARN_LOG_FMT(PRIMEHACK, "Preset name '{}' not found for mod '{}' with game {} {}",
                 preset_name, pack_name, game_str(game), region_str(region));
    return;
  }

  for (auto& var : var_list) {
    for (auto const& [var_name, var_type, var_val] : presets->vals) {
      if (var.name == var_name && var.type == var_type) {
        var.value = var_val;
        break;
      }
    }
  }
}

void ElfMod::load_defaults() {
  for (auto& var : var_list) {
    var.value = var.def;
  }
}

void ElfMod::update_or_create_preset(std::string const& name) {
  Presets* updating_preset = nullptr;
  for (auto& p : saved_presets) {
    if (p.name == name) {
      updating_preset = &p;
      break;
    }
  }
  if (updating_preset == nullptr) {
    updating_preset = &saved_presets.emplace_back();
  }
  updating_preset->game = game;
  updating_preset->region = region;
  updating_preset->name = name;
  updating_preset->dirty = true;

  updating_preset->vals.clear();
  for (auto const& cvar : var_list) {
    updating_preset->vals.emplace_back(cvar.name, cvar.type, cvar.value);
  }
}

void ElfMod::flush() {
  // Bit of a hack, forcefully update the persistent preset with current state, dirty it, then it's
  // guaranteed to flush immediately after
  update_or_create_preset(FCC(game, region).to_string());

  for (auto const& preset : saved_presets) {
    if (preset.dirty) {
      save_preset(preset, presets_dir);
      preset.dirty = false;
    }
  }
}

void RefreshMods() {
  // Can't update available mods list when emulation is running
  auto& system = Core::System::GetInstance();
  if (Core::IsRunningOrStarting(system)) {
    WARN_LOG_FMT(PRIMEHACK, "Attempted to refresh modpack list when emulation is active");
    return;
  }

  const fs::path modloader_root = StringToPath(File::GetUserPath(D_PRIMEHACK_MODLOADER_IDX));

  sDiscoveredMods.clear();

  for (auto it : fs::directory_iterator(modloader_root)) {
    if (!fs::is_directory(it.path())) {
      continue;
    }

    for (auto mod_it : fs::directory_iterator(it.path())) {
      if (mod_it.path().extension() == ".mpk") {
        auto parse_result = parse_mpk(mod_it.path().string());
        if (parse_result.has_value()) {
          read_modpack_data(*parse_result, it.path());
          sDiscoveredMods.emplace_back(std::move(*parse_result));
          break;
        } else {
          ERROR_LOG_FMT(PRIMEHACK, "Failed to parse modpack {}, reason: {}",
                        mod_it.path().string(), parse_result.error());
        }
      }
    }
  }
}

std::string ImportNewMod(std::string const& path) {
  void* zip_reader = mz_zip_reader_create();
  if (!zip_reader) {
    return "Internal error";
  }

  Common::ScopeGuard file_guard{[&] { mz_zip_reader_delete(&zip_reader); }};

  if (mz_zip_reader_open_file(zip_reader, path.c_str()) != MZ_OK) {
    return "Invalid ZIP file";
  }
  if (mz_zip_reader_goto_first_entry(zip_reader) != MZ_OK) {
    return "Mod is missing .mpk file";
  }

  std::string mpk_name;
  do {
    mz_zip_file* file_info;
    mz_zip_reader_entry_get_info(zip_reader, &file_info);
    std::string_view fname = std::string_view(file_info->filename, file_info->filename_size);
    std::string extension;
    if (!SplitPath(fname, nullptr, nullptr, &extension)) {
      continue;
    }

    if (extension == ".mpk") {
      mpk_name = fname;
      break;
    }
  } while (mz_zip_reader_goto_next_entry(zip_reader) != MZ_END_OF_LIST);

  if (mpk_name.empty()) {
    return "Mod is missing .mpk file";
  }

  std::string tmpdir = File::CreateTempDir();
  if (tmpdir.empty()) {
    return "Internal error";
  }

  if (!extract_zip(zip_reader, tmpdir)) {
    return "Failed to extract ZIP";
  }

  auto parse_result = parse_mpk(tmpdir + "/" + mpk_name);
  if (parse_result.has_value()) {
    std::string modloader_root = File::GetUserPath(D_PRIMEHACK_MODLOADER_IDX);
    File::Copy(tmpdir, modloader_root + parse_result->name, true);
    File::DeleteDirRecursively(tmpdir);
    return "";
  } else {
    return fmt::format("Bad ModPack: {}", parse_result.error());
  }
}

} // namespace prime
