// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

class InputConfig;
enum class PadGroup;
struct GCPadStatus;

namespace ControllerEmu
{
class ControlGroup;
}

namespace Pad
{
void Shutdown();
void Initialize();
void LoadConfig();
void GenerateDynamicInputTextures();
bool IsInitialized();

InputConfig* GetConfig();

GCPadStatus GetStatus(int pad_num);
ControllerEmu::ControlGroup* GetGroup(int pad_num, PadGroup group);
void Rumble(int pad_num, ControlState strength);
void ResetRumble(int pad_num);

bool GetMicButton(int pad_num);

void ChangeUIPrimeHack(int pad_num, bool useMetroidUI);

bool CheckSpringBall(int pad_num);
bool CheckPitchRecentre(int pad_num);
bool PrimeUseController(int pad_num);

void PrimeSetMode(int pad_num, bool controller);

bool PrimeUseGyro(int pad_num);

bool CheckForward(int pad_num);
bool CheckBack(int pad_num);
bool CheckLeft(int pad_num);
bool CheckRight(int pad_num);
bool CheckJump(int pad_num);

std::tuple<double, double> GetPrimeStickXY(int pad_num);
std::tuple<double, double> GetPrimeGyroPitchYaw(int pad_num);

std::tuple<double, double, bool, bool, bool> PrimeSettings(int pad_num);
}  // namespace Pad
