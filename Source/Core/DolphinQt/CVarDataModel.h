#pragma once

#include "Core/PrimeHack/ElfModLoaderInterface.h"

#include <QAbstractTableModel>

class CVarDataModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  explicit CVarDataModel(prime::ElfMod* mod, QObject* parent = nullptr)
    : QAbstractTableModel(parent), m_mod(mod) {}

  enum class Column
  {
    Name,
    Value,

    ColumnCount,
  };

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  bool ShouldDisplayCVar(const QString& filter, int row) const;
  void DataChanged();
  int NumRows() const;

private:
  prime::ElfMod* m_mod;
};
