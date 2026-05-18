#pragma once

#include "DolphinQt/Config/SettingsWindow.h"

class ConfigureModWindow : public StackedSettingsWindow
{
  Q_OBJECT
public:
  explicit ConfigureModWindow(std::string const& mod_name, QWidget* parent = nullptr);

  void keyPressEvent(QKeyEvent*) override;

private:
  void CreateMainLayout();
  void ShowSearch();

private:
  std::string m_mod_name;
};
