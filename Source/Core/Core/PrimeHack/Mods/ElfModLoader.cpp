#include "Core/PrimeHack/Mods/ElfModLoader.h"

#include "Common/SymbolDB.h"
#include "Core/Boot/ElfReader.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PrimeHack/ElfModLoaderInterface.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/System.h"

#include <variant>
#include <vector>

namespace prime {

using Symbol = Common::Symbol;

void ElfModLoader::run_mod(Game game, Region region) {
  if (!ModLoaderEnabled()) {
    return;
  }

  // ELF is mapped into an extended memory region
  // We would do this with instruction patches but
  // dolphin can't patch fast enough to catch the BATs
  // being assigned!
  update_bat_regs();

  // Block loading any mods until the callgate region is mapped in and cleanup is hooked
  if (!cg.valid) {
    cg.remap();
    return;
  }

  if (!cleanup_hooked) {
    init_cleanup(game, region);
    cleanup_hooked = true;
    return;
  }

  sync_mod_states();

  constexpr u8 kGuestStateActive = 0;
  constexpr u8 kGuestStateSignalOff = 1;
  constexpr u8 kGuestStateAckOff = 2;
  for (auto& mod : active_mods) {
    if (mod.state == State::INIT) {
      if (load_mod(mod, game, region)) {
        mod.state = State::ACTIVE;
        write8(kGuestStateActive, cg.state_table_base + mod.state_tbl_idx);
      }
    } else if (mod.state == State::REINIT) {
      set_code_group_state(mod.pack_name, ModState::ENABLED);
      mod.state = State::ACTIVE;
      write8(kGuestStateActive, cg.state_table_base + mod.state_tbl_idx);
    }

    switch (mod.state) {
      case State::ACTIVE:
        for (size_t i = 0; i < mod.linked.base->var_list.size(); i++) {
          write_cvar_val(mod.linked.base->var_list[i].value, mod.linked.var_addr_list[i]);
        }
        break;

      case State::UNLOAD_REQ:
        write8(kGuestStateSignalOff, cg.state_table_base + mod.state_tbl_idx);
        mod.state = State::UNLOAD_PEND;
        set_code_group_state(mod.pack_name, ModState::DISABLED);
        break;

      case State::UNLOAD_PEND:
        if (read8(cg.state_table_base + mod.state_tbl_idx) == kGuestStateAckOff) {
          mod.state = State::UNLOADED;
        }
        break;

      default:
        break;
    }
  }

  if (debug_output_addr != 0) {
    std::string debug_str = PowerPC::MMU::HostGetString(*active_guard, debug_output_addr);
  }
}

bool ElfModLoader::init_mod(Game game, Region region) {
  active_mods.clear();
  cg.reset();
  symbol_db.Clear();
  debug_output_addr = 0;
  next_load_slide = 0;
  cleanup_hooked = false;
  return true;
}

void ElfModLoader::on_reset() {
  active_mods.clear();
  cg.reset();
  symbol_db.Clear();
  debug_output_addr = 0;
  next_load_slide = 0;
  cleanup_hooked = false;
}

constexpr u32 kStateEntSize = 1;
constexpr u32 kCgEntSize = 12;
constexpr u32 kDpEntSize = 12;
constexpr u32 kTrEntSize = 8;
constexpr u32 kClEntSize = 8;

void ElfModLoader::CallgateData::remap() {
  constexpr u32 kSentinelSize = 4;
  // 11 Instructions
  constexpr u32 kDispatcherSize = 13 * 4;
  // 27 Instructions
  constexpr u32 kCleanupSize = 30 * 4;
  // BLR of Cleanup
  constexpr u32 kFiniNullsubOffset = 26 * 4;
  constexpr u32 kStateTableSize = kStateEntSize * kMaxMods;
  // Cleanup table takes an extra entry which is never touched
  constexpr u32 kClTableSize = kClEntSize * (kMaxMods + 1);
  constexpr u32 kMaxHooks = 1024;
  constexpr u32 kCgTableSize = kCgEntSize * kMaxHooks;
  constexpr u32 kDpTableSize = kDpEntSize * kMaxHooks;
  constexpr u32 kTrTableSize = kTrEntSize * kMaxHooks;

  constexpr u32 kRegionSz =
    kSentinelSize + kDispatcherSize + kCleanupSize + kStateTableSize + kClTableSize +
    kCgTableSize + kDpTableSize + kTrTableSize;

  sentinel_base = 0x82000000 - kRegionSz;
  dispatcher_base = sentinel_base + kSentinelSize;
  cleanup_base = dispatcher_base + kDispatcherSize;
  fini_nullsub_base = cleanup_base + kFiniNullsubOffset;
  state_table_base = cleanup_base + kCleanupSize;
  cl_table_base = state_table_base + kStateTableSize;
  cg_table_base = cl_table_base + kClTableSize;
  dp_table_base = cg_table_base + kCgTableSize;
  tr_table_base = dp_table_base + kDpTableSize;

  state_free_idx = 0;
  cl_free_idx = 0;
  cg_free_idx = 0;
  dp_free_idx = 0;
  tr_free_idx = 0;

  // # INPUT: r11 = dispatch table entry
  // _callgate_dispatch:
  //   lis r12, state_table_base@ha
  //   ori r12, r12, state_table_base@l
  //   lwz r0, 8(r11)
  //   lbzx r0, r12, r0
  //   cmpwi r0, 0
  //   beq _callgate_dispatch_runhook
  //
  // _callgate_dispatch_runorig:
  //   lwz r11, 0(r11)
  //   cmpwi r11, 0
  //   # Loader encodes a 0 for any callsite hooks of a bctrl
  //   beq _callgate_dispatch_forward
  //   b _callgate_dispatch_mtctr
  //
  // _callgate_dispatch_runhook:
  //   lwz r11, 4(r11)
  //
  // _callgate_dispatch_mtctr:
  //   mtctr r11
  // _callgate_dispatch_forward:
  //   bctr
  //
  // _cleanup_dispatch:
  //   stwu r1, -0x10(r1)
  //   mflr r0
  //   stw r0, 0x14(r1)
  //   stw r30, 0x8(r1)
  //   stw r31, 0xc(r1)
  //   lis r30, cleanup_table_base@ha
  //   ori r30, r30, cleanup_table_base@l
  //   lis r31, state_table_base@ha
  //   ori r31, r31, state_table_base@l
  // # Cleanup dispatcher loop: Table is always terminated by a null routine ptr, including the last
  // _cleanup_dispatch_loop:
  //   lwz r12, 0(r30)
  //   cmpwi r12, 0
  //   beq _cleanup_dispatch_end
  //   lwz r0, 4(r30)
  //   lbzx r0, r31, r0
  //   # state_table[mod_idx] == GuestState::RequestFini
  //   cmpwi r0, 1
  //   # Cleanup always hooks a useless function, thus it's safe to return
  //   beq _cleanup_dispatch_loop_iter
  //   mtctr r12
  //   bctrl
  // _cleanup_dispatch_loop_iter:
  //   addi r30, r30, 8
  //   addi r31, r31, 1
  //   b _cleanup_dispatch_loop
  // _cleanup_dispatch_end:
  //   lwz r0, 0x14(r1)
  //   lwz r30, 0x8(r1)
  //   lwz r31, 0xc(r1)
  //   addi r1, r1, 0x10
  //   mtlr r0
  //   blr

  const u8 stb0 = (state_table_base >> 24) & 0xff;
  const u8 stb1 = (state_table_base >> 16) & 0xff;
  const u8 stb2 = (state_table_base >> 8) & 0xff;
  const u8 stb3 = state_table_base & 0xff;
  const u8 clb0 = (cl_table_base >> 24) & 0xff;
  const u8 clb1 = (cl_table_base >> 16) & 0xff;
  const u8 clb2 = (cl_table_base >> 8) & 0xff;
  const u8 clb3 = cl_table_base & 0xff;
  const u8 dispatcher_stub[] = {
    0x3d, 0x80, stb0, stb1, 0x61, 0x8c, stb2, stb3, 0x80, 0x0b, 0x00, 0x08,
    0x7c, 0x0c, 0x00, 0xae, 0x2c, 0x00, 0x00, 0x00, 0x41, 0x82, 0x00, 0x14,
    0x81, 0x6b, 0x00, 0x00, 0x2c, 0x0b, 0x00, 0x00, 0x41, 0x82, 0x00, 0x10,
    0x48, 0x00, 0x00, 0x08, 0x81, 0x6b, 0x00, 0x04, 0x7d, 0x69, 0x03, 0xa6,
    0x4e, 0x80, 0x04, 0x20
  };
  const u8 cleanup_stub[] = {
    0x94, 0x21, 0xff, 0xf0, 0x7c, 0x08, 0x02, 0xa6, 0x90, 0x01, 0x00, 0x14,
    0x93, 0xc1, 0x00, 0x08, 0x93, 0xe1, 0x00, 0x0c, 0x3f, 0xc0, clb0, clb1,
    0x63, 0xde, clb2, clb3, 0x3f, 0xe0, stb0, stb1, 0x63, 0xff, stb2, stb3,
    0x81, 0x9e, 0x00, 0x00, 0x2c, 0x0c, 0x00, 0x00, 0x41, 0x82, 0x00, 0x34,
    0x80, 0x1e, 0x00, 0x04, 0x7c, 0x1f, 0x00, 0xae, 0x2c, 0x00, 0x00, 0x01,
    0x40, 0x82, 0x00, 0x18, 0x7d, 0x89, 0x03, 0xa6, 0x4e, 0x80, 0x04, 0x21,
    0x38, 0x00, 0x00, 0x02, 0x80, 0x7e, 0x00, 0x04, 0x7c, 0x1f, 0x19, 0xae,
    0x3b, 0xde, 0x00, 0x08, 0x3b, 0xff, 0x00, 0x01, 0x4b, 0xff, 0xff, 0xc8,
    0x80, 0x01, 0x00, 0x14, 0x83, 0xc1, 0x00, 0x08, 0x83, 0xe1, 0x00, 0x0c,
    0x38, 0x21, 0x00, 0x10, 0x7c, 0x08, 0x03, 0xa6, 0x4e, 0x80, 0x00, 0x20,
  };
  auto& memory = Core::System::GetInstance().GetMemory();

  static_assert(sizeof(dispatcher_stub) == kDispatcherSize);
  static_assert(sizeof(cleanup_stub) == kCleanupSize);
  constexpr u32 kSentinel = 0xea7f00d5;
  memory.Write_U32(kSentinel, sentinel_base);
  memory.CopyToEmu(dispatcher_base, dispatcher_stub, kDispatcherSize);
  memory.CopyToEmu(cleanup_base, cleanup_stub, kCleanupSize);
  memory.Memset(state_table_base, 0, kStateTableSize);
  memory.Memset(cl_table_base, 0, kClTableSize);
  memory.Memset(cg_table_base, 0, kCgTableSize);
  memory.Memset(dp_table_base, 0, kDpTableSize);
  memory.Memset(tr_table_base, 0, kTrTableSize);

  valid = (memory.Read_U32(sentinel_base) == kSentinel);
}

void ElfModLoader::sync_mod_states() {
  for (auto const& modpack : GetAvailableMods()) {
    if (!modpack.is_mod_enabled()) {
      continue;
    }
    bool has_entry = false;

    for (auto& active_mod : active_mods) {
      if (active_mod.pack_name == modpack.name) {
        has_entry = true;
        // If this mod was previously loaded in this session, we can turn it back on
        if (active_mod.state == State::UNLOADED) {
          active_mod.state = State::REINIT;
        }
      }
    }

    // If this is a new mod being added, set state to init and let load_mod figure it out
    if (!has_entry) {
      active_mods.emplace_back(LiveMod {
        .pack_name = modpack.name,
        .state = State::INIT,
      });
    }
  }

  // Run the other direction to determine what's disabled
  // Obviously this could be done more efficiently but chances are there's only ever 1 mod running
  // this bitch
  for (auto& active_mod : active_mods) {
    bool deactivated = false;
    for (auto const& modpack : GetAvailableMods()) {
      if (active_mod.pack_name == modpack.name &&
          active_mod.state == State::ACTIVE &&
          !modpack.is_mod_enabled()) {
        deactivated = true;
      }
    }

    if (deactivated) {
      active_mod.state = State::UNLOAD_REQ;
    }
  }
}

void ElfModLoader::update_bat_regs() {
  Core::System& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();
  auto& mmu = system.GetMMU();
  bool should_update = !(ppc_state.spr[SPR_DBAT2U] & 0x00000100 || ppc_state.spr[SPR_IBAT2U] & 0x00000100);
  if (should_update) {
    ppc_state.spr[SPR_DBAT2U] |= 0x00000100;
    ppc_state.spr[SPR_IBAT2U] |= 0x00000100;

    mmu.DBATUpdated();
    mmu.IBATUpdated();
  }
}

void ElfModLoader::init_cleanup(Game game, Region region) {
  ASSERT(cg.valid);

  u32 bl_hook_addr = 0;
  switch (game) {
    case Game::PRIME_1_GCN:
      if (region == Region::NTSC_U) {
        bl_hook_addr = 0x8000579c;
      } else if (region == Region::PAL) {
        bl_hook_addr = 0x80005890;
      }
      break;

    case Game::PRIME_1_GCN_R1:
      bl_hook_addr = 0x8000579c;
      break;

    case Game::PRIME_1_GCN_R2:
      bl_hook_addr = 0x8000579c;
      break;

    case Game::PRIME_1:
      if (region == Region::NTSC_U) {
        bl_hook_addr = 0x802ae560;
      } else if (region == Region::PAL) {
        bl_hook_addr = 0x802ae8b8;
      }
      break;

    case Game::PRIME_2_GCN:
      if (region == Region::NTSC_U) {
        bl_hook_addr = 0x80006370;
        // This hooks a useless function which returns 0, so to facilitate any FINI
        // to work, force a specific codepath to be followed (which was followed regardless)
        // Same principle applies to PAL, Trilogy, and all of MP3
        add_code_change(0x80006378, 0x48000014);
      } else if (region == Region::PAL) {
        bl_hook_addr = 0x80006374;
        add_code_change(0x8000637c, 0x48000014);
      }
      break;

    case Game::PRIME_2:
      if (region == Region::NTSC_U) {
        bl_hook_addr = 0x802a7e34;
        add_code_change(0x802a7e3c, 0x48000014);
      } else if (region == Region::PAL) {
        bl_hook_addr = 0x802aa390;
        add_code_change(0x802aa398, 0x48000014);
      }
      break;

    case Game::PRIME_3_STANDALONE:
      if (region == Region::NTSC_U) {
        bl_hook_addr = 0x802ee268;
        add_code_change(0x802ee270, 0x48000014);
      } else if (region == Region::PAL) {
        bl_hook_addr = 0x802ef9bc;
        add_code_change(0x802ef9c4, 0x48000014);
      }
      break;

    case Game::PRIME_3:
      if (region == Region::NTSC_U) {
        bl_hook_addr = 0x802ece98;
        add_code_change(0x802ecea0, 0x48000040);
      } else if (region == Region::PAL) {
        bl_hook_addr = 0x802ec63c;
        add_code_change(0x802ec644, 0x48000040);
      }
      break;

    default:
      break;
  }

  if (bl_hook_addr != 0) {
    add_code_change(bl_hook_addr, gen_branch_link(bl_hook_addr, cg.cleanup_base));
  }
}

bool ElfModLoader::load_mod(LiveMod& mod, Game game, Region region) {
  ModPack const* template_pack = GetPack(mod.pack_name);

  ElfMod const* template_mod = nullptr;
  for (auto const& game_mod : template_pack->supported_games) {
    if (game_mod.game == game && game_mod.region == region) {
      template_mod = &game_mod;
      break;
    }
  }

  if (template_mod == nullptr) {
    mod.state = State::NOT_FOUND;
    return false;
  }
  mod.linked.base = template_mod;
  mod.load_slide = next_load_slide;

  ElfReader elf_file(template_mod->elf_path);

  if (elf_file.IsValid()) {
    elf_file.LoadIntoMemory(Core::System::GetInstance(), mod.load_slide, false);
    elf_file.LoadSymbols(*active_guard, symbol_db, template_pack->name, mod.load_slide);
  } else {
    ERROR_LOG_FMT(PRIMEHACK, "Failed to load mod {} for game {} {}",
                  mod.pack_name, game_str(game), region_str(region));
    return false;
  }

  const u32 mod_idx = cg.state_free_idx++;

  mod.state_tbl_idx = mod_idx;
  mod.linked.var_addr_list.resize(template_mod->var_list.size());
  for (size_t i = 0; i < mod.linked.base->var_list.size(); i++) {
    CVar const& cvar = mod.linked.base->var_list[i];
    Symbol const* sym = symbol_db.GetSymbolFromName(cvar.name);
    if (sym == nullptr) {
      ERROR_LOG_FMT(PRIMEHACK, "Failed to locate cvar symbol {} in mod {} for game {} {}",
                    cvar.name, mod.pack_name, game_str(game), region_str(region));
      return false;
    }
    mod.linked.var_addr_list[i] = sym->address;
  }

  for (auto const& vt_hook : template_mod->vt_hooks) {
    Symbol const* sym = symbol_db.GetSymbolFromName(vt_hook.first);
    if (sym == nullptr) {
      ERROR_LOG_FMT(PRIMEHACK, "Failed to locate vt hook symbol {} in mod {} for game {} {}",
                    vt_hook.first, mod.pack_name, game_str(game), region_str(region));
      return false;
    }
    mod.linked.vt_hooks.emplace_back(sym->address, vt_hook.second);
    create_vthook_callgated(sym->address, vt_hook.second, mod_idx);
  }
  for (auto const& bl_hook : template_mod->bl_hooks) {
    Symbol const* sym = symbol_db.GetSymbolFromName(bl_hook.first);
    if (sym == nullptr) {
      ERROR_LOG_FMT(PRIMEHACK, "Failed to locate bl hook symbol {} in mod {} for game {} {}",
                    bl_hook.first, mod.pack_name, game_str(game), region_str(region));
      return false;
    }
    mod.linked.bl_hooks.emplace_back(sym->address, bl_hook.second);
    create_blhook_callgated(sym->address, bl_hook.second, mod_idx);
  }
  for (auto const& trampoline : template_mod->trampolines) {
    Symbol const* sym = symbol_db.GetSymbolFromName(trampoline.first);
    if (sym == nullptr) {
      ERROR_LOG_FMT(PRIMEHACK, "Failed to locate trampoline symbol {} in mod {} for game {} {}",
                    trampoline.first, mod.pack_name, game_str(game), region_str(region));
      return false;
    }
    mod.linked.trampolines.emplace_back(sym->address, trampoline.second);
    create_trampoline_callgated(sym->address, trampoline.second, mod_idx);
  }

  create_cleanup_entry(mod_idx);

  for (auto const& change : template_mod->changes) {
    add_code_change(change.address, change.var, template_mod->pack_name);
  }

  // Slide for next mod is below the current one, rounded to nearest 32 bit boundary
  next_load_slide += (elf_file.MappedSize() + 3) & ~0x3;
  return true;
}

u32 ElfModLoader::add_callgate_entry(u32 hook_target, u32 original_target, u32 mod_index) {
  // Fill in the dispatch table with original target and hook target
  const u32 dispatch_loc = kDpEntSize * cg.dp_free_idx + cg.dp_table_base;
  const u32 callgate_fn_table_loc = kCgEntSize * cg.cg_free_idx + cg.cg_table_base;
  write32(original_target, dispatch_loc + 0);
  write32(hook_target, dispatch_loc + 4);
  write32(mod_index, dispatch_loc + 8);

  // Set up the callgate (put the (original,hook) pair into r11, jump to dispatcher)
  const u32 r11_dispatch_lis = gen_lis(11, static_cast<u16>(dispatch_loc >> 16));
  const u32 r11_dispatch_ori = gen_ori(11, 11, static_cast<u16>(dispatch_loc));
  const u32 branch_to_cg_dispatch = gen_branch(callgate_fn_table_loc + 8, cg.dispatcher_base);
  write32(r11_dispatch_lis, callgate_fn_table_loc + 0);
  write32(r11_dispatch_ori, callgate_fn_table_loc + 4);
  write32(branch_to_cg_dispatch, callgate_fn_table_loc + 8);
  cg.dp_free_idx++;
  cg.cg_free_idx++;

  return callgate_fn_table_loc;
}

u32 ElfModLoader::add_trampoline_restore_entry(u32 func_start) {
  const u32 trampoline_restore_loc = kTrEntSize * cg.tr_free_idx + cg.tr_table_base;
  const u32 original_instruction = readi(func_start);
  const u32 branch_to_after_trampoline = gen_branch(trampoline_restore_loc + 4, func_start + 4);
  write32(original_instruction, trampoline_restore_loc + 0);
  write32(branch_to_after_trampoline, trampoline_restore_loc + 4);
  cg.tr_free_idx++;

  return trampoline_restore_loc;
}

void ElfModLoader::create_vthook_callgated(u32 hook_target, u32 vfte_addr, u32 mod_index) {
  const u32 callgate_fn_table_loc = add_callgate_entry(hook_target, read32(vfte_addr), mod_index);
  // VTable now redirects to our callgate func, leading to dispatcher
  add_code_change(vfte_addr, callgate_fn_table_loc);
}

void ElfModLoader::create_blhook_callgated(u32 hook_target, u32 bl_addr, u32 mod_index) {
  const u32 insn = readi(bl_addr);
  u32 bl_target;
  if (insn == kBctrlEncoding) {
    bl_target = 0;
  } else {
    bl_target = bl_addr + get_branch_offset(readi(bl_addr));
  }
  const u32 callgate_fn_table_loc = add_callgate_entry(hook_target, bl_target, mod_index);
  // BL will now redirect to the callgate func, leading to the dispatcher
  add_code_change(bl_addr, gen_branch_link(bl_addr, callgate_fn_table_loc));
}

void ElfModLoader::create_trampoline_callgated(u32 hook_target, u32 func_start, u32 mod_index) {
  const u32 trampoline_restore_loc = add_trampoline_restore_entry(func_start);
  const u32 callgate_fn_table_loc = add_callgate_entry(hook_target, trampoline_restore_loc, mod_index);
  add_code_change(func_start, gen_branch(func_start, callgate_fn_table_loc));
}

void ElfModLoader::create_cleanup_entry(u32 mod_index) {
  Symbol const* sym = symbol_db.GetSymbolFromName("mod_fini");

  // If no cleanup exists, run a nullsub in the callgate region
  const u32 fini_address = sym != nullptr ? sym->address : cg.fini_nullsub_base;

  const u32 cleanup_ent = kClEntSize * cg.cl_free_idx + cg.cl_table_base;
  write32(fini_address, cleanup_ent + 0);
  write32(mod_index, cleanup_ent + 4);
  cg.cl_free_idx++;
}

void ElfModLoader::write_cvar_val(CVarVal var, u32 addr) {
  if (uint8_t const* v8 = std::get_if<uint8_t>(&var); v8 != nullptr) {
    write8(*v8, addr);
  } else if (uint16_t const* v16 = std::get_if<uint16_t>(&var); v16 != nullptr) {
    write16(*v16, addr);
  } else if (uint32_t const* v32 = std::get_if<uint32_t>(&var); v32 != nullptr) {
    write32(*v32, addr);
  } else if (uint64_t const* v64 = std::get_if<uint64_t>(&var); v64 != nullptr) {
    write64(*v64, addr);
  } else if (float const* f32 = std::get_if<float>(&var); f32 != nullptr) {
    writef32(*f32, addr);
  } else if (double const* f64 = std::get_if<double>(&var); f64 != nullptr) {
    writef64(*f64, addr);
  } else if (bool const* b = std::get_if<bool>(&var); b != nullptr) {
    write8(*b, addr);
  }
}

} // namespace prime
