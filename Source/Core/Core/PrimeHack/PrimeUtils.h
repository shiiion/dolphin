#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/Transform.h"

#include <cmath>
#include <sstream>

#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/HackConfig.h"
#include "InputCommon/GenericMouse.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoCommon.h"

extern std::string info_str;

namespace prime {
constexpr u32 kBranchOffsetMask = 0x3fffffc;
constexpr u32 gen_branch(const u32 src, const u32 dst) { return 0x48000000 | (((dst) - (src)) & kBranchOffsetMask); }
constexpr u32 gen_branch_link(const u32 src, const u32 dst) { return gen_branch(src, dst) | (u32{1}); }
constexpr u32 gen_lis(const u32 dst_gpr, const u16 val) {
  return (0b001111 << 26) | (dst_gpr << 21) | (0 << 16) | val;
}
constexpr u32 gen_ori(const u32 dst_gpr, const u32 src_gpr, const u16 val) {
  return (0b011000 << 26) | (dst_gpr << 21) | (src_gpr << 16) | val;
}
constexpr u32 get_branch_offset(u32 instr) {
  u32 offset_masked = instr & kBranchOffsetMask;
  if (offset_masked & 0x2000000) {
    offset_masked |= 0xfc000000;
  }
  return offset_masked;
}
constexpr u32 gen_vmcall(const u32 call_idx, const u32 param) {
  return (0b010011 << 26) | ((call_idx & 0x3ff) << 16) | ((param & 0x1f) << 11) | ((0b0000110011) << 1) | (0b0);
}

constexpr float kTurnrateRatio = 0.00498665500569808449206349206349f;
constexpr float kPi = 3.141592654f;

int get_beam_switch(std::array<int, 4> const& beams);
std::tuple<int, int> get_visor_switch(std::array<std::tuple<int, int>, 4> const& visors, bool combat_visor);

void handle_cursor(Core::CPUThreadGuard const& guard, u32 x_address, u32 y_address, Region region);
void handle_reticle(Core::CPUThreadGuard const& guard, u32 x_address, u32 y_address, Region region, float fov);

bool mem_check(u32 address);

void set_beam_owned(int index, bool owned);
void set_visor_owned(int index, bool owned);
void set_cursor_pos(float x, float y);

void swap_alt_profiles(u32 ball_state, u32 transition_state, u32 screw_state);

// Borrowed from DolphinQt MathUtil.h
template <typename T, typename F>
constexpr auto Lerp(const T& x, const T& y, const F& a) -> decltype(x + (y - x) * a)
{
  return x + (y - x) * a;
}

template <typename T, typename F>
constexpr auto AntiLerp(const T& x, const T& y, const F& a) -> decltype((a - x) / (y - x))
{
  return (a - x) / (y - x);
}

}  // namespace prime
