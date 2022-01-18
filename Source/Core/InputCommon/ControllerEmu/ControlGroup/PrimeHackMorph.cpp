#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackMorph.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
  PrimeHackMorph::PrimeHackMorph(const std::string& name_, const std::string& default_selection)
    : ControlGroup(name_, GroupType::PrimeHackMorph), m_selection_value(default_selection)
  {
  }

    // Always return controller mode for platforms with input APIs we don't support.
  const std::string& PrimeHackMorph::GetSelection() const
  {
    return m_selection_value;
  }

  const std::string& PrimeHackMorph::GetMainProfileName() const
  {
    return m_previous_selection;
  }

  void PrimeHackMorph::SetSelection(std::string& val)
  {
    m_selection_value = val;
  }

  //Used to store the original profile from the main profile for the current WiimoteEmu Window
  void PrimeHackMorph::SetMainProfileName(std::string& val)
  {
    m_previous_selection = val;
  }
}  // namespace ControllerEmu
