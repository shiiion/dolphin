// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCPad.h"

#include "Common/Common.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Pad
{
static InputConfig s_config("GCPadNew", _trans("Pad"), "GCPad", "Pad");
InputConfig* GetConfig()
{
  return &s_config;
}

void Shutdown()
{
  s_config.UnregisterHotplugCallback();

  s_config.ClearControllers();
}

void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = 0; i < 4; ++i)
      s_config.CreateController<GCPad>(i);
  }

  s_config.RegisterHotplugCallback();

  // Load the saved controller config
  s_config.LoadConfig();
}

void LoadConfig()
{
  s_config.LoadConfig();
}

void GenerateDynamicInputTextures()
{
  s_config.GenerateControllerTextures();
}

bool IsInitialized()
{
  return !s_config.ControllersNeedToBeCreated();
}

GCPadStatus GetStatus(int pad_num)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetInput();
}

ControllerEmu::ControlGroup* GetGroup(int pad_num, PadGroup group)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetGroup(group);
}

void Rumble(const int pad_num, const ControlState strength)
{
  static_cast<GCPad*>(s_config.GetController(pad_num))->SetOutput(strength);
}

void ResetRumble(const int pad_num)
{
  static_cast<GCPad*>(s_config.GetController(pad_num))->SetOutput(0.0);
}

bool GetMicButton(const int pad_num)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetMicButton();
}

void ChangeUIPrimeHack(int pad_num, bool useMetroidUI)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  gcpad->ChangeUIPrimeHack(useMetroidUI);
}

bool CheckSpringBall(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->CheckSpringBallCtrl();
}

bool CheckPitchRecentre(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->CheckPitchRecentre();
}

bool PrimeUseController(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->PrimeControllerMode();
}

void PrimeSetMode(int pad_num, bool useController)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  gcpad->SetPrimeMode(useController);
}

bool PrimeUseGyro(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->PrimeUseGyro();
}

std::tuple<double, double> GetPrimeStickXY(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->GetPrimeStickXY();
}

std::tuple<double, double> GetPrimeGyroPitchYaw(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->GetPrimeGyroPitchYaw();
}

bool CheckForward(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->groups[1]->controls[0].get()->control_ref->State() > 0.5;
}

bool CheckBack(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->groups[1]->controls[1].get()->control_ref->State() > 0.5;
}

bool CheckLeft(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->groups[1]->controls[2].get()->control_ref->State() > 0.5;
}

bool CheckRight(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->groups[1]->controls[3].get()->control_ref->State() > 0.5;
}

bool CheckJump(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->groups[0]->controls[1]->control_ref->State() > 0.5;
}

std::tuple<double, double, bool, bool, bool> PrimeSettings(int pad_num)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(pad_num));

  return gcpad->GetPrimeSettings();
}

}  // namespace Pad
