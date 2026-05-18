#include "DolphinQt/ConfigureModWindow.h"

#include "Common/Assert.h"
#include "Core/PrimeHack/ElfModLoaderInterface.h"
#include "Core/PrimeHack/HackManager.h"
#include "DolphinQt/CVarDataModel.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/SearchBar.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

template <int Rows>
class SizedTableView : public QTableView
{
public:
  SizedTableView(QWidget* parent = nullptr) : QTableView(parent) {}

  QSize sizeHint() const override
  {
    QSize base_size = QTableView::sizeHint();
    int height = (std::min(model()->rowCount() + 1, Rows)) * horizontalHeader()->height();
    return QSize(base_size.width(), height);
  }
};

using DefaultSizedTableView = SizedTableView<15>;

class ModConfigWidget : public QWidget
{
public:
  explicit ModConfigWidget(prime::ElfMod* mod, QDialog* parent)
    : QWidget(parent), m_model(nullptr), m_mod(mod)
  {
    CreateMainLayout();
  }

  void ShowSearchBar()
  {
    m_search_bar->Show();
  }

  void HideSearchBar()
  {
    m_search_bar->Hide();
  }

  void FlushChanges()
  {
    m_mod->flush();
  }

private:
  using Column = CVarDataModel::Column;

  void CreateMainLayout()
  {
    auto* const main_layout = new QVBoxLayout(this);

    auto* const preset_box = new QGroupBox(tr("Presets"));
    auto* const cvar_box = new QGroupBox(tr("CVars"));

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
    preset_load_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    preset_save_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    default_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    preset_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    RebuildPresetsDropdown();

    connect(preset_load_button, &QPushButton::clicked, this, &ModConfigWidget::OnLoadPresetPressed);
    connect(preset_save_button, &QPushButton::clicked, this, &ModConfigWidget::OnSavePresetPressed);
    connect(default_button, &QPushButton::pressed, this, [this] {
      m_mod->load_defaults();
      m_model->DataChanged();
    });

    // CVars Box
    auto cvar_layout = new QHBoxLayout;
    m_model = new CVarDataModel(m_mod);
    m_table = new DefaultSizedTableView;
    m_table->setModel(m_model);
    m_table->setShowGrid(false);
    m_table->setCurrentIndex(QModelIndex());
    m_table->setSortingEnabled(false);
    m_table->setFrameStyle(QFrame::NoFrame);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);

    m_table->horizontalHeader()->setSectionResizeMode(
      static_cast<int>(Column::Name), QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
      static_cast<int>(Column::Value), QHeaderView::Stretch);
    m_table->setColumnWidth(static_cast<int>(Column::Value), 250);
    m_table->verticalHeader()->hide();
    cvar_layout->addWidget(m_table);
    cvar_box->setLayout(cvar_layout);
    cvar_box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    // Search Bar
    m_search_bar = new SearchBar(this);
    m_search_bar->SetPlaceholderText(tr("Search CVars..."));
    connect(m_search_bar, &SearchBar::Search, this, &ModConfigWidget::UpdateFilter);

    main_layout->addWidget(preset_box);
    main_layout->addWidget(cvar_box);
    main_layout->addWidget(m_search_bar);
  }

  void UpdateFilter(const QString& filter)
  {
    m_var_filter = filter;
    RefreshFilter();
  }

  void RefreshFilter()
  {
    if (m_var_filter.isEmpty())
    {
      for (int i = 0; i < m_model->NumRows(); i++)
      {
        m_table->setRowHidden(i, false);
      }
    }
    else
    {
      for (int i = 0; i < m_model->NumRows(); i++)
      {
        m_table->setRowHidden(i, !m_model->ShouldDisplayCVar(m_var_filter, i));
      }
    }
  }

  void OnLoadPresetPressed()
  {
    UpdatePresetsIndex();

    if (m_presets_dropdown->currentIndex() == -1)
    {
      ModalMessageBox error(this);
      error.setIcon(QMessageBox::Critical);
      error.setWindowTitle(tr("Error"));
      error.setText(tr("The preset '%1' does not exist").arg(m_presets_dropdown->currentText()));
      error.exec();
      return;
    }

    m_mod->apply_preset(m_presets_dropdown->currentText().toStdString());
    m_model->DataChanged();

    // Probe the table view to update its values
    m_table->update();
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

private:
  CVarDataModel* m_model;
  DefaultSizedTableView* m_table;
  prime::ElfMod* m_mod;
  QComboBox* m_presets_dropdown;
  SearchBar* m_search_bar;
  QString m_var_filter;
};

ConfigureModWindow::ConfigureModWindow(std::string const& mod_name, QWidget* parent)
  : StackedSettingsWindow(parent), m_mod_name(mod_name)
{
  CreateMainLayout();
}

void ConfigureModWindow::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape)
  {
    auto* cfg_widget = static_cast<ModConfigWidget*>(GetActivePane());
    cfg_widget->HideSearchBar();
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void ConfigureModWindow::CreateMainLayout()
{
  prime::ModPack* modpack = prime::GetPack(m_mod_name);
  ASSERT(modpack != nullptr);

  int active_index = 0;
  int current_index = 0;
  const prime::Game active_game = prime::GetActiveGame();
  const prime::Region active_region = prime::GetActiveRegion();
  for (prime::ElfMod& mod : modpack->supported_games)
  {
    if (mod.game == active_game && mod.region == active_region)
    {
      active_index = current_index;
    }
    ModConfigWidget* const mod_tab = new ModConfigWidget(&mod, this);
    connect(this, &ConfigureModWindow::accepted, mod_tab, &ModConfigWidget::FlushChanges);
    connect(this, &ConfigureModWindow::rejected, mod_tab, &ModConfigWidget::FlushChanges);
    auto tab_title = std::string(prime::game_str(mod.game));
    AddPane(mod_tab, QString::fromStdString(tab_title));
    current_index++;
  }
  OnDoneCreatingPanes();
  // Set active pane to the currently active game (or first if not present)
  ActivatePane(active_index);

  addAction(tr("Search"), QKeySequence::Find, this, &ConfigureModWindow::ShowSearch);
  addAction(tr("Close"), QKeySequence::Close, this, &ConfigureModWindow::reject);

  setWindowTitle(tr("%1 Mod Settings").arg(QString::fromStdString(modpack->name)));
}

void ConfigureModWindow::ShowSearch()
{
  auto* cfg_widget = static_cast<ModConfigWidget*>(GetActivePane());
  cfg_widget->ShowSearchBar();
}
