// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/PrimeWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <qcolordialog.h>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"
#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"
#include "DolphinQt/Settings.h"

#include "Core/Core.h"
#include "Core/PrimeHack/HackConfig.h"
#include "VideoCommon/VideoConfig.h"

PrimeWidget::PrimeWidget(GraphicsPane* parent)
{
  CreateWidgets();
  ConnectWidgets();
  AddDescriptions();

  ToggleShowCrosshair(m_toggle_gc_show_crosshair->isChecked());
  ArmPositionModeChanged(m_manual_arm_position->isChecked());

  m_select_colour->setStyleSheet(
      tr("border: 1px solid #") +
      QString::number(Config::Get(Config::GC_CROSSHAIR_COLOR_RGBA) & 0xFFFFFF00, 16) + tr(";"));
}

void PrimeWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Graphics
  auto* graphics_box = new QGroupBox(tr("General"));
  auto* graphics_layout = new QGridLayout();

  graphics_box->setLayout(graphics_layout);

  m_motions_lock =
      new ConfigBool(tr("Lock Camera for Motion Puzzles"), Config::LOCKCAMERA_IN_PUZZLES);
  m_toggle_arm_position =
      new ConfigBool(tr("Enable Viewmodel Adjustments"), Config::TOGGLE_ARM_REPOSITION);
  m_toggle_culling = new ConfigBool(tr("Disable Culling"), Config::TOGGLE_CULLING);
  m_toggle_secondaryFX =
      new ConfigBool(tr("Enable GCN Arm Cannon Effects"), Config::ENABLE_SECONDARY_GUNFX);
  m_toggle_gc_show_crosshair =
      new ConfigBool(tr("Enable GCN Crosshair"), Config::GC_SHOW_CROSSHAIR);
  m_autofogtoggle_mp3 =
      new ConfigBool(tr("Disable Fog for Scan Visor [MP3]"), Config::AUTO_FOG_TOGGLE_MP3);

  graphics_layout->addWidget(m_motions_lock, 1, 0);
  graphics_layout->addWidget(m_toggle_secondaryFX, 2, 0);
  graphics_layout->addWidget(m_toggle_culling, 3, 0);
  graphics_layout->addWidget(m_autofogtoggle_mp3, 4, 0);

  // Bloom
  auto* bloom_box = new QGroupBox(tr("Bloom"));
  auto* bloom_layout = new QGridLayout();

  bloom_box->setLayout(bloom_layout);

  m_disable_bloom = new ConfigBool(tr("Disable Bloom [MP1/MP2]"), Config::DISABLE_BLOOM);
  m_reduce_bloom = new ConfigBool(tr("Reduce Bloom Offset [MP3]"), Config::REDUCE_BLOOM);
  m_bloom_intensity_val = new ConfigInteger(0, 100, Config::BLOOM_INTENSITY);
  m_bloom_intensity_val->setMaximumWidth(50);
  m_bloom_intensity = new ConfigSlider(0, 100, Config::BLOOM_INTENSITY);
  m_bloom_intensity->setMaximumWidth(200);
  m_bloom_intensity->setMinimum(0);
  m_bloom_intensity->setMaximum(100);
  m_bloom_intensity->setPageStep(1);

  m_bloom_intensity->setEnabled(Config::Get(Config::REDUCE_BLOOM));
  m_bloom_intensity_val->setEnabled(Config::Get(Config::REDUCE_BLOOM));

  bloom_layout->addWidget(m_disable_bloom, 1, 0);
  bloom_layout->addWidget(m_reduce_bloom, 2, 0);
  bloom_layout->addWidget(new QLabel(tr("Bloom Intensity")), 3, 0);
  bloom_layout->addWidget(m_bloom_intensity, 3, 1);
  bloom_layout->addWidget(m_bloom_intensity_val, 3, 2);

  // Crosshair Color
  auto* crosshair_color_box = new QGroupBox(tr("Crosshair"));
  auto* crosshair_color_layout = new QHBoxLayout();

  crosshair_color_box->setLayout(crosshair_color_layout);

  colorpicker = new QColorDialog();
  m_select_colour = new QPushButton(tr("Select Color"));
  m_reset_colour = new QPushButton(tr("Reset Color"));
  m_reset_colour->setMaximumWidth(100);

  crosshair_color_layout->addWidget(m_toggle_gc_show_crosshair);
  crosshair_color_layout->addSpacing(4);
  crosshair_color_layout->addWidget(m_reset_colour);
  crosshair_color_layout->addWidget(m_select_colour);

  // FOV
  auto* fov_box = new QGroupBox(tr("FOV"));
  auto* fov_layout = new QGridLayout();
  fov_box->setLayout(fov_layout);

  m_fov_toggle = new ConfigBool(tr("Enable Field of View Adjustment"), Config::FOV_ENABLE);

  m_fov_axis = new ConfigSlider(1, 170, Config::FOV);
  m_fov_axis->setMaximumWidth(200);
  m_fov_axis->setMinimum(1);
  m_fov_axis->setMaximum(170);
  m_fov_axis->setPageStep(1);

  m_fov_counter = new ConfigInteger(1, 170, Config::FOV);
  m_fov_counter->setMaximumWidth(50);
  m_fov_counter->setEnabled(Config::Get(Config::FOV_ENABLE));
  m_fov_axis->setEnabled(Config::Get(Config::FOV_ENABLE));

  fov_layout->addWidget(m_fov_toggle, 0, 0);
  fov_layout->addWidget(new QLabel(tr("Field of View")), 1, 0);
  fov_layout->addWidget(m_fov_axis, 1, 1);
  fov_layout->addWidget(m_fov_counter, 1, 2);

  // Viewmodel Position
  auto* viewmodel_box = new QGroupBox(tr("Viewmodel"));
  auto* viewmodel_layout = new QGridLayout();

  viewmodel_box->setLayout(viewmodel_layout);

  m_auto_arm_position =
      new ConfigRadioInt(tr("Automatic Adjustment"), Config::ARMPOSITION_MODE, 0);
  m_manual_arm_position =
      new ConfigRadioInt(tr("Manual Adjustment"), Config::ARMPOSITION_MODE, 1);

  m_x_axis = new ConfigSlider(-50, 50, Config::ARMPOSITION_LEFTRIGHT);
  m_z_axis = new ConfigSlider(-50, 50, Config::ARMPOSITION_FORWARDBACK);
  m_y_axis = new ConfigSlider(-50, 50, Config::ARMPOSITION_UPDOWN);

  x_counter = new ConfigInteger(-50, 50, Config::ARMPOSITION_LEFTRIGHT);
  x_counter->setMaximumWidth(50);

  z_counter = new ConfigInteger(-50, 50, Config::ARMPOSITION_FORWARDBACK);
  z_counter->setMaximumWidth(50);

  y_counter = new ConfigInteger(-50, 50, Config::ARMPOSITION_UPDOWN);
  y_counter->setMaximumWidth(50);

  m_x_axis->setMaximumWidth(200);
  m_x_axis->setMinimum(-50);
  m_x_axis->setMaximum(50);
  m_x_axis->setPageStep(1);

  m_z_axis->setMaximumWidth(200);
  m_z_axis->setMinimum(-50);
  m_z_axis->setMaximum(50);
  m_z_axis->setPageStep(1);

  m_y_axis->setMaximumWidth(200);
  m_y_axis->setMinimum(-50);
  m_y_axis->setMaximum(50);
  m_y_axis->setPageStep(1);

  viewmodel_layout->addWidget(m_toggle_arm_position, 0, 0);
  viewmodel_layout->addWidget(m_auto_arm_position, 1, 0);
  viewmodel_layout->addWidget(m_manual_arm_position, 1, 1);

  viewmodel_layout->addWidget(new QLabel(tr("Up/Down")), 4, 0);
  viewmodel_layout->addWidget(new QLabel(tr("Left/Right")), 5, 0);
  viewmodel_layout->addWidget(new QLabel(tr("Forwards/Backwards")), 6, 0);

  viewmodel_layout->addWidget(m_y_axis, 4, 1);
  viewmodel_layout->addWidget(m_x_axis, 5, 1);
  viewmodel_layout->addWidget(m_z_axis, 6, 1);

  viewmodel_layout->addWidget(y_counter, 4, 2);
  viewmodel_layout->addWidget(x_counter, 5, 2);
  viewmodel_layout->addWidget(z_counter, 6, 2);

  main_layout->addWidget(graphics_box);
  main_layout->addWidget(bloom_box);
  main_layout->addWidget(crosshair_color_box);
  main_layout->addWidget(fov_box);
  main_layout->addWidget(viewmodel_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void PrimeWidget::ArmPositionModeChanged(bool mode)
{
  m_x_axis->setEnabled(mode);
  m_y_axis->setEnabled(mode);
  m_z_axis->setEnabled(mode);

  x_counter->setEnabled(mode);
  y_counter->setEnabled(mode);
  z_counter->setEnabled(mode);
}

void PrimeWidget::ToggleShowCrosshair(bool mode)
{
  m_select_colour->setEnabled(mode);
}

void PrimeWidget::ConnectWidgets()
{
  connect(m_fov_toggle, &ConfigBool::clicked, this, [this](bool checked) {
    m_fov_counter->setEnabled(checked);
    m_fov_axis->setEnabled(checked);
  });
  connect(m_reduce_bloom, &ConfigBool::clicked, this, [this](bool checked) {
    m_bloom_intensity->setEnabled(checked);
    m_bloom_intensity_val->setEnabled(checked);
  });
  connect(m_toggle_gc_show_crosshair, &ConfigBool::clicked, this,
          [this](bool checked) { PrimeWidget::ToggleShowCrosshair(checked); });
  connect(m_auto_arm_position, &QRadioButton::clicked, this,
          [this](bool checked) { PrimeWidget::ArmPositionModeChanged(!checked); });
  connect(m_manual_arm_position, &QRadioButton::clicked, this,
          [this](bool checked) { PrimeWidget::ArmPositionModeChanged(checked); });
  connect(m_toggle_arm_position, &QCheckBox::clicked, this, [this](bool checked) {
    m_auto_arm_position->setEnabled(checked);
    m_manual_arm_position->setEnabled(checked);
    PrimeWidget::ArmPositionModeChanged(m_manual_arm_position->isChecked());
  });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    if (state != Core::State::Uninitialized)
    {
      m_toggle_culling->setEnabled(true);
    }
    else
    {
      if (prime::GetFov(prime::Game::PRIME_1) > 94)
      {
        m_toggle_culling->setEnabled(false);
        m_toggle_culling->setChecked(true);
      }
    }
  });

  connect(m_select_colour, &QPushButton::clicked, this, [this]() {
    QColor c = colorpicker->getColor(QColor::fromRgba(0x4b7ea331), this,
                                     tr("Select a cursor color"), QColorDialog::ShowAlphaChannel);

    int r, g, b, a;
    c.getRgb(&r, &g, &b, &a);
    u32 colour = r << 24 | g << 16 | b << 8 | a;

    m_select_colour->setStyleSheet(tr("border: 2px solid ") + c.name() + tr(";"));
    Config::SetBaseOrCurrent(Config::GC_CROSSHAIR_COLOR_RGBA, colour);
  });
  connect(m_reset_colour, &QPushButton::clicked, this, [this]() {
    m_select_colour->setStyleSheet(tr("border: 2px double #4b7ea3;"));
    Config::SetBaseOrCurrent(Config::GC_CROSSHAIR_COLOR_RGBA, 0x4b7ea331);
  });
}

void PrimeWidget::AddDescriptions()
{
  static const char TR_GUNEFFECTS[] = QT_TR_NOOP(
      "Reintroduce the original secondary gun effects that were in the GameCube version of Metroid "
      "Prime 1\n"
      "These effects were disabled and cut in the Trilogy but still remained as unused assets.");
  static const char TR_DISABLE_BLOOM[] =
      QT_TR_NOOP("Disables bloom.\n\nSource: TheHatedGravity and dreamsyntax.");
  static const char TR_REDUCE_BLOOM[] =
      QT_TR_NOOP("Reduces bloom offset. Upscaling the resolution massively exaggerates the bloom "
                 "in Prime 3. This option fixes this.");
  static const char TR_BLOOM_INTENSITY[] =
      QT_TR_NOOP("Controls the intensity of the bloom effect in Prime 3. Default value is 100, "
                 "recommended value is 65, 0 will effectively disable bloom.\n\n"
                 "Source: TheHatedGravity and dreamsyntax.");
  static const char TR_TOGGLE_ARM_POSITION[] =
      QT_TR_NOOP("Allows for adjustments for the position of Samus' arms, recommended for a higher"
                  "than normal FOV.");
  static const char TR_TOGGLE_CULL[] =
      QT_TR_NOOP("Disables graphical culling. This allows for Field of Views above 101 in Metroid "
                 "Prime 1 and Metroid Prime 2, and above 94 in Metroid Prime 3.\n\n"
                 "This option is forced on above FOV 96");
  static const char TR_MANUAL_POSITION[] = QT_TR_NOOP(
      "Allows you to manually modify the XYZ positioning of Samus's arms in the viewmodel.");
  static const char TR_AUTO_POSITION[] =
      QT_TR_NOOP("Automatically adjusts the Samus's arm position in the view model, relative to "
                 "the Field Of View.");
  static const char TR_X_AXIS[] =
      QT_TR_NOOP("Modifies the arm position on the X axis. This is left and right.");
  static const char TR_Y_AXIS[] =
      QT_TR_NOOP("Modifies the arm position on the Y axis. This is up and down.");
  static const char TR_Z_AXIS[] =
      QT_TR_NOOP("Modifies the arm position on the Z axis. This is back and forward.");
  static const char TR_MOTION_LOCK[] =
      QT_TR_NOOP("Automatically locks the camera in all motion puzzles and buttons.");
  static const char TR_FOV_TOGGLE[] =
      QT_TR_NOOP("Enables FOV adjustments, disabling will cause games to revert to their original\n"
                 "values when this setting is disabled.");
  static const char TR_FOV[] =
      QT_TR_NOOP("Modifies the Field of View. The Prime games are not designed to go beyond 101 "
                 "FOV (94 in Prime 3), so if you do PrimeHack has to "
                 "disable culling and modify the znear values in the game. The higher the FOV, the "
                 "more glitches you may encounter."
                 "\n\nGenerally the best FOV values are between 75 and 100.");
  static const char TR_AUTOFOG[] =
      QT_TR_NOOP("Hack fix which toggles fog off only when in scan visor.\n\n"
                 "Fixes Prime 3 issue where menu background is filled with the fog color.\n\n"
                 "By Timdeuces.");
  static const char TR_GC_SHOW_CROSSHAIR[] =
      QT_TR_NOOP("Forces the lock-on crosshair as the persistent crosshair.");

  m_motions_lock->SetDescription(tr(TR_MOTION_LOCK));
  m_toggle_secondaryFX->SetDescription(tr(TR_GUNEFFECTS));
  m_disable_bloom->SetDescription(tr(TR_DISABLE_BLOOM));
  m_reduce_bloom->SetDescription(tr(TR_REDUCE_BLOOM));
  m_bloom_intensity_val->SetDescription(tr(TR_BLOOM_INTENSITY));
  m_toggle_culling->SetDescription(tr(TR_TOGGLE_CULL));
  m_toggle_arm_position->SetDescription(tr(TR_TOGGLE_ARM_POSITION));
  m_manual_arm_position->SetDescription(tr(TR_MANUAL_POSITION));
  m_auto_arm_position->SetDescription(tr(TR_AUTO_POSITION));
  m_x_axis->SetDescription(tr(TR_X_AXIS));
  m_x_axis->SetDescription(tr(TR_Y_AXIS));
  m_x_axis->SetDescription(tr(TR_Z_AXIS));
  m_fov_toggle->SetDescription(tr(TR_FOV_TOGGLE));
  m_fov_axis->SetDescription(tr(TR_FOV));
  m_fov_counter->SetDescription(tr(TR_FOV));
  m_autofogtoggle_mp3->SetDescription(tr(TR_AUTOFOG));
  m_toggle_gc_show_crosshair->SetDescription(tr(TR_GC_SHOW_CROSSHAIR));
}
