#include "DolphinQt/ConfigureModWindow.h"

#include "Common/Assert.h"
#include "Core/PrimeHack/ElfModLoaderInterface.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QTabWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class ModConfigWidget : public QWidget
{
public:
  explicit ModConfigWidget(prime::ElfMod* mod, QWidget* parent) : QWidget(parent), m_mod(mod)
  {
    CreateMainLayout();
  }

private:
  void CreateMainLayout()
  {
    auto* const main_layout = new QVBoxLayout(this);

    auto* const general_box = new QGroupBox(tr("General"));
    auto* const preset_box = new QGroupBox(tr("Presets"));
    auto* const cvar_box = new QGroupBox(tr("CVars"));

    auto* const top_layout = new QHBoxLayout;
    auto* const cvar_layout = new QGridLayout;

    // General Box
    auto* const general_layout = new QHBoxLayout;
    m_enabled_checkbox = new QCheckBox;
    auto* default_button = new QPushButton(tr("Default"));
    general_layout->addWidget(new QLabel(tr("Enabled")));
    general_layout->addWidget(m_enabled_checkbox, Qt::AlignLeft);
    general_layout->addSpacing(20);
    general_layout->addWidget(default_button);
    general_box->setLayout(general_layout);
    general_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Presets Box
    auto* const preset_layout = new QHBoxLayout;
    auto* const preset_buttons_layout = new QHBoxLayout;
    auto* preset_load_button = new QPushButton(tr("Load"));
    auto* preset_save_button = new QPushButton(tr("Save"));
    m_presets_dropdown = new QComboBox;
    m_presets_dropdown->setMinimumWidth(200);
    m_presets_dropdown->setEditable(true);
    preset_layout->addWidget(m_presets_dropdown);
    preset_buttons_layout->addWidget(preset_load_button);
    preset_buttons_layout->addWidget(preset_save_button);
    preset_layout->addLayout(preset_buttons_layout);
    preset_box->setLayout(preset_layout);
    RebuildPresetsDropdown();

    connect(preset_load_button, &QPushButton::clicked, this, &ModConfigWidget::OnLoadPresetPressed);
    connect(preset_save_button, &QPushButton::clicked, this, &ModConfigWidget::OnSavePresetPressed);

    // Top area
    top_layout->addWidget(general_box);
    top_layout->addWidget(preset_box);

    // CVars Box
    int row = 0;
    const QIcon question_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);

    for (auto const& cvar : m_mod->var_list)
    {
      auto* var_lbl = new QLabel(QString::fromStdString(cvar.name + ":"));
      QWidget* entry_widget;
      if (cvar.type == prime::CVarType::BOOLEAN)
      {
        auto* checkbox = new QCheckBox;
        m_cvar_entries.emplace_back(checkbox);
        entry_widget = checkbox;
      }
      else
      {
        auto* text = new QLineEdit;
        m_cvar_entries.emplace_back(text);
        entry_widget = text;
      }

      auto* curval_lbl = new QLabel(QString::fromStdString(prime::CVarValString(cvar.value)));
      m_cvar_current_vals.emplace_back(curval_lbl);

      auto* help_desc_lbl = new QLabel;
      help_desc_lbl->setPixmap(question_icon.pixmap(20));
      help_desc_lbl->setAlignment(Qt::AlignCenter);
      help_desc_lbl->setToolTip(QString::fromStdString(cvar.description));

      cvar_layout->addWidget(var_lbl, row, 0);
      cvar_layout->addWidget(entry_widget, row, 1, Qt::AlignRight);
      cvar_layout->addWidget(curval_lbl, row, 2, Qt::AlignRight);
      cvar_layout->addWidget(help_desc_lbl, row, 3, Qt::AlignRight);

      row++;
    }
    cvar_box->setLayout(cvar_layout);

    main_layout->addLayout(top_layout);
    main_layout->addWidget(cvar_box);
    main_layout->addStretch();
  }

  void OnLoadPresetPressed()
  {
    UpdatePresetsIndex();

    if (m_presets_dropdown->currentIndex() == -1)
    {
      ModalMessageBox error(this);
      error.setIcon(QMessageBox::Critical);
      error.setWindowTitle(tr("Error"));
      error.setText(tr("The profile '%1' does not exist").arg(m_presets_dropdown->currentText()));
      error.exec();
      return;
    }

    m_mod->apply_preset(m_presets_dropdown->currentText().toStdString());

    UpdateCurrentValues();
  }

  void OnSavePresetPressed()
  {
    UpdatePresetsIndex();

    const QString preset_name = m_presets_dropdown->currentText();

    m_mod->update_or_create_preset(preset_name.toStdString());
    m_mod->flush();

    if (m_presets_dropdown->findText(preset_name) == -1)
    {
      RebuildPresetsDropdown();
      m_presets_dropdown->setCurrentIndex(m_presets_dropdown->findText(preset_name));
    }
  }

  void RebuildPresetsDropdown()
  {
    m_presets_dropdown->clear();

    for (auto const& p : m_mod->saved_presets)
    {
      // Don't include the persistent preset in the dropdown
      if (p.is_persistent())
      {
        continue;
      }

      m_presets_dropdown->addItem(QString::fromStdString(p.name));
    }

    m_presets_dropdown->setCurrentIndex(-1);
  }

  void UpdatePresetsIndex()
  {
    const auto current_text = m_presets_dropdown->currentText();
    const int text_index = m_presets_dropdown->findText(current_text);
    m_presets_dropdown->setCurrentIndex(text_index);

    if (text_index == -1)
    {
      m_presets_dropdown->setCurrentText(current_text);
    }
  }

  void UpdateCurrentValues()
  {
    for (size_t i = 0; i < m_cvar_current_vals.size(); i++)
    {
      m_cvar_current_vals[i]->setText(QString::fromStdString(prime::CVarValString(m_mod->var_list[i].value)));
    }
  }

private:
  prime::ElfMod* m_mod;
  QCheckBox* m_enabled_checkbox;
  QComboBox* m_presets_dropdown;

  std::vector<std::variant<QCheckBox*, QLineEdit*>> m_cvar_entries;
  std::vector<QLabel*> m_cvar_current_vals;
};

ConfigureModWindow::ConfigureModWindow(std::string const& mod_name, QWidget* parent)
  : QDialog(parent), m_mod_name(mod_name)
{
  CreateMainLayout();
}

void ConfigureModWindow::CreateMainLayout()
{
  prime::ModPack* modpack = prime::GetPack(m_mod_name);
  ASSERT(modpack != nullptr);

  auto* const main_layout = new QVBoxLayout(this);
  auto* const tab_widget = new QTabWidget;

  main_layout->addWidget(tab_widget);

  for (prime::ElfMod& mod : modpack->supported_games)
  {
    ModConfigWidget* const mod_tab = new ModConfigWidget(&mod, this);
    QWidget* const wrapped_general = GetWrappedWidget(mod_tab);
    auto tab_title = fmt::format("{} {}", prime::game_str(mod.game), prime::region_str(mod.region));
    tab_widget->addTab(wrapped_general, QString::fromStdString(tab_title));
  }

  setWindowTitle(tr("%1 Mod Settings").arg(QString::fromStdString(modpack->name)));
}
