#include "Core/PrimeHack/HackManager.h"

#include "Common/Assembler/GekkoAssembler.h"
#include "Core/AchievementManager.h"
#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/HackConfig.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/Mods/ModListMacro.h"
#include "Core/PrimeHack/Mods/ModHeaders.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"
#include "InputCommon/GenericMouse.h"

namespace prime {
namespace {

#define FOURCC(a, b, c, d) (((static_cast<u32>(a) << 24) & 0xff000000) | \
                            ((static_cast<u32>(b) << 16) & 0x00ff0000) | \
                            ((static_cast<u32>(c) << 8) & 0x0000ff00) | \
                            (static_cast<u32>(d) & 0x000000ff))

#define CSEP(x) x,
#define X(x) x
std::tuple<MOD_LIST(CSEP, X)> sModsTuple;
#undef CSEP
#undef X

// Active game tracking globals
Game sActiveGame = Game::INVALID_GAME;
Game sLastGame = Game::INVALID_GAME;
Region sActiveRegion = Region::INVALID_REGION;
Region sLastRegion = Region::INVALID_REGION;
std::vector<GameChangeCallback> sGameChangeCbList;

bool sHardcoreEnabled = false;

// Dynalib tracking globals
std::unordered_map<std::string, std::pair<u32, u32>> sModuleList;
std::vector<CodeChange> sDynalibOriginal;
std::vector<CodeChange> sDynalibChanges;
u32 sEpilogueHookStub;
constexpr u32 kModuleEpilogueOffset = 0x28;
constexpr u32 kEpilogueStubSize = 0x18;
constexpr std::string_view kEpilogueAsmStub = R"(
.defvar StubBegin, 0x{stub_begin:x}
.defvar VmcallInst, 0x{vmcall_inst:x}

.locate StubBegin
.4byte VmcallInst
# The above vmcall will fix the epilogue address for us
lwz r12, 0x28(r3)
cmpwi r12, 0
beqlr
mtctr r12
bctr
)";

// Updates the sActiveGame and sActiveRegion globals
void update_active_game_region(const Core::CPUThreadGuard& cpu_guard) {
  switch (PowerPC::MMU::HostRead_Instruction(cpu_guard, 0x8046d340)) {
    case 0x38000018:
      sActiveGame = Game::MENU;
      sActiveRegion = Region::NTSC_U;
      break;
    case 0x7c0000d0:
      sActiveGame = Game::MENU;
      sActiveRegion = Region::PAL;
      break;
    case 0x4e800020:
      sActiveGame = Game::PRIME_1;
      sActiveRegion = Region::NTSC_U;
      break;
    case 0x7c962378:
      sActiveGame = Game::PRIME_1;
      sActiveRegion = Region::PAL;
      break;
    case 0x4bff64e1:
      sActiveGame = Game::PRIME_2;
      sActiveRegion = Region::NTSC_U;
      break;
    case 0x80830000:
      sActiveGame = Game::PRIME_2;
      sActiveRegion = Region::PAL;
      break;
    case 0x80010070:
      if (PowerPC::MMU::HostRead<u32>(cpu_guard, 0x80576ae8) == 0x7d415378) {
        sActiveGame = Game::PRIME_3;
        sActiveRegion = Region::NTSC_U;
      } else {
        sActiveGame = Game::INVALID_GAME;
        sActiveRegion = Region::INVALID_REGION;
      }
      break;
    case 0x3a800000:
      if (PowerPC::MMU::HostRead<u32>(cpu_guard, 0x805795a4) == 0x7d415378) {
        sActiveGame = Game::PRIME_3;
        sActiveRegion = Region::PAL;
      } else {
        sActiveGame = Game::INVALID_GAME;
        sActiveRegion = Region::INVALID_REGION;
      }
      break;
    default:
      switch (PowerPC::MMU::HostRead<u32>(cpu_guard, 0x80000000)) {
        case FOURCC('G', 'M', '8', 'E'):
          sActiveRegion = Region::NTSC_U;
          switch (PowerPC::MMU::HostRead<u8>(cpu_guard, 0x80000007)) {
            case 0:
              sActiveGame = Game::PRIME_1_GCN;
              break;
            case 1:
              sActiveGame = Game::PRIME_1_GCN_R1;
              break;
            case 2:
              sActiveGame = Game::PRIME_1_GCN_R2;
              break;
            default:
              sActiveGame = Game::INVALID_GAME;
              sActiveRegion = Region::INVALID_REGION;
              break;
          }
          break;
        case FOURCC('G', 'M', '8', 'P'):
          sActiveGame = Game::PRIME_1_GCN;
          sActiveRegion = Region::PAL;
          break;
        case FOURCC('G', '2', 'M', 'E'):
          sActiveGame = Game::PRIME_2_GCN;
          sActiveRegion = Region::NTSC_U;
          break;
        case FOURCC('G', '2', 'M', 'P'):
          sActiveGame = Game::PRIME_2_GCN;
          sActiveRegion = Region::PAL;
          break;
        case FOURCC('R', 'M', '3', 'E'):
          sActiveGame = Game::PRIME_3_STANDALONE;
          sActiveRegion = Region::NTSC_U;
          break;
        case FOURCC('R', 'M', '3', 'P'):
          sActiveGame = Game::PRIME_3_STANDALONE;
          sActiveRegion = Region::PAL;
          break;
        default:
          sActiveGame = Game::INVALID_GAME;
          sActiveRegion = Region::INVALID_REGION;
          break;
      }
      break;
  }
}

bool update_hardcore_enabled() {
#ifdef USE_RETRO_ACHIEVEMENTS
  return sHardcoreEnabled = AchievementManager::GetInstance().IsHardcoreModeActive();
#endif
}

void update_mod_state_from_config() {
  SetModEnabled<AutoEFB>(UseMPAutoEFB());
  SetModEnabled<CutBeamFxMP1>(GetEnableSecondaryGunFX());
  SetModEnabled<AutoFogToggleMP3>(GetAutoFogToggleEnabled());

  if (Config::Get(Config::MAIN_ENABLE_CHEATS)) {
    SetModEnabled<Noclip>(Config::Get(Config::PRIMEHACK_NOCLIP));
    SetModEnabled<Invulnerability>(Config::Get(Config::PRIMEHACK_INVULNERABILITY));
    SetModEnabled<SkipCutscene>(Config::Get(Config::PRIMEHACK_SKIPPABLE_CUTSCENES));
    SetModEnabled<RestoreDashing>(Config::Get(Config::PRIMEHACK_RESTORE_SCANDASH));
    SetModEnabled<FriendVouchers>(Config::Get(Config::PRIMEHACK_FRIENDVOUCHERS));
    SetModEnabled<PortalSkipMP2>(Config::Get(Config::PRIMEHACK_SKIPMP2_PORTAL));
    SetModEnabled<DisableHudMemoPopup>(Config::Get(Config::PRIMEHACK_DISABLE_HUDMEMO));
    SetModEnabled<UnlockHypermode>(Config::Get(Config::PRIMEHACK_UNLOCK_HYPERMODE));
    SetModEnabled<AllDoorAnyBeam>(Config::Get(Config::PRIMEHACK_ANYBEAM_DOOR));
  } else {
    DisableMod<Noclip>();
    DisableMod<Invulnerability>();
    DisableMod<SkipCutscene>();
    DisableMod<RestoreDashing>();
    DisableMod<FriendVouchers>();
    DisableMod<PortalSkipMP2>();
    DisableMod<DisableHudMemoPopup>();
    DisableMod<UnlockHypermode>();
    DisableMod<AllDoorAnyBeam>();
  }

  // Disallow any PrimeHack control mods
  if (!Config::Get(Config::PRIMEHACK_ENABLE) || UsingRealWiimote()) {
    DisableMod<FpsControls>();
    DisableMod<SpringballButton>();
    DisableMod<ContextSensitiveControls>();
    DisableMod<MapController>();
    return;
  } else {
    EnableMod<FpsControls>();
    EnableMod<SpringballButton>();
    EnableMod<ContextSensitiveControls>();
    if (ImprovedMotionControls()) {
      DisablePatches<ContextSensitiveControls>();
    } else {
      EnablePatches<ContextSensitiveControls>();
    }
    SetModEnabled<MapController>(NewMapControlsEnabled());
  }
}

template <typename Fn>
void foreach_mod(Fn&& fn) {
  std::apply([fn = std::forward<Fn>(fn)](auto&&... args) {
    (fn(args), ...);
  }, sModsTuple);
}

template <typename Mem>
void add_rso(Mem&& mem, u32 module_base, u32 name_base) {
  // Add a handler for the epilogue
  u32 old_epilogue_addr = mem.template Read<u32>(module_base + kModuleEpilogueOffset);
  mem.template Write<u32>(sEpilogueHookStub, module_base + kModuleEpilogueOffset);

  std::string module_name = std::filesystem::path(readin_str(mem, name_base)).filename().string();
  sModuleList.emplace(module_name, std::make_pair(module_base, old_epilogue_addr));

  foreach_mod([&mem, module_base, &module_name](PrimeMod& mod) {
    if (mod.mod_state() == ModState::DISABLED) {
      return;
    }

    auto const* changes = mod.get_pending_dyna_changes(module_name);
    if (changes == nullptr) {
      return;
    }

    for (auto const& change : *changes) {
      mem.template Write<u32>(change.var, change.address + module_base);
    }
  });
}

void mp3_post_rso_link(PowerPC::PowerPCState& state, PowerPC::MMU& mmu, u32) {
  // Hook point is directly after RSO prologue is ran. RSO is in r31, filesystem path in r29
  add_rso(mmu, state.gpr[31], state.gpr[29]);
  // Original instruction: lbz r0, 9(r1)
  state.gpr[0] = mmu.Read<u8>(state.gpr[1] + 9);
}

u32 remove_rso(PowerPC::MMU& mmu, u32 mod_base) {
  u32 epilogue = 0;
  std::erase_if(sModuleList, [&epilogue, mod_base](auto const& item) {
    if (item.second.first == mod_base) {
      epilogue = item.second.second;
      return true;
    }
    return false;
  });
  return epilogue;
}

void mp3_rso_unlink(PowerPC::PowerPCState& state, PowerPC::MMU& mmu, u32) {
  // Hook point is the prolog itself, so r3 should be a pointer to the module base
  const u32 epilogue = remove_rso(mmu, state.gpr[3]);
  // Write the old epilogue back so that savestating doesn't shit itself
  mmu.Write<u32>(epilogue, state.gpr[3] + kModuleEpilogueOffset);
}

void init_dynalib(Core::CPUThreadGuard const& guard, Game game, Region region) {
  sDynalibChanges.clear();
  sDynalibOriginal.clear();

  using namespace Common::GekkoAssembler;
  if (game != Game::PRIME_3 && game != Game::PRIME_3_STANDALONE) {
    return;
  }

  u32 load_hook = 0;
  if (game == Game::PRIME_3) {
    if (region == Region::NTSC_U) {
      load_hook = 0x8019a30c;
    } else if (region == Region::PAL) {
      load_hook = 0x80199dec;
    }
  } else if (game == Game::PRIME_3_STANDALONE) {
    if (region == Region::NTSC_U) {
      load_hook = 0x8019f8cc;
    } else if (region == Region::PAL) {
      load_hook = 0x801a05dc;
    }
  }

  const int vmc_load_hook_mp3_idx =
    Core::System::GetInstance().GetPowerPC().RegisterVmcall(mp3_post_rso_link);
  const int vmc_unload_hook_mp3_idx =
    Core::System::GetInstance().GetPowerPC().RegisterVmcall(mp3_rso_unlink);
  const u32 unload_vmc = gen_vmcall(vmc_unload_hook_mp3_idx, 0);

  sEpilogueHookStub = GuestAllocAligned(kEpilogueStubSize, 2);
  auto result =
    Assemble(fmt::format(fmt::runtime(kEpilogueAsmStub),
                         fmt::arg("stub_begin", sEpilogueHookStub),
                         fmt::arg("vmcall_inst", unload_vmc)), 0);
  ASSERT(!IsFailure(result));
  std::vector<CodeBlock> const& code_changes_blocks = GetT(result);
  for (auto const& block : code_changes_blocks) {
    for (u32 i = 0; i < block.instructions.size(); i += 4) {
      sDynalibChanges.emplace_back(block.block_address + i,
                                   Common::swap32(&block.instructions[i]));
    }
  }
  sDynalibChanges.emplace_back(load_hook, gen_vmcall(vmc_load_hook_mp3_idx, 0));

  for (CodeChange const& cc : sDynalibChanges) {
    sDynalibOriginal.emplace_back(cc.address, PowerPC::MMU::HostRead<u32>(guard, cc.address));
  }

  // This is exclusively to handle savestate loads more gracefully
  const u32 rso_list_base = GetAddressDB()->lookup_address(game, region, "rso_list_base");
  for (u32 cur_entry = PowerPC::MMU::HostRead<u32>(guard, rso_list_base + 4);
       cur_entry != 0 && cur_entry != rso_list_base;
       cur_entry = PowerPC::MMU::HostRead<u32>(guard, cur_entry + 4)) {
    const u32 rso_metadata = PowerPC::MMU::HostRead<u32>(guard, cur_entry + 0xc);
    if (rso_metadata == 0) {
      continue;
    }

    // Presumably the load state of the module, 1 seems to correspond with a loaded & linked mod
    if (PowerPC::MMU::HostRead<u32>(guard, rso_metadata + 0x20) != 1) {
      continue;
    }

    u32 module_base = PowerPC::MMU::HostRead<u32>(guard, rso_metadata + 0x18);
    u32 name_base = PowerPC::MMU::HostRead<u32>(guard, rso_metadata);

    add_rso(HostMem(guard), module_base, name_base);
  }
}

void apply_cc_list(const Core::CPUThreadGuard& cpu_guard, std::vector<CodeChange> const& cc_list) {
  for (CodeChange const& cc : cc_list) {
    if (PowerPC::MMU::HostRead<u32>(cpu_guard, cc.address) != cc.var) {
      PowerPC::MMU::HostWrite<u32>(cpu_guard, cc.var, cc.address);
    }
  }
}

} // namespace

void RunActiveMods(const Core::CPUThreadGuard& cpu_guard) {
  // When launching a new title, the EH being 0 is a good sign
  // that the game isn't done loading yet
  u32 exception_hook = PowerPC::MMU::HostRead<u32>(cpu_guard, 0x80000048);
  if (exception_hook == 0) {
    return;
  }

  update_active_game_region(cpu_guard);

  // Before doing any mod-related code, set the cpu guard
  foreach_mod([&cpu_guard](PrimeMod& mod) { mod.set_temporary_cpu_guard(&cpu_guard); });

  if (sActiveGame != sLastGame || sActiveRegion != sLastRegion) {
    UpdateHackSettings();
    AllocSwitchGame(sActiveGame, sActiveRegion);
    foreach_mod([](PrimeMod& mod) { mod.reset_mod(); });
    GetVariableManager()->reset_variables();
    sDynalibChanges.clear();
    sDynalibOriginal.clear();

    for (auto& cb : sGameChangeCbList) {
      cb(sActiveGame, sActiveRegion);
    }
    // Cache hardcore state at each game change
    update_hardcore_enabled();
  }

  update_mod_state_from_config();

  if (sActiveGame != Game::INVALID_GAME && sActiveRegion != Region::INVALID_REGION) {
    foreach_mod([](PrimeMod& mod) {
      // Skip any "cheat" mods
      if (mod.is_cheat() && sHardcoreEnabled) {
        return;
      }

      if (!mod.is_initialized() && mod.init_mod(sActiveGame, sActiveRegion)) {
        mod.mark_initialized();
      }
      if (mod.should_apply_changes()) {
        mod.update_original_instructions();
        mod.apply_instruction_changes();
      }
    });

    // Initialize the dynalib AFTER mods to capture their requested changes for savestates
    if (sDynalibChanges.empty()) {
      init_dynalib(cpu_guard, sActiveGame, sActiveRegion);
    }
    apply_cc_list(cpu_guard, sDynalibChanges);

    sLastGame = sActiveGame;
    sLastRegion = sActiveRegion;

    foreach_mod([](PrimeMod& mod) {
      if (mod.is_cheat() && sHardcoreEnabled) {
        return;
      }

      if (mod.mod_state() == ModState::ENABLED) {
        mod.run_mod(sActiveGame, sActiveRegion);
      }
    });
  }

  foreach_mod([](PrimeMod& mod) {
    mod.set_temporary_cpu_guard(nullptr);
  });

  prime::g_mouse_input->ResetDeltas();
}

Game GetActiveGame() {
  return sActiveGame;
}

Region GetActiveRegion() {
  return sActiveRegion;
}

void Shutdown() {
  // HACK: Called from place that does not provide
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  foreach_mod([&guard](PrimeMod& mod) {
    mod.set_temporary_cpu_guard(&guard);
    mod.reset_mod();
    mod.set_temporary_cpu_guard(nullptr);
  });
  sDynalibChanges.clear();
  sDynalibOriginal.clear();
  sModuleList.clear();

  sActiveGame = sLastGame = Game::INVALID_GAME;
  sActiveRegion = sLastRegion = Region::INVALID_REGION;
  for (auto& cb : sGameChangeCbList) {
    cb(Game::INVALID_GAME, Region::INVALID_REGION);
  }
}

void AddOnGameChangeCallback(GameChangeCallback cb) {
  sGameChangeCbList.emplace_back(std::move(cb));
}

void StashMemoryChanges() {
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  foreach_mod([&guard](PrimeMod& mod) {
    mod.set_temporary_cpu_guard(&guard);
    mod.overlay_disable();
    mod.set_temporary_cpu_guard(nullptr);
  });
  apply_cc_list(guard, sDynalibOriginal);
  for (auto const& module : sModuleList) {
    PowerPC::MMU::HostWrite<u32>(guard, module.second.second,
                                 module.second.first + kModuleEpilogueOffset);
  }
}

void RestoreMemoryChanges() {
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  foreach_mod([&guard](PrimeMod& mod) {
    mod.set_temporary_cpu_guard(&guard);
    mod.lift_overlay();
    mod.set_temporary_cpu_guard(nullptr);
  });
  apply_cc_list(guard, sDynalibChanges);
  for (auto const& module : sModuleList) {
    PowerPC::MMU::HostWrite<u32>(guard, sEpilogueHookStub,
                                 module.second.first + kModuleEpilogueOffset);
  }
}

bool CachedHardcoreEnabled() {
  return sHardcoreEnabled;
}

// Autogenerate GetMod<ty>
#define GEN_GET_MOD(ty) \
  template <> \
  ty* GetMod() { \
    return &std::get<ty>(sModsTuple); \
  }

MOD_LIST(GEN_GET_MOD, GEN_GET_MOD)

} // namespace prime
