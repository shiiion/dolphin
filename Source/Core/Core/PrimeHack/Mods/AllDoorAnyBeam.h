#pragma once

#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/PrimeMod.h"

#include <fmt/format.h>

namespace prime {

// Wii needs 4 less bytes but, whatever
constexpr u32 kDoorOverrideHookBufferSizeMax = 0xfc;
constexpr std::string_view door_override_template = R"(
.defvar IsWii, {wii_version}
.defvar VTableLoc, 0x{vt_hook_addr:x}
.defvar HookBuffer, 0x{hook_buffer_addr:x}
.defvar StateManager, 0x{state_manager_addr:x}
.defvar DamageVulnOff, 0x{damage_vuln_off:x}

.if IsWii
  .defvar ItemVecOff, 0x2c
.else
  .defvar ItemVecOff, 0x28
.endif

.locate VTableLoc
.4byte HookBuffer

.locate HookBuffer
lis r11, StateManager@ha
ori r11, r11, StateManager@l
.if IsWii
  lwz r12, 0x8b4(r11) # player state
.else
  lwz r12, 0x8b8(r11) # player state
  lwz r12, 0(r12)
.endif

addi r10, r3, DamageVulnOff

lwz r11, 0x4(r10) # Check if trigger is vulnerable to ice beam
cmpwi r11, 1
bne 0f
lwz r0, (ItemVecOff+0x1*8+4)(r12) # check if we have ice beam
cmpwi r0, 0
beq 0f
li r0, 1
stw r0, 0x0(r10) # set vulnerable to power beam
stw r0, 0x4(r10) # set vulnerable to ice beam
stw r0, 0x8(r10) # set vulnerable to wave beam
stw r0, 0xc(r10) # set vulnerable to plasma beam
stw r0, 0x10(r10) # set vulnerable to bombs
stw r0, 0x14(r10) # set vulnerable to power bombs
stw r0, 0x18(r10) # set vulnerable to missiles
0:

lwz r11, 0x8(r10) # Check if trigger is vulnerable to wave beam
cmpwi r11, 1
bne 0f
lwz r0, (ItemVecOff+0x2*8+4)(r12) # check if we have wave beam
cmpwi r0, 0
beq 0f
li r0, 1
stw r0, 0x0(r10) # set vulnerable to power beam
stw r0, 0x4(r10) # set vulnerable to ice beam
stw r0, 0x8(r10) # set vulnerable to wave beam
stw r0, 0xc(r10) # set vulnerable to plasma beam
stw r0, 0x10(r10) # set vulnerable to bombs
stw r0, 0x14(r10) # set vulnerable to power bombs
stw r0, 0x18(r10) # set vulnerable to missiles
0:

lwz r11, 0xc(r10) # Check if trigger is vulnerable to plasma beam
cmpwi r11, 1
bne 0f
lwz r0, (ItemVecOff+0x3*8+4)(r12) # check if we have plasma beam
cmpwi r0, 0
beq 0f
li r0, 1
stw r0, 0x0(r10) # set vulnerable to power beam
stw r0, 0x4(r10) # set vulnerable to ice beam
stw r0, 0x8(r10) # set vulnerable to wave beam
stw r0, 0xc(r10) # set vulnerable to plasma beam
stw r0, 0x10(r10) # set vulnerable to bombs
stw r0, 0x14(r10) # set vulnerable to power bombs
stw r0, 0x18(r10) # set vulnerable to missiles
0:

lwz r11, 0x18(r10) # Check if trigger is vulnerable to missiles
cmpwi r11, 1
bne 0f
lwz r0, (ItemVecOff+0x4*8+4)(r12) # check if we have missiles
cmpwi r0, 0
beq 0f
li r0, 1
stw r0, 0x0(r10) # set vulnerable to power beam
stw r0, 0x4(r10) # set vulnerable to ice beam
stw r0, 0x8(r10) # set vulnerable to wave beam
stw r0, 0xc(r10) # set vulnerable to plasma beam
stw r0, 0x10(r10) # set vulnerable to bombs
stw r0, 0x14(r10) # set vulnerable to power bombs
stw r0, 0x18(r10) # set vulnerable to missiles
0:

addi r3, r3, DamageVulnOff
blr
)";

class AllDoorAnyBeam : public PrimeMod {
public:
  void run_mod(Game, Region) override {}
  bool init_mod(Game game, Region region) override {
    LOOKUP(state_manager);
    if (game != Game::PRIME_1_GCN && game != Game::PRIME_1_GCN_R1 &&
        game != Game::PRIME_1_GCN_R2 && game != Game::PRIME_1) {
      return true;
    }

    const u32 hook_buffer = GuestAllocAligned(kDoorOverrideHookBufferSizeMax, 2);
    switch (game) {
      case Game::PRIME_1_GCN:
        if (region == Region::NTSC_U) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template),
            fmt::arg("wii_version", 0),
            fmt::arg("vt_hook_addr", 0x803dfd40),
            fmt::arg("hook_buffer_addr", hook_buffer),
            fmt::arg("state_manager_addr", state_manager),
            fmt::arg("damage_vuln_off", 0x174)
          ));
        } else if (region == Region::PAL) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template),
            fmt::arg("wii_version", 0),
            fmt::arg("vt_hook_addr", 0x803ca2e0),
            fmt::arg("hook_buffer_addr", hook_buffer),
            fmt::arg("state_manager_addr", state_manager),
            fmt::arg("damage_vuln_off", 0x184)
          ));
        }
        break;
      case Game::PRIME_1_GCN_R1:
        add_asm_patch(fmt::format(fmt::runtime(door_override_template),
          fmt::arg("wii_version", 0),
          fmt::arg("vt_hook_addr", 0x803dff20),
          fmt::arg("hook_buffer_addr", hook_buffer),
          fmt::arg("state_manager_addr", state_manager),
          fmt::arg("damage_vuln_off", 0x174)
        ));
        break;
      case Game::PRIME_1_GCN_R2:
        add_asm_patch(fmt::format(fmt::runtime(door_override_template),
          fmt::arg("wii_version", 0),
          fmt::arg("vt_hook_addr", 0x803e0e00),
          fmt::arg("hook_buffer_addr", hook_buffer),
          fmt::arg("state_manager_addr", state_manager),
          fmt::arg("damage_vuln_off", 0x184)
        ));
        break;
      case Game::PRIME_1:
        if (region == Region::NTSC_U) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template),
            fmt::arg("wii_version", 1),
            fmt::arg("vt_hook_addr", 0x8048c068),
            fmt::arg("hook_buffer_addr", hook_buffer),
            fmt::arg("state_manager_addr", state_manager),
            fmt::arg("damage_vuln_off", 0x184)
          ));
        } else if (region == Region::PAL) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template),
            fmt::arg("wii_version", 1),
            fmt::arg("vt_hook_addr", 0x8048f548),
            fmt::arg("hook_buffer_addr", hook_buffer),
            fmt::arg("state_manager_addr", state_manager),
            fmt::arg("damage_vuln_off", 0x184)
          ));
        }
        break;
      default:
        break;
    }

    return true;
  }
  void on_state_change(ModState) override {}

  GEN_NAME(AllDoorAnyBeam)
};

} // namespace prime
