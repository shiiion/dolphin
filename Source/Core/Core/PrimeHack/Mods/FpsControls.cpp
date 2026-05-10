#include "Core/PrimeHack/Mods/FpsControls.h"

#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/Mods/StrafeControlPatches.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/System.h"

#include <cmath>

namespace prime {
namespace {

const std::array<int, 4> prime_one_beams = {0, 2, 1, 3};
const std::array<int, 4> prime_two_beams = {0, 1, 2, 3};

const std::array<std::tuple<int, int>, 4> prime_one_visors = {
  std::make_tuple<int, int>(0, 0x11), std::make_tuple<int, int>(2, 0x05),
  std::make_tuple<int, int>(3, 0x09), std::make_tuple<int, int>(1, 0x0d)};
const std::array<std::tuple<int, int>, 4> prime_two_visors = {
  std::make_tuple<int, int>(0, 0x08), std::make_tuple<int, int>(2, 0x09),
  std::make_tuple<int, int>(3, 0x0a), std::make_tuple<int, int>(1, 0x0b)};
const std::array<std::tuple<int, int>, 4> prime_three_visors = {
  std::make_tuple<int, int>(0, 0x0b), std::make_tuple<int, int>(1, 0x0c),
  std::make_tuple<int, int>(2, 0x0d), std::make_tuple<int, int>(3, 0x0e)};

constexpr u32 ORBIT_STATE_GRAPPLE = 5;

void null_players_on_destruct_mp2_gc(PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu, u32) {
  // r27 is an iterator variable pointing to start of statemgr
  mmu.Write<u32>(0, ppc_state.gpr[27] + 0x14fc);

  // Original instruction: addi r27, r27, 4
  ppc_state.gpr[27] += 4;
}

void wiimote_shake_override(PowerPC::PowerPCState& ppc_state, PowerPC::MMU&, u32) {
  ppc_state.gpr[26] = CheckJump() ? 1 : 0;
}

} // namespace

bool FpsControls::in_ridley_fight(Region active_region) {
  constexpr u64 kNorionWorldId = 0x6fb8ef2a9523c343;
  constexpr u32 kRidleyFightArea = 0x16;
  LOOKUP_DYN(world_id);
  LOOKUP_DYN(area_id);
  if (read64(world_id) != kNorionWorldId) {
    return false;
  }

  return read32(area_id) == kRidleyFightArea;
}

void FpsControls::run_mod(Game game, Region region) {
  switch (game) {
    case Game::MENU:
      run_mod_menu(game, region);
      break;
    case Game::PRIME_1:
      run_mod_mp1(region);
      break;
    case Game::PRIME_2:
      run_mod_mp2(region);
      break;
    case Game::PRIME_3:
    case Game::PRIME_3_STANDALONE:
      run_mod_mp3(game, region);
      break;
    case Game::PRIME_1_GCN:
    case Game::PRIME_1_GCN_R1:
    case Game::PRIME_1_GCN_R2:
      run_mod_mp1_gc(region);
      break;
    case Game::PRIME_2_GCN:
      run_mod_mp2_gc(region);
      break;
    default:
      break;
  }
}

bool FpsControls::input_disabled() const {
  LOOKUP_DYN(menu_state);
  return read32(menu_state) != 0;
}

void FpsControls::calculate_pitchyaw_delta() {
  if (input_disabled()) {
    return;
  }

  constexpr auto yaw_clamp = [](float t) -> float {
    constexpr float PI = 3.141592654f;
    constexpr float TWO_PI = PI * 2.f;
    return (t > PI) ? (t - TWO_PI) : ((t < -PI) ? (t + TWO_PI) : (t));
  };

  const float compensated_sens = GetSensitivity() * kTurnrateRatio / 60.f;

  if (CheckPitchRecentre()) {
    calculate_pitch_to_target(0.f);
    return;
  }
  interpolating = false;

  pitch += static_cast<float>(GetVerticalAxis()) * compensated_sens *
    (InvertedY() ? 1.f : -1.f);
  pitch = std::clamp(pitch, -1.52f, 1.52f);

  yaw += static_cast<float>(GetHorizontalAxis()) * compensated_sens *
    (InvertedX() ? 1.f : -1.f);
  yaw = yaw_clamp(yaw);
}

void FpsControls::update_pitchyaw_locked() {
  // Calculate the pitch based on the XF matrix to allow us to write out the pitch
  // even while locked onto a target, the pitch will be written to match the lock
  // angle throughout the entire lock-on. The very first frame when the lock is
  // released (and before this mod has run again) the game will still render the
  // correct angle. If we stop writing the angle during lock-on then a 1-frame snap
  // occurs immediately after releasing the lock, due to the mod running after the
  // frame has already been rendered.
  LOOKUP_DYN(object_list);
  LOOKUP_DYN(camera_manager);
  LOOKUP(xf_offset);
  const u16 camera_uid = read16(camera_manager);
  if (camera_uid == 0xffff) {
    return;
  }
  const u32 camera = read32(object_list + ((camera_uid & 0x3ff) << 3) + 4);

  Transform camera_xf;
  camera_xf.read_from(*active_guard, camera + xf_offset);
  const vec3 fwd = camera_xf.fwd();
  yaw = atan2f(fwd.y, fwd.x);
  pitch = asin(fwd.z);
  pitch = std::clamp(pitch, -1.52f, 1.52f);
}

void FpsControls::calculate_pitch_to_target(float target_pitch) {
  // Smoothly transitions pitch to target through interpolation

  const float margin = 0.05f;
  if (pitch >= (target_pitch - margin) && pitch <= (target_pitch + margin)) {
    pitch = target_pitch;
    interpolating = false;

    return;
  }

  if (!interpolating) {
    delta = 0;
    start_pitch = pitch;
    interpolating = true;
  }

  pitch = Lerp(start_pitch, target_pitch, delta / 15.f);
  pitch = std::clamp(pitch, -1.52f, 1.52f);

  delta++;

  return;
}

float FpsControls::calculate_yaw_vel() {
  return GetHorizontalAxis() * GetSensitivity() * (InvertedX() ? 1.f : -1.f);;
}

void FpsControls::handle_beam_visor_switch(std::array<int, 4> const &beams,
                                           std::array<std::tuple<int, int>, 4> const &visors) {
  // Global array of all powerups (measured in "ammunition"
  // even for things like visors/beams)
  LOOKUP_DYN(powerups_array);
  LOOKUP(powerups_size);
  LOOKUP(powerups_offset);

  // We copy out the ownership status of beams and visors to our own array for
  // get_beam_switch and get_visor_switch
  for (int i = 0; i < 4; i++) {
    const bool visor_owned =
      read32(powerups_array + std::get<1>(visors[i]) *
        powerups_size + powerups_offset) ? true : false;
    set_visor_owned(i, visor_owned);
    if (has_beams) {
      const bool beam_owned =
        read32(powerups_array + beams[i] *
          powerups_size + powerups_offset) ? true : false;
      set_beam_owned(i, beam_owned);
    }
  }

  if (has_beams && GetVariableManager()->get_uint(*active_guard, "switch_ready")) {
    const int beam_id = get_beam_switch(beams);
    if (beam_id != -1) {
      prime::GetVariableManager()->set_variable(*active_guard, "new_beam", static_cast<u32>(beam_id));
      prime::GetVariableManager()->set_variable(*active_guard, "beamchange_flag", u32{1});
    }
  }

  LOOKUP_DYN(active_visor);
  int visor_id, visor_off;
  std::tie(visor_id, visor_off) = get_visor_switch(visors, read32(active_visor) == 0);

  if (visor_id != -1) {
    if (read32(powerups_array + (visor_off * powerups_size) + powerups_offset)) {
      write32(visor_id, active_visor);

      // Trigger holster animation.
      // Prime 3 already animates holstering. Gamecube does not need to use the visor controls.
      if (visor_id == 2) {
        auto active_game = GetActiveGame();
        if (active_game == Game::PRIME_1 || active_game == Game::PRIME_2) {
          LOOKUP_DYN(gun_holster_state);
          LOOKUP(holster_timer_offset);

          write32(3, gun_holster_state);
          writef32(0.2f, gun_holster_state + holster_timer_offset); // Holster timer
        }
      }
    }
  }
}

void FpsControls::run_mod_menu(Game game, Region region) {
  if (region == Region::NTSC_U) {
    u32 p0 = read32(Core::System::GetInstance().GetPPCState().gpr[13] - 0x2870);
    if (!mem_check(p0)) {
      return;
    }
    p0 = read32(p0 + 0xc54) + 0x9c;
    if (!mem_check(p0)) {
      return;
    }

    handle_cursor(*active_guard, p0, p0 + 0xc0, region);
  } else if (region == Region::PAL) {
    u32 cursor_address = read32(0x80621ffc);
    handle_cursor(*active_guard, cursor_address + 0xdc, cursor_address + 0x19c, region);
  }
}

void FpsControls::run_mod_mp1(Region region) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  handle_beam_visor_switch(prime_one_beams, prime_one_visors);
  CheckBeamVisorSetting(Game::PRIME_1);

  // Is beam/visor menu showing on screen
  LOOKUP_DYN(beamvisor_menu_state);
  bool beamvisor_menu_enabled = read32(beamvisor_menu_state) == 1;

  LOOKUP_DYN(orbit_state);
  LOOKUP_DYN(lockon_state);

  // Allows freelook in grapple, otherwise we are orbiting (locked on) to something
  const bool locked = (read32(orbit_state) != ORBIT_STATE_GRAPPLE &&
    read8(lockon_state)) || beamvisor_menu_enabled;

  LOOKUP(xf_offset);
  Transform cplayer_xf;
  cplayer_xf.read_from(*active_guard, player + xf_offset);

  LOOKUP_DYN(cursor);
  LOOKUP_DYN(firstperson_pitch);
  if (locked) {
    update_pitchyaw_locked();
    writef32(pitch, firstperson_pitch);

    if (beamvisor_menu_enabled) {
      LOOKUP_DYN(beamvisor_menu_mode);
      // if the menu id is not null
      if (read32(beamvisor_menu_mode) != 0xffffffff) {
        if (menu_open == false) {
          set_code_group_state("beam_change", ModState::DISABLED);
        }

        handle_reticle(*active_guard, cursor + 0x9c, cursor + 0x15c, region, GetFov(Game::PRIME_1));
        menu_open = true;
      }
    } else if (HandleReticleLockOn()) {  // If we handle menus, this doesn't need to be ran
      handle_reticle(*active_guard, cursor + 0x9c, cursor + 0x15c, region, GetFov(Game::PRIME_1));
    }

    return;
  }

  if (menu_open) {
    set_code_group_state("beam_change", ModState::ENABLED);
    menu_open = false;
  }

  set_cursor_pos(0, 0);
  write32(0, cursor + 0x9c);
  write32(0, cursor + 0x15c);

  LOOKUP_DYN(ball_state);
  LOOKUP_DYN(menu_state);
  swap_alt_profiles(read32(ball_state), read32(menu_state), 0);

  LOOKUP_DYN(camera_state);
  if (read32(camera_state) != 0) {
    vec3 fwd = cplayer_xf.fwd();
    yaw = atan2f(fwd.y, fwd.x);
    // Pitch is always 0 after returning from morph
    pitch = 0;
    return;
  }

  calculate_pitchyaw_delta();
  writef32(pitch, firstperson_pitch);
  cplayer_xf.build_rotation(yaw);
  cplayer_xf.write_to(*active_guard, player + xf_offset);

  LOOKUP(tweak_player);
  // Max pitch angle, as abs val (any higher = gimbal lock)
  writef32(1.52f, tweak_player + 0x134);
}

void FpsControls::run_mod_mp1_gc(Region region) {
  const bool show_crosshair = GetShowGCCrosshair();
  const u32 crosshair_color_rgba = show_crosshair ? GetGCCrosshairColor() : 0x4b7ea331;
  set_code_group_state("show_crosshair", show_crosshair ? ModState::ENABLED : ModState::DISABLED);
  LOOKUP(crosshair_color);
  if (show_crosshair) {
    write32(crosshair_color_rgba, crosshair_color);
  }

  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  LOOKUP(xf_offset);
  Transform cplayer_xf;
  cplayer_xf.read_from(*active_guard, player + xf_offset);

  LOOKUP_DYN(firstperson_pitch);
  LOOKUP_DYN(orbit_state);
  const u32 orbit_state_val = read32(orbit_state);
  if (orbit_state_val != ORBIT_STATE_GRAPPLE && orbit_state_val != 0) {
    update_pitchyaw_locked();
    writef32(pitch, firstperson_pitch);
    return;
  }

  LOOKUP_DYN(camera_state);
  if (read32(camera_state) != 0) {
    vec3 fwd = cplayer_xf.fwd();
    yaw = atan2f(fwd.y, fwd.x);
    // Pitch is always 0 after returning from morph
    pitch = 0;
    return;
  }

  calculate_pitchyaw_delta();
  writef32(pitch, firstperson_pitch);
  cplayer_xf.build_rotation(yaw);
  cplayer_xf.write_to(*active_guard, player + xf_offset);

  // Tweak patches, but these don't impact normal gameplay so correcting them should be unnecessary
  LOOKUP(tweak_player);
  writef32(1.52f, tweak_player + 0x134);
  writef32(1000.f, tweak_player + 0x280);
  writef32(1000.f, tweak_player + 0x2b0);
}

void FpsControls::run_mod_mp2(Region region) {
  CheckBeamVisorSetting(Game::PRIME_2);

  // VERY similar to mp1, this time CPlayer isn't TOneStatic (presumably because
  // of multiplayer mode in the GCN version?)
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  LOOKUP_DYN(load_state);
  if (read32(load_state) != 1) {
    return;
  }

  handle_beam_visor_switch(prime_two_beams, prime_two_visors);

  // Is beam/visor menu showing on screen
  LOOKUP_DYN(beamvisor_menu_state);
  bool beamvisor_menu = read32(beamvisor_menu_state) == 1;

  // Allows freelook in grapple, otherwise we are orbiting (locked on) to something
  LOOKUP_DYN(orbit_state);
  LOOKUP_DYN(lockon_state);
  const bool locked = (read32(orbit_state) != ORBIT_STATE_GRAPPLE &&
    read8(lockon_state)) || beamvisor_menu;

  LOOKUP(xf_offset);
  Transform cplayer_xf;
  cplayer_xf.read_from(*active_guard, player + xf_offset);

  LOOKUP_DYN(firstperson_pitch);
  LOOKUP_DYN(cursor);
  if (locked) {
    update_pitchyaw_locked();
    writef32(pitch, firstperson_pitch);

    if (beamvisor_menu) {
      LOOKUP_DYN(beamvisor_menu_mode);

      // if the menu id is not null
      if (read32(beamvisor_menu_mode) != 0xffffffff) {
        if (menu_open == false) {
          set_code_group_state("beam_change", ModState::DISABLED);
        }

        handle_reticle(*active_guard, cursor + 0x9c, cursor + 0x15c, region, GetFov(Game::PRIME_2));
        menu_open = true;
      }
    } else if (HandleReticleLockOn()) {
      handle_reticle(*active_guard, cursor + 0x9c, cursor + 0x15c, region, GetFov(Game::PRIME_2));
    }
    return;
  }

  if (menu_open) {
    set_code_group_state("beam_change", ModState::ENABLED);
    menu_open = false;
  }

  set_cursor_pos(0, 0);
  write32(0, cursor + 0x9c);
  write32(0, cursor + 0x15c);

  LOOKUP_DYN(ball_state);
  LOOKUP_DYN(menu_state);
  LOOKUP_DYN(screw_state);
  swap_alt_profiles(read32(ball_state), read32(menu_state), read32(screw_state));

  LOOKUP_DYN(camera_state);
  if (read32(camera_state) != 0) {
    const vec3 fwd = cplayer_xf.fwd();
    yaw = atan2f(fwd.y, fwd.x);
    // Pitch is always 0 after returning from morph
    pitch = 0;
    return;
  }

  calculate_pitchyaw_delta();
  writef32(pitch, firstperson_pitch);
  cplayer_xf.build_rotation(yaw);
  cplayer_xf.write_to(*active_guard, player + xf_offset);

  LOOKUP(tweak_player_offset);
  const u32 tweak_player_address =
    read32(read32(Core::System::GetInstance().GetPPCState().gpr[13] + tweak_player_offset));
  if (mem_check(tweak_player_address)) {
    // This one's stored as degrees instead of radians
    writef32(87.0896f, tweak_player_address + 0x180);
  }
}

void FpsControls::run_mod_mp2_gc(Region region) {
  LOOKUP_DYN(world);
  if (world == 0) {
    return;
  }
  // World loading phase == 4 -> complete
  if (read32(world + 0x4) != 4) {
    return;
  }

  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  const bool show_crosshair = GetShowGCCrosshair();
  const u32 crosshair_color_rgba = show_crosshair ? GetGCCrosshairColor() : 0x4b7ea331;
  set_code_group_state("show_crosshair", show_crosshair ? ModState::ENABLED : ModState::DISABLED);
  LOOKUP(tweakgui_offset);
  u32 crosshair_color_addr = read32(read32(Core::System::GetInstance().GetPPCState().gpr[13] + tweakgui_offset)) + 0x268;
  if (show_crosshair) {
    write32(crosshair_color_rgba, crosshair_color_addr);
  }

  LOOKUP(xf_offset);
  Transform cplayer_xf;
  cplayer_xf.read_from(*active_guard, player + xf_offset);

  LOOKUP_DYN(orbit_state);
  LOOKUP_DYN(firstperson_pitch);
  if (read32(orbit_state) != ORBIT_STATE_GRAPPLE && read32(orbit_state) != 0) {
    update_pitchyaw_locked();
    writef32(pitch, firstperson_pitch);
    return;
  }

  LOOKUP_DYN(camera_state);
  if (read32(camera_state) != 0) {
    vec3 fwd = cplayer_xf.fwd();
    yaw = atan2f(fwd.y, fwd.x);
    // Pitch is always 0 after returning from morph
    pitch = 0;
    // Prime 2 has a flicker for one frame after unmorphing
    writef32(pitch, firstperson_pitch);
    return;
  }

  calculate_pitchyaw_delta();
  writef32(pitch, firstperson_pitch);
  cplayer_xf.build_rotation(yaw);
  cplayer_xf.write_to(*active_guard, player + xf_offset);

  LOOKUP(tweak_player_offset);
  const u32 tweak_player_address = read32(read32(Core::System::GetInstance().GetPPCState().gpr[13] + tweak_player_offset));
  if (mem_check(tweak_player_address)) {
    // Freelook rotation speed tweak
    writef32(1000.f, tweak_player_address + 0x188);
    // Freelook pitch half-angle range tweak
    writef32(87.0896f, tweak_player_address + 0x184);
  }
}

void FpsControls::mp3_handle_lasso(u32 grapple_state_addr) {
  if (GrappleCtlBound()) {
    set_code_group_state("grapple_lasso", ModState::ENABLED);

    // Disable animation code changes if trying to use grapple voltage.
    set_code_group_state("grapple_lasso_animation",
      CheckForward() || CheckBack() ? ModState::DISABLED : ModState::ENABLED);
  } else {
    set_code_group_state("grapple_lasso", ModState::DISABLED);
    set_code_group_state("grapple_lasso_animation", ModState::DISABLED);
  }

  // This is outside of the grapple detection so that people can quickly tap the bind to pull rather than wait.
  bool holding_grapple = prime::CheckGrappleCtl();

  // If currently locked onto a grapple point. This must be seperate from lock-on for grapple swing.
  // 1 => Locked On
  // 2 => Grapple Lasso(/Voltage)
  // 3 => Grapple Swing
  if (read32(grapple_state_addr) == 2) {
    if (holding_grapple && !grapple_button_state) {
      grapple_button_state = true;

      grapple_swap_axis = !grapple_swap_axis;

      // 0.45 for repeated taps. 1 will instantly complete the grapple.
      if (GrappleTappingMode())
        grapple_force += 0.40f;
    }
    else if (!holding_grapple) {
      grapple_button_state = false;

      if (!GrappleTappingMode())
        grapple_force += 1.f;
    }

    constexpr float force_delta = 0.045f;

    // Use tapping/force method
    if (grapple_force > 0) {
      grapple_hand_pos += force_delta;
      grapple_force -= force_delta;
    } else {
      grapple_hand_pos -= 0.045f;
      grapple_force = 0;
    }

    grapple_hand_pos = std::clamp(grapple_hand_pos, 0.f, 1.0f);

    prime::GetVariableManager()->set_variable(*active_guard, "grapple_hand_x", grapple_hand_pos);
    prime::GetVariableManager()->set_variable(*active_guard, "grapple_hand_y", grapple_hand_pos / 4);

    if (grapple_hand_pos >= 1.0f) {
      // State 4 completes the grapple (e.g pull door from frame)
      prime::GetVariableManager()->set_variable(*active_guard, "grapple_lasso_state", (u32) 4);
    } else {
      // State 2 "holds" the grapple for lasso/voltage.
      prime::GetVariableManager()->set_variable(*active_guard, "grapple_lasso_state", (u32) 2);
    }
  } else {
    prime::GetVariableManager()->set_variable(*active_guard, "grapple_hand_x", 0.f);
    prime::GetVariableManager()->set_variable(*active_guard, "grapple_hand_y", 0.f);
    prime::GetVariableManager()->set_variable(*active_guard, "grapple_lasso_state", (u32) 0);

    grapple_hand_pos = 0;
    grapple_force = 0;
    grapple_button_state = false;
  }
}

// this game is
// fucking annoying
void FpsControls::run_mod_mp3(Game active_game, Region active_region) {
  CheckBeamVisorSetting(active_game);

  if (GrappleCtlBound()) {
    set_code_group_state("grapple_lasso", ModState::ENABLED);

    // Disable animation code changes if trying to use grapple voltage.
    set_code_group_state("grapple_lasso_animation",
      CheckForward() || CheckBack() ? ModState::DISABLED : ModState::ENABLED);
  } else {
    set_code_group_state("grapple_lasso", ModState::DISABLED);
    set_code_group_state("grapple_lasso_animation", ModState::DISABLED);
  }

  LOOKUP_DYN(cursor);
  const auto mp3_handle_cursor = [this, cursor, active_region] (bool locked, bool for_reticle) {
    if (locked) {
      write32(0, cursor + 0x9c);
      write32(0, cursor + 0x15c);
    } else if (for_reticle) {
      handle_reticle(*active_guard, cursor + 0x9c, cursor + 0x15c, active_region, GetFov(Game::PRIME_3));
    } else {
      handle_cursor(*active_guard, cursor + 0x9c, cursor + 0x15c, active_region);
    }

  };

  LOOKUP_DYN(ball_state);
  LOOKUP_DYN(menu_state);
  LOOKUP_DYN(screw_state);

  swap_alt_profiles(read32(ball_state), read32(menu_state), read32(screw_state));

  // Handles menu screen cursor
  LOOKUP(cursor_dlg_enabled);
  if (read8(cursor_dlg_enabled)) {
    mp3_handle_cursor(false, false);
    return;
  }

  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  handle_beam_visor_switch({}, prime_three_visors);

  if (in_ridley_fight(active_region)) {
    if (!was_in_ridley_fight) {
      was_in_ridley_fight = true;
      disable_patches();
    }
    mp3_handle_cursor(false, true);
    return;
  } else if (was_in_ridley_fight) {
    enable_patches();
    was_in_ridley_fight = false;
  }

  prime::GetVariableManager()->set_variable(*active_guard, "trigger_grapple",
                                            prime::CheckGrappleCtl() ? u32{1} : u32{0});

  LOOKUP(xf_offset);
  Transform cplayer_xf;
  cplayer_xf.read_from(*active_guard, player + xf_offset);

  LOOKUP_DYN(firstperson_pitch);
  LOOKUP_DYN(beamvisor_menu_state);
  LOOKUP_DYN(lockon_type);
  LOOKUP(lockon_state);
  const bool beamvisor_menu = read32(beamvisor_menu_state) == 3;
  if ((read32(lockon_type) == 0 && read8(lockon_state)) ||
      read32(lockon_type) == 1 || beamvisor_menu) {
    update_pitchyaw_locked();

    if (HandleReticleLockOn() || beamvisor_menu) {
      mp3_handle_cursor(false, true);
    }

    writef32(pitch, firstperson_pitch);
    return;
  }

  // Handle grapple lasso bind
  mp3_handle_lasso(lockon_type);

  // Lock Camera according to ContextSensitiveControls and interpolate to pitch 0
  if (prime::GetLockCamera() != Unlocked) {
    const float target_pitch = Centre == prime::GetLockCamera() ? 0.f : 0.23f;

    if (pitch == target_pitch) {
      writef32(target_pitch, pitch);
      mp3_handle_cursor(false, false);

      return;
    }

    calculate_pitch_to_target(target_pitch);
    writef32(pitch, firstperson_pitch);

    // Known edge-case, if a user were to exit a camera lock without reaching
    // the target (e.g loadstate), the next time they enter a camera lock,
    // it will attempt to centre to the last target pitch.
    // We cannot reset here due to `Reset Camera Pitch`

    return;
  }

  mp3_handle_cursor(true, true);
  set_cursor_pos(0, 0);

  const vec3 fwd = cplayer_xf.fwd();
  yaw = atan2f(fwd.y, fwd.x);

  if (read32(ball_state) != 0) {
    // Pitch is always 0 after returning from morph
    pitch = 0;
    return;
  }

  LOOKUP_DYN(control_state);
  if (read32(control_state) != 1) {
    return;
  }

  calculate_pitchyaw_delta();
  writef32(pitch, firstperson_pitch);
  cplayer_xf.build_rotation(yaw);
  cplayer_xf.write_to(*active_guard, player + xf_offset);
  LOOKUP_DYN(angular_moment_z);
  // Just so that the game doesn't kill itself when turning off fpscontrols
  writef32(1e-7f, angular_moment_z);
}

void FpsControls::CheckBeamVisorSetting(Game game)
{
  bool beam,visor;
  std::tie<bool, bool>(beam, visor) = GetMenuOptions();

  switch (game) {
    case Game::PRIME_1:
    case Game::PRIME_2:
      set_code_group_state("beam_menu", beam ? ModState::DISABLED : ModState::ENABLED);
      [[fallthrough]];
    case Game::PRIME_3:
    case Game::PRIME_3_STANDALONE:
      set_code_group_state("visor_menu", visor ? ModState::DISABLED : ModState::ENABLED);
      break;
    default:
      break;
  }
}

bool FpsControls::init_mod(Game game, Region region) {
  swap_alt_profiles(0, 0, 0);
  pitch = 0;
  yaw = 0;

  switch (game) {
    case Game::PRIME_1:
      init_mod_mp1(region);
      break;
    case Game::PRIME_1_GCN:
      init_mod_mp1_gc(region);
      break;
    case Game::PRIME_1_GCN_R1:
      init_mod_mp1_gc_r1();
      break;
    case Game::PRIME_1_GCN_R2:
      init_mod_mp1_gc_r2();
      break;
    case Game::PRIME_2:
      init_mod_mp2(region);
      break;
    case Game::PRIME_2_GCN:
      init_mod_mp2_gc(region);
      break;
    case Game::PRIME_3:
      init_mod_mp3(game, region);
      break;
    case Game::PRIME_3_STANDALONE:
      init_mod_mp3_standalone(game, region);
      break;
    default:
      break;
  }
  return true;
}

void FpsControls::add_beam_change_code_mp1(u32 start_point) {
  std::string_view patch_str = R"(
.defvar Start, 0x{start:x}
.defvar BeamchangeFlag, 0x{beamchange_flag:x}
.defvar NewBeam, 0x{new_beam:x}
.defvar SwitchReady, 0x{switch_ready:x}
.locate Start
lis r6, SwitchReady@ha
ori r6, r6, SwitchReady@l
li r3, 1
stw r3, 0(r6)
lis r4, BeamchangeFlag@ha
ori r4, r4, BeamchangeFlag@l
lwz r3, 0(r4)
cmpwi r3, 0
beq _after
lis r5, NewBeam@ha
ori r5, r5, NewBeam@l
lwz r26, 0(r5)
mr r25, r26
li r3, 0
stw r3, 0(r4)
stw r3, 0(r6)
b _after

.locate Start + 0x68
_after:
)";

  u32 beamchange_flag = prime::GetVariableManager()->get_address("beamchange_flag");
  u32 new_beam = prime::GetVariableManager()->get_address("new_beam");
  u32 switch_ready = prime::GetVariableManager()->get_address("switch_ready");
  add_asm_patch(fmt::format(fmt::runtime(patch_str), fmt::arg("start", start_point), fmt::arg("beamchange_flag", beamchange_flag),
                            fmt::arg("new_beam", new_beam), fmt::arg("switch_ready", switch_ready)),
                "beam_change");
}

void FpsControls::add_beam_change_code_mp2(u32 start_point) {
  std::string_view patch_str = R"(
.defvar Start, 0x{start:x}
.defvar BeamchangeFlag, 0x{beamchange_flag:x}
.defvar NewBeam, 0x{new_beam:x}
.defvar SwitchReady, 0x{switch_ready:x}
.locate Start
lis r6, SwitchReady@ha
ori r6, r6, SwitchReady@l
li r3, 1
stw r3, 0(r6)
lis r4, BeamchangeFlag@ha
ori r4, r4, BeamchangeFlag@l
lwz r3, 0(r4)
cmpwi r3, 0
beq _after
lis r5, NewBeam@ha
ori r5, r5, NewBeam@l
lwz r31, 0(r5)
mr r30, r31
li r3, 0
stw r3, 0(r4)
stw r3, 0(r6)
b _after

.locate Start + 0x6c
_after:
)";

  u32 beamchange_flag = prime::GetVariableManager()->get_address("beamchange_flag");
  u32 new_beam = prime::GetVariableManager()->get_address("new_beam");
  u32 switch_ready = prime::GetVariableManager()->get_address("switch_ready");
  add_asm_patch(fmt::format(fmt::runtime(patch_str), fmt::arg("start", start_point), fmt::arg("beamchange_flag", beamchange_flag),
                            fmt::arg("new_beam", new_beam), fmt::arg("switch_ready", switch_ready)),
                "beam_change");
}

void FpsControls::add_grapple_slide_code_mp3(u32 start_point) {
  std::string_view patch_str = R"(
.locate 0x{start:x}
nop
nop
nop
lfs f0, 0x240(sp)  # X component of new origin
stfs f0, 0x48(r31) # Set player transform X origin
lfs f0, 0x244(sp)  # Y component of new origin
stfs f0, 0x58(r31) # Set player transform Y origin
lfs f0, 0x248(sp)  # Z component of new origin
stfs f0, 0x68(r31) # Set player transform Z origin
addi r4, r31, 0x3c # SetTransform is called on player, have the transform be set to itself)";
  add_asm_patch(fmt::format(fmt::runtime(patch_str), fmt::arg("start", start_point)));
}

void FpsControls::add_grapple_lasso_code_mp3(u32 func1, u32 func2, u32 func3) {
  u32 lis_x, ori_x, lis_y, ori_y;

  std::tie<u32, u32>(lis_x, ori_x) = prime::GetVariableManager()->make_lis_ori(11, "grapple_hand_x");
  std::tie<u32, u32>(lis_y, ori_y) = prime::GetVariableManager()->make_lis_ori(11, "grapple_hand_y");

  // Controls how tight the lasso is and if to turn yellow.
  add_code_change(func1, lis_x, "grapple_lasso");
  add_code_change(func1 + 0x4, ori_x, "grapple_lasso");
  add_code_change(func1 + 0xC, 0xC04B0000, "grapple_lasso");  // lfs f2, 0(r11)

                                                              // Skips the game's checks and lets us control grapple lasso ourselves.
  add_code_change(func1 + 0x8, 0x40800158, "grapple_lasso"); // first conditional branch changed to jmp to end
  add_code_change(func1 + 0x18, 0x40810148, "grapple_lasso"); // second conditional branch changed to jmp to end
  add_code_change(func1 + 0x54, 0x4800010C, "grapple_lasso"); // end of 'yellow' segment jmp to end

                                                              // Controls the pulling animation.
  add_code_change(func2, lis_x, "grapple_lasso_animation");
  add_code_change(func2 + 0x4, ori_x, "grapple_lasso_animation");
  add_code_change(func2 + 0x8, 0xc00b0000, "grapple_lasso_animation");  // lfs f0, 0(r11)
  add_code_change(func2 + 0xc, lis_y, "grapple_lasso_animation");
  add_code_change(func2 + 0x10, ori_y, "grapple_lasso_animation");
  add_code_change(func2 + 0x14, 0xc04b0000, "grapple_lasso_animation");  // lfs f2, 0(r11)
  add_code_change(func2 + 0x18, 0xd05701c8, "grapple_lasso_animation");  // stfs f2, 0x1C8(r23)

  u32 lis, ori;
  // Controls the return value of the "ProcessGrappleLasso" function.
  std::tie<u32, u32>(lis, ori) = prime::GetVariableManager()->make_lis_ori(30, "grapple_lasso_state");
  add_code_change(func1 + 0x160, lis, "grapple_lasso");
  add_code_change(func1 + 0x164, ori, "grapple_lasso");
  add_code_change(func1 + 0x168, 0x83de0000, "grapple_lasso"); // lwz r30, 0(r30)

                                                               // Triggers grapple.
  std::tie<u32, u32>(lis, ori) = prime::GetVariableManager()->make_lis_ori(3, "trigger_grapple");
  add_code_change(func3 + 0x0, lis, "grapple_lasso");
  add_code_change(func3 + 0x4, ori, "grapple_lasso");
  add_code_change(func3 + 0x8, 0x80630000, "grapple_lasso"); // lwz r3, 0(r3)
  add_code_change(func3 + 0xc, 0x4e800020, "grapple_lasso"); // blr
}

void FpsControls::add_control_state_hook_mp3(u32 start_point, Game game, Region region) {
  if (region == Region::NTSC_U) {
    if (game == Game::PRIME_3) {
      add_code_change(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c
      add_code_change(start_point + 0x04, 0x38636c40);  // addi r3, r3, 0x6c40
    } else {
      add_code_change(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c
      add_code_change(start_point + 0x04, 0x38634f6c);  // addi r3, r3, 0x4f6c
    }
  } else if (region == Region::PAL) {
    if (game == Game::PRIME_3) {
      add_code_change(start_point + 0x00, 0x3c60805d);  // lis  r3, 0x805d
      add_code_change(start_point + 0x04, 0x3863a0c0);  // subi r3, r3, 0x5f40
    } else {
      add_code_change(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c
      add_code_change(start_point + 0x04, 0x38637570);  // addi r3, r3, 0x7570
    }
  }
  add_code_change(start_point + 0x08, 0x8063002c);  // lwz  r3, 0x2c(r3)
  if (game == Game::PRIME_3_STANDALONE && region == Region::NTSC_U) {
    add_code_change(start_point + 0x0c, 0x60000000);  // nop
  } else {
    add_code_change(start_point + 0x0c, 0x80630004);  // lwz  r3, 0x04(r3)
  }
  add_code_change(start_point + 0x10, 0x80632184);  // lwz  r3, 0x2184(r3)
  add_code_change(start_point + 0x14, 0x7c03f800);  // cmpw r3, r31
  add_code_change(start_point + 0x18, 0x4d820020);  // beqlr
  add_code_change(start_point + 0x1c, 0x7fe3fb78);  // mr   r3, r31
  add_code_change(start_point + 0x20, 0x90c30078);  // stw  r6, 0x78(r3)
  add_code_change(start_point + 0x24, 0x4e800020);  // blr
}

void FpsControls::init_mod_mp1(Region region) {
  prime::GetVariableManager()->register_variable("new_beam");
  prime::GetVariableManager()->register_variable("beamchange_flag");
  prime::GetVariableManager()->register_variable("switch_ready");
  if (region == Region::NTSC_U) {
    // This instruction change is used in all 3 games, all 3 regions. It's an update to what I believe
    // to be interpolation for player camera pitch The change is from fmuls f0, f0, f1 (0xec000072) to
    // fmuls f0, f1, f1 (0xec010072), where f0 is the output. The higher f0 is, the faster the pitch
    // *can* change in-game, and squaring f1 seems to do the job.
    add_code_change(0x80098ee4, 0xec010072);

    // NOPs for changing First Person Camera's pitch based on floor (normal? angle?)
    add_code_change(0x80099138, 0x60000000);
    add_code_change(0x80183a8c, 0x60000000);
    add_code_change(0x80183a64, 0x60000000);
    add_code_change(0x8017661c, 0x60000000);
    // Cursor location, sets to f17 (always 0 due to little use)
    add_code_change(0x802fb5b4, 0xd23f009c);
    add_code_change(0x8019fbcc, 0x60000000);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x8018b8d4, 0x48000354);

    add_code_change(0x80075f24, 0x60000000, "beam_menu");
    add_code_change(0x80075f0c, 0x60000000, "visor_menu");

    add_beam_change_code_mp1(0x8018e544);

    // Steps over bounds checking on the reticle
    add_code_change(0x80015164, 0x4800010c);
  } else if (region == Region::PAL) {
    // Same as NTSC but slightly offset
    add_code_change(0x80099068, 0xec010072);
    add_code_change(0x800992c4, 0x60000000);
    add_code_change(0x80183cfc, 0x60000000);
    add_code_change(0x80183d24, 0x60000000);
    add_code_change(0x801768b4, 0x60000000);
    add_code_change(0x802fb84c, 0xd23f009c);
    add_code_change(0x8019fe64, 0x60000000);
    add_code_change(0x80015894, 0x48000108);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x8018bb6c, 0x48000354);

    add_code_change(0x80075f74, 0x60000000, "beam_menu");
    add_code_change(0x80075f8c, 0x60000000, "visor_menu");

    add_beam_change_code_mp1(0x8018e7dc);

    // Steps over bounds checking on the reticle
    add_code_change(0x80015164, 0x4800010c);
  }
  has_beams = true;
}

void FpsControls::init_mod_mp1_gc(Region region) {
  if (region == Region::NTSC_U) {
    //add_code_change(0x8000f63c, 0x48000048);
    add_code_change(0x800ea15c, 0x38810044); // output cannon bob only for viewbob
    add_code_change(0x8000e538, 0x60000000);
    //add_code_change(0x80016ee4, 0x4e800020);
    add_code_change(0x80014820, 0x4e800020);
    add_code_change(0x8000e73c, 0x60000000);
    add_code_change(0x8000f810, 0x48000244);
    // When attached to a grapple point and spinning around it
    // the player's yaw is adjusted, this ensures only position is updated
    // Grapple point yaw fix
    add_code_change(0x8017a18c, 0x7fa3eb78);
    add_code_change(0x8017a190, 0x3881006c);
    add_code_change(0x8017a194, 0x4bed8cf9);

    // Show crosshair but don't consider pressing R button
    add_code_change(0x80016ee4, 0x3b000001, "show_crosshair"); // li r24, 1
    add_code_change(0x80016ee8, 0x8afd09c4, "show_crosshair"); // lbz r23, 0x9c4(r29)
    add_code_change(0x80016eec, 0x53173672, "show_crosshair"); // rlwimi r23, r24, 6, 25, 25 (00000001)
    add_code_change(0x80016ef0, 0x9afd09c4, "show_crosshair"); // stb r23, 0x9c4(r29)
    add_code_change(0x80016ef4, 0x4e800020, "show_crosshair"); // blr

    add_asm_patch(build_strafe_code_100(GuestAllocAligned(kPlanarMoveCodeBufSize, 2)));
  } else if (region == Region::PAL) {
    //add_code_change(0x8000fb4c, 0x48000048);
    add_code_change(0x800e2190, 0x38810044); // output cannon bob only for viewbob
    add_code_change(0x8000ea60, 0x60000000);
    //add_code_change(0x80017878, 0x4e800020);
    add_code_change(0x80015258, 0x4e800020);
    add_code_change(0x8000ec64, 0x60000000);
    add_code_change(0x8000fd20, 0x4800022c);
    // Grapple point yaw fix
    add_code_change(0x8016fc54, 0x7fa3eb78);
    add_code_change(0x8016fc58, 0x38810064); // 6c-8 = 64
    add_code_change(0x8016fc5c, 0x4bee4345); // bl 80053fa0

    // Show crosshair but don't consider pressing R button
    add_code_change(0x80017878, 0x3b000001, "show_crosshair"); // li r24, 1
    add_code_change(0x8001787c, 0x8afd09d4, "show_crosshair"); // lbz r23, 0x9d4(r29)
    add_code_change(0x80017880, 0x53173672, "show_crosshair"); // rlwimi r23, r24, 6, 25, 25 (00000001)
    add_code_change(0x80017884, 0x9afd09d4, "show_crosshair"); // stb r23, 0x9d4(r29)
    add_code_change(0x80017888, 0x4e800020, "show_crosshair"); // blr

    add_asm_patch(build_strafe_code_pal(GuestAllocAligned(kPlanarMoveCodeBufSize, 2)));
  } else {}
}

void FpsControls::init_mod_mp1_gc_r1() {
  //add_code_change(0x8000f6b8, 0x48000048);
  add_code_change(0x800ea1d8, 0x38810044); // output cannon bob only for viewbob

  add_code_change(0x8000e5b4, 0x60000000);

  add_code_change(0x8001489c, 0x4e800020);
  add_code_change(0x8000e7b8, 0x60000000);
  add_code_change(0x8000f88c, 0x48000244);
  // When attached to a grapple point and spinning around it
  // the player's yaw is adjusted, this ensures only position is updated
  // Grapple point yaw fix
  add_code_change(0x8017a208, 0x7fa3eb78);
  add_code_change(0x8017a20c, 0x3881006c);
  add_code_change(0x8017a210, 0x4bed8cf9);

  // Show crosshair but don't consider pressing R button
  add_code_change(0x80016f60, 0x3b000001, "show_crosshair");
  add_code_change(0x80016f64, 0x8afd09c4, "show_crosshair");
  add_code_change(0x80016f68, 0x53173672, "show_crosshair");
  add_code_change(0x80016f6c, 0x9afd09c4, "show_crosshair");
  add_code_change(0x80016f70, 0x4e800020, "show_crosshair");

  add_asm_patch(build_strafe_code_101(GuestAllocAligned(kPlanarMoveCodeBufSize, 2)));
}

void FpsControls::init_mod_mp1_gc_r2() {
  //add_code_change(0x8000f8f8, 0x48000048);
  add_code_change(0x800ea6e0, 0x38810044); // output cannon bob only for viewbob

  add_code_change(0x8000e7f4, 0x60000000);

  add_code_change(0x80014aec, 0x4e800020);
  add_code_change(0x8000e9f8, 0x60000000);
  add_code_change(0x8000facc, 0x4800022c);
  // Grapple point yaw fix
  add_code_change(0x8017a970, 0x7fa3eb78);
  add_code_change(0x8017a974, 0x3881006c);
  add_code_change(0x8017a978, 0x4bed885d);

  // Show crosshair but don't consider pressing R button
  add_code_change(0x800171c4, 0x3b000001, "show_crosshair"); // li r24, 1
  add_code_change(0x800171c8, 0x8afd09d4, "show_crosshair"); // lbz r23, 0x9d4(r29)
  add_code_change(0x800171cc, 0x53173672, "show_crosshair"); // rlwimi r23, r24, 6, 25, 25 (00000001)
  add_code_change(0x800171d0, 0x9afd09d4, "show_crosshair"); // stb r23, 0x9d4(r29)
  add_code_change(0x800171d4, 0x4e800020, "show_crosshair"); // blr

  add_asm_patch(build_strafe_code_102(GuestAllocAligned(kPlanarMoveCodeBufSize, 2)));
}

void FpsControls::init_mod_mp2(Region region) {
  prime::GetVariableManager()->register_variable("new_beam");
  prime::GetVariableManager()->register_variable("beamchange_flag");
  prime::GetVariableManager()->register_variable("switch_ready");
  if (region == Region::NTSC_U) {
    add_code_change(0x8008ccc8, 0xc0430184);
    add_code_change(0x8008cd1c, 0x60000000);
    add_code_change(0x80147f70, 0x60000000);
    add_code_change(0x80147f98, 0x60000000);
    add_code_change(0x80135b20, 0x60000000);
    add_code_change(0x8008bb48, 0x60000000);
    add_code_change(0x8008bb18, 0x60000000);
    add_code_change(0x803054a0, 0xd23f009c);
    add_code_change(0x80169dbc, 0x60000000);
    add_code_change(0x80143d00, 0x48000050);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x8018a7ec, 0x48000468);

    add_code_change(0x8006fde0, 0x60000000, "beam_menu");
    add_code_change(0x8006fdc4, 0x60000000, "visor_menu");

    add_beam_change_code_mp2(0x8018cc88);

    // Steps over bounds checking on the reticle
    add_code_change(0x80018528, 0x48000144);
  } else if (region == Region::PAL) {
    add_code_change(0x8008e30c, 0xc0430184);
    add_code_change(0x8008e360, 0x60000000);
    add_code_change(0x801496e4, 0x60000000);
    add_code_change(0x8014970c, 0x60000000);
    add_code_change(0x80137240, 0x60000000);
    add_code_change(0x8008d18c, 0x60000000);
    add_code_change(0x8008d15c, 0x60000000);
    add_code_change(0x80307d2c, 0xd23f009c);
    add_code_change(0x8016b534, 0x60000000);
    add_code_change(0x80145474, 0x48000050);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x8018bf88, 0x48000468);

    add_code_change(0x80071358, 0x60000000, "beam_menu");
    add_code_change(0x8007133c, 0x60000000, "visor_menu");

    add_beam_change_code_mp2(0x8018e41c);

    // Steps over bounds checking on the reticle
    add_code_change(0x80018528, 0x48000144);
  } else {}
  has_beams = true;
}

void FpsControls::init_mod_mp2_gc(Region region) {
  if (region == Region::NTSC_U) {
    //add_code_change(0x801b00b4, 0x48000050);
    add_code_change(0x800bcd44, 0x38810044); // output cannon bob only for viewbob

    add_code_change(0x801aef58, 0x60000000);
    add_code_change(0x800129c8, 0x4e800020);
    add_code_change(0x801af160, 0x60000000);
    add_code_change(0x801b0248, 0x48000078);
    add_code_change(0x801af450, 0x48000a34);
    // Enable strafing with left/right on L-Stick
    add_code_change(0x8018846c, 0xc022a5b0);
    add_code_change(0x80188104, 0x4800000c);
    // Grapple point yaw fix
    add_code_change(0x8011d9c4, 0x389d0054);
    add_code_change(0x8011d9c8, 0x4bf2d1fd);

    add_code_change(0x80015ed8, 0x3aa00001, "show_crosshair"); // li r21, 1
    add_code_change(0x80015edc, 0x8add1268, "show_crosshair"); // lbz r22, 0x1268(r29)
    add_code_change(0x80015ee0, 0x52b63672, "show_crosshair"); // rlwimi r22, r21, 6, 25, 25 (00000001)
    add_code_change(0x80015ee4, 0x9add1268, "show_crosshair"); // stb r22, 0x1268(r29)
    add_code_change(0x80015ee8, 0x4e800020, "show_crosshair"); // blr

    add_code_change(0x800614f8, 0xc022d3e8);
    const int null_players_vmc_idx = Core::System::GetInstance().GetPowerPC().RegisterVmcall(null_players_on_destruct_mp2_gc);
    u32 null_players_vmc = gen_vmcall(null_players_vmc_idx, 0);
    add_code_change(0x80042994, null_players_vmc);
  } else if (region == Region::PAL) {
    //add_code_change(0x801b03c0, 0x48000050);
    add_code_change(0x800bcdd0, 0x38810044); // output cannon bob only for viewbob

    add_code_change(0x801af264, 0x60000000);
    add_code_change(0x80012a2c, 0x4e800020);
    add_code_change(0x801af46c, 0x60000000);
    add_code_change(0x801b0554, 0x48000078);
    add_code_change(0x801af75c, 0x48000a34);
    // Enable strafing with left/right on L-Stick
    add_code_change(0x80188754, 0xc022a5a0);
    add_code_change(0x801883ec, 0x4800000c);
    // Grapple point yaw fix
    add_code_change(0x8011dbf8, 0x389d0054);
    add_code_change(0x8011dbfc, 0x4bf2d145);  // bl 8004ad40

    add_code_change(0x80015f74, 0x3aa00001, "show_crosshair"); // li r21, 1
    add_code_change(0x80015f78, 0x8add1268, "show_crosshair"); // lbz r22, 0x1268(r29)
    add_code_change(0x80015f7c, 0x52b63672, "show_crosshair"); // rlwimi r22, r21, 6, 25, 25 (00000001)
    add_code_change(0x80015f80, 0x9add1268, "show_crosshair"); // stb r22, 0x1268(r29)
    add_code_change(0x80015f84, 0x4e800020, "show_crosshair"); // blr

    add_code_change(0x800615f8, 0xc022d3c0); // not c022d3d8 ???
    const int null_players_vmc_idx = Core::System::GetInstance().GetPowerPC().RegisterVmcall(null_players_on_destruct_mp2_gc);
    u32 null_players_vmc = gen_vmcall(null_players_vmc_idx, 0);
    add_code_change(0x80042b04, null_players_vmc);
  } else {}
}

void FpsControls::init_mod_mp3(Game game, Region region) {
  prime::GetVariableManager()->register_variable("grapple_lasso_state");
  prime::GetVariableManager()->register_variable("grapple_hand_x");
  prime::GetVariableManager()->register_variable("grapple_hand_y");
  prime::GetVariableManager()->register_variable("trigger_grapple");
  prime::GetVariableManager()->register_variable("new_beam");
  prime::GetVariableManager()->register_variable("beamchange_flag");

  if (region == Region::NTSC_U) {
    add_code_change(0x80080ac0, 0xec010072);
    add_code_change(0x8014e094, 0x60000000);
    add_code_change(0x8014e06c, 0x60000000);
    add_code_change(0x80134328, 0x60000000);
    add_code_change(0x80133970, 0x60000000);
    add_code_change(0x8000ab58, 0x4bffad29);
    add_code_change(0x80080d44, 0x60000000);
    add_code_change(0x8007fdc8, 0x480000e4);
    add_code_change(0x8017f88c, 0x60000000);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x80183734, 0x48000498);
    // This makes sure the stupid gun shoots where you're actually aiming because of course prime 3
    // does this differently too
    add_code_change(0x80184e98, 0x60000000);

    // Grapple Lasso
    add_grapple_lasso_code_mp3(0x800dde64, 0x80170cf0, 0x80171ad8);

    add_control_state_hook_mp3(0x80005880, game, region);
    add_grapple_slide_code_mp3(0x8017f2a0);

    // Steps over bounds checking on the reticle
    add_code_change(0x80016f48, 0x48000120);
  } else if (region == Region::PAL) {
    add_code_change(0x80080ab8, 0xec010072);
    add_code_change(0x8014d9e0, 0x60000000);
    add_code_change(0x8014d9b8, 0x60000000);
    add_code_change(0x80133c74, 0x60000000);
    add_code_change(0x801332bc, 0x60000000);
    add_code_change(0x8000ab58, 0x4bffad29);
    add_code_change(0x80080d44, 0x60000000);
    add_code_change(0x8007fdc8, 0x480000e4);
    add_code_change(0x8017f1d8, 0x60000000);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x80183080, 0x48000498);
    add_code_change(0x801847e4, 0x60000000);

    // Grapple Lasso
    add_grapple_lasso_code_mp3(0x800dde44, 0x8017063c, 0x80171424);

    add_control_state_hook_mp3(0x80005880, game, region);
    add_grapple_slide_code_mp3(0x8017ebec);

    // Steps over bounds checking on the reticle
    add_code_change(0x80016f48, 0x48000120);
  } else {}

  // Same for both.
  add_code_change(0x800614d0, 0x60000000, "visor_menu");
  const int wiimote_shake_override_idx = Core::System::GetInstance().GetPowerPC().RegisterVmcall(wiimote_shake_override);
  add_code_change(0x800a8fc0, gen_vmcall(wiimote_shake_override_idx, 0));
  has_beams = false;
}

void FpsControls::init_mod_mp3_standalone(Game game, Region region) {
  prime::GetVariableManager()->register_variable("grapple_lasso_state");
  prime::GetVariableManager()->register_variable("grapple_hand_x");
  prime::GetVariableManager()->register_variable("grapple_hand_y");
  prime::GetVariableManager()->register_variable("trigger_grapple");
  prime::GetVariableManager()->register_variable("new_beam");
  prime::GetVariableManager()->register_variable("beamchange_flag");
  if (region == Region::NTSC_U) {
    add_code_change(0x80080be8, 0xec010072);
    add_code_change(0x801521f0, 0x60000000);
    add_code_change(0x801521c8, 0x60000000);
    add_code_change(0x80139108, 0x60000000);
    add_code_change(0x80138750, 0x60000000);
    add_code_change(0x8000ae44, 0x4bffaa3d);
    add_code_change(0x80080e6c, 0x60000000);
    add_code_change(0x8007fef0, 0x480000e4);
    add_code_change(0x80183288, 0x60000000);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x80186f98, 0x48000498);
    add_code_change(0x801886f0, 0x60000000);

    add_code_change(0x800617c8, 0x60000000, "visor_menu");

    // Grapple Lasso
    add_grapple_lasso_code_mp3(0x800df790, 0x80174d70, 0x80175b54);

    add_control_state_hook_mp3(0x80005880, game, region);
    add_grapple_slide_code_mp3(0x80182c9c);

    // Steps over bounds checking on the reticle
    add_code_change(0x80017290, 0x48000120);
    const int wiimote_shake_override_idx = Core::System::GetInstance().GetPowerPC().RegisterVmcall(wiimote_shake_override);
    add_code_change(0x800a8f40, gen_vmcall(wiimote_shake_override_idx, 0));
  } else if (region == Region::PAL) {
    add_code_change(0x80080e84, 0xec010072);
    add_code_change(0x80152d50, 0x60000000);
    add_code_change(0x80152d28, 0x60000000);
    add_code_change(0x80139860, 0x60000000);
    add_code_change(0x80138ea8, 0x60000000);
    add_code_change(0x8000ae44, 0x4bffaa3d);
    add_code_change(0x80081108, 0x60000000);
    add_code_change(0x8008018c, 0x480000e4);
    add_code_change(0x80183dc8, 0x60000000);

    // This stops armcannon stuttering by having the XF updater think you're in orbit mode
    add_code_change(0x80187ad8, 0x48000498);
    add_code_change(0x801886f0, 0x60000000);

    add_code_change(0x80061a88, 0x60000000, "visor_menu");

    // Grapple Lasso
    add_grapple_lasso_code_mp3(0x800dfc4c, 0x80175914, 0x801766fc);

    add_control_state_hook_mp3(0x80005880, game, region);
    add_grapple_slide_code_mp3(0x801837dc);

    // Steps over bounds checking on the reticle
    add_code_change(0x80017258, 0x48000120);

    const int wiimote_shake_override_idx = Core::System::GetInstance().GetPowerPC().RegisterVmcall(wiimote_shake_override);
    add_code_change(0x800a938c, gen_vmcall(wiimote_shake_override_idx, 0));
  } else {}
  has_beams = false;
}

} // namespace prime
