#pragma once

#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/PrimeMod.h"

#include <fmt/format.h>

namespace prime {

constexpr std::string_view door_override_template_mp2 = R"(
.defvar IsWii, {wii_version}
.defvar VTableLoc, 0x{vt_hook_addr:x}
.defvar HookBuffer, 0x{hook_buffer_addr:x}

.if IsWii
.defvar DamageVulnOff, 0x42c
.defvar ChargeVulnTable, 0x15
.defvar ComboVulnTable, 0x19
.else
.defvar DamageVulnOff, 0x43c
.defvar ChargeVulnTable, 0x18
.defvar ComboVulnTable, 0x1c
.endif

.locate VTableLoc
.4byte HookBuffer

.locate HookBuffer
li r0, 1
stb r0, (DamageVulnOff+0x3)(r3) # Regular projectile
stb r0, (DamageVulnOff+ChargeVulnTable+0x3)(r3) # Charge shot
stb r0, (DamageVulnOff+ComboVulnTable+0x3)(r3) # Missile combo
addi r3, r3, DamageVulnOff
blr
)";

// Wii needs 4 less bytes but, whatever
constexpr u32 kDoorOverrideHookBufferSizeMax = 0x17c;
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
stw r0, 0x3c(r10) # set vulnerable to power charge
stw r0, 0x40(r10) # set vulnerable to ice charge
stw r0, 0x44(r10) # set vulnerable to wave charge
stw r0, 0x48(r10) # set vulnerable to plasma charge
stw r0, 0x4c(r10) # set vulnerable to super missile
stw r0, 0x50(r10) # set vulnerable to ice spreader
stw r0, 0x54(r10) # set vulnerable to wavebuster
stw r0, 0x58(r10) # set vulnerable to flame thrower
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
stw r0, 0x3c(r10) # set vulnerable to power charge
stw r0, 0x40(r10) # set vulnerable to ice charge
stw r0, 0x44(r10) # set vulnerable to wave charge
stw r0, 0x48(r10) # set vulnerable to plasma charge
stw r0, 0x4c(r10) # set vulnerable to super missile
stw r0, 0x50(r10) # set vulnerable to ice spreader
stw r0, 0x54(r10) # set vulnerable to wavebuster
stw r0, 0x58(r10) # set vulnerable to flame thrower
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
stw r0, 0x3c(r10) # set vulnerable to power charge
stw r0, 0x40(r10) # set vulnerable to ice charge
stw r0, 0x44(r10) # set vulnerable to wave charge
stw r0, 0x48(r10) # set vulnerable to plasma charge
stw r0, 0x4c(r10) # set vulnerable to super missile
stw r0, 0x50(r10) # set vulnerable to ice spreader
stw r0, 0x54(r10) # set vulnerable to wavebuster
stw r0, 0x58(r10) # set vulnerable to flame thrower
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
stw r0, 0x3c(r10) # set vulnerable to power charge
stw r0, 0x40(r10) # set vulnerable to ice charge
stw r0, 0x44(r10) # set vulnerable to wave charge
stw r0, 0x48(r10) # set vulnerable to plasma charge
stw r0, 0x4c(r10) # set vulnerable to super missile
stw r0, 0x50(r10) # set vulnerable to ice spreader
stw r0, 0x54(r10) # set vulnerable to wavebuster
stw r0, 0x58(r10) # set vulnerable to flame thrower
0:

addi r3, r3, DamageVulnOff
blr
)";

// MOD PURPOSE:
//  MP1 - Allow all X doors to be opened by any beam as long as you have X beam
//  MP2 - Allow all doors to be opened by annihilator, does not include door locks
class AllDoorAnyBeam : public PrimeMod {
public:
  void run_mod(Game, Region) override {}
  bool init_mod(Game game, Region region) override {
    LOOKUP(state_manager);
    if (game != Game::PRIME_1_GCN && game != Game::PRIME_1_GCN_R1 &&
        game != Game::PRIME_1_GCN_R2 && game != Game::PRIME_1 && game != Game::PRIME_2_GCN && game != Game::PRIME_2) {
      return true;
    }

    const u32 hook_buffer = GuestAllocAligned(kDoorOverrideHookBufferSizeMax, 2);
    switch (game) {
      case Game::PRIME_2_GCN:
        if (region == Region::NTSC_U) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template_mp2),
            fmt::arg("wii_version", 0),
            fmt::arg("vt_hook_addr", 0x803b26d8),
            fmt::arg("hook_buffer_addr", hook_buffer)
          ));
        }
        else if (region == Region::PAL) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template_mp2),
            fmt::arg("wii_version", 0),
            fmt::arg("vt_hook_addr", 0x803b3a58),
            fmt::arg("hook_buffer_addr", hook_buffer)
          ));
        }
        break;
      case Game::PRIME_2:
        if (region == Region::NTSC_U) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template_mp2),
            fmt::arg("wii_version", 1),
            fmt::arg("vt_hook_addr", 0x804b8168),
            fmt::arg("hook_buffer_addr", hook_buffer)
          ));
        }
        else if (region == Region::PAL) {
          add_asm_patch(fmt::format(fmt::runtime(door_override_template_mp2),
            fmt::arg("wii_version", 1),
            fmt::arg("vt_hook_addr", 0x804be968),
            fmt::arg("hook_buffer_addr", hook_buffer)
          ));
        }
        break;
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

  bool is_cheat() const override { return true; }
  GEN_NAME(AllDoorAnyBeam)
};

} // namespace prime
