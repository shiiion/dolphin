#include "Core/PrimeHack/Mods/AutoConvertFriendVouchers.h"

#include <array>

namespace prime {

struct VoucherSTRG {
  u64 hash;
  // Bit of a hack here, but all patches we're doing ensure that the replacement len is shorter
  std::string_view patch_to;
};

constexpr std::array<VoucherSTRG, 16> kPatchTable = {
  VoucherSTRG {0x39cfde5b035c366e, "&just=center;&main-color=#FFFFFFFF;Juggling Bonus - 20\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0xa6e1e368464939cb, "&just=center;&main-color=#FFFFFFFF;&killcount; Kills\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x105a3e6b9ff6f889, "&just=center;&main-color=#FFFFFFFF;GF Trooper Saved\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x109661868c439384, "&just=center;&main-color=#FFFFFFFF;Flawless Escape\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0xcdc8ab924361b0ed, "&just=center;&main-color=#FFFFFFFF;Shortcut Discovered\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x387e1ec1ce8126fb, "&just=center;&main-color=#FFFFFFFF;Icy Reptilicide\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x5f091ab745230085, "&just=center;&main-color=#FFFFFFFF;Perfect Execution\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x00211573e2634ffd, "&just=center;&main-color=#FFFFFFFF;Exterminator\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x82a0f6a1919da5c0, "&just=center;&main-color=#FFFFFFFF;Stylish Kill\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0xb5327bb92ec18be0, "&just=center;&main-color=#FFFFFFFF;Bowling For Bots\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x117f95ef773c18fe, "&just=center;&main-color=#FFFFFFFF;Crash Landing\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x3a5213af947e3a1c, "&just=center;&main-color=#FFFFFFFF;New Area Discovered\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0xed0119d30958dc8c, "&just=center;&main-color=#FFFFFFFF;20 Commandos Defeated\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0xb2e8f68093bed5a2, "&just=center;&main-color=#FFFFFFFF;Harvester Destroyed\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0x3deed020bd1102f4, "&just=center;&main-color=#FFFFFFFF;Leviathan Humiliated\n&image=0xF76434386E7D8B5C;"},
  VoucherSTRG {0xe3c2a28876d74558, "&just=center;&main-color=#FFFFFFFF;Secret Message Discovered\n&image=0xF76434386E7D8B5C;"},
};

void patch_script_achieve_string(PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu, u32 job) {
  // Original instruction: li r5, 0
  ppc_state.gpr[5] = 0;

  // r4 points to the STRG header having its contents read out. r29 points to the CScriptAchievement
  const u32 token_ptr = mmu.Read<u32>(ppc_state.gpr[29] + 0x3c);
  const u64 resource_hash = mmu.Read<u64>(token_ptr + 0x10);

  for (auto const& patch : kPatchTable) {
    if (patch.hash == resource_hash) {
      const u32 strg_hdr = ppc_state.gpr[4];
      const u32 string_tbl = mmu.Read<u32>(strg_hdr + 0x1c);
      const u32 string_base = mmu.Read<u32>(string_tbl);

      // If I'm not an idiot then all of these strings should be the same length or shorter than the
      // originals
      u32 i;
      for (i = 0; i < static_cast<u32>(patch.patch_to.length()); i++) {
        mmu.Write<u8>(patch.patch_to[i], string_base + i);
      }
      mmu.Write<u8>(0, string_base + i);
      break;
    }
  }
}

} // namespace prime
