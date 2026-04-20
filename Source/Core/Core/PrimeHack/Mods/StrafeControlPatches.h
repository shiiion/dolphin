#pragma once

#include "Common/CommonTypes.h"

#include <fmt/format.h>
#include <string>
#include <string_view>

// This file exists to compartmentalize the changes to MP1's ComputeMovement
// to adapt movement closer to what Trilogy MP1 does. It can't perfectly match
// thanks to the controller inputs (through the game's input collection or dolphin)
namespace prime {

constexpr u32 kPlanarMoveCodeBufSize = 0x188;
// Shared chunk of the mp1 gc strafe code
constexpr std::string_view kMp1StrafeShared = R"(
.locate 0x{planar_move_buffer_addr:x}
  .defvar back_chain, 0x38
  .defvar saved_lr, 0x3c
  .defvar regsave, 0x8
  .defvar input_vec2, 0x20
  .defvar local_velocity, 0x2c

  # PARAM: r3 = player
  #        r4 = output force vector
  #        f1 = forward input
  #        f2 = side input, f3 = dt
planar_move_wii:
  stwu r1, -back_chain(sp)
  mfspr r0, LR
  stw r0, saved_lr(sp)
  stfd f31, regsave+0x00(sp)
  stfd f30, regsave+0x08(sp)
  stw r31, regsave+0x10(sp)
  stw r30, regsave+0x14(sp)

  .defvar OutputForceVecGPR, r31
  mr OutputForceVecGPR, r4
  .defvar CPlayerGPR, r30
  mr CPlayerGPR, r3
  .defvar DtFPR, f31
  fmr DtFPR, f3

  # Initially zero out xy components of force vector
  lfs f0, ZeroTOC(rtoc)
  stfs f0, 0(OutputForceVecGPR)
  stfs f0, 4(OutputForceVecGPR)

  # Compute input magnitude
  stfs f2, input_vec2 + 0(sp)
  stfs f1, input_vec2 + 4(sp)
  addi r3, sp, input_vec2
  bl Mag2D
  .defvar InputMagFPR, f30
  fmr InputMagFPR, f1

  # Check if Mag is negligible
  lfs f0, EpsilonTOC(rtoc)
  fcmpo cr0, f1, f0
  ble _end

  # TransposeRotate player velocity to be local to look direction
  addi r3, sp, local_velocity
  addi r4, CPlayerGPR, TransformOff
  addi r5, CPlayerGPR, VelOff
  bl TransposeRotate

  # Normalize the input vector (in-place)
  addi r3, sp, input_vec2
  bl Normalized2D

  # Compute dot product between input direction and velocity to get component of input in direction of current vel
  addi r3, sp, input_vec2
  addi r4, sp, local_velocity
  bl Dot2D

  mr r3, CPlayerGPR
  fmr f3, DtFPR
  fmr f2, f1
  fmr f1, InputMagFPR
  bl compute_input_force_scale
  fdivs f0, f1, InputMagFPR

  # Scale xy component of input direction vector by previous result
  lfs f1, input_vec2+0x0(sp)
  lfs f2, input_vec2+0x4(sp)
  fmuls f1, f0, f1
  fmuls f2, f0, f2
  stfs f1, 0(OutputForceVecGPR)
  stfs f2, 4(OutputForceVecGPR)

_end:
  lfd f31, regsave+0x00(sp)
  lfd f30, regsave+0x08(sp)
  lwz r31, regsave+0x10(sp)
  lwz r30, regsave+0x14(sp)
  lwz r0, saved_lr(sp)
  mtspr LR, r0
  addi r1, r1, back_chain
  blr


  # Rough copy of 0x80193fdc from Trilogy MP1
  # This is a mishmash of a bunch of tweakable values
  # INPUTS: r3 = player
  #         f1 = input magnitude
  #         f2 = input dir * local vel
  #         f3 = dt
compute_input_force_scale:
  # Compute restraint index
  lwz r0, OutOfWaterOff(r3)
  cmpwi r0, 2
  bne 0f
  lwz r0, RestraintOff(r3)
  b 1f
0:
  li r0, 4
1:
  slwi r0, r0, 2

  lwz r9, PlayerTweakR13(r13)
  add r5, r9, r0
  lfs f7, 0xa4(r5) # Max Translational Velocity, MTV
  lfs f4, 0x44(r5) # Friction
  lfs f8, 0x4(r5) # Max Translational Acceleration, MTA
  lfs f0, SixtyTOC(rtoc)
  fmuls f4, f3, f4
  fmuls f6, f0, f4
  fmuls f5, f7, f6
  lfs f6, MassOff(r3) # f6 = Mass
  lfs f9, ZeroTOC(rtoc)
  fmuls f5, f5, f6
  fmuls f3, f3, f8
  lfs f0, Epsilon1e5TOC(rtoc)
  fsubs f4, f9, f1
  fdivs f6, f5, f3
  fabs f4, f4
  frsp f3, f4
  fcmpo cr0, f3, f0
  mfcr r0
  srwi. r0, r0, 31
  bne 0f
  fcmpo cr0, f1, f9
  ble 1f
  fmr f5, f6
  b 2f
1:
  fneg f5, f6
2:
  fsubs f0, f7, f6
  lfs f3, Neg1TOC(rtoc)
  lfs f4, Pos1TOC(rtoc)
  fmuls f0, f1, f0
  fadds f0, f0, f5
  fsubs f0, f0, f2
  fdivs f0, f0, f7
  fsubs f1, f3, f0
  fsel f1, f1, f3, f0
  fsubs f0, f1, f4
  fsel f0, f0, f4, f1
  fmuls f9, f8, f0
0:
  fmr f1, f9
  blr

# Patch for GetDampedClampedVelocityWR, Prime 1 GC did it stupid-ly
.locate DampedClampedVelocityAddr
  .defvar back_chain_2, 0x48
  .defvar saved_lr_2, 0x4c
  .defvar regsave_2, 0x8
  .defvar local_vel_2, 0x38
  stwu sp, -back_chain_2(sp)
  mflr r0
  stw r0, saved_lr_2(sp)
  stfd f31, regsave_2+0x00(sp)
  stfd f30, regsave_2+0x08(sp)
  stfd f29, regsave_2+0x10(sp)
  stfd f28, regsave_2+0x18(sp)
  stw r31, regsave_2+0x20(sp)
  stw r30, regsave_2+0x24(sp)
  stw r29, regsave_2+0x28(sp)
  mr r31, r4
  mr r30, r3

  addi r3, sp, local_vel_2
  addi r4, r31, TransformOff
  addi r5, r31, VelOff
  bl TransposeRotate

  addi r3, sp, local_vel_2
  bl Mag2D
  fmr f28, f1

  lwz r3, PlayerTweakR13(r13)
  lwz r0, OutOfWaterOff(r31)
  cmpwi r0, 2
  bne 0f
  lwz r29, RestraintOff(r31)
  b 1f
0:
  li r29, 4
1:
  slwi r0, r29, 2
  add r3, r0, r3
  lfs f30, 0xa4(r3) # MTV
  lfs f29, 0x44(r3) # Friction
  lwz r0, OrbitStateOff(r31) # OrbitState
  cmpwi r0, 0
  bne _clamp_orbit
  lwz r0, MoveStateOff(r31) # MoveState
  cmpwi r0, 0
  beq 0f
  lfs f0, GrappleJumpTimeout(r31) # GrappleJumpTimeout
  lfs f1, ZeroTOC(rtoc)
  fcmpo cr0, f0, f1
  bgt _clamp_orbit # Apparently grapple timeout has different vel clamping?
0:
  cmpwi r29, 1 # Restraint == air
  bne _after_air_friction

  lfs f29, SevenHalvesTOC(rtoc) # 3.5f
  lfs f0, MassOff(r31) # Mass
  fdivs f0, f28, f0 # f0=LocalVel.xy.mag / mass
  fmuls f29, f0, f29
_after_air_friction:
  lfs f0, DampEpsilonTOC(rtoc)
  lfs f1, local_vel_2(sp)
  lfs f2, local_vel_2+4(sp)
  fmuls f1, f1, f1
  fmuls f2, f2, f2
  fadds f1, f1, f2
  fcmpo cr0, f1, f0
  cror eq, gt, eq
  bne _clamp_zvel
  lfs f0, ZeroTOC(rtoc)
  fsubs f2, f28, f29
  fsubs f3, f0, f2
  fsel f2, f3, f0, f2
  fsubs f0, f2, f30
  fsel f0, f0, f30, f2
  fdivs f1, f0, f28
  addi r3, sp, local_vel_2
  bl Scale2D
  b _clamp_zvel
_clamp_orbit:
  fneg f2, f30
  lfs f0, local_vel_2+0x4(sp)
  fsubs f1, f2, f0
  fsel f0, f1, f2, f0
  fsubs f1, f0, f30
  fsel f0, f1, f30, f0
  stfs f0, local_vel_2+0x4(sp)
_clamp_zvel:
  lwz r0, MoveStateOff(r31)
  cmpwi r0, 0
  bne 0f
  lfs f0, ZeroTOC(rtoc)
  stfs f0, local_vel_2+8(sp)
0:
  mr r3, r30
  addi r4, r31, TransformOff
  addi r5, sp, local_vel_2
  bl MatRotate

  lfd f31, regsave_2+0x00(sp)
  lfd f30, regsave_2+0x08(sp)
  lfd f29, regsave_2+0x10(sp)
  lfd f28, regsave_2+0x18(sp)
  lwz r31, regsave_2+0x20(sp)
  lwz r30, regsave_2+0x24(sp)
  lwz r29, regsave_2+0x28(sp)
  lwz r0, saved_lr_2(sp)
  mtlr r0
  addi sp, sp, back_chain_2
  blr
)";

// Heading portion of strafe code, accounts for addresses and offsets per-version
constexpr std::string_view kMp1Rev0StrafeHeading = R"(
.defvar DampedClampedVelocityAddr, 0x802884f0
.defsym Mag2D, 0x803140f4 # CVector2f::Magnitude
.defsym Normalized2D, 0x803141b0 # CVector2f::Normalize
.defsym TransposeRotate, 0x80312a24 # CTransform4f::TransposeRotate
.defsym MatRotate, 0x80312a80 # CTransform4f::Rotate
.defsym Dot2D, 0x80313fa8 # CVector2f::Dot
.defsym Scale2D, 0x8031414c # CVector2f::operator*=
.defsym ApplyForceOR, 0x8011c144 # CPhysicsActor::ApplyForceOR
.defsym AxisAngleIdentity, 0x8001b534 # CAxisAngle::Identity
.defsym StrafeInput, 0x80286c50 # CPlayer::StrafeInput
# rtoc/r13 Offsets
.defvar ZeroTOC, -0x4260 # 0
.defvar EpsilonTOC, -0x4e00 # 0x34000000
.defvar Epsilon1e5TOC, -0x47d8 # 1e-5
.defvar SixtyTOC, -0x4d70 # 60.f
.defvar Neg1TOC, -0x5740 # -1.f
.defvar Pos1TOC, -0x378c # 1.f
.defvar SevenHalvesTOC, -0x54f8 # 3.5f
.defvar DampEpsilonTOC, -0x36e0 # 1e-28
.defvar PlayerTweakR13, -0x5ee8
# Player offsets
.defvar TransformOff, 0x34
.defvar MassOff, 0xe8
.defvar VelOff, 0x138
.defvar MoveStateOff, 0x258
.defvar RestraintOff, 0x2ac
.defvar OutOfWaterOff, 0x2b0
.defvar OrbitStateOff, 0x304
.defvar GrappleJumpTimeout, 0x3d8
)";

constexpr std::string_view kMp1Rev1StrafeHeading = R"(
.defvar DampedClampedVelocityAddr, 0x8028856c
.defsym Mag2D, 0x803141d4 # CVector2f::Magnitude
.defsym Normalized2D, 0x80314290 # CVector2f::Normalize
.defsym TransposeRotate, 0x80312b04 # CTransform4f::TransposeRotate
.defsym MatRotate, 0x80312b60 # CTransform4f::Rotate
.defsym Dot2D, 0x80314088 # CVector2f::Dot
.defsym Scale2D, 0x8031422c # CVector2f::operator*=
.defsym ApplyForceOR, 0x8011c1c0 # CPhysicsActor::ApplyForceOR
.defsym AxisAngleIdentity, 0x8001b5b0 # CAxisAngle::Identity
.defsym StrafeInput, 0x80286ccc # CPlayer::StrafeInput
# rtoc/r13 Offsets
.defvar ZeroTOC, -0x3b48 # 0
.defvar EpsilonTOC, -0x3b38 # 0x34000000
.defvar Epsilon1e5TOC, -0x470c # 1e-5
.defvar SixtyTOC, -0x4520 # 60.f
.defvar Neg1TOC, -0x38d8 # -1.f
.defvar Pos1TOC, -0x5194 # 1.f
.defvar SevenHalvesTOC, -0x54f8 # 3.5f
.defvar DampEpsilonTOC, -0x36e0 # 1e-28
.defvar PlayerTweakR13, -0x5ee8
# Player offsets
.defvar TransformOff, 0x34
.defvar MassOff, 0xe8
.defvar VelOff, 0x138
.defvar MoveStateOff, 0x258
.defvar RestraintOff, 0x2ac
.defvar OutOfWaterOff, 0x2b0
.defvar OrbitStateOff, 0x304
.defvar GrappleJumpTimeout, 0x3d8
)";

constexpr std::string_view kMp1Rev2StrafeHeading = R"(
.defvar DampedClampedVelocityAddr, 0x80288ea4
.defsym Mag2D, 0x80314b44 # CVector2f::Magnitude
.defsym Normalized2D, 0x80314c00 # CVector2f::Normalize
.defsym TransposeRotate, 0x80313474 # CTransform4f::TransposeRotate
.defsym MatRotate, 0x803134d0 # CTransform4f::Rotate
.defsym Dot2D, 0x803149f8 # CVector2f::Dot
.defsym Scale2D, 0x80314b9c # CVector2f::operator*=
.defsym ApplyForceOR, 0x8011c854 # CPhysicsActor::ApplyForceOR
.defsym AxisAngleIdentity, 0x8001b814 # CAxisAngle::Identity
.defsym StrafeInput, 0x802875dc # CPlayer::StrafeInput
# rtoc/r13 Offsets
.defvar ZeroTOC, -0x34bc # 0
.defvar EpsilonTOC, -0x3320 # 0x34000000
.defvar Epsilon1e5TOC, -0x38fc # 1e-5
.defvar SixtyTOC, -0x4148 # 60.f
.defvar Neg1TOC, -0x33a8 # -1.f
.defvar Pos1TOC, -0x4160 # 1.f
.defvar SevenHalvesTOC, -0x54f8 # 3.5f
.defvar DampEpsilonTOC, -0x36d8 # 1e-28
.defvar PlayerTweakR13, -0x5ec8
# Player offsets
.defvar TransformOff, 0x34
.defvar MassOff, 0xf8
.defvar VelOff, 0x148
.defvar MoveStateOff, 0x268
.defvar RestraintOff, 0x2bc
.defvar OutOfWaterOff, 0x2c0
.defvar OrbitStateOff, 0x314
.defvar GrappleJumpTimeout, 0x3e8
)";

constexpr std::string_view kMp1PALStrafeHeading = R"(
.defvar DampedClampedVelocityAddr, 0x802758ec
.defsym Mag2D, 0x802fce80 # CVector2f::Magnitude
.defsym Normalized2D, 0x802fcf3c # CVector2f::Normalize
.defsym TransposeRotate, 0x802fb7f4 # CTransform4f::TransposeRotate
.defsym MatRotate, 0x802fb850 # CTransform4f::Rotate
.defsym Dot2D, 0x802fcd34 # CVector2f::Dot
.defsym Scale2D, 0x802fced8 # CVector2f::operator*=
.defsym ApplyForceOR, 0x80113720 # CPhysicsActor::ApplyForceOR
.defsym AxisAngleIdentity, 0x8001bea4 # CAxisAngle::Identity
.defsym StrafeInput, 0x80274034 # CPlayer::StrafeInput
# rtoc/r13 Offsets
.defvar ZeroTOC, -0x4920 # 0
.defvar EpsilonTOC, -0x3774 # 0x34000000
.defvar Epsilon1e5TOC, -0x3914 # 1e-5
.defvar SixtyTOC, -0x431c # 60.f
.defvar Neg1TOC, -0x3a70 # -1.f
.defvar Pos1TOC, -0x4ab4 # 1.f
.defvar SevenHalvesTOC, -0x5f94 # 3.5f
.defvar DampEpsilonTOC, -0x35e0 # 1e-28
.defvar PlayerTweakR13, -0x5e70
# Player offsets
.defvar TransformOff, 0x34
.defvar MassOff, 0xf8
.defvar VelOff, 0x148
.defvar MoveStateOff, 0x268
.defvar RestraintOff, 0x2bc
.defvar OutOfWaterOff, 0x2c0
.defvar OrbitStateOff, 0x314
.defvar GrappleJumpTimeout, 0x3e8
)";

// Miscellaneous per-version patches of the mp1 gc strafe code
constexpr std::string_view kMp1Rev0StrafeMisc = R"(
# Patch to final ApplyForceOR inputs with our hook
.locate 0x80287590
  mr r3, r29 # CPlayer
  addi r4, sp, 0xdc # Output force_vec passed to CPhysicsActor::ApplyForceOR
  fmr f1, f31 # Forward input
  fmr f2, f30 # Side input
  fmr f3, f27 # dt
  bl planar_move_wii
  bl AxisAngleIdentity
  stfs f29, 0xe4(sp) # Set force_vec.z to jump_input
  mr r5, r3
  mr r3, r29
  addi r4, sp, 0xdc
  bl ApplyForceOR
  # Skip past the block which applies torque based on turn_input
  b `0x80287604`

# Other patches to make this strafing work
# Have CPlayer::ComputeMovement call CPlayer::StrafeInput instead of CPlayer::TurnInput
.locate 0x80286fe0
  bl StrafeInput

# Have CPlayer::StrafeInput ignore orbit state
.locate 0x80286c88
  b `0x80286c94`

# Remove modifications to the strafe input register
.locate 0x8028739c
  nop
.locate 0x802873e0
  nop

# Force CPlayer::ComputeMovement to apply torque always
.locate 0x8028707c
  nop

# Remove calls to CPhysicsActor::SetAngularVelocityOR
.locate 0x802871bc
  nop
.locate 0x80287288
  nop
)";

constexpr std::string_view kMp1Rev1StrafeMisc = R"(
.locate 0x8028760c
  mr r3, r29
  addi r4, sp, 0xdc
  fmr f1, f31
  fmr f2, f30
  fmr f3, f27
  bl planar_move_wii
  bl AxisAngleIdentity
  stfs f29, 0xe4(sp) # Set force_vec.z to jump_input
  mr r5, r3
  mr r3, r29
  addi r4, sp, 0xdc
  bl ApplyForceOR
  # Skip past the block which applies torque based on turn_input
  b `0x80287680`
# Other patches to make this strafing work
# Have CPlayer::ComputeMovement call CPlayer::StrafeInput instead of CPlayer::TurnInput
.locate 0x8028705c
  bl StrafeInput

# Have CPlayer::StrafeInput ignore orbit state
.locate 0x80286d04
  b `0x80286d10`

# Remove modifications to the strafe input register
.locate 0x80287418
  nop
.locate 0x8028745c
  nop

# Force CPlayer::ComputeMovement to apply torque always
.locate 0x802870f8
  nop

# Remove calls to CPhysicsActor::SetAngularVelocityOR
.locate 0x80287238
  nop
.locate 0x80287304
  nop
)";

constexpr std::string_view kMp1Rev2StrafeMisc = R"(
.locate 0x80287f1c
  mr r3, r29
  addi r4, sp, 0xdc
  fmr f1, f31
  fmr f2, f30
  fmr f3, f27
  bl planar_move_wii
  bl AxisAngleIdentity
  stfs f29, 0xe4(sp) # Set force_vec.z to jump_input
  mr r5, r3
  mr r3, r29
  addi r4, sp, 0xdc
  bl ApplyForceOR
  # Skip past the block which applies torque based on turn_input
  b `0x80287f90`
# Other patches to make this strafing work
# Have CPlayer::ComputeMovement call CPlayer::StrafeInput instead of CPlayer::TurnInput
.locate 0x8028796c
  bl StrafeInput

# Have CPlayer::StrafeInput ignore orbit state
.locate 0x80287614
  b `0x80287620`

# Remove modifications to the strafe input register
.locate 0x80287d28
  nop
.locate 0x80287d6c
  nop

# Force CPlayer::ComputeMovement to apply torque always
.locate 0x80287a08
  nop

# Remove calls to CPhysicsActor::SetAngularVelocityOR
.locate 0x80287b48
  nop
.locate 0x80287c14
  nop
)";

constexpr std::string_view kMp1PALStrafeMisc = R"(
.locate 0x80274974
  mr r3, r29
  addi r4, sp, 0xdc
  fmr f1, f31
  fmr f2, f30
  fmr f3, f27
  bl planar_move_wii
  bl AxisAngleIdentity
  stfs f29, 0xe4(sp) # Set force_vec.z to jump_input
  mr r5, r3
  mr r3, r29
  addi r4, sp, 0xdc
  bl ApplyForceOR
  # Skip past the block which applies torque based on turn_input
  b `0x802749e8`

# Other patches to make this strafing work
# Have CPlayer::ComputeMovement call CPlayer::StrafeInput instead of CPlayer::TurnInput
.locate 0x802743c4
  bl StrafeInput

# Have CPlayer::StrafeInput ignore orbit state
.locate 0x8027406c
  b `0x80274078`

# Remove modifications to the strafe input register
.locate 0x80274780
  nop
.locate 0x802747c4
  nop

# Force CPlayer::ComputeMovement to apply torque always
.locate 0x80274460
  nop

# Remove calls to CPhysicsActor::SetAngularVelocityOR
.locate 0x802745a0
  nop
.locate 0x8027466c
  nop
)";

static std::string build_strafe_code_shared(u32 buf_start, std::string_view heading, std::string_view misc) {
  const std::string shared_str =
    fmt::format(fmt::runtime(kMp1StrafeShared), fmt::arg("planar_move_buffer_addr", buf_start));
  return fmt::format("{}\n{}\n{}", heading, shared_str, misc);
}

static std::string build_strafe_code_100(u32 buf_start) {
  return build_strafe_code_shared(buf_start, kMp1Rev0StrafeHeading, kMp1Rev0StrafeMisc);
}

static std::string build_strafe_code_101(u32 buf_start) {
  return build_strafe_code_shared(buf_start, kMp1Rev1StrafeHeading, kMp1Rev1StrafeMisc);
}

static std::string build_strafe_code_102(u32 buf_start) {
  return build_strafe_code_shared(buf_start, kMp1Rev2StrafeHeading, kMp1Rev2StrafeMisc);
}

static std::string build_strafe_code_pal(u32 buf_start) {
  return build_strafe_code_shared(buf_start, kMp1PALStrafeHeading, kMp1PALStrafeMisc);
}

} // namespace prime
