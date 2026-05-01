#pragma once

#include "Common/CommonTypes.h"

#include <QDialog>

class ConfigureModWindow : public QDialog
{
  Q_OBJECT
public:
  explicit ConfigureModWindow(std::string const& mod_name, QWidget* parent = nullptr);

private:
  void CreateMainLayout();

private:
  std::string m_mod_name;
};
