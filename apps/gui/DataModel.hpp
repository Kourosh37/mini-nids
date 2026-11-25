#pragma once
#include <QAbstractTableModel>
#include <QHash>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <atomic>
#include <memory>

struct sqlite3;
struct sqlite3_stmt;
enum class DataKind { Packets, Flows, Alerts, Dns };
struct DataRow {
    QVariantList cells;
    QString details;
    quint64 offset{};
    quint32 captured{};
    int source_port{-1}, destination_port{-1};
    qlonglong timestamp{};
};

// One private, automatically removed database per analysis. Only the analysis thread writes.
class SessionStore {
  public:
    SessionStore();
    ~SessionStore();
    SessionStore(const SessionStore &) = delete;
    SessionStore &operator=(const SessionStore &) = delete;
    void append(DataKind kind, const DataRow &row, quint64 sequence = 0);
    void flush();
    QString fileName() const;

  private:
    QTemporaryDir directory_;
    sqlite3 *database_{};
    sqlite3_stmt *insert_[4]{};
    quint64 counts_[4]{};
    unsigned pending_{};
};

// The model holds just one page. Search, count and global sorting run away from the UI thread.
class DataProxy : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(qulonglong totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(qulonglong matchCount READ matchCount NOTIFY countChanged)
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY countChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY countChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY countChanged)
    Q_PROPERTY(bool queryBusy READ queryBusy NOTIFY countChanged)
    Q_PROPERTY(QString queryError READ queryError NOTIFY countChanged)
    Q_PROPERTY(QString searchHelp READ searchHelp CONSTANT)
    Q_PROPERTY(QStringList searchExamples READ searchExamples CONSTANT)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int sortedColumn READ sortedColumn NOTIFY countChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY countChanged)
  public:
    explicit DataProxy(DataKind kind, QStringList columns, QObject *parent = nullptr);
    ~DataProxy() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const {
        return rowCount();
    }
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void attach(std::shared_ptr<SessionStore> store);
    void refresh();
    qulonglong totalCount() const {
        return total_;
    }
    qulonglong matchCount() const {
        return matches_;
    }
    int page() const {
        return page_;
    }
    int pageSize() const {
        return page_size_;
    }
    int pageCount() const;
    bool queryBusy() const {
        return busy_;
    }
    QString queryError() const {
        return error_;
    }
    QString searchHelp() const;
    QStringList searchExamples() const;
    QString filter() const {
        return filter_;
    }
    int sortedColumn() const {
        return sorted_column_;
    }
    bool sortDescending() const {
        return descending_;
    }
    void setFilter(const QString &value);
    void setPage(int value);
    void setPageSize(int value);
    void setNanosecondResolution(bool value) {
        nanosecond_resolution_ = value;
    }
    Q_INVOKABLE void sortColumn(int column);
    Q_INVOKABLE QString rowSummary(int row) const;
    const DataRow *sourceRow(int row) const;
  signals:
    void countChanged();
    void filterChanged();

  private:
    void schedule(int delay = 0);
    void query();
    DataKind kind_;
    QStringList columns_;
    QVector<DataRow> rows_;
    std::shared_ptr<SessionStore> store_;
    QThreadPool pool_;
    QTimer debounce_;
    std::atomic<quint64> generation_{};
    quint64 total_{}, matches_{};
    int page_{}, page_size_{100}, sorted_column_{-1};
    bool descending_{}, busy_{}, nanosecond_resolution_{true};
    QString filter_, error_;
};
