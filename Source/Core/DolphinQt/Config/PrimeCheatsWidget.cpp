#include "PrimeCheatsWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/AchievementManager.h"
#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Common/Config/Config.h"
#include "DolphinQt/Config/CheatWarningWidget.h"
#include "DolphinQt/Settings.h"

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Config/HardcoreWarningWidget.h"
#endif

PrimeCheatsWidget::PrimeCheatsWidget(std::string game_id, bool restart_required)
  : m_game_id(game_id), m_restart_required(restart_required)
{
  CreateWidgets();
  OnLoadConfig();
  ConnectWidgets();
  AddDescriptions();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this] { UpdateHardcoreChange(); });
}

void PrimeCheatsWidget::CreateWidgets()
{
  auto* group_box = new QGroupBox(QStringLiteral(""));
  auto* main_layout = new QVBoxLayout;
  auto* layout = new QVBoxLayout;

  group_box->setLayout(layout);

  m_checkbox_noclip = new QCheckBox(tr("Noclip"));
  m_checkbox_invulnerability = new QCheckBox(tr("Invulnerability"));
  m_checkbox_scandash = new QCheckBox(tr("Restore Scan Dash"));
  m_checkbox_hudmemo = new QCheckBox(tr("Disable Pickup Notifications"));
  // Merge?
  m_checkbox_skipcutscenes = new QCheckBox(tr("Skippable Cutscenes"));
  m_checkbox_skipportalmp2 = new QCheckBox(tr("Skip MP2 Portal Cutscenes"));
  m_checkbox_hypermode = new QCheckBox(tr("Unlock Hypermode (Hard) Difficulty"));
  m_checkbox_friendvouchers = new QCheckBox(tr("Bypass Friend Vouchers (Trilogy Only)"));
  m_checkbox_anybeam = new QCheckBox(tr("Beam Door Requirement Bypass"));
  m_warning = new CheatWarningWidget(m_game_id, m_restart_required, this);

#ifdef USE_RETRO_ACHIEVEMENTS
  UpdateHardcoreChange();

  auto hc_warning = new HardcoreWarningWidget(this);
  layout->addWidget(hc_warning);
  connect(hc_warning, &HardcoreWarningWidget::OpenAchievementSettings, this,
          &PrimeCheatsWidget::OpenAchievementSettings);
#endif

  layout->addWidget(m_warning);
  layout->addWidget(m_checkbox_noclip);
  layout->addWidget(m_checkbox_invulnerability);
  layout->addWidget(m_checkbox_scandash);
  layout->addWidget(m_checkbox_hudmemo);
  layout->addWidget(m_checkbox_skipcutscenes);
  layout->addWidget(m_checkbox_skipportalmp2);
  layout->addWidget(m_checkbox_hypermode);
  layout->addWidget(m_checkbox_friendvouchers);
  layout->addWidget(m_checkbox_anybeam);

  main_layout->addWidget(group_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void PrimeCheatsWidget::ConnectWidgets()
{
  connect(m_checkbox_noclip, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_invulnerability, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_skipcutscenes, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_scandash, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_skipportalmp2, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_friendvouchers, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_hudmemo, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_hypermode, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_anybeam, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_warning, &CheatWarningWidget::OpenCheatEnableSettings, this, &PrimeCheatsWidget::OpenGeneralSettings);
}

void PrimeCheatsWidget::OnSaveConfig()
{
#ifdef USE_RETRO_ACHIEVEMENTS
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
  {
    return;
  }
#endif
  Config::SetBaseOrCurrent(Config::PRIMEHACK_NOCLIP, m_checkbox_noclip->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_INVULNERABILITY, m_checkbox_invulnerability->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_SKIPPABLE_CUTSCENES, m_checkbox_skipcutscenes->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_RESTORE_SCANDASH, m_checkbox_scandash->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_SKIPMP2_PORTAL, m_checkbox_skipportalmp2->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_FRIENDVOUCHERS, m_checkbox_friendvouchers->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_DISABLE_HUDMEMO, m_checkbox_hudmemo->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_UNLOCK_HYPERMODE, m_checkbox_hypermode->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_ANYBEAM_DOOR, m_checkbox_anybeam->isChecked());
}

void PrimeCheatsWidget::OnLoadConfig()
{
#ifdef USE_RETRO_ACHIEVEMENTS
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
  {
    return;
  }
#endif
  m_checkbox_noclip->setChecked(Config::Get(Config::PRIMEHACK_NOCLIP));
  m_checkbox_invulnerability->setChecked(Config::Get(Config::PRIMEHACK_INVULNERABILITY));
  m_checkbox_skipcutscenes->setChecked(Config::Get(Config::PRIMEHACK_SKIPPABLE_CUTSCENES));
  m_checkbox_scandash->setChecked(Config::Get(Config::PRIMEHACK_RESTORE_SCANDASH));
  m_checkbox_skipportalmp2->setChecked(Config::Get(Config::PRIMEHACK_SKIPMP2_PORTAL));
  m_checkbox_friendvouchers->setChecked(Config::Get(Config::PRIMEHACK_FRIENDVOUCHERS));
  m_checkbox_hudmemo->setChecked(Config::Get(Config::PRIMEHACK_DISABLE_HUDMEMO));
  m_checkbox_hypermode->setChecked(Config::Get(Config::PRIMEHACK_UNLOCK_HYPERMODE));
  m_checkbox_anybeam->setChecked(Config::Get(Config::PRIMEHACK_ANYBEAM_DOOR));
}

void PrimeCheatsWidget::AddDescriptions()
{
  static const char TR_NOCLIP[] =
    QT_TR_NOOP("Disables player collision for all solid objects.");
  static const char TR_INVULNERABILITY[] =
    QT_TR_NOOP("Become invulnerable to most types of damage.");
  static const char TR_SKIPCUTSCENES[] =
    QT_TR_NOOP("Make most cutscenes skippable.\n*The skip button varies from each game (usually the Jump or Menu button).");
  static const char TR_SCANDASH[] =
    QT_TR_NOOP("Restores the scan dashing glitch from rev 0 to later revisions.");
  static const char TR_SKIPPORTAL[] =
    QT_TR_NOOP("Skips portal activation cutscenes in Metroid Prime 2.");
  static const char TR_FRIENDVOUCHERS[] =
    QT_TR_NOOP("Bypasses the friend voucher requirement for all unlockables.");
  static const char TR_HUDMEMO[] =
    QT_TR_NOOP("Automatically skips the item pickup screen and explanation screen for powerups.");
  static const char TR_HYPERMODE[] =
    QT_TR_NOOP("Bypasses completed save requirement to unlock Hypermode (Hard) difficulty.");
  static const char TR_ANYBEAM[] =
    QT_TR_NOOP("MP1: Doors can be opened by any beam as long as the correspoding beam has been obtained.\nMP2: Annihilator Beam can open Dark or Light Beam doors.");

  m_checkbox_noclip->setToolTip(tr(TR_NOCLIP));
  m_checkbox_invulnerability->setToolTip(tr(TR_INVULNERABILITY));
  m_checkbox_skipcutscenes->setToolTip(tr(TR_SKIPCUTSCENES));
  m_checkbox_scandash->setToolTip(tr(TR_SCANDASH));
  m_checkbox_skipportalmp2->setToolTip(tr(TR_SKIPPORTAL));
  m_checkbox_friendvouchers->setToolTip(tr(TR_FRIENDVOUCHERS));
  m_checkbox_hudmemo->setToolTip(tr(TR_HUDMEMO));
  m_checkbox_hypermode->setToolTip(tr(TR_HYPERMODE));
  m_checkbox_anybeam->setToolTip(tr(TR_ANYBEAM));
}

void PrimeCheatsWidget::UpdateHardcoreChange()
{
#ifdef USE_RETRO_ACHIEVEMENTS
  bool enabled = AchievementManager::GetInstance().IsHardcoreModeActive();
  m_checkbox_noclip->setEnabled(!enabled);
  m_checkbox_invulnerability->setEnabled(!enabled);
  m_checkbox_scandash->setEnabled(!enabled);
  m_checkbox_hudmemo->setEnabled(!enabled);
  m_checkbox_skipcutscenes->setEnabled(!enabled);
  m_checkbox_skipportalmp2->setEnabled(!enabled);
  m_checkbox_hypermode->setEnabled(!enabled);
  m_checkbox_friendvouchers->setEnabled(!enabled);
  m_checkbox_anybeam->setEnabled(!enabled);

  if (enabled)
  {
    m_checkbox_noclip->setChecked(false);
    m_checkbox_invulnerability->setChecked(false);
    m_checkbox_scandash->setChecked(false);
    m_checkbox_hudmemo->setChecked(false);
    m_checkbox_skipcutscenes->setChecked(false);
    m_checkbox_skipportalmp2->setChecked(false);
    m_checkbox_hypermode->setChecked(false);
    m_checkbox_friendvouchers->setChecked(false);
    m_checkbox_anybeam->setChecked(false);
  }
  else
  {
    OnLoadConfig();
  }
#endif
}

void PrimeCheatsWidget::showEvent(QShowEvent*)
{
  OnLoadConfig();
}
