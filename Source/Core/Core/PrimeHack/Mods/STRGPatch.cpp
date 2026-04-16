#include "Core/PrimeHack/Mods/STRGPatch.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/System.h"

namespace prime {
namespace {
enum PatchTable : u32 {
  kMenuNTSC,
  kMenuPAL,
  kMP3StandaloneNTSC,
  kMP3NTSC,
  kMP3PAL,
  kPatchTableSize,
};
constexpr u32 kPatchTargetTableStarts[kPatchTableSize] = {
  0x80626100, // kMenuNTSC
  0x8062b800, // kMenuPAL
  0x80684800, // kMP3StandaloneNTSC
  0x80676c00, // kMP3NTSC
  0x8067a400, // kMP3PAL
};

std::string readin_str(PowerPC::MMU& mmu, u32 str_ptr) {
  std::ostringstream key_readin;

  for (char c = mmu.Read_U8(str_ptr); c; c = mmu.Read_U8(++str_ptr)) {
    key_readin << c;
  }
  return key_readin.str();
}

u32 bsearch_strg_table(PowerPC::MMU& mmu, std::string const& key, u32 strg_header) {
  u32 bsearch_left = mmu.Read_U32(strg_header + 0x14);
  int dist = mmu.Read_U32(strg_header + 0x8);
  while (dist > 0) {
    int midpoint_offset = (dist * 4) & ~0x7;
    int half_dist = dist >> 1;
    std::string test_key = readin_str(mmu, mmu.Read_U32(bsearch_left + midpoint_offset));
    if (test_key.compare(key) < 0) {
      dist -= (1 + half_dist);
      bsearch_left += midpoint_offset + 8;
    } else {
      dist = half_dist;
    }
  }
  return bsearch_left;
}

void patch_strg_entry_mp3_and_menu(PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu, u32 vers) {
  static_cast<STRGPatch*>(GetHackManager()->get_mod("strg_patch"))->patch_strg_entry_vmc_common(
    ppc_state, mmu, kPatchTargetTableStarts[vers], ppc_state.gpr[3], ppc_state.gpr[4]);
  ppc_state.gpr[0] = ppc_state.spr[SPR_LR];
}
}

void STRGPatch::patch_strg_entry_vmc_common(PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu,
    u32 patched_table_addr, u32 strg_header, u32 key_ptr) {
  std::string key = readin_str(mmu, key_ptr);

  auto replacement = replace_tbl.find(key);
  if (replacement != replace_tbl.end()) {
    u32 bsearch_result = bsearch_strg_table(mmu, key, strg_header);
    std::string found_key = readin_str(mmu, mmu.Read_U32(bsearch_result));
    if (found_key == key) {
      u32 strg_val_index = mmu.Read_U32(bsearch_result + 4);
      u32 strg_val_table = mmu.Read_U32(strg_header + 0x1c);
      mmu.Write_U32(replacement->second.first + patched_table_addr, strg_val_table + 4 * strg_val_index);
    }
  }
}

void STRGPatch::run_mod(Game game, Region region) {
  switch (game) {
  case Game::MENU:
    if (region == Region::NTSC_U) {
      run_mod_common(kPatchTargetTableStarts[kMenuNTSC]);
    } else if (region == Region::PAL) {
      run_mod_common(kPatchTargetTableStarts[kMenuPAL]);
    }
    break;

  case Game::PRIME_1:
  case Game::PRIME_1_GCN:
  case Game::PRIME_1_GCN_R1:
  case Game::PRIME_1_GCN_R2:
  case Game::PRIME_2:
  case Game::PRIME_2_GCN:
    break;

  case Game::PRIME_3_STANDALONE:
    if (region == Region::NTSC_U) {
      run_mod_common(kPatchTargetTableStarts[kMP3StandaloneNTSC]);
    }
    break;

  case Game::PRIME_3:
    if (region == Region::NTSC_U) {
      run_mod_common(kPatchTargetTableStarts[kMP3NTSC]);
    } else if (region == Region::PAL) {
      run_mod_common(kPatchTargetTableStarts[kMP3PAL]);
    }
    break;

  default:
    break;
  }
}

bool STRGPatch::init_mod(Game game, Region region) {
  clear_table();

  switch (game) {
  case Game::MENU: {
    add_table_entry("NunchukRequired", GetMotd());
    add_table_entry("DifficultyMenu_Easiest",
      "&link=[starteasiest]?typewrite=reverse;&wholepane;&rollover=menu2_hl;[ Easy ]&endlink;");
    add_table_entry("DifficultyMenu_Medium",
      "&link=[startmedium]?typewrite=reverse;&wholepane;&rollover=menu3_hl;[ Normal ]&endlink;");
    add_table_entry("DifficultyMenu_Hardest",
      "&if=HypermodeUnlocked;&link=[starthardest]?typewrite=reverse;&wholepane;&rollover=menu4_hl;[ Hard ]&endlink;&endif;");
    int vmc_id = Core::System::GetInstance().GetPowerPC().RegisterVmcall(patch_strg_entry_mp3_and_menu);
    if (region == Region::NTSC_U) {
      add_code_change(0x8037e510, gen_vmcall(vmc_id, kMenuNTSC));
    } else if (region == Region::PAL) {
      add_code_change(0x8037e15c, gen_vmcall(vmc_id, kMenuPAL));
    }
    break;
  }
  case Game::PRIME_1:
  case Game::PRIME_1_GCN:
  case Game::PRIME_1_GCN_R1:
  case Game::PRIME_1_GCN_R2:
  case Game::PRIME_2:
  case Game::PRIME_2_GCN:
    break;
  case Game::PRIME_3_STANDALONE: {
    add_table_entry("ShakeOffGandrayda",
                    "&just=center;Mash Jump [&image=0x5FC17B1F30BAA7AE;] to shake off Gandrayda!");
    int vmc_id = Core::System::GetInstance().GetPowerPC().RegisterVmcall(patch_strg_entry_mp3_and_menu);
    if (region == Region::NTSC_U) {
      add_code_change(0x803cdd64, gen_vmcall(vmc_id, kMP3StandaloneNTSC));
    }
    break;
  }
  case Game::PRIME_3: {
    add_table_entry("ShakeOffGandrayda",
                    "&just=center;Mash Jump [&image=0x5FC17B1F30BAA7AE;] to shake off Gandrayda!");
    int vmc_id = Core::System::GetInstance().GetPowerPC().RegisterVmcall(patch_strg_entry_mp3_and_menu);
    if (region == Region::NTSC_U) {
      add_code_change(0x803cc3f4, gen_vmcall(vmc_id, kMP3NTSC));
    } else if (region == Region::PAL) {
      add_code_change(0x803cbb10, gen_vmcall(vmc_id, kMP3PAL));
    }
    break;
  }
  default:
    break;
  }
  return true;
}

void STRGPatch::add_table_entry(std::string key, std::string val) {
  if (key.empty()) {
    return;
  }
  if (replace_tbl.count(key) > 0) {
    replace_tbl[key].second = val;
    recompute_tbl_off();
  } else {
    replace_tbl[key] = std::make_pair(current_tbl_off, val);
    current_tbl_off += static_cast<u32>(val.length()) + 1;
  }
}

void STRGPatch::recompute_tbl_off() {
  current_tbl_off = 0;
  for (auto& [k, vp] : replace_tbl) {
    vp.first = current_tbl_off;
    current_tbl_off += static_cast<u32>(vp.second.length()) + 1;
  }
}

void STRGPatch::clear_table() {
  current_tbl_off = 0;
  replace_tbl.clear();
}

void STRGPatch::run_mod_common(u32 tbl_address) {
  for (auto const& [_, repl_pair] : replace_tbl) {
    for (u32 i = 0; i < static_cast<u32>(repl_pair.second.length()); i++) {
      write8(repl_pair.second[i], tbl_address + repl_pair.first + i);
    }
    write8(0, tbl_address + repl_pair.first + static_cast<u32>(repl_pair.second.length()));
  }
}

}
