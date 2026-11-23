#pragma once
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStringList>

struct DataRow {
    QVariantList cells;
    QString details;
    quint64 offset{};
    quint32 captured{};
};
class DataModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    explicit DataModel(QStringList columns, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QVector<DataRow> rows);
    const DataRow *row(int index) const;

  private:
    QStringList columns_;
    QVector<DataRow> rows_;
};
class DataProxy : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
  public:
    explicit DataProxy(DataModel *model, QObject *parent = nullptr);
    int count() const {
        return rowCount();
    }
    QString filter() const {
        return filter_;
    }
    void setFilter(const QString &value);
    Q_INVOKABLE void sortColumn(int column);
    Q_INVOKABLE QString rowSummary(int row) const;
    const DataRow *sourceRow(int row) const;
  signals:
    void countChanged();
    void filterChanged();

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &parent) const override;

  private:
    QString filter_;
    int sorted_column_{-1};
    Qt::SortOrder order_{Qt::AscendingOrder};
};
