#pragma once

#include <string_view>

std::string spring_ball_template = R"(
.locate 0x{hook_start:08x}
b _hook

.locate 0x{hook_buffer:08x}
_hook:
stwu r1, -40(r1)
mfspr r0, LR
stw r0, 36(r1)
stw r3, 8(r1)
stw r4, 12(r1)
stw r5, 16(r1)
stfs f1, 20(r1)
# Check input (call digital input)
# Using power beam bind because all other options are in use or are sub-optimal. Revisit later.
{get_digital_input_param_stub}
bl `0x{get_digital_input:08x}`
cmpwi r3, 0
beq end

lwz r3, 8(r1)
lwz r3, 0(r3)
lwz r3, 0x{player_movement_state:08x}(r3)
cmpwi r3, 0
bne end

# Bomb requirement check
{get_player_state_stub}
# call HasPowerUp, passing it the CPlayerState and the powerup id 6
li r4, {bomb_pup_id}
bl `0x{has_power_up:08x}`
# r3 will either be 0 or 1
cmpwi r3, 0
beq end
.defvar TransformOff, 0x{transform_off:08x}
lwz r3, 8(r1)
lwz r3, 0(r3)
lwz r5, 16(r1)
lwz r0, TransformOff+0xc(r3)
stw r0, 24(r1)
lwz r0, TransformOff+0x1c(r3)
stw r0, 28(r1)
lwz r0, TransformOff+0x2c(r3)
stw r0, 32(r1)
addi r4, r1, 24
bl `0x{bomb_jump:08x}`
end:
lwz r0, 36(r1)
lwz r3, 8(r1)
lwz r4, 12(r1)
lwz r5, 16(r1)
lfs f1, 20(r1)
addi r1, r1, 40
stwu r1, -0x20(r1)
b `0x{hook_return:08x}`
)";

std::string door_override_template = R"(
.locate 0x{vt_hook:08x}
.4byte 0x{hook_buffer:08x}

.locate 0x{hook_buffer:08x}
.defvar StateManager, 0x{state_manager:08x}
lis r11, StateManager@ha
ori r11, r11, StateManager@l
lwz r12, 0x8b8(r11) # player state
lwz r12, 0(r12)

addi r10, r3, 0x174

lwz r11, 0x4(r10) # Check if trigger is vulnerable to ice beam
cmpwi r11, 1
bne 0f
lwz r0, (0x28+0x1*8+4)(r12) # check if we have ice beam
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
lwz r0, (0x28+0x2*8+4)(r12) # check if we have wave beam
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
lwz r0, (0x28+0x3*8+4)(r12) # check if we have plasma beam
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
lwz r0, (0x28+0x4*8+4)(r12) # check if we have missiles
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

addi r3, r3, 0x174
blr
)";
