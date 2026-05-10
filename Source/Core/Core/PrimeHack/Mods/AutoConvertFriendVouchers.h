#pragma once

#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/GuestAllocator.h"
#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include <fmt/format.h>

namespace prime {

constexpr size_t kFriendVoucherBufferSize = 20;
constexpr std::string_view friend_voucher_converter_template = R"(
.defvar HookPoint, 0x{hook_point:x}
.defvar HookBuffer, 0x{hook_buffer:x}

.locate HookPoint
b _hook

.locate HookBuffer
_hook:
cmpwi r4, 4
beq _skip
li r4, 3
_skip:
# Original instruction
slwi r0, r4, 2
b `HookPoint + 4`
)";

void patch_script_achieve_string(PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu, u32 job);

class AutoConvertFriendVouchers : public PrimeMod {
public:
  void run_mod(Game, Region) override {}
  bool init_mod(Game game, Region region) override {
    if (game != Game::PRIME_3 && game != Game::PRIME_3_STANDALONE) {
      return true;
    }

    const u32 hook_buffer = GuestAllocAligned(kFriendVoucherBufferSize, 2);
    const int patch_string =
      Core::System::GetInstance().GetPowerPC().RegisterVmcall(patch_script_achieve_string);
    const u32 vmc_patch_string = gen_vmcall(static_cast<u32>(patch_string), 0);
    switch (game) {
      case Game::PRIME_3: {
        if (region == Region::NTSC_U) {
          add_module_code_change(0xac8, vmc_patch_string, "RSO_ScriptAchievement.rso");
          add_asm_patch(fmt::format(fmt::runtime(friend_voucher_converter_template),
                                    fmt::arg("hook_point", 0x802aad44),
                                    fmt::arg("hook_buffer", hook_buffer)));
        } else if (region == Region::PAL) {
          add_asm_patch(fmt::format(fmt::runtime(friend_voucher_converter_template),
                                    fmt::arg("hook_point", 0x802aaa1c),
                                    fmt::arg("hook_buffer", hook_buffer)));
        }
        break;
      }
      case Game::PRIME_3_STANDALONE:
        if (region == Region::NTSC_U) {
          add_asm_patch(fmt::format(fmt::runtime(friend_voucher_converter_template),
                                    fmt::arg("hook_point", 0x8003d278),
                                    fmt::arg("hook_buffer", hook_buffer)));
        } else if (region == Region::PAL) {
          add_asm_patch(fmt::format(fmt::runtime(friend_voucher_converter_template),
                                    fmt::arg("hook_point", 0x8003d360),
                                    fmt::arg("hook_buffer", hook_buffer)));
        }
        break;
      default:
        break;
    }

    return true;
  }
  void on_state_change(ModState) override {}

  GEN_NAME(AutoConvertFriendVouchers)
};

} // namespace prime
