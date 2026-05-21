#include "DolphinQt/CVarDataModel.h"

QVariant CVarDataModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
  {
    return QVariant();
  }

  prime::CVar const& cvar = m_mod->var_list[index.row()];

  if (role == Qt::ToolTipRole)
  {
    return QString(QStringLiteral("%1\nDefault: %2"))
      .arg(QString::fromStdString(cvar.description))
      .arg(QString::fromStdString(prime::CVarValString(cvar.def)));
  }

  switch (static_cast<Column>(index.column()))
  {
    case Column::Name:
      if (role == Qt::DisplayRole)
      {
        return QString::fromStdString(cvar.name);
      }
      break;
    case Column::Value:
      if (role == Qt::DisplayRole && cvar.type != prime::CVarType::BOOLEAN)
      {
        return QString::fromStdString(prime::CVarValString(cvar.value));
      }
      else if (role == Qt::CheckStateRole && cvar.type == prime::CVarType::BOOLEAN)
      {
        return std::get<bool>(cvar.value) ? Qt::Checked : Qt::Unchecked;
      }
      break;
    default:
      break;
  }
  return QVariant();
}

QVariant CVarDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
  {
    return QVariant();
  }
  switch (static_cast<Column>(section))
  {
    case Column::Name:
      return tr("Name");
    case Column::Value:
      return tr("Value");
    default:
      return QVariant();
  }
}

bool CVarDataModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!index.isValid())
  {
    return false;
  }
  if (static_cast<Column>(index.column()) != Column::Value)
  {
    return false;
  }

  prime::CVar& cvar = m_mod->var_list[index.row()];
  if (role == Qt::EditRole && cvar.type != prime::CVarType::BOOLEAN)
  {
    std::string str_val = value.toString().toStdString();
    auto parse = prime::ParseCvarValue(cvar.type, str_val);
    if (parse)
    {
      cvar.value = *parse;
      emit dataChanged(index, index);
      return true;
    }
    else
    {
      return false;
    }
  }
  else if (role == Qt::CheckStateRole && cvar.type == prime::CVarType::BOOLEAN)
  {
    cvar.value = value.toUInt() == Qt::Checked ? true : false;
    emit dataChanged(index, index);
    return true;
  }
  return false;
}

int CVarDataModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
  {
    return 0;
  }
  return static_cast<int>(m_mod->var_list.size());
}

int CVarDataModel::columnCount(const QModelIndex& parent) const
{
  return static_cast<int>(Column::ColumnCount);
}

Qt::ItemFlags CVarDataModel::flags(const QModelIndex& index) const
{
  if (!index.isValid())
  {
    return Qt::NoItemFlags;
  }
  if (static_cast<Column>(index.column()) == Column::Value)
  {
    if (m_mod->var_list[index.row()].type == prime::CVarType::BOOLEAN)
    {
      return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
    }
    else
    {
      return Qt::ItemIsEditable | Qt::ItemIsEnabled;
    }
  }

  return Qt::ItemIsEnabled;
}

bool CVarDataModel::ShouldDisplayCVar(const QString& filter, int row) const
{
  if (row < 0 || row >= static_cast<int>(m_mod->var_list.size()))
  {
    return false;
  }

  return QString::fromStdString(m_mod->var_list[row].name).contains(filter, Qt::CaseInsensitive);
}

void CVarDataModel::DataChanged()
{
  emit dataChanged(createIndex(0, static_cast<int>(Column::Value)),
                   createIndex(NumRows() - 1, static_cast<int>(Column::Value)));
}

int CVarDataModel::NumRows() const
{
  return static_cast<int>(m_mod->var_list.size());
}
