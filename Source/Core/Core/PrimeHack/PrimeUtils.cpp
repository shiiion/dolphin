#include "Core/PrimeHack/PrimeUtils.h"
#include "Common/BitUtils.h"

#include "Core/Core.h"

#include <Common/Timer.h>
#include <Common/BitUtils.h>
#include <cstdarg>


#include "Core/Core.h"

std::string info_str;


namespace prime
{
/*
* Prime one beam IDs: 0 = power, 1 = ice, 2 = wave, 3 = plasma
* Prime one visor IDs: 0 = combat, 1 = xray, 2 = scan, 3 = thermal
* Prime two beam IDs: 0 = power, 1 = dark, 2 = light, 3 = annihilator
* Prime two visor IDs: 0 = combat, 1 = echo, 2 = scan, 3 = dark
* ADDITIONAL INFO: Equipment have-status offsets:
* Beams can be ignored (for now) as the existing code handles that for us
* Prime one visor offsets: combat = 0x11, scan = 0x05, thermal = 0x09, xray = 0x0d
* Prime two visor offsets: combat = 0x08, scan = 0x09, dark = 0x0a, echo = 0x0b
*/

static float cursor_x = 0, cursor_y = 0;
static int current_beam = 0;
static int current_visor = 0;
static std::array<bool, 4> beam_owned = {false, false, false, false};
static std::array<bool, 4> visor_owned = {false, false, false, false};
static bool noclip_enabled = false;

std::array<std::array<CodeChange, static_cast<int>(Game::MAX_VAL) + 1>,
  static_cast<int>(Region::MAX_VAL) + 1> noclip_enable_codes;
std::array<std::array<CodeChange, static_cast<int>(Game::MAX_VAL) + 1>,
  static_cast<int>(Region::MAX_VAL) + 1> noclip_disable_codes;

static u32 noclip_msg_time;
static u32 invulnerability_msg_time;
static u32 cutscene_msg_time;
static u32 restore_dashing_msg_time;

u8 read8(u32 addr) {
  return PowerPC::HostRead_U8(addr);
}

u16 read16(u32 addr) {
  return PowerPC::HostRead_U16(addr);
}

u32 read32(u32 addr) {
  return PowerPC::HostRead_U32(addr);
}

u32 readi(u32 addr) {
  return PowerPC::HostRead_Instruction(addr);
}

u64 read64(u32 addr) {
  return PowerPC::HostRead_U64(addr);
}

float readf32(u32 addr) {
  return Common::BitCast<float>(read32(addr));
}

void write8(u8 var, u32 addr) {
  PowerPC::HostWrite_U8(var, addr);
}

void write16(u16 var, u32 addr) {
  PowerPC::HostWrite_U16(var, addr);
}

void write32(u32 var, u32 addr) {
  PowerPC::HostWrite_U32(var, addr);
}

void write64(u64 var, u32 addr) {
  PowerPC::HostWrite_U64(var, addr);
}

void writef32(float var, u32 addr) {
  PowerPC::HostWrite_F32(var, addr);
}

void set_beam_owned(int index, bool owned) {
  beam_owned[index] = owned;
}

void set_visor_owned(int index, bool owned) {
  visor_owned[index] = owned;
}

void set_cursor_pos(float x, float y) {
  cursor_x = x;
  cursor_y = y;
}

void write_invalidate(u32 address, u32 value) {
  write32(value, address);
  PowerPC::ScheduleInvalidateCacheThreadSafe(address);
}

std::tuple<int, int> get_visor_switch(std::array<std::tuple<int, int>, 4> const& visors, bool combat_visor) {
  static bool pressing_button = false;
  if (CheckVisorCtl(0)) {
    if (!combat_visor) {
      if (!pressing_button) {
        pressing_button = true;
        return visors[0];
      }
      else {
        return std::make_tuple(-1, 0);
      }
    }
  }
  if (CheckVisorCtl(1)) {
    if (!pressing_button) {
      pressing_button = true;
      return visors[1];
    }
  }
  else if (CheckVisorCtl(2)) {
    if (!pressing_button) {
      pressing_button = true;
      return visors[2];
    }
  }
  else if (CheckVisorCtl(3)) {
    if (!pressing_button) {
      pressing_button = true;
      return visors[3];
    }
  }
  else if (CheckVisorScrollCtl(true)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (visor_owned[current_visor = (current_visor + 1) % 4]) break;
      }
      return visors[current_visor];
    }
  }
  else if (CheckVisorScrollCtl(false)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (visor_owned[current_visor = (current_visor + 3) % 4]) break;
      }
      return visors[current_visor];
    }
  }
  else {
    pressing_button = false;
  }
  return std::make_tuple(-1, 0);
}

int get_beam_switch(std::array<int, 4> const& beams) {
  static bool pressing_button = false;
  if (CheckBeamCtl(0)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[0];
    }
  }
  else if (CheckBeamCtl(1)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[1];
    }
  }
  else if (CheckBeamCtl(2)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[2];
    }
  }
  else if (CheckBeamCtl(3)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[3];
    }
  }
  else if (CheckBeamScrollCtl(true)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (beam_owned[current_beam = (current_beam + 1) % 4]) break;
      }
      return beams[current_beam];
    }
  }
  else if (CheckBeamScrollCtl(false)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (beam_owned[current_beam = (current_beam + 3) % 4]) break;
      }
      return beams[current_beam];
    }
  }
  else {
    pressing_button = false;
  }
  return -1;
}


// Added by LT_Schmiddy: PrimeHack regularly needs to find the address of the player and camera, but the implementation is typed out over and over.
// So, here's my attempt for a DRY alternative:

u32 get_player_address(Game game, Region region)
{
  u32 cplayer_ptr_address;
  u32 state_mgr_ptr_address;
  switch (game)
  {
  case Game::PRIME_1:
    if (region == Region::NTSC)
    {
      return 0x804d3c20;
    }
    else if (region == Region::PAL)
    {
      return 0x804d7b60;

    }
    break;
  case Game::PRIME_1_GCN:
    if (region == Region::NTSC)
    {
      return 0x8046b97c;
    }
    else if (region == Region::PAL)
    {
      return 0x803f38a4;

    }

    break;
  case Game::PRIME_2:

    cplayer_ptr_address = 0;
    if (region == Region::NTSC)
    {
      cplayer_ptr_address = 0x804e87dc;
    }
    else if (region == Region::PAL)
    {
      cplayer_ptr_address = 0x804efc2c;
    }

    return read32(cplayer_ptr_address);
    //break;
  case Game::PRIME_3:
    state_mgr_ptr_address = 0;
    if (region == Region::NTSC)
    {
      state_mgr_ptr_address = 0x805c6c40;
    }
    else if (region == Region::PAL)
    {
      //state_mgr_ptr_address = 0;
      return 0;
    }

    return read32(read32(state_mgr_ptr_address + 0x28) + 0x2184);
    //break;
  default:
    return 0;
    //break;
  }
  return 0;
}

u32 get_player_address()
{
  HackManager* hm = GetHackManager();
  return get_player_address(hm->get_active_game(), hm->get_active_region());

}

// Same for cameras:
active_cam_info get_active_cam(Game game, Region region)
{
  if (game == Game::PRIME_1)
  {
    u32 object_list_ptr_address = 0;
    u32 camera_uid_address = 0;

    if (region == Region::NTSC)
    {
      object_list_ptr_address = 0x804bfc30;
      camera_uid_address = 0x804c4a08;
    }
    else if (region == Region::PAL)
    {
      object_list_ptr_address = 0x804c3b70;
      camera_uid_address = 0x804c8948;
    }

    const u32 camera_ptr = read32(object_list_ptr_address);
    const u32 camera_offset = (((read32(camera_uid_address) + 10) >> 16) & 0x3ff) << 3;
    return { read32(camera_ptr + camera_offset + 4), camera_offset };  // camera_ptr,
    // break;
  }
  else if (game == Game::PRIME_1_GCN)
  {
    u32 camera_uid_address = 0;
    u32 state_mgr_address = 0;

    if (region == Region::NTSC)
    {
      state_mgr_address = 0x8045a1a8;
      camera_uid_address = 0x8045c5b4;
    }
    else if (region == Region::PAL)
    {
      state_mgr_address = 0x803e2088;
      camera_uid_address = 0x803e44dc;
    }

    const u16 camera_uid = read16(camera_uid_address);
    if (camera_uid == -1)
    {
      return { 0, 0 };
    }
    u32 camera_offset = (camera_uid & 0x3ff) << 3;
    return { read32(read32(state_mgr_address + 0x810) + 4 + camera_offset), camera_offset };
  }
  else if (game == Game::PRIME_2)
  {
    u32 object_list_ptr_address = 0;
    u32 camera_uid_address = 0;

    if (region == Region::NTSC)
    {
      object_list_ptr_address = 0x804e7af8;
      camera_uid_address = 0x804eb9b0;
    }
    else if (region == Region::PAL)
    {
      object_list_ptr_address = 0x804eef48;
      camera_uid_address = 0x804f2f50;
    }

    u32 object_list_address = read32(object_list_ptr_address);
    if (!mem_check(object_list_address))
    {
      return { 0, 0 };
    }
    const u16 camera_uid = read16(camera_uid_address);
    if (camera_uid == -1)
    {
      return { 0, 0 };
    }
    const u32 camera_offset = (camera_uid & 0x3ff) << 3;
    return { read32(object_list_address + 4 + camera_offset), camera_offset };
  }
  else if (game == Game::PRIME_3)
  {
    u32 state_mgr_ptr_address = 0;
    u32 cam_uid_ptr_address = 0;
    if (region == Region::NTSC)
    {
      state_mgr_ptr_address = 0x805c6c40;
      cam_uid_ptr_address = 0x805c6c78;
    }
    else if (region == Region::PAL)
    {
    }

    u32 object_list_address = read32(read32(state_mgr_ptr_address + 0x28) + 0x1010);
    if (!mem_check(object_list_address))
    {
      return { 0, 0 };
    }
    const u16 camera_id = read16(read32(read32(cam_uid_ptr_address) + 0xc) + 0x16);
    if (camera_id == 0xffff)
    {
      return { 0, 0 };
    }
    return { read32(object_list_address + 4 + (8 * camera_id)), camera_id };
  }

  return { 0, 0 };

}

active_cam_info get_active_cam()
{
  HackManager* hm = GetHackManager();
  return get_active_cam(hm->get_active_game(), hm->get_active_region());
}



std::string as_hex_string(u32 val)
{
  char retVal[20];
  sprintf(retVal, "%x", val);
  return std::string(retVal);
}

// Sometimes, printing to dolphin's own logging system to be a bit more useful than spamming the on-screen popups... Sometimes:
void print_to_log(const char* message, const char* file, int line, Common::Log::LOG_LEVELS level, Common::Log::LOG_TYPE type)
{
  Common::Log::LogManager* log_ptr = Common::Log::LogManager::GetInstance();
  if (log_ptr == NULL)
  {
    Core::DisplayMessage("Log Manager is NULL!!!", 1000);
    return;
  }

  log_ptr->Log(level, type, file, line, message, {});
}

void print_to_log(const char* message, const char* file, int line)
{
  print_to_log(message, file, line, Common::Log::LINFO, Common::Log::PRIMEHACK);
}

void print_to_log(const char* message)
{
  print_to_log(message, "<unnamed>", 0);
}



void print_to_log(std::string message, const char* file, int line, Common::Log::LOG_LEVELS level, Common::Log::LOG_TYPE type)
{
  print_to_log(message.c_str(), file, line, level, type);
}

void print_to_log(std::string message, const char* file, int line)
{
  print_to_log(message.c_str(), file, line);
}


void print_to_log(std::string message)
{
  print_to_log(message.c_str());
}


std::stringstream ss;
void DevInfo(const char* name, const char* format, ...)
{
  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);

  std::vector<char> buf(1+std::vsnprintf(nullptr, 0, format, args1));
  std::vsnprintf(buf.data(), buf.size(), format, args2);

  ss << name << ": " << buf.data() << std::endl;
  va_end(args2);
}

void DevInfoMatrix(const char* name, const Transform& t)
{
  const char* format =
    "\n   %.3f    %.3f    %.3f    %.3f"
    "\n   %.3f    %.3f    %.3f    %.3f"
    "\n   %.3f    %.3f    %.3f    %.3f";

  u32 bufsize = snprintf(NULL, 0, format, 
    t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3], 
    t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3], 
    t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);

  std::vector<char> buf;
  buf.resize(bufsize + 1);

  snprintf(buf.data(), bufsize + 1, format,
    t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3], 
    t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3], 
    t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);

  ss << name << ":" << buf.data() << std::endl;
}

std::string GetDevInfo()
{
  std::string result = ss.str();

  return result;
}

// Common::Timer::GetTimeMs()
std::tuple<u32, u32, u32, u32> GetCheatsTime()
{
  return std::make_tuple(noclip_msg_time, invulnerability_msg_time, cutscene_msg_time, restore_dashing_msg_time);
}

void AddCheatsTime(int index, u32 time)
{
  switch (index)
  {
  case 0:
    noclip_msg_time = Common::Timer::GetTimeMs() + 3000;
    break;
  case 1:
    invulnerability_msg_time = Common::Timer::GetTimeMs() + 3000;
    break;
  case 2:
    cutscene_msg_time = Common::Timer::GetTimeMs() + 3000;
    break;
  case 3:
    restore_dashing_msg_time = Common::Timer::GetTimeMs() + 3000;
  }
}

void ClrDevInfo()
{
  ss = std::stringstream();
}

bool mem_check(u32 address) {
  return (address >= 0x80000000) && (address < 0x81800000);
}

float get_aspect_ratio() {
  const unsigned int scale = g_renderer->GetEFBScale();
  float sW = scale * EFB_WIDTH;
  float sH = scale * EFB_HEIGHT;
  return sW / sH;
}

void handle_cursor(u32 x_address, u32 y_address, float right_bound, float bottom_bound) {
  int dx = GetHorizontalAxis(), dy = GetVerticalAxis();

  float aspect_ratio = get_aspect_ratio();
  if (std::isnan(aspect_ratio)) {
    return;
  }

  const float cursor_sensitivity_conv = prime::GetCursorSensitivity() / 10000.f;
  cursor_x += static_cast<float>(dx) * cursor_sensitivity_conv;
  cursor_y += static_cast<float>(dy) * cursor_sensitivity_conv * aspect_ratio;

  cursor_x = std::clamp(cursor_x, -1.f, right_bound);
  cursor_y = std::clamp(cursor_y, -1.f, bottom_bound);

  writef32(cursor_x, x_address);
  writef32(cursor_y, y_address);
}
}  // namespace prime
