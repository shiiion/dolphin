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

constexpr u32 kSpringballHookBufferSizeGC = 0xc8;
constexpr std::string_view spring_ball_template_gc = R"(
.defvar HookStart, 0x{hook_start_addr:x}
.defvar HookBuffer, 0x{hook_buffer_addr:x}
.defvar SpringballInputAddr, 0x{springball_addr:x}
.defvar BombPowerupId, {bomb_pup_id}
.defsym HasPowerup, 0x{has_power_up_addr:x}
.defvar TransformOff, 0x{transform_off:x}
.defsym BombJumpSub, 0x{bomb_jump_addr:x}
.defsym HookReturn, 0x{hook_return_addr:x}

.locate HookStart
b _hook_start

.locate HookBuffer
.defvar var_back_chain, 40
.defvar var_saved_lr, 36
.defvar var_morphball, 8
.defvar var_finalinput, 12
.defvar var_state_mgr, 16
.defvar var_dt, 20
.defvar var_position_x, 24
.defvar var_position_y, 28
.defvar var_position_z, 32

get_playerstate_mp1:
lwz r3, var_state_mgr(sp)
lwz r3, 0x8b8(r3)
lwz r3, 0(r3)
blr

get_playerstate_mp2:
# Chain: Stack -> Morphball+0 -> Player+1314 -> PlayerState
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r3, 0x1314(r3)
blr

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

# Check that player has bombs
bl {get_playerstate_fn}
li r4, BombPowerupId
bl HasPowerup
cmpwi r3, 0
beq _hook_end
lwz r3, var_morphball(sp)
lwz r3, 0(r3)
lwz r5, var_state_mgr(sp)
lwz r0, TransformOff+0xc(r3)
stw r0, var_position_x(sp)
lwz r0, TransformOff+0x1c(r3)
stw r0, var_position_y(sp)
lwz r0, TransformOff+0x2c(r3)
stw r0, var_position_z(sp)
addi r4, sp, var_position_x
bl BombJumpSub

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
    // TODO: Debug springball
    case Game::PRIME_1:
      if (region == Region::NTSC_U) {
        springball_code(0x801476d0);
      } else if (region == Region::PAL) {
        springball_code(0x80147820);
      } else if (region == Region::NTSC_J) {
        springball_code(0x80147cd0);
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
      } else if (region == Region::NTSC_J) {
        springball_code(0x8010b368);
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
      } else if (region == Region::NTSC_J) {
        springball_code(0x8010d49c);
      }
      break;
    default:
      break;
  }
  return true;
}

void SpringballButton::springball_code_gc(Game game, u32 start_point, u32 bomb_pup_id, u32 has_power_up, u32 bomb_jump) {
  LOOKUP(transform_offset);
  const u32 hook_buffer = GuestAllocAligned(kSpringballHookBufferSizeGC, 2);
  const u32 springball_trigger = GetVariableManager()->get_address("springball_trigger");

  add_asm_patch(fmt::format(fmt::runtime(spring_ball_template_gc),
    fmt::arg("hook_start_addr", start_point),
    fmt::arg("hook_buffer_addr", hook_buffer),
    fmt::arg("springball_addr", springball_trigger),
    fmt::arg("bomb_pup_id", bomb_pup_id),
    fmt::arg("has_power_up_addr", has_power_up),
    fmt::arg("transform_off", transform_offset),
    fmt::arg("bomb_jump_addr", bomb_jump),
    fmt::arg("hook_return_addr", start_point + 8),
    fmt::arg("get_playerstate_fn", game == Game::PRIME_2_GCN ? "get_playerstate_mp2" : "get_playerstate_mp1")
  ));
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
