#include "DolphinQt/ConfigureModWindow.h"

#include "Common/Assert.h"
#include "Core/PrimeHack/ElfModLoaderInterface.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEnterEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QTabWidget>
#include <QToolTip>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
class InstantTooltipLabel : public QLabel
{
  QString m_tt_text;

public:
  InstantTooltipLabel(QString const& text) : QLabel(), m_tt_text(text) {}

  void enterEvent(QEnterEvent* event) override
  {
    QToolTip::showText(event->globalPosition().toPoint(), m_tt_text, this);
  }
};
}

class ModConfigWidget : public QWidget
{
public:
  explicit ModConfigWidget(prime::ElfMod* mod, QDialog* parent)
    : QWidget(parent), m_mod(mod), m_dialog_parent(parent)
  {
    CreateMainLayout();
  }

private:
  void CreateMainLayout()
  {
    auto* const main_layout = new QVBoxLayout(this);

    auto* const preset_box = new QGroupBox(tr("Presets"));
    auto* const cvar_box = new QGroupBox(tr("CVars"));

    auto* const top_layout = new QHBoxLayout;
    auto* const cvar_layout = new QGridLayout;

    // Presets Box
    auto* const preset_layout = new QHBoxLayout;
    auto* const preset_buttons_layout = new QHBoxLayout;
    auto* preset_load_button = new QPushButton(tr("Load"));
    auto* preset_save_button = new QPushButton(tr("Save"));
    auto* default_button = new QPushButton(tr("Default"));
    m_presets_dropdown = new QComboBox;
    m_presets_dropdown->setMinimumWidth(200);
    m_presets_dropdown->setEditable(true);
    preset_layout->addWidget(m_presets_dropdown);
    preset_buttons_layout->addWidget(preset_load_button);
    preset_buttons_layout->addWidget(preset_save_button);
    preset_buttons_layout->addWidget(default_button);
    preset_layout->addLayout(preset_buttons_layout);
    preset_box->setLayout(preset_layout);
    RebuildPresetsDropdown();

    connect(preset_load_button, &QPushButton::clicked, this, &ModConfigWidget::OnLoadPresetPressed);
    connect(preset_save_button, &QPushButton::clicked, this, &ModConfigWidget::OnSavePresetPressed);
    connect(default_button, &QPushButton::pressed, this, [this] {
      m_mod->load_defaults();
      UpdateCurrentValues();
    });

    // Top area
    top_layout->addWidget(preset_box);

    // CVars Box
    int row = 0;
    const QIcon question_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);

    size_t cvar_idx = 0;
    for (auto const& cvar : m_mod->var_list)
    {
      auto* var_lbl = new QLabel(QString::fromStdString(cvar.name + ":"));
      QWidget* entry_widget;
      QHBoxLayout* h_layout = new QHBoxLayout;
      if (cvar.type == prime::CVarType::BOOLEAN)
      {
        auto* checkbox = new QCheckBox;
        checkbox->setChecked(std::get<bool>(cvar.value));
        m_cvar_entries.emplace_back(cvar_idx, checkbox);
        entry_widget = checkbox;
      }
      else
      {
        auto* text = new QLineEdit;
        m_cvar_entries.emplace_back(cvar_idx, text);
        entry_widget = text;
      }

      auto* curval_lbl = new QLabel(QString::fromStdString(prime::CVarValString(cvar.value)));
      m_cvar_current_vals.emplace_back(curval_lbl);

      QString help_desc = QStringLiteral("%1\nDefault value: %2")
        .arg(cvar.description)
        .arg(prime::CVarValString(cvar.def));
      auto* help_desc_lbl = new InstantTooltipLabel(help_desc);
      help_desc_lbl->setPixmap(question_icon.pixmap(20));
      help_desc_lbl->setAlignment(Qt::AlignCenter);

      cvar_layout->addWidget(var_lbl, row, 0);
      curval_lbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
      {
        QFontMetrics fm(curval_lbl->font());
        // Widest reasonable value with extra padding
        curval_lbl->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("0.0000000e+00")) + 10);
      }
      help_desc_lbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
      entry_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
      entry_widget->setMinimumWidth(250);
      h_layout->addWidget(curval_lbl, Qt::AlignLeft);
      h_layout->addStretch();
      h_layout->addWidget(entry_widget, Qt::AlignRight);
      h_layout->addWidget(help_desc_lbl, Qt::AlignLeft);
      cvar_layout->addLayout(h_layout, row, 1);

      row++;
      cvar_idx++;
    }
    cvar_layout->setVerticalSpacing(5);
    cvar_layout->setHorizontalSpacing(5);
    cvar_box->setLayout(cvar_layout);


    auto* button_box = new QDialogButtonBox(
      QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(button_box->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &ModConfigWidget::ApplyChanges);
    connect(button_box, &QDialogButtonBox::accepted, this, [this] {
      ApplyChanges();
      m_dialog_parent->accept();
    });
    connect(button_box, &QDialogButtonBox::rejected, m_dialog_parent, &QDialog::reject);
    main_layout->addLayout(top_layout);
    main_layout->addWidget(cvar_box);
    main_layout->addStretch();
    main_layout->addWidget(button_box);
  }

  void ApplyChanges()
  {
    for (auto const& [var_idx, entry] : m_cvar_entries)
    {
      prime::CVar& cvar = m_mod->var_list[var_idx];
      if (auto ent_bool_p = std::get_if<QCheckBox*>(&entry); ent_bool_p != nullptr)
      {
        ASSERT(cvar.type == prime::CVarType::BOOLEAN);
        auto* ent_bool = *ent_bool_p;
        cvar.value = ent_bool->isChecked();
      }
      else if (auto ent_str_p = std::get_if<QLineEdit*>(&entry); ent_str_p != nullptr)
      {
        auto* ent_str = *ent_str_p;
        ent_str->setPalette(QPalette());
        ent_str->setToolTip(QStringLiteral(""));
        const std::string contents = ent_str->text().trimmed().toStdString();
        if (contents.empty())
        {
          continue;
        }

        auto prs_res = prime::ParseCvarValue(cvar.type, contents);
        if (prs_res)
        {
          cvar.value = *prs_res;
        }
        else
        {
          QPalette err_colors;
          err_colors.setColor(QPalette::Base, Qt::darkRed);
          ent_str->setPalette(err_colors);
          switch (cvar.type)
          {
            case prime::CVarType::FLOAT32:
            case prime::CVarType::FLOAT64:
              ent_str->setToolTip(QStringLiteral("Invalid value, expected a float"));
              break;
            case prime::CVarType::INT8:
            case prime::CVarType::INT16:
            case prime::CVarType::INT32:
            case prime::CVarType::INT64:
              ent_str->setToolTip(QStringLiteral("Invalid value, expected a positive integer"));
              break;
            default:
              break;
          }
        }
      }
    }

    m_mod->flush();
    UpdateCurrentValues();
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
      m_cvar_current_vals[i]->setText(
        QString::fromStdString(prime::CVarValString(m_mod->var_list[i].value)));
    }
  }

private:
  prime::ElfMod* m_mod;
  QDialog* m_dialog_parent;
  QComboBox* m_presets_dropdown;

  std::vector<std::pair<size_t, std::variant<QCheckBox*, QLineEdit*>>> m_cvar_entries;
  std::vector<QLabel*> m_cvar_current_vals;
};

ConfigureModWindow::ConfigureModWindow(std::string const& mod_name, QWidget* parent)
  : StackedSettingsWindow(parent, false), m_mod_name(mod_name)
{
  CreateMainLayout();
}

void ConfigureModWindow::CreateMainLayout()
{
  prime::ModPack* modpack = prime::GetPack(m_mod_name);
  ASSERT(modpack != nullptr);

  for (prime::ElfMod& mod : modpack->supported_games)
  {
    ModConfigWidget* const mod_tab = new ModConfigWidget(&mod, this);
    auto tab_title = std::string(prime::game_str(mod.game));
    AddWrappedPane(mod_tab, QString::fromStdString(tab_title));
  }
  OnDoneCreatingPanes();

  setWindowTitle(tr("%1 Mod Settings").arg(QString::fromStdString(modpack->name)));
}
