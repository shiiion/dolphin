#pragma once

#include "Common/CommonTypes.h"
#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

void AllocSwitchGame(Game, Region);
u32 GuestAlloc(u32 size);
u32 GuestAllocAligned(u32 size, u8 bits);

}
