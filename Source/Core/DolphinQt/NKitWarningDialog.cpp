// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NKitWarningDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "DolphinQt/Resources.h"

bool NKitWarningDialog::ShowUnlessDisabled(QWidget* parent)
{
  if (Config::Get(Config::MAIN_SKIP_NKIT_WARNING))
    return true;

  NKitWarningDialog dialog(parent);
  return dialog.exec() == QDialog::Accepted;
}

NKitWarningDialog::NKitWarningDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Warning"));
  setWindowIcon(Resources::GetAppIcon());

  QVBoxLayout* main_layout = new QVBoxLayout;

  QLabel* warning = new QLabel(
      tr("This dump is compressed with NKit, this format is known to cause various issues ranging from performance to unexpected game behavior.<br><br> Issues include, but are not limited to:<br>"
         "• Glitches and other unexpected behavior<br>"
         "• Crashes<br>"
         "• Longer load times<br>"
         "• TAS recordings are incompatible with uncompressed dumps<br>"
         "• Savestates are incompatible with uncompressed dumps<br>"
         "• NKit is not backwards-compatible with older buillds of Dolphin<br><br>"
         "Continue anyway?<br>"      
         "<a href=\"https://dolphin-emu.org/blog/2020/07/05/dolphin-progress-report-may-and-june-2020/#about-the-nkit-format\">More information</a><br>"));
  warning->setTextInteractionFlags(Qt::TextBrowserInteraction);
  warning->setOpenExternalLinks(true);
  warning->setTextFormat(Qt::RichText);
  warning->setWordWrap(true);
  main_layout->addWidget(warning);

  QCheckBox* checkbox_accept = new QCheckBox(tr("I accept the risks and want to continue"));
  main_layout->addWidget(checkbox_accept);

  QCheckBox* checkbox_skip = new QCheckBox(tr("Don't show this again"));
  main_layout->addWidget(checkbox_skip);

  QHBoxLayout* button_layout = new QHBoxLayout;
  QPushButton* ok = new QPushButton(tr("OK"));
  button_layout->addWidget(ok);
  QPushButton* cancel = new QPushButton(tr("Cancel"));
  button_layout->addWidget(cancel);
  main_layout->addLayout(button_layout);

  QHBoxLayout* top_layout = new QHBoxLayout;

  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
  QLabel* icon_label = new QLabel;
  icon_label->setPixmap(icon.pixmap(100));
  icon_label->setAlignment(Qt::AlignTop);
  top_layout->addWidget(icon_label);
  top_layout->addSpacing(10);

  top_layout->addLayout(main_layout);

  setLayout(top_layout);

  connect(ok, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

  ok->setEnabled(false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(checkbox_accept, &QCheckBox::checkStateChanged,
          [ok](Qt::CheckState state) { ok->setEnabled(state == Qt::Checked); });
#else
  connect(checkbox_accept, &QCheckBox::stateChanged,
          [ok](int state) { ok->setEnabled(state == Qt::Checked); });
#endif

  connect(this, &QDialog::accepted, [checkbox_skip] {
    Config::SetBase(Config::MAIN_SKIP_NKIT_WARNING, checkbox_skip->isChecked());
  });
}
