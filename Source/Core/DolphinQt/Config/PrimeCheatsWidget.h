#include <QWidget>

#include "Common/CommonTypes.h"

class QCheckBox;
class CheatWarningWidget;

class PrimeCheatsWidget : public QWidget
{
  Q_OBJECT
public:
  explicit PrimeCheatsWidget(std::string game_id, bool restart_required);
signals:
  void OpenGeneralSettings();
  void OpenAchievementSettings();
protected:
  void showEvent(QShowEvent*);
private:
  void CreateWidgets();
  void ConnectWidgets();
  void OnSaveConfig();
  void OnLoadConfig();
  void AddDescriptions();
  void UpdateHardcoreChange();

  QCheckBox* m_checkbox_noclip;
  QCheckBox* m_checkbox_invulnerability;
  QCheckBox* m_checkbox_skipcutscenes;
  QCheckBox* m_checkbox_scandash;
  QCheckBox* m_checkbox_skipportalmp2;
  QCheckBox* m_checkbox_friendvouchers;
  QCheckBox* m_checkbox_hudmemo;
  QCheckBox* m_checkbox_hypermode;
  QCheckBox* m_checkbox_anybeam;
  CheatWarningWidget* m_warning;
  std::string m_game_id;
  bool m_restart_required;
};
