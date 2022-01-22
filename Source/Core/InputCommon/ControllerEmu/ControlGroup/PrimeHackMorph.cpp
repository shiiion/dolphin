#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackMorph.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
  PrimeHackMorph::PrimeHackMorph(const std::string& name_, const std::string& default_selection)
    : ControlGroup(name_, GroupType::PrimeHackMorph), m_selection_value(default_selection)
  {
  }

    // Returns the MorphBall Profile
  const std::string& PrimeHackMorph::GetMorphBallProfileName() const
  {
    return m_selection_value;
  }

  const std::string& PrimeHackMorph::GetMainProfileName() const
  {
    return m_previous_selection;
  }

  void PrimeHackMorph::SetMorphBallProfileName(std::string& val)
  {
    m_selection_value = val;
  }

  void PrimeHackMorph::SetMainProfileName(std::string& val)
  {
    m_previous_selection = val;
  }
}  // namespace ControllerEmu
