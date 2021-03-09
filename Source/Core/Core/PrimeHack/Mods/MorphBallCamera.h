#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/Transform.h"

namespace prime
{
class MorphBallCamera : public PrimeMod
{
public:
  void run_mod(Game game, Region region) override;
  void init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override;

private:
  float calculate_yaw_vel();
  void handle_camera_rotation_mp1(u32 camera_address, u32 player_addr, u16 transform_offset);
  void handle_camera_rotation(u32 camera_address, u32 player_addr, u16 transform_offset);
  void run_mod_mp1(Region region);
  void run_mod_mp2(Region region);
  void run_mod_mp3(Region region);
  void run_mod_mp1_gc(Region region);

  union
  {
    struct
    {
      u32 cplayer_address;
      u32 object_list_ptr_address;
      u32 camera_uid_address;
      u32 control_flag_address;

      u32 ballcam_ball_vel_flat_offset;
      u32 ballcam_ball_delta_offset;
      u32 ballcam_ball_delta_flat_offset;

    } mp1_static;

    struct
    {
      u32 cplayer_address;
      u32 state_mgr_address;
      u32 camera_uid_address;
    } mp1_gc_static;

    struct
    {
      u32 cplayer_ptr_address;
      u32 object_list_ptr_address;
      u32 camera_uid_address;
      u32 control_flag_address;
      u32 load_state_address;
    } mp2_static;

    struct
    {
      u32 state_mgr_ptr_address;
      u32 cam_uid_ptr_address;
    } mp3_static;
  };

  u64 old_matexclude_list;

  Transform camera_tf;
  Transform player_tf;
  bool had_control = true;

  // mp2 demands it
};

}  // namespace prime
