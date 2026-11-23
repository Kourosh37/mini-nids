#include "DataModel.hpp"
DataModel::DataModel(QStringList columns, QObject *parent)
    : QAbstractTableModel(parent), columns_(std::move(columns)) {}
int DataModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}
int DataModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(columns_.size());
}
QVariant DataModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size() || index.column() < 0 ||
        index.column() >= columns_.size())
        return {};
    if (role == Qt::DisplayRole || role == Qt::UserRole)
        return rows_[index.row()].cells.value(index.column());
    return {};
}
QVariant DataModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section < 0 || section >= columns_.size())
        return {};
    return columns_[section];
}
QHash<int, QByteArray> DataModel::roleNames() const {
    return {{Qt::DisplayRole, "display"}};
}
void DataModel::replace(QVector<DataRow> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}
const DataRow *DataModel::row(int index) const {
    return index >= 0 && index < rows_.size() ? &rows_[index] : nullptr;
}
DataProxy::DataProxy(DataModel *model, QObject *parent) : QSortFilterProxyModel(parent) {
    setSourceModel(model);
    setSortRole(Qt::UserRole);
    connect(this, &QAbstractItemModel::modelReset, this, &DataProxy::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &DataProxy::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &DataProxy::countChanged);
}
void DataProxy::setFilter(const QString &value) {
    if (filter_ == value)
        return;
    filter_ = value;
    invalidateFilter();
    emit filterChanged();
    emit countChanged();
}
bool DataProxy::filterAcceptsRow(int row, const QModelIndex &) const {
    const auto terms = filter_.split(' ', Qt::SkipEmptyParts);
    const auto *value = static_cast<const DataModel *>(sourceModel())->row(row);
    if (!value)
        return false;
    QString searchable;
    for (const auto &cell : value->cells)
        searchable += cell.toString() + ' ';
    for (const auto &term : terms)
        if (!searchable.contains(term, Qt::CaseInsensitive))
            return false;
    return true;
}
void DataProxy::sortColumn(int column) {
    order_ =
        sorted_column_ == column && order_ == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    sorted_column_ = column;
    sort(column, order_);
}
const DataRow *DataProxy::sourceRow(int row) const {
    if (row < 0 || row >= rowCount())
        return nullptr;
    return static_cast<const DataModel *>(sourceModel())->row(mapToSource(index(row, 0)).row());
}
QString DataProxy::rowSummary(int row) const {
    const auto *r = sourceRow(row);
    if (!r)
        return {};
    QStringList cells;
    for (const auto &cell : r->cells)
        cells << cell.toString();
    return cells.join("  |  ");
}
