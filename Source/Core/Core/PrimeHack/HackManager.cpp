#include "Core/PrimeHack/HackManager.h"

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
std::tuple<MOD_LIST(CSEP, X)> mods_tuple;
#undef CSEP
#undef X

} // namespace

#define GEN_GET_MOD(ty) \
  template <> \
  ty* GetMod() { \
    return &std::get<ty>(mods_tuple); \
  }

MOD_LIST(GEN_GET_MOD, GEN_GET_MOD)

namespace {

Game active_game = Game::INVALID_GAME;
Game last_game = Game::INVALID_GAME;
Region active_region = Region::INVALID_REGION;
Region last_region = Region::INVALID_REGION;

// Updates the active_game and active_region globals
void update_active_game_region(const Core::CPUThreadGuard& cpu_guard) {
  switch (PowerPC::MMU::HostRead_Instruction(cpu_guard, 0x8046d340)) {
    case 0x38000018:
      active_game = Game::MENU;
      active_region = Region::NTSC_U;
      break;
    case 0x7c0000d0:
      active_game = Game::MENU;
      active_region = Region::PAL;
      break;
    case 0x4e800020:
      active_game = Game::PRIME_1;
      active_region = Region::NTSC_U;
      break;
    case 0x7c962378:
      active_game = Game::PRIME_1;
      active_region = Region::PAL;
      break;
    case 0x4bff64e1:
      active_game = Game::PRIME_2;
      active_region = Region::NTSC_U;
      break;
    case 0x80830000:
      active_game = Game::PRIME_2;
      active_region = Region::PAL;
      break;
    case 0x80010070:
      if (PowerPC::MMU::HostRead_U32(cpu_guard, 0x80576ae8) == 0x7d415378) {
        active_game = Game::PRIME_3;
        active_region = Region::NTSC_U;
      } else {
        active_game = Game::INVALID_GAME;
        active_region = Region::INVALID_REGION;
      }
      break;
    case 0x3a800000:
      if (PowerPC::MMU::HostRead_U32(cpu_guard, 0x805795a4) == 0x7d415378) {
        active_game = Game::PRIME_3;
        active_region = Region::PAL;
      } else {
        active_game = Game::INVALID_GAME;
        active_region = Region::INVALID_REGION;
      }
      break;
    default:
      switch (PowerPC::MMU::HostRead_U32(cpu_guard, 0x80000000)) {
        case FOURCC('G', 'M', '8', 'E'):
          active_region = Region::NTSC_U;
          switch (PowerPC::MMU::HostRead_U8(cpu_guard, 0x80000007)) {
            case 0:
              active_game = Game::PRIME_1_GCN;
              break;
            case 1:
              active_game = Game::PRIME_1_GCN_R1;
              break;
            case 2:
              active_game = Game::PRIME_1_GCN_R2;
              break;
            default:
              active_game = Game::INVALID_GAME;
              active_region = Region::INVALID_REGION;
              break;
          }
          break;
        case FOURCC('G', 'M', '8', 'P'):
          active_game = Game::PRIME_1_GCN;
          active_region = Region::PAL;
          break;
        case FOURCC('G', '2', 'M', 'E'):
          active_game = Game::PRIME_2_GCN;
          active_region = Region::NTSC_U;
          break;
        case FOURCC('G', '2', 'M', 'P'):
          active_game = Game::PRIME_2_GCN;
          active_region = Region::PAL;
          break;
        case FOURCC('R', 'M', '3', 'E'):
          active_game = Game::PRIME_3_STANDALONE;
          active_region = Region::NTSC_U;
          break;
        case FOURCC('R', 'M', '3', 'P'):
          active_game = Game::PRIME_3_STANDALONE;
          active_region = Region::PAL;
          break;
        default:
          active_game = Game::INVALID_GAME;
          active_region = Region::INVALID_REGION;
          break;
      }
      break;
  }
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
  }, mods_tuple);
}

} // namespace

void RunActiveMods(const Core::CPUThreadGuard& cpu_guard) {
  // When launching a new title, the EH being 0 is a good sign
  // that the game isn't done loading yet
  u32 exception_hook = PowerPC::MMU::HostRead_U32(cpu_guard, 0x80000048);
  if (exception_hook == 0) {
    return;
  }

  update_active_game_region(cpu_guard);

  // Before doing any mod-related code, set the cpu guard
  foreach_mod([&cpu_guard](PrimeMod& mod) { mod.set_temporary_cpu_guard(&cpu_guard); });

  if (active_game != last_game || active_region != last_region) {
    AllocSwitchGame(active_game, active_region);
    foreach_mod([](PrimeMod& mod) { mod.reset_mod(); });
    GetVariableManager()->reset_variables();
  }

  update_mod_state_from_config();

  if (active_game != Game::INVALID_GAME && active_region != Region::INVALID_REGION) {
    foreach_mod([](PrimeMod& mod) {
      if (!mod.is_initialized() && mod.init_mod(active_game, active_region)) {
        mod.mark_initialized();
      }
      if (mod.should_apply_changes()) {
        mod.update_original_instructions();
        mod.apply_instruction_changes();
      }
    });

    last_game = active_game;
    last_region = active_region;

    foreach_mod([](PrimeMod& mod) {
      if (mod.mod_state() == ModState::ENABLED) {
        mod.run_mod(active_game, active_region);
      }
    });
  }

  foreach_mod([](PrimeMod& mod) {
    mod.set_temporary_cpu_guard(nullptr);
  });

  prime::g_mouse_input->ResetDeltas();
}

Game GetActiveGame() {
  return active_game;
}

Region GetActiveRegion() {
  return active_region;
}

void Shutdown() {
  // HACK: Called from place that does not provide
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  foreach_mod([&guard](PrimeMod& mod) {
    mod.set_temporary_cpu_guard(&guard);
    mod.reset_mod();
    mod.set_temporary_cpu_guard(nullptr);
  });

  last_game = Game::INVALID_GAME;
  last_region = Region::INVALID_REGION;
}

} // namespace prime
