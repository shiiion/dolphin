// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/Common.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/ControllerEmu/Control/Control.h"

struct GCPadStatus;

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class MixedTriggers;
class PrimeHackModes;
class IMUGyroscope;
}  // namespace ControllerEmu

enum class PadGroup
{
  Buttons,
  MainStick,
  CStick,
  DPad,
  Triggers,
  Rumble,
  Mic,
  Options,
  Triforce,

  Beams,
  Visors,
  Camera,
  Misc,
  ControlStick,
  Modes,
  GyroCamera
};

class GCPad : public ControllerEmu::EmulatedController
{
public:
  explicit GCPad(unsigned int index);
  GCPadStatus GetInput() const;
  void SetOutput(const ControlState strength);

  bool GetMicButton() const;

  std::string GetName() const override;

  InputConfig* GetConfig() const override;

  ControllerEmu::ControlGroup* GetGroup(PadGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;
  void LoadPrimeHackDefaults(const ControllerInterface& ciface) override;

  void ChangeUIPrimeHack(bool useMetroidUI);

  bool CheckSpringBallCtrl();
  bool PrimeControllerMode();
  bool PrimeUseGyro();

  void SetPrimeMode(bool controller);

  bool CheckPitchRecentre();
  std::tuple<double, double> GetPrimeStickXY();
  std::tuple<double, double> GetPrimeGyroPitchYaw();

  std::tuple<double, double, bool, bool, bool> GetPrimeSettings();

  // Values averaged from multiple genuine GameCube controllers.
  static constexpr ControlState MAIN_STICK_GATE_RADIUS = 0.7937125;
  static constexpr ControlState C_STICK_GATE_RADIUS = 0.7221375;

  static constexpr const char* BUTTONS_GROUP = _trans("Buttons");
  static constexpr const char* MAIN_STICK_GROUP = "Main Stick";
  static constexpr const char* C_STICK_GROUP = "C-Stick";
  static constexpr const char* DPAD_GROUP = _trans("D-Pad");
  static constexpr const char* TRIGGERS_GROUP = _trans("Triggers");
  static constexpr const char* RUMBLE_GROUP = _trans("Rumble");
  static constexpr const char* MIC_GROUP = _trans("Microphone");
  static constexpr const char* OPTIONS_GROUP = _trans("Options");
  static constexpr const char* TRIFORCE_GROUP = _trans("Triforce");

  static constexpr const char* A_BUTTON = "A";
  static constexpr const char* B_BUTTON = "B";
  static constexpr const char* X_BUTTON = "X";
  static constexpr const char* Y_BUTTON = "Y";
  static constexpr const char* Z_BUTTON = "Z";
  static constexpr const char* START_BUTTON = "Start";

  // Special Triforce buttons
  static constexpr const char* TEST_BUTTON = _trans("Test");
  static constexpr const char* SERVICE_BUTTON = _trans("Service");
  static constexpr const char* COIN_BUTTON = _trans("Coin");

  // i18n: The left trigger button (labeled L on real controllers)
  static constexpr const char* L_DIGITAL = _trans("L");
  // i18n: The right trigger button (labeled R on real controllers)
  static constexpr const char* R_DIGITAL = _trans("R");
  // i18n: The left trigger button (labeled L on real controllers) used as an analog input
  static constexpr const char* L_ANALOG = _trans("L-Analog");
  // i18n: The right trigger button (labeled R on real controllers) used as an analog input
  static constexpr const char* R_ANALOG = _trans("R-Analog");

private:
  bool using_metroid_ui = false;
  std::vector<std::unique_ptr<ControllerEmu::Control>> trigger_controls_temp;

  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_main_stick;
  ControllerEmu::AnalogStick* m_c_stick;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::MixedTriggers* m_triggers;
  ControllerEmu::ControlGroup* m_rumble;
  ControllerEmu::Buttons* m_mic;
  ControllerEmu::ControlGroup* m_options;
  ControllerEmu::Buttons* m_triforce;

  ControllerEmu::SettingValue<bool> m_always_connected_setting;

  ControllerEmu::ControlGroup* m_primehack_camera;
  ControllerEmu::ControlGroup* m_primehack_misc;
  ControllerEmu::AnalogStick* m_primehack_stick;
  ControllerEmu::IMUGyroscope* m_primehack_gyro;
  ControllerEmu::PrimeHackModes* m_primehack_modes;

  ControllerEmu::SettingValue<double> m_primehack_camera_sensitivity;
  ControllerEmu::SettingValue<double> m_primehack_horizontal_sensitivity;
  ControllerEmu::SettingValue<double> m_primehack_vertical_sensitivity;
  ControllerEmu::SettingValue<double> m_primehack_gyro_horizontal_sensitivity;
  ControllerEmu::SettingValue<double> m_primehack_gyro_vertical_sensitivity;

  ControllerEmu::SettingValue<bool> m_primehack_invert_y;
  ControllerEmu::SettingValue<bool> m_primehack_invert_x;
  ControllerEmu::SettingValue<bool> m_primehack_remap_map_controls;
  ControllerEmu::SettingValue<bool> m_primehack_gyro_enable;

  static constexpr u8 STICK_GATE_RADIUS = 0x60;
  static constexpr u8 STICK_CENTER = 0x80;
  static constexpr u8 STICK_RADIUS = 0x7F;

  const unsigned int m_index;
};
