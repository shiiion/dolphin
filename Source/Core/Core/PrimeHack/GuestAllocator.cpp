#include "Core/PrimeHack/GuestAllocator.h"

#include <vector>

namespace prime {
namespace {

class AllocZone {
public:
  constexpr AllocZone(u32 base, u32 bounds)
    : _base(base), _bounds(bounds), _free_base(base) {}
  constexpr u32 lo() const { return _base; }
  constexpr u32 hi() const { return _base + _bounds; }
  constexpr u32 rem() const { return hi() - _free_base; }
  constexpr u32 rem_aligned(u8 bits) const {
    const u32 bits_align = (1 << bits) - 1;
    const u32 bits_lomask = ~bits_align;
    const u32 aligned_free_base = (_free_base + bits_align) & bits_lomask;
    return hi() > aligned_free_base ? hi() - aligned_free_base : 0;
  }
  constexpr u32 alloc(u32 size) {
    const u32 ret = _free_base;
    _free_base += size;
    return ret;
  }
  constexpr u32 alloc_align(u32 size, u8 bits) {
    const u32 bits_align = (1 << bits) - 1;
    const u32 bits_lomask = ~bits_align;
    const u32 ret = (_free_base + bits_align) & bits_lomask;
    _free_base = ret + size;
    return ret;
  }

private:
  u32 _base;
  u32 _bounds;
  u32 _free_base;
};

std::vector<AllocZone> avail_zones;

} // namespace

void AllocSwitchGame(Game game, Region region) {
  avail_zones.clear();

  switch (game) {
    case Game::MENU:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x80626100, 0xe000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x8062b800, 0xe000);
      }
      break;
    case Game::PRIME_1_GCN:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x805afc00, 0xe000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x80471c00, 0xe000);
      }
      break;

    case Game::PRIME_1_GCN_R1:
      avail_zones.emplace_back(0x805afc00, 0xe000);
      break;

    case Game::PRIME_1_GCN_R2:
      avail_zones.emplace_back(0x805b0c00, 0xe000);
      break;

    case Game::PRIME_1:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x805c9000, 0xe000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x805cd400, 0xe000);
      }
      break;

    case Game::PRIME_2_GCN:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x80420000, 0xe000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x80421000, 0xe000);
      }
      break;

    case Game::PRIME_2:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x805d2000, 0xe000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x805da000, 0xe000);
      }
      break;

    case Game::PRIME_3_STANDALONE:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x80684800, 0xd000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x80687000, 0xd000);
      }
      break;

    case Game::PRIME_3:
      if (region == Region::NTSC_U) {
        avail_zones.emplace_back(0x80676c00, 0xe000);
      } else if (region == Region::PAL) {
        avail_zones.emplace_back(0x8067a400, 0xe000);
      }
      break;

    default:
      break;
  }
}

// Dumb-as-bricks allocator, but effectively what we're doing before
// This just simplifies tracking all of the random bits of free space
u32 GuestAlloc(u32 size) {
  for (auto& zone : avail_zones) {
    if (zone.rem() >= size) {
      return zone.alloc(size);
    }
  }

  return 0;
}

u32 GuestAllocAligned(u32 size, u8 bits) {
  for (auto& zone : avail_zones) {
    if (zone.rem_aligned(bits) >= size) {
      return zone.alloc_align(size, bits);
    }
  }

  return 0;
}

} // namespace prime
