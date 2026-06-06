// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <qobject.h>
#include <QString>

ModalMessageBox::ModalMessageBox(QWidget* parent, Qt::WindowModality modality)
    : QMessageBox(parent != nullptr ? parent->window() : nullptr)
{
  setWindowModality(modality);
  setWindowFlags(Qt::Sheet | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

  // No parent is still preferable to showing a hidden parent here.
  if (parent != nullptr && !parent->window()->isVisible())
    setParent(nullptr);
}

static inline int ExecMessageBox(ModalMessageBox::Icon icon, QWidget* parent, const QString& title,
                                 const QString& text, ModalMessageBox::StandardButtons buttons,
                                 ModalMessageBox::StandardButton default_button,
                                 Qt::WindowModality modality, QString detailed_text)
{
  ModalMessageBox msg(parent, modality);
  msg.setIcon(icon);
  msg.setWindowTitle(title);
  msg.setText(text);
  msg.setStandardButtons(buttons);
  msg.setDefaultButton(default_button);
  msg.setDetailedText(detailed_text);

  return msg.exec();
}

static inline int ExecPrimeHackMessage(QWidget* parent)
{
  ModalMessageBox msg(parent, Qt::WindowModal);
  msg.setIcon(QMessageBox::Information);
  msg.setWindowTitle(QString::fromStdString("Advisory"));
  msg.setTextFormat(Qt::RichText);
  msg.setText(QString::fromStdString(
    "<p>PrimeHack has detected it is being ran for the first time:"
    "</p><p>"
    "It is strongly recommended to click the <b>Default</b> button for whichever input method is being used to ensure the intended defaults are loaded.\nIt is not required, but be advised any existing settings <i>may</i> cause conflicts."
    "</p><p>"
    "If you have any further questions, please see our <a href='https://github.com/shiiion/dolphin/wiki'>wiki</a> or visit our <a href='https://discord.gg/Gc2HcPH'>Discord</a>.</p>"));
  msg.setStandardButtons(QMessageBox::Ok);
  msg.addButton(QMessageBox::Help);
  msg.setDefaultButton(QMessageBox::NoButton);

  return msg.exec();
}

static inline int ExecPrimeHackWiiTabMessage(QWidget* parent)
{
  ModalMessageBox msg(parent, Qt::WindowModal);
  msg.setIcon(QMessageBox::Information);
  msg.setWindowTitle(QString::fromStdString("PrimeHack"));
  msg.setTextFormat(Qt::RichText);
  msg.setText(QString::fromStdString(
    "It is strongly recommended to use the <b>PrimeHack</b> preset, the <b>Emullated Wii Remote</b> option still works, "
    "but lacks certain features and options that make for a more user-friendly experience."
    "</p><p>"
    "To proceed to the PrimeHack window, press <b>Open</b>.<br>"
));
  msg.setStandardButtons(QMessageBox::StandardButton::Ignore);
  msg.addButton(QMessageBox::StandardButton::Open);
  msg.setDefaultButton(QMessageBox::NoButton);

  return msg.exec();
}

static inline int ExecPrimeHackGCTabMessage(QWidget* parent)
{
  ModalMessageBox msg(parent, Qt::WindowModal);
  msg.setIcon(QMessageBox::Information);
  msg.setWindowTitle(QString::fromStdString("PrimeHack"));
  msg.setTextFormat(Qt::RichText);
  msg.setText(QString::fromStdString(
    "It is strongly recommended to use the <b>PrimeHack</b> preset, the <b>Standard Controller</b> option still works, "
    "but lacks certain features and options that make for a more user-friendly experience."
    "</p><p>"
    "To proceed to the PrimeHack window, press <b>Open</b>.<br>"));
  msg.setStandardButtons(QMessageBox::StandardButton::Ignore);
  msg.addButton(QMessageBox::StandardButton::Open);
  msg.setDefaultButton(QMessageBox::NoButton);

  return msg.exec();
}

void ModalMessageBox::primehack_initialrun(QWidget* parent)
{
  if (ExecPrimeHackMessage(parent) == QMessageBox::Help) {
    QDesktopServices::openUrl(QUrl(QString::fromStdString("https://github.com/shiiion/dolphin/wiki/Installation")));
  }
}

bool ModalMessageBox::primehack_wiitab(QWidget* parent)
{
  if (ExecPrimeHackWiiTabMessage(parent) == QMessageBox::Open) {
    return true;
  }

  return false;
}

bool ModalMessageBox::primehack_gctab(QWidget* parent)
{
  if (ExecPrimeHackGCTabMessage(parent) == QMessageBox::Open) {
    return true;
  }

  return false;
}

int ModalMessageBox::critical(QWidget* parent, const QString& title, const QString& text,
                              StandardButtons buttons, StandardButton default_button,
                              Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Critical, parent, title, text, buttons, default_button,
                        modality, detailedText);
}

int ModalMessageBox::information(QWidget* parent, const QString& title, const QString& text,
                                 StandardButtons buttons, StandardButton default_button,
                                 Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Information, parent, title, text, buttons, default_button,
                        modality, detailedText);
}

int ModalMessageBox::primehackInitial()
{
	return 0;
}

int ModalMessageBox::question(QWidget* parent, const QString& title, const QString& text,
                              StandardButtons buttons, StandardButton default_button,
                              Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Warning, parent, title, text, buttons, default_button,
                        modality, detailedText);
}

int ModalMessageBox::warning(QWidget* parent, const QString& title, const QString& text,
                             StandardButtons buttons, StandardButton default_button,
                             Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Warning, parent, title, text, buttons, default_button,
                        modality, detailedText);
}
