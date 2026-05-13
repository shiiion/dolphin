#pragma once

#include "Common/CommonTypes.h"
#include "DolphinQt/Config/SettingsWindow.h"

class ConfigureModWindow : public StackedSettingsWindow
{
  Q_OBJECT
public:
  explicit ConfigureModWindow(std::string const& mod_name, QWidget* parent = nullptr);

private:
  void CreateMainLayout();

private:
  std::string m_mod_name;
};
