#include "Core/PrimeHack/Mods/SpringballButton.h"

#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
namespace {

constexpr std::string_view spring_ball_template_wii = R"(
.defvar PatchStart, 0x{patch_start_addr:x}
.defvar SpringballInputAddr, 0x{springball_addr:x}

.locate PatchStart
lis r4, SpringballInputAddr@ha
ori r4, r4, SpringballInputAddr@l
lbz r3, 0(r4)
li r5, 0
stb r5, 0(r4)
cmpwi r3, 0
)";

constexpr u32 kSpringballHookBufferSizeMP1GC = 0x118;
constexpr std::string_view spring_ball_template_mp1_gc = R"(
.defvar HookStart, 0x{hook_start_addr:x}
.defvar HookBuffer, 0x{hook_buffer_addr:x}
.defvar SpringballInputAddr, 0x{springball_addr:x}
.defvar BombPowerupId, {bomb_pup_id}
.defsym HasPowerup, 0x{has_power_up_addr:x}
.defvar TransformOff, 0x{transform_off:x}
.defvar VelOff, 0x{vel_off:x}
.defsym BombJumpSub, 0x{bomb_jump_addr:x}
.defsym HookReturn, 0x{hook_return_addr:x}

.locate HookStart
b _hook_start

.locate HookBuffer
.defvar var_back_chain, 48
.defvar var_saved_lr, 44
.defvar var_morphball, 8
.defvar var_finalinput, 12
.defvar var_state_mgr, 16
.defvar var_dt, 20
.defvar var_position_x, 24
.defvar var_position_y, 28
.defvar var_position_z, 32
.defvar var_hor_vel_x, 36
.defvar var_hor_vel_y, 40

_hook_start:
stwu sp, -var_back_chain(sp)
mfspr r0, LR
stw r0, var_saved_lr(sp)
stw r3, var_morphball(sp)
stw r4, var_finalinput(sp)
stw r5, var_state_mgr(sp)
stfs f1, var_dt(sp)

# Check input being pressed
lis r3, SpringballInputAddr@ha
ori r3, r3, SpringballInputAddr@l
lbz r3, 0(r3)
cmpwi r3, 0
beq _hook_end

# Use the cached collision info list to ensure we're on some kind of surface
lis r3, _allowedNormalZ@ha
ori r3, r3, _allowedNormalZ@l
lfs f2, 0(r3)
lwz r3, var_morphball(sp)
lwz r4, 0x74(r3)
__cond:
cmplwi r4, 0
beq _hook_end
lfs f0, 0x78+0x50(r3)
fcmpo cr0, f0, f2
bge __pass
__iter:
addi r3, r3, 0x60
subi r4, r4, 1
b __cond

# Detected a springball-able surface
__pass:
# Check that player has bombs
lwz r3, var_state_mgr(sp)
lwz r3, 0x8b8(r3)
lwz r3, 0(r3)
li r4, BombPowerupId
bl HasPowerup
cmpwi r3, 0
beq _hook_end
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r0, VelOff(r3)
stw r0, var_hor_vel_x(sp)
lwz r0, VelOff+0x4(r3)
stw r0, var_hor_vel_y(sp)
lwz r5, var_state_mgr(sp)
lwz r0, TransformOff+0xc(r3)
stw r0, var_position_x(sp)
lwz r0, TransformOff+0x1c(r3)
stw r0, var_position_y(sp)
lwz r0, TransformOff+0x2c(r3)
stw r0, var_position_z(sp)
addi r4, sp, var_position_x

bl BombJumpSub
# We want to retain horizontal velocity when jumping, so add it back here
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r0, var_hor_vel_x(sp)
stw r0, VelOff(r3)
lwz r0, var_hor_vel_y(sp)
stw r0, VelOff+0x4(r3)

_hook_end:
lwz r0, var_saved_lr(sp)
lwz r3, var_morphball(sp)
lwz r4, var_finalinput(sp)
lwz r5, var_state_mgr(sp)
lfs f1, var_dt(sp)
addi sp, sp, var_back_chain

# Rerun clobbered instruction from trampoline
# Since LR is already in r0, skip LR->r0 prologue
stwu sp, -0x20(sp)
b HookReturn

_allowedNormalZ:
.float 0.70710677
)";

constexpr u32 kSpringballHookBufferSizeMP2GC = 0x110;
constexpr std::string_view spring_ball_template_mp2_gc = R"(
.defvar HookStart, 0x{hook_start_addr:x}
.defvar HookBuffer, 0x{hook_buffer_addr:x}
.defvar SpringballInputAddr, 0x{springball_addr:x}
.defvar BombPowerupId, {bomb_pup_id}
.defsym HasPowerup, 0x{has_power_up_addr:x}
.defvar TransformOff, 0x{transform_off:x}
.defvar VelOff, 0x{vel_off:x}
.defsym BombJumpSub, 0x{bomb_jump_addr:x}
.defsym HookReturn, 0x{hook_return_addr:x}

.locate HookStart
b _hook_start

.locate HookBuffer
.defvar var_back_chain, 52
.defvar var_saved_lr, 48
.defvar var_morphball, 8
.defvar var_finalinput, 12
.defvar var_state_mgr, 16
.defvar var_dt, 20
.defvar var_position_x, 24
.defvar var_position_y, 28
.defvar var_position_z, 32
.defvar var_hor_vel_x, 36
.defvar var_hor_vel_y, 40
.defvar var_saved_reset_timer, 44

_hook_start:
stwu sp, -var_back_chain(sp)
mfspr r0, LR
stw r0, var_saved_lr(sp)
stw r3, var_morphball(sp)
stw r4, var_finalinput(sp)
stw r5, var_state_mgr(sp)
stfs f1, var_dt(sp)

# Check input being pressed
lis r3, SpringballInputAddr@ha
ori r3, r3, SpringballInputAddr@l
lbz r3, 0(r3)
cmpwi r3, 0
beq _hook_end

# Use the cached collision info list to ensure we're on some kind of surface
lis r3, _allowedNormalZ@ha
ori r3, r3, _allowedNormalZ@l
lfs f2, 0(r3)
lwz r3, var_morphball(sp)
lwz r4, 0x74(r3)
__cond:
cmplwi r4, 0
beq _hook_end
lfs f0, 0x78+0x48(r3)
fcmpo cr0, f0, f2
bge __pass
__iter:
addi r3, r3, 0x60
subi r4, r4, 1
b __cond

# Detected a springball-able surface
__pass:
# Check that player has bombs
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r3, 0x1314(r3)
li r4, BombPowerupId
bl HasPowerup
cmpwi r3, 0
beq _hook_end
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r4, 0xebc(r3)
lwz r0, 0x668(r4)
stw r0, var_saved_reset_timer(sp)
lwz r0, VelOff(r3)
stw r0, var_hor_vel_x(sp)
lwz r0, VelOff+0x4(r3)
stw r0, var_hor_vel_y(sp)
lwz r5, var_state_mgr(sp)
lwz r0, TransformOff+0xc(r3)
stw r0, var_position_x(sp)
lwz r0, TransformOff+0x1c(r3)
stw r0, var_position_y(sp)
lwz r0, TransformOff+0x2c(r3)
stw r0, var_position_z(sp)
addi r4, sp, var_position_x

bl BombJumpSub
# We want to retain horizontal velocity when jumping, so add it back here
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r0, var_saved_reset_timer(sp)
lwz r4, 0xebc(r3)
# Springball shouldn't affect the reset timer for bombs
stw r0, 0x668(r4)
lwz r0, var_hor_vel_x(sp)
stw r0, VelOff(r3)
lwz r0, var_hor_vel_y(sp)
stw r0, VelOff+0x4(r3)

_hook_end:
lwz r0, var_saved_lr(sp)
lwz r3, var_morphball(sp)
lwz r4, var_finalinput(sp)
lwz r5, var_state_mgr(sp)
lfs f1, var_dt(sp)
addi sp, sp, var_back_chain

# Rerun clobbered instruction from trampoline
# Since LR is already in r0, skip LR->r0 prologue
stwu sp, -0x20(sp)
b HookReturn

_allowedNormalZ:
.float 0.70710677
)";

} // namespace

void SpringballButton::run_mod(Game game, Region region) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }
  springball_check();
}

bool SpringballButton::init_mod(Game game, Region region) {
  prime::GetVariableManager()->register_variable("springball_trigger");

  switch (game) {
    case Game::PRIME_1:
      if (region == Region::NTSC_U) {
        springball_code(0x801476d0);
      } else if (region == Region::PAL) {
        springball_code(0x80147820);
      }
      break;
    case Game::PRIME_1_GCN:
      if (region == Region::NTSC_U) {
        springball_code_gc(game, 0x800f8d28, 6, 0x80091ac0, 0x802853ec);
      } else if (region == Region::PAL) {
        springball_code_gc(game, 0x800f0a60, 6, 0x80091e24, 0x80272788);
      }
      break;
    case Game::PRIME_1_GCN_R1:
      springball_code_gc(game, 0x800f8da4, 6, 0x80091b3c, 0x80285468);
      break;
    case Game::PRIME_1_GCN_R2:
      springball_code_gc(game, 0x800f92ac, 6, 0x80092044, 0x80285d78);
      break;
    case Game::PRIME_2:
      if (region == Region::NTSC_U) {
        springball_code(0x8010bd98);
      } else if (region == Region::PAL) {
        springball_code(0x8010d440);
      }
      break;
    case Game::PRIME_2_GCN:
      if (region == Region::NTSC_U) {
        springball_code_gc(game, 0x800ce864, 18, 0x80085480, 0x80186838);
      } else if (region == Region::PAL) {
        springball_code_gc(game, 0x800ce93c, 18, 0x800855bc, 0x80186b1c);
      }
      break;
    case Game::PRIME_3:
      if (region == Region::NTSC_U) {
        springball_code(0x801077d4);
      } else if (region == Region::PAL) {
        springball_code(0x80107120);
      }
      break;
    case Game::PRIME_3_STANDALONE:
      if (region == Region::NTSC_U) {
        springball_code(0x8010c984);
      } else if (region == Region::PAL) {
        springball_code(0x8010ced4);
      }
      break;
    default:
      break;
  }
  return true;
}

void SpringballButton::springball_code_gc(Game game, u32 start_point, u32 bomb_pup_id, u32 has_power_up, u32 bomb_jump) {
  LOOKUP(xf_offset);
  LOOKUP(vel_offset);
  const u32 springball_trigger = GetVariableManager()->get_address("springball_trigger");

  if (game == Game::PRIME_1_GCN || game == Game::PRIME_1_GCN_R1 || game == Game::PRIME_1_GCN_R2) {
    const u32 hook_buffer = GuestAllocAligned(kSpringballHookBufferSizeMP1GC, 2);
    add_asm_patch(fmt::format(fmt::runtime(spring_ball_template_mp1_gc),
      fmt::arg("hook_start_addr", start_point),
      fmt::arg("hook_buffer_addr", hook_buffer),
      fmt::arg("springball_addr", springball_trigger),
      fmt::arg("bomb_pup_id", bomb_pup_id),
      fmt::arg("has_power_up_addr", has_power_up),
      fmt::arg("transform_off", xf_offset),
      fmt::arg("bomb_jump_addr", bomb_jump),
      fmt::arg("hook_return_addr", start_point + 8),
      fmt::arg("vel_off", vel_offset)
    ));
  } else if (game == Game::PRIME_2_GCN) {
    const u32 hook_buffer = GuestAllocAligned(kSpringballHookBufferSizeMP2GC, 2);
    add_asm_patch(fmt::format(fmt::runtime(spring_ball_template_mp2_gc),
      fmt::arg("hook_start_addr", start_point),
      fmt::arg("hook_buffer_addr", hook_buffer),
      fmt::arg("springball_addr", springball_trigger),
      fmt::arg("bomb_pup_id", bomb_pup_id),
      fmt::arg("has_power_up_addr", has_power_up),
      fmt::arg("transform_off", xf_offset),
      fmt::arg("bomb_jump_addr", bomb_jump),
      fmt::arg("hook_return_addr", start_point + 8),
      fmt::arg("vel_off", vel_offset)
    ));
  }
}

void SpringballButton::springball_code(u32 start_point) {
  const u32 springball_trigger = GetVariableManager()->get_address("springball_trigger");

  add_asm_patch(fmt::format(fmt::runtime(spring_ball_template_wii),
    fmt::arg("springball_addr", springball_trigger),
    fmt::arg("patch_start_addr", start_point)
  ));
}

void SpringballButton::springball_check() {
  if (CheckSpringBallCtl()) {
    LOOKUP_DYN(ball_state);
    LOOKUP_DYN(move_state);
    u32 ball_state_val = read32(ball_state);
    u32 move_state_val = read32(move_state);

    if ((ball_state_val == 1 || ball_state_val == 2) && move_state_val == 0) {
      prime::GetVariableManager()->set_variable(*active_guard, "springball_trigger", u8{ 1 });
    } else {
      prime::GetVariableManager()->set_variable(*active_guard, "springball_trigger", u8{ 0 });
    }
  } else {
    prime::GetVariableManager()->set_variable(*active_guard, "springball_trigger", u8{ 0 });
  }
}

} // namespace prime
