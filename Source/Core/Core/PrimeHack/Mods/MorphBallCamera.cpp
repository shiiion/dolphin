#include "Core/PrimeHack/Mods/MorphBallCamera.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/Transform.h"

#include "Common/BitUtils.h"

#include <cmath>
#include <math.h>

#define PI 3.141592654f


//using namespace Common::Log;
namespace prime
{

  u32 last_camera_address;

  struct ang_vector_2d
  {

    float dist;
    float rot;

    ang_vector_2d()
    {
      dist = 0;
      rot = 0;
    }

    ang_vector_2d(float p_x, float p_y) {
      set(p_x, p_y);
    }

    void set(float p_x, float p_y)
    {
      dist = sqrt(pow(abs(p_x), 2) + pow(abs(p_y), 2));
      rot = atan2(p_y, p_x);
    }

    // X & Y getters:
    float x() {
      return -(cos(rot) * dist);
    }

    float y() {
      return -(sin(rot) * dist);
    }

    // rot setter:
    void set_rot(float p_rot) {
      rot = p_rot;

      if (rot > 2.0f * PI)
      {
        rot -= 2.0f * PI;
      }

      if (rot < 0)
      {
        rot += 2.0f * PI;
      }

    }

    // X & Y setters:
    void x(float p_x) {
      set(p_x, y());
    }

    void y(float p_y) {
      set(x(), p_y);
    }
  };

  void MorphBallCamera::run_mod(Game game, Region region)
  {
    //Core::DisplayMessage("MorphBall Camera Loaded", 100);
    switch (game)
    {
    case Game::PRIME_1:
      run_mod_mp1(region);
      break;
    case Game::PRIME_2:
      run_mod_mp2(region);
      break;
    case Game::PRIME_3:
      run_mod_mp3(region);
      break;
    case Game::PRIME_1_GCN:
      run_mod_mp1_gc(region);
      break;
    default:
      break;
    }

  }

  float MorphBallCamera::calculate_yaw_vel()
  {
    return GetHorizontalAxis() * GetSensitivity() * (InvertedX() ? 1.f : -1.f);
  }

  

  void MorphBallCamera::handle_camera_rotation_mp1(u32 camera_address, u32 player_addr, u16 transform_offset)
  //void handle_camera_rotation(u32 player_tf_addr, u32 camera_tf_addr)
  {
    float start_x = player_tf.m[0][3] - camera_tf.m[0][3];
    float start_y = player_tf.m[1][3] - camera_tf.m[1][3];


    ang_vector_2d cam_pos_vector(start_x, start_y);

    cam_pos_vector.set_rot(cam_pos_vector.rot + (calculate_yaw_vel() / 5000.0f));
    //cam_pos_vector.set_rot(cam_pos_vector.rot + 0.1f);

    //camera_tf.build_rotation_loc_safe(cam_pos_vector.rot - PI / 2.0f, 0, 0);
    //print_to_log("Camera Dist: " + std::to_string(cam_pos_vector.dist));
    camera_tf.m[0][3] = cam_pos_vector.x() + player_tf.m[0][3];
    camera_tf.m[1][3] = cam_pos_vector.y() + player_tf.m[1][3];

    camera_tf.write_to(camera_address + transform_offset);



  }
  
  void MorphBallCamera::handle_camera_rotation(u32 camera_address, u32 player_addr, u16 transform_offset)
  //void handle_camera_rotation(u32 player_tf_addr, u32 camera_tf_addr)
  {
    float start_x = player_tf.m[0][3] - camera_tf.m[0][3];
    float start_y = player_tf.m[1][3] - camera_tf.m[1][3];


    ang_vector_2d cam_pos_vector(start_x, start_y);

    cam_pos_vector.set_rot(cam_pos_vector.rot + (calculate_yaw_vel() / 5000.0f));

    //print_to_log("Camera Dist: " + std::to_string(cam_pos_vector.dist));
    camera_tf.m[0][3] = cam_pos_vector.x() + player_tf.m[0][3];
    camera_tf.m[1][3] = cam_pos_vector.y() + player_tf.m[1][3];

    camera_tf.write_to(camera_address + transform_offset);
  }


  void MorphBallCamera::run_mod_mp1(Region region)
  {
    //const u32 camera_ptr = read32(mp1_static.object_list_ptr_address);
    //const u32 camera_offset = (((read32(mp1_static.camera_uid_address)) >> 16) & 0x3ff) << 3;
    //const u32 camera_address = read32(camera_ptr + camera_offset + 4);



    active_cam_info acm = get_active_cam();

    if (acm.offset == 32)
    {
      if (acm.address != last_camera_address)
      {
        print_to_log("BALL CAM - New Address: " + as_hex_string(acm.address));
        last_camera_address = acm.address;
      }

      u32 cam_state_addr = 0x809d42d0;
      if (read32(cam_state_addr) != 0 && read32(cam_state_addr) != 5) {
        return;
      }

      writef32(100.0f, acm.address + 0x1b0);

      //print_to_log("Camera Address: 0x" + as_hex_string(camera_address));
      //const u32 camera_tf_addr = camera_address + 0x2c;

      camera_tf.read_from(acm.address + 0x2c);
      player_tf.read_from(mp1_static.cplayer_address + 0x2c);

      if (abs(calculate_yaw_vel()) > 0.0f)
      {
        handle_camera_rotation_mp1(acm.address, mp1_static.cplayer_address, 0x2c);
        //write32(5, cam_state_addr);
      }
      //else {
        //write32(0, cam_state_addr);
      //}


       //vec3 spline_vector = camera_tf.loc();
       //spline_vector.write_to(acm.address + 0x33c);
       //camera_tf.loc().write_to(acm.address + 0x33c);
       //camera_tf.loc().write_to(acm.address + 0x330);

    }


  }
  void MorphBallCamera::run_mod_mp2(Region region)
  {
    active_cam_info acm = get_active_cam();
    u32 player_addr = get_player_address();

    if (acm.address != last_camera_address)
    {
      print_to_log("BALL CAM - New Address: " + as_hex_string(acm.address));
      print_to_log("Offset: " + as_hex_string(acm.offset));
      last_camera_address = acm.address;
    }

    if (acm.offset == 0x40)
    {
      //writef32(100.0f, acm.address + 0x1b0);

      // print_to_log("Camera Address: 0x" + as_hex_string(camera_address));
      // const u32 camera_tf_addr = camera_address + 0x2c;

      //current rotation speed offset: 0x210
      //target rotation speed offset: 0x214
      writef32(100.0f, acm.address + 0x214);

      camera_tf.read_from(acm.address + 0x20);
      player_tf.read_from(player_addr + 0x20);

      if (abs(calculate_yaw_vel()) > 0.0f)
      {
        handle_camera_rotation(acm.address, player_addr, 0x20);


        vec3 cam_m_vector(acm.address + 0x50);
        cam_m_vector = camera_tf.loc();
        cam_m_vector.write_to(acm.address + 0x50);

      }
    }
  }
  void MorphBallCamera::run_mod_mp3(Region region)
  {
    active_cam_info acm = get_active_cam();
    u32 player_addr = get_player_address();

    if (acm.address != last_camera_address)
    {
      print_to_log("BALL CAM - New Address: " + as_hex_string(acm.address));
      print_to_log("Offset: " + as_hex_string(acm.offset));
      last_camera_address = acm.address;
    }

    if (acm.offset == 0x04)
    {


      //writef32(100.0f, acm.address + 0x1b0);

      // print_to_log("Camera Address: 0x" + as_hex_string(camera_address));
      // const u32 camera_tf_addr = camera_address + 0x2c;

      camera_tf.read_from(acm.address + 0x3c);
      player_tf.read_from(player_addr + 0x3c);

      if (abs(calculate_yaw_vel()) > 0.0f)
      {
        handle_camera_rotation(acm.address, get_player_address(), 0x3c);

        vec3 cam_m_vector(acm.address + 0x6c);
        cam_m_vector = camera_tf.loc();
        cam_m_vector.write_to(acm.address + 0x6c);
      }
    }
  }
  void MorphBallCamera::run_mod_mp1_gc(Region region)
  {
  }

  void MorphBallCamera::init_mod(Game game, Region region)
  {
    initialized = true;
    switch (game)
    {
    case Game::PRIME_1:
      if (region == Region::NTSC)
      {

        // Disables updates to ballcam.ballDelta and ballcam.ballDeltaFlat.
        // If the camera doesn't realize that the ball is moving, it may not try to update on it's own.
        //code_changes.emplace_back(0x8004767c, 0x60000000);
        //code_changes.emplace_back(0x80047680, 0x60000000);
        //code_changes.emplace_back(0x80047684, 0x60000000);
        //code_changes.emplace_back(0x80047688, 0x60000000);
        //code_changes.emplace_back(0x8004768c, 0x60000000);


        mp1_static.control_flag_address = 0x8052e9b8;
        mp1_static.cplayer_address = 0x804d3c20;
        mp1_static.object_list_ptr_address = 0x804bfc30;
        mp1_static.camera_uid_address = 0x804c4a08;

        /*mp1_static.ballcam_ball_vel_flat_offset = 0x2e4;
        mp1_static.ballcam_ball_delta_offset = 0x2ec;
        mp1_static.ballcam_ball_delta_flat_offset = 0x2f8;*/


      }
      else if (region == Region::PAL)
      {
        mp1_static.control_flag_address = 0x80532b38;
        mp1_static.cplayer_address = 0x804d7b60;
        mp1_static.object_list_ptr_address = 0x804c3b70;
        mp1_static.camera_uid_address = 0x804c8948;
      }
      else
      {
      }
      break;
    case Game::PRIME_1_GCN:
      if (region == Region::NTSC)
      {
        mp1_gc_static.cplayer_address = 0x8046b97c;
        mp1_gc_static.state_mgr_address = 0x8045a1a8;
        mp1_gc_static.camera_uid_address = 0x8045c5b4;
      }
      else if (region == Region::PAL)
      {
        mp1_gc_static.cplayer_address = 0x803f38a4;
        mp1_gc_static.state_mgr_address = 0x803e2088;
        mp1_gc_static.camera_uid_address = 0x803e44dc;
      }
      else
      {
      }
      break;
    case Game::PRIME_2:
      if (region == Region::NTSC)
      {
        mp2_static.cplayer_ptr_address = 0x804e87dc;
        mp2_static.object_list_ptr_address = 0x804e7af8;
        mp2_static.camera_uid_address = 0x804eb9b0;
        mp2_static.control_flag_address = 0x805373f8;
        mp2_static.load_state_address = 0x804e8824;
      }
      else if (region == Region::PAL)
      {
        mp2_static.cplayer_ptr_address = 0x804efc2c;
        mp2_static.object_list_ptr_address = 0x804eef48;
        mp2_static.camera_uid_address = 0x804f2f50;
        mp2_static.control_flag_address = 0x8053ebf8;
        mp2_static.load_state_address = 0x804efc74;
      }
      else
      {
      }
      break;
    case Game::PRIME_3:
      if (region == Region::NTSC)
      {
        mp3_static.state_mgr_ptr_address = 0x805c6c40;
        mp3_static.cam_uid_ptr_address = 0x805c6c78;
      }
      else if (region == Region::PAL)
      {
      }
      else
      {
      }
      break;
    default:
      break;
    }
  }

  void MorphBallCamera::on_state_change(ModState old_state)
  {
  }
}  // namespace prime

   /*
    ang_vector cam_ang_vector(camera_tf.m[0][0], camera_tf.m[0][1]);
   float cam_rot = cam_ang_vector.rot - delta_rot;


    
    if (rot2 > 2.0f * PI)
    {
      rot2 -= 2.0f * PI;
    }

    if (rot2 < 0.0f)
    {
      rot2 += 2.0f * PI;
    }

    print_to_log("rot2: " + std::to_string(rot2));
    float flat_angle = (PI) + rot2;
    float height_angle = atan2(start_z, cam_pos_vector.dist);

    if (flat_angle > 2.0f * PI)
    {
      flat_angle -= 2.0f * PI;
    }

    if (flat_angle < 0.0f)
    {
      flat_angle += 2.0f * PI;
    }


    //camera_tf.build_rotation_loc_safe(cam_rot, height_angle, 0);
    camera_tf.build_rotation(cam_rot, height_angle, 0);*/
