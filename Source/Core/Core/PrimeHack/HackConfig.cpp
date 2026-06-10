#include "HackConfig.h"

#include <string>
#include <mutex>

#include "Common/Config/Config.h"
#include "Common/IniFile.h"
#include "Common/FileUtil.h"
#include "Core/PrimeHack/Mods/ModHeaders.h"
#include "Core/PrimeHack/EmuVariableManager.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/GCPad.h"
#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Host.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackAltProfile.h"
#include "InputCommon/InputConfig.h"

constexpr const char* PROFILES_DIR = "Profiles/";

namespace prime {
namespace {

float sensitivity;
float cursor_sensitivity;

bool inverted_x = false;
bool inverted_y = false;
bool scale_cursor_sens = false;
AddressDB addr_db;
EmuVariableManager var_mgr;
bool is_running = false;
CameraLock lock_camera = CameraLock::Unlocked;
bool reticle_lock = false;
bool new_map_controls = false;

std::string trilogy_motd = "Thanks for using PrimeHack!\nPlease see our wiki for help!";
std::mutex motd_lock;

} // namespace

void InitializeHack() {
  if (is_running) {
    return;
  }

  is_running = true;
  PrimeMod::set_address_database(GetAddressDB());
  init_db(*GetAddressDB());
  EnableDefaultMods();
}

void EnableDefaultMods() {
  EnableMod<SkipCutscene>(false);
  EnableMod<ViewModifier>(false);
  EnableMod<DisableBloom>(false);
  EnableMod<BloomIntensityMP3>(false);
  EnableMod<MapController>(false);
  EnableMod<STRGPatch>(false);
  EnableMod<MetareePatch>(false);
  EnableMod<AutoConvertFriendVouchers>(false);

  // Enable no PrimeHack control mods
  if (!Config::Get(Config::PRIMEHACK_ENABLE)) {
    return;
  }

  EnableMod<FpsControls>(false);
  EnableMod<SpringballButton>(false);
  EnableMod<ContextSensitiveControls>(false);
  EnableMod<ElfModLoader>(false);
}

bool CheckBeamCtl(int beam_num) {
  return Wiimote::CheckBeam(0, beam_num);
}

bool CheckVisorCtl(int visor_num) {
  return Wiimote::CheckVisor(0, visor_num);
}

bool CheckVisorScrollCtl(bool direction) {
  return Wiimote::CheckVisorScroll(0, direction);
}

bool CheckBeamScrollCtl(bool direction) {
  return Wiimote::CheckBeamScroll(0, direction);
}

bool CheckSpringBallCtl() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::CheckSpringBall(0);
  } else {
    return Wiimote::CheckSpringBall(0);
  }
}

bool ImprovedMotionControls() {
  return Wiimote::CheckImprovedMotions(0);
}

bool CheckForward() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::CheckForward(0);
  } else {
    return Wiimote::CheckForward(0);
  }
}

bool CheckBack() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::CheckBack(0);
  } else {
    return Wiimote::CheckBack(0);
  }
}

bool CheckLeft() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::CheckLeft(0);
  } else {
    return Wiimote::CheckLeft(0);
  }
}

bool CheckRight() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::CheckRight(0);
  } else {
    return Wiimote::CheckRight(0);
  }
}

bool CheckJump() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::CheckJump(0);
  } else {
    return Wiimote::CheckJump(0);
  }
}

bool CheckGrappleCtl() {
  return Wiimote::CheckGrapple(0);
}

bool GrappleTappingMode() {
  return Wiimote::UseGrappleTapping(0);
}

bool GrappleCtlBound() {
  return Wiimote::GrappleCtlBound(0);
}

void SetEFBToTexture(bool toggle) {
  return Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, toggle);
}

bool GetFogDisabled() {
  return Config::Get(Config::GFX_DISABLE_FOG);
}

void SetFogDisabled(bool toggle) {
  Config::SetCurrent(Config::GFX_DISABLE_FOG, toggle);
}

bool GetAutoFogToggleEnabled() {
  return Config::Get(Config::AUTO_FOG_TOGGLE_MP3);
}

bool UseMPAutoEFB() {
  return Config::Get(Config::AUTO_EFB);
}

bool LockCameraInPuzzles() {
  return Config::Get(Config::LOCKCAMERA_IN_PUZZLES);
}

bool GetEFBTexture() {
  return Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
}

bool GetBloom() {
  return Config::Get(Config::DISABLE_BLOOM);
}

bool GetReduceBloom() {
  return Config::Get(Config::REDUCE_BLOOM);
}

float GetBloomIntensity() {
  return Config::Get(Config::BLOOM_INTENSITY);
}

bool GetEnableSecondaryGunFX() {
  return Config::Get(Config::ENABLE_SECONDARY_GUNFX);
}

bool GetShowGCCrosshair() {
  return Config::Get(Config::GC_SHOW_CROSSHAIR);
}

u32 GetGCCrosshairColor() {
  return Config::Get(Config::GC_CROSSHAIR_COLOR_RGBA);
}

bool GetAutoArmAdjust() {
  return Config::Get(Config::ARMPOSITION_MODE) == 0;
}

bool GetToggleArmAdjust() {
  return Config::Get(Config::TOGGLE_ARM_REPOSITION);
}

std::tuple<float, float, float> GetArmXYZ() {
  float x = Config::Get(Config::ARMPOSITION_LEFTRIGHT) / 100.f;
  float y = Config::Get(Config::ARMPOSITION_FORWARDBACK) / 100.f;
  float z = Config::Get(Config::ARMPOSITION_UPDOWN) / 100.f;

  return std::make_tuple(x, y, z);
}


std::pair<std::string, std::string> GetProfiles() {
  auto* group = static_cast<ControllerEmu::PrimeHackAltProfile*>(
    Wiimote::GetWiimoteGroup(0, WiimoteEmu::WiimoteGroup::AltProfileControls));

  const std::string alt_profname = group->GetAltProfileName();
  std::string alt_profile_path;
  std::string main_profile_path;

  if (!alt_profname.empty() && (alt_profname != std::string("Disabled"))) {
    alt_profile_path = File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR +
                         Wiimote::GetConfig()->GetProfileKey() + "/" + group->GetAltProfileName() +
      ".ini";
  } else {
    alt_profile_path = "";
  }

  main_profile_path = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME + "_Backup.ini";

  return { alt_profile_path, main_profile_path };
}

void ChangeControllerProfileAlt(std::string profile_path) {
  Common::IniFile ini;
  ini.Load(profile_path);

  std::string profile_name = ini.GetSection("Wiimote1") ? "Wiimote1" : "Profile";

  Wiimote::GetConfig()->GetController(0)->LoadConfig(ini.GetOrCreateSection(profile_name));
  Wiimote::GetConfig()->GetController(0)->UpdateReferences(g_controller_interface);
}

void UpdateHackSettings() {
  double camera, cursor;
  bool invertx, inverty, scale_sens = false, lock = false, new_controls;

  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    std::tie<double, double, bool, bool, bool>(camera, cursor, invertx, inverty, new_controls) =
      Pad::PrimeSettings(0);
  } else {
    std::tie<double, double, bool, bool, bool, bool>(camera, cursor, invertx, inverty, scale_sens, lock, new_controls) =
      Wiimote::PrimeSettings(0);
  }

  SetSensitivity((float)camera);
  SetCursorSensitivity((float)cursor);
  SetInvertedX(invertx);
  SetInvertedY(inverty);
  SetScaleCursorSensitivity(scale_sens);
  SetReticleLock(lock);
  SetNewMapControls(new_controls);
}

float GetSensitivity() {
  return sensitivity;
}

void SetSensitivity(float sens) {
  sensitivity = sens;
}

bool HandleReticleLockOn() {
  return reticle_lock;
}

bool NewMapControlsEnabled() {
  return new_map_controls;
}

void SetNewMapControls(bool new_controls) {
  new_map_controls = new_controls;
}

void SetReticleLock(bool lock) {
  reticle_lock = lock;
}

float GetCursorSensitivity() {
  return cursor_sensitivity;
}

void SetCursorSensitivity(float sens) {
  cursor_sensitivity = sens;
}

// Restore default FOV based on config
float GetFov(Game game) {
  if (Config::Get(Config::FOV_ENABLE)) {
    return Config::Get(Config::FOV);
  } else {
    switch (game) {
      case Game::PRIME_1:
      case Game::PRIME_1_GCN:
      case Game::PRIME_1_GCN_R1:
      case Game::PRIME_1_GCN_R2:
        return 55.f;
      default:
        return 60.f;
      }
  }
}

bool InvertedY() {
  return inverted_y;
}

void SetInvertedY(bool inverted) {
  inverted_y = inverted;
}

bool InvertedX() {
  return inverted_x;
}

void SetInvertedX(bool inverted) {
  inverted_x = inverted;
}

bool ScaleCursorSensitivity() {
  return scale_cursor_sens;
}

void SetScaleCursorSensitivity(bool scale) {
  scale_cursor_sens = scale;
}

bool CheckPitchRecentre() {
  if (ControllerMode()) {
    if (GetActiveGame() >= Game::PRIME_1_GCN) {
      return Pad::CheckPitchRecentre(0);
    } else {
      return Wiimote::CheckPitchRecentre(0);
    }
  }

  return false;
}

bool ControllerMode() {
  if (GetActiveGame() == Game::INVALID_GAME) {
    return true;
  }

  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    return Pad::PrimeUseController(0);
  } else {
    return Wiimote::PrimeUseController(0);
  }
}

double GetHorizontalAxis() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    if (Pad::PrimeUseController(0)) {
      return std::get<0>(Pad::GetPrimeStickXY(0))
             + (Pad::PrimeUseGyro(0) ? std::get<1>(Pad::GetPrimeGyroPitchYaw(0)) : 0.0);
    }
  } else if (Wiimote::PrimeUseController(0)) {
    return std::get<0>(Wiimote::GetPrimeStickXY(0))
           + (Wiimote::PrimeUseGyro(0) ? std::get<1>(Wiimote::GetPrimeGyroPitchYaw(0)) : 0.0);
  }

  if (!Host_RendererHasFocus()) {
    return 0;
  }

  return static_cast<double>(g_mouse_input->GetDeltaHorizontalAxis());
}

double GetVerticalAxis() {
  if (GetActiveGame() >= Game::PRIME_1_GCN) {
    if (Pad::PrimeUseController(0)) {
      return std::get<1>(Pad::GetPrimeStickXY(0))
             + (Pad::PrimeUseGyro(0) ? std::get<0>(Pad::GetPrimeGyroPitchYaw(0)) : 0.0);
    }
  } else if (Wiimote::PrimeUseController(0)) {
    return std::get<1>(Wiimote::GetPrimeStickXY(0))
           + (Wiimote::PrimeUseGyro(0) ? std::get<0>(Wiimote::GetPrimeGyroPitchYaw(0)) : 0.0);
  }

  if (!Host_RendererHasFocus()) {
    return 0;
  }

  return static_cast<double>(g_mouse_input->GetDeltaVerticalAxis());
}

bool GetCulling() {
  return Config::Get(Config::TOGGLE_CULLING);
}

void SetLockCamera(CameraLock lock) {
  lock_camera = lock;
}

CameraLock GetLockCamera() {
  return lock_camera;
}

std::tuple<bool, bool> GetMenuOptions() {
  return Wiimote::GetBVMenuOptions(0);
}

AddressDB* GetAddressDB() {
  return &addr_db;
}

EmuVariableManager* GetVariableManager() {
  return &var_mgr;
}

void SetMotd(std::string const& motd) {
  auto lock = std::lock_guard<std::mutex>(motd_lock);
  trilogy_motd = motd;
}

std::string GetMotd() {
  auto lock = std::lock_guard<std::mutex>(motd_lock);
  return trilogy_motd;
}

bool UsingRealWiimote() {
  return Wiimote::GetSource(0) == WiimoteSource::Real;
}

}  // namespace prime
