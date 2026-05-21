// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GameList/GridProxyModel.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSize>

#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/Resources.h"

#include "Core/Config/UISettings.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include "UICommon/GameFile.h"

const QSize LARGE_BANNER_SIZE(144, 48);

using namespace prime;

GridProxyModel::GridProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
}

QVariant GridProxyModel::data(const QModelIndex& i, int role) const
{
  auto* model = static_cast<GameListModel*>(sourceModel());
  QModelIndex source_index = mapToSource(i);

  auto support_level = GetGameSupportLevel(*model->GetGameFile(source_index.row()));
  if (role == Qt::DisplayRole)
  {
    return sourceModel()->data(
        sourceModel()->index(source_index.row(), static_cast<int>(GameListModel::Column::Title)),
        Qt::DisplayRole);
  }
  else if (role == Qt::DecorationRole)
  {
    const auto& buffer = model->GetGameFile(source_index.row())->GetCoverImage().buffer;

    QSize size = Config::Get(Config::MAIN_USE_GAME_COVERS) ? QSize(160, 224) : LARGE_BANNER_SIZE;
    QPixmap pixmap(size * model->GetScale() * QPixmap().devicePixelRatio());

    constexpr auto draw_supp_pixmap =
      [](QPainter& painter, QPixmap& canvas, GameSupportLevel supp_level) {
      int supp_width = canvas.width() / 5;
      int supp_height = supp_width;
      int supp_x = canvas.width() - supp_width;
      int supp_y = canvas.height() - supp_height;
      QPixmap supp_pixmap = Resources::GetResourceIcon(GetIconNameForSupportLevel(supp_level))
        .pixmap(QSize(supp_width, supp_height), 1);
      painter.drawPixmap(supp_x, supp_y, supp_width, supp_height, supp_pixmap);
    };

    if (buffer.empty() || !Config::Get(Config::MAIN_USE_GAME_COVERS))
    {
      QPixmap banner = model
                           ->data(model->index(source_index.row(),
                                               static_cast<int>(GameListModel::Column::Banner)),
                                  Qt::DecorationRole)
                           .value<QPixmap>();

      banner = banner.scaled(pixmap.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

      pixmap.fill();

      QPainter painter(&pixmap);

      painter.drawPixmap(0, pixmap.height() / 2 - banner.height() / 2, banner.width(),
                         banner.height(), banner);
      draw_supp_pixmap(painter, pixmap, support_level);

      return pixmap;
    }
    else
    {
      pixmap = QPixmap::fromImage(QImage::fromData(
          reinterpret_cast<const unsigned char*>(&buffer[0]), static_cast<int>(buffer.size())));

      QPainter painter(&pixmap);
      draw_supp_pixmap(painter, pixmap, support_level);

      return pixmap.scaled(QSize(160, 224) * model->GetScale() * pixmap.devicePixelRatio(),
                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }
  else if (role == Qt::ToolTipRole)
  {
    if (support_level != GameSupportLevel::NotApplicable)
    {
      return QString::fromStdString(std::string(SupportLevelToolTip(support_level)));
    }
  }
  return QVariant();
}

bool GridProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  GameListModel* glm = qobject_cast<GameListModel*>(sourceModel());
  return glm->ShouldDisplayGameListItem(source_row);
}

bool GridProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
  if (left.data(GameListModel::SORT_ROLE) != right.data(GameListModel::SORT_ROLE))
    return QSortFilterProxyModel::lessThan(left, right);

  // If two items are otherwise equal, compare them by their title
  const auto right_title = sourceModel()
                               ->index(right.row(), static_cast<int>(GameListModel::Column::Title))
                               .data()
                               .toString();
  const auto left_title = sourceModel()
                              ->index(left.row(), static_cast<int>(GameListModel::Column::Title))
                              .data()
                              .toString();

  if (sortOrder() == Qt::AscendingOrder)
    return left_title < right_title;

  return right_title < left_title;
}
