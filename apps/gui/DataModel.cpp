#include "DataModel.hpp"
#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <mininids/common/Types.hpp>
#include <sqlite3.h>
#include <stdexcept>

namespace {
QString table(DataKind kind) {
    return QStringList{"packets", "flows", "alerts", "dns"}[static_cast<int>(kind)];
}
void check(int code, sqlite3 *db) {
    if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW)
        throw std::runtime_error(sqlite3_errmsg(db));
}
void execute(sqlite3 *db, const QString &sql) {
    check(sqlite3_exec(db, sql.toUtf8().constData(), nullptr, nullptr, nullptr), db);
}
struct Statement {
    sqlite3_stmt *value{};
    explicit Statement(sqlite3 *db, const QString &sql) {
        check(sqlite3_prepare_v2(db, sql.toUtf8().constData(), -1, &value, nullptr), db);
    }
    ~Statement() {
        sqlite3_finalize(value);
    }
};
void bind(sqlite3_stmt *statement, int index, const QVariant &value) {
    int result;
    if (!value.isValid() || value.isNull())
        result = sqlite3_bind_null(statement, index);
    else if (value.metaType().id() == QMetaType::QString) {
        const auto bytes = value.toString().toUtf8();
        result =
            sqlite3_bind_text64(statement, index, bytes.constData(),
                                static_cast<sqlite3_uint64>(bytes.size()), SQLITE_TRANSIENT, SQLITE_UTF8);
    } else
        result = sqlite3_bind_int64(statement, index, value.toLongLong());
    check(result, sqlite3_db_handle(statement));
}
QVariant read(sqlite3_stmt *statement, int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_INTEGER)
        return QVariant::fromValue<qlonglong>(sqlite3_column_int64(statement, column));
    if (sqlite3_column_type(statement, column) == SQLITE_NULL)
        return {};
    return QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(statement, column)),
                             sqlite3_column_bytes(statement, column));
}
qlonglong captureTime(const QString &value) {
    if (value.contains('T')) {
        static const QRegularExpression iso("^([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2})(?:\\.(["
                                            "0-9]{1,9}))?(Z|[+-][0-9]{2}:[0-9]{2})$");
        const auto match = iso.match(value);
        const auto date = QDateTime::fromString(match.captured(1) + match.captured(3), Qt::ISODate);
        if (!match.hasMatch() || !date.isValid())
            throw std::runtime_error("Use a valid ISO timestamp with Z or an explicit UTC offset.");
        const auto base = captureTime(QString::number(date.toSecsSinceEpoch()));
        const auto fraction = match.captured(2).leftJustified(9, '0').toLongLong();
        if (base > std::numeric_limits<qlonglong>::max() - fraction)
            throw std::runtime_error("Capture timestamp is out of range.");
        return base + fraction;
    }
    static const QRegularExpression syntax("^([+-]?)([0-9]+)(?:\\.([0-9]{1,9}))?$");
    const auto match = syntax.match(value);
    bool valid = false;
    const auto seconds = match.captured(2).toULongLong(&valid);
    const bool negative = match.captured(1) == "-";
    const auto limit = static_cast<qulonglong>(std::numeric_limits<qlonglong>::max()) + (negative ? 1 : 0);
    const auto fraction = match.captured(3).leftJustified(9, '0').toULongLong();
    if (!match.hasMatch() || !valid || seconds > limit / 1000000000ULL ||
        (seconds == limit / 1000000000ULL && fraction > limit % 1000000000ULL))
        throw std::runtime_error("Time must be Unix seconds with at most nine decimal places.");
    const auto nanoseconds = seconds * 1000000000ULL + fraction;
    if (negative && nanoseconds == limit)
        return std::numeric_limits<qlonglong>::min();
    return negative ? -static_cast<qlonglong>(nanoseconds) : static_cast<qlonglong>(nanoseconds);
}
struct Filter {
    QString sql;
    QVariantList parameters;
};
// Only whitelisted identifiers become SQL. Every user value is a bound parameter.
Filter compileFilter(DataKind kind, const QString &text) {
    QHash<QString, QString> fields;
    switch (kind) {
    case DataKind::Packets:
        fields = {{"id", "c0"},       {"time", "ts"},  {"src", "c2"},      {"dst", "c3"},
                  {"protocol", "c4"}, {"bytes", "c5"}, {"src_port", "sp"}, {"dst_port", "dp"}};
        break;
    case DataKind::Flows:
        fields = {{"src", "c0"},   {"dst", "c1"},   {"protocol", "c2"}, {"packets", "c3"},
                  {"bytes", "c4"}, {"state", "c5"}, {"src_port", "sp"}, {"dst_port", "dp"}};
        break;
    case DataKind::Alerts:
        fields = {{"time", "ts"}, {"rule", "c1"}, {"severity", "c2"},
                  {"src", "c3"},  {"dst", "c4"},  {"count", "c5"}};
        break;
    case DataKind::Dns:
        fields = {{"time", "ts"}, {"id", "c1"},  {"direction", "c2"},
                  {"name", "c3"}, {"src", "c4"}, {"rcode", "c5"}};
        break;
    }
    QList<QPair<QString, bool>> tokens;
    QString token;
    bool quoted = false, escaped = false, active = false, wasQuoted = false;
    for (const auto ch : text) {
        if (escaped) {
            token += ch;
            escaped = false;
        } else if (ch == '\\' && quoted) {
            escaped = true;
        } else if (ch == '"') {
            quoted = !quoted;
            wasQuoted = true;
            active = true;
        } else if (ch.isSpace() && !quoted) {
            if (active)
                tokens << qMakePair(token, wasQuoted);
            token.clear();
            active = false;
            wasQuoted = false;
        } else {
            token += ch;
            active = true;
        }
    }
    if (quoted || escaped)
        throw std::runtime_error("Close the quoted phrase to finish this search.");
    if (active)
        tokens << qMakePair(token, wasQuoted);
    Filter result;
    QStringList groups, terms;
    static const QRegularExpression fieldPattern("^([A-Za-z_]+)(:|>=|<=|!=|=|>|<)(.*)$");
    const QStringList numeric{"id",       "bytes",    "packets", "count", "rcode",
                              "src_port", "dst_port", "port",    "time"};
    bool needsTerm = true;
    for (const auto &[textValue, literal] : tokens) {
        auto value = textValue;
        if (!literal && (value == "OR" || value == "AND")) {
            if (needsTerm)
                throw std::runtime_error("Put a search term on both sides of AND / OR.");
            needsTerm = true;
            if (value == "AND")
                continue;
            groups << "(" + terms.join(" AND ") + ")";
            terms.clear();
            continue;
        }
        const bool negative = value.startsWith('-');
        if (negative)
            value.remove(0, 1);
        if (value.isEmpty())
            throw std::runtime_error("Enter a value after the search operator.");
        const auto match = fieldPattern.match(value);
        QString clause;
        if (match.hasMatch()) {
            auto name = match.captured(1).toLower();
            if (name == "proto")
                name = "protocol";
            const auto op = match.captured(2), operand = match.captured(3);
            const bool port = name == "port" && (kind == DataKind::Packets || kind == DataKind::Flows);
            if ((!fields.contains(name) && !port) || operand.isEmpty())
                throw std::runtime_error(("Unknown field or missing value: " + name).toStdString());
            QStringList columns = port ? QStringList{"sp", "dp"} : QStringList{fields[name]};
            QStringList alternatives;
            for (const auto &column : columns) {
                if (numeric.contains(name)) {
                    bool valid;
                    qlonglong number{};
                    if (name == "time") {
                        number = captureTime(operand);
                        valid = true;
                    } else {
                        number = operand.toLongLong(&valid);
                    }
                    if (!valid)
                        throw std::runtime_error(("Expected a number for " + name).toStdString());
                    const auto comparison = column + (op == ":" ? "=" : op) + "?";
                    alternatives << (port && op == "!=" ? "coalesce(" + comparison + ", 1)" : comparison);
                    result.parameters << number;
                } else if (op == ":") {
                    alternatives << "instr(lower(" + column + "), lower(?)) > 0";
                    result.parameters << operand;
                } else if (op == "=" || op == "!=") {
                    alternatives << column + " COLLATE NOCASE " + op + " ?";
                    result.parameters << operand;
                } else {
                    throw std::runtime_error(("Use : or = to search the text field " + name).toStdString());
                }
            }
            // NULL ports are absent, and must still match a negated port predicate.
            clause = "coalesce((" + alternatives.join(port && op == "!=" ? " AND " : " OR ") + "), 0)";
        } else if (value.toUcs4().size() >= 3) {
            auto phrase = value;
            phrase.replace("\"", "\"\"");
            clause = "seq IN (SELECT rowid FROM " + table(kind) + "_search WHERE " + table(kind) +
                     "_search MATCH ?)";
            result.parameters << "\"" + phrase + "\"";
        } else {
            clause = "instr(lower(search), lower(?)) > 0";
            result.parameters << value;
        }
        terms << (negative ? "NOT (" + clause + ")" : "(" + clause + ")");
        needsTerm = false;
    }
    if (!tokens.isEmpty() && needsTerm)
        throw std::runtime_error("Put a search term after AND / OR.");
    if (!terms.isEmpty())
        groups << "(" + terms.join(" AND ") + ")";
    if (!groups.isEmpty())
        result.sql = " WHERE " + groups.join(" OR ");
    return result;
}
struct Connection {
    sqlite3 *value{};
    Connection(const QString &file, int flags) {
        const auto result = sqlite3_open_v2(file.toUtf8().constData(), &value, flags, nullptr);
        if (result != SQLITE_OK) {
            const std::string message = value ? sqlite3_errmsg(value) : "Cannot open session database";
            sqlite3_close(value);
            throw std::runtime_error(message);
        }
        sqlite3_busy_timeout(value, 1000);
    }
    ~Connection() {
        sqlite3_close(value);
    }
};
} // namespace

SessionStore::SessionStore() : directory_(QDir::tempPath() + "/mini-nids-session-XXXXXX") {
    if (!directory_.isValid())
        throw std::runtime_error("Cannot create temporary session storage.");
    Connection connection(fileName(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto *db = connection.value;
    execute(db, "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA cache_size=-8192; PRAGMA "
                "temp_store=FILE;");
    try {
        for (int i = 0; i < 4; ++i) {
            const auto name = table(static_cast<DataKind>(i));
            execute(
                db,
                "CREATE TABLE " + name +
                    "(seq INTEGER PRIMARY KEY,c0,c1,c2,c3,c4,c5,detail TEXT,offset INTEGER,captured INTEGER,"
                    "ts INTEGER,sp INTEGER,dp INTEGER,search TEXT)");
            execute(db, "CREATE VIRTUAL TABLE " + name + "_search USING fts5(search,content='" + name +
                            "',content_rowid='seq',tokenize='trigram')");
            execute(db, "CREATE TRIGGER " + name + "_insert AFTER INSERT ON " + name + " BEGIN INSERT INTO " +
                            name + "_search(rowid,search) VALUES(new.seq,new.search); END");
            execute(db, "CREATE INDEX " + name + "_time ON " + name + "(ts)");
            if (i == 0)
                execute(db, "CREATE INDEX packets_id ON packets(c0)");
            Statement statement(db, "INSERT INTO " + name + " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            insert_[i] = statement.value;
            statement.value = nullptr;
        }
        execute(db, "BEGIN");
    } catch (...) {
        for (auto *statement : insert_)
            sqlite3_finalize(statement);
        throw;
    }
    database_ = db;
    connection.value = nullptr;
}
SessionStore::~SessionStore() {
    if (database_) {
        sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
        for (auto *statement : insert_)
            sqlite3_finalize(statement);
        sqlite3_close(database_);
    }
}
QString SessionStore::fileName() const {
    return directory_.filePath("rows.sqlite");
}
void SessionStore::append(DataKind kind, const DataRow &row) {
    const auto index = static_cast<int>(kind);
    auto *statement = insert_[index];
    bind(statement, 1, QVariant::fromValue<qulonglong>(counts_[index] + 1));
    QStringList searchable;
    for (int col = 0; col < 6; ++col) {
        bind(statement, col + 2, row.cells.value(col));
        searchable << row.cells.value(col).toString();
    }
    bind(statement, 8, row.details);
    bind(statement, 9, QVariant::fromValue<qulonglong>(row.offset));
    bind(statement, 10, row.captured);
    bind(statement, 11, row.timestamp);
    bind(statement, 12, row.source_port < 0 ? QVariant() : QVariant(row.source_port));
    bind(statement, 13, row.destination_port < 0 ? QVariant() : QVariant(row.destination_port));
    if (row.source_port >= 0)
        searchable << QString::number(row.source_port);
    if (row.destination_port >= 0)
        searchable << QString::number(row.destination_port);
    if (kind != DataKind::Packets)
        searchable << row.details;
    if (kind != DataKind::Flows)
        searchable << QString::fromStdString(mininids::timestamp_utc(row.timestamp));
    bind(statement, 14, searchable.join(' '));
    check(sqlite3_step(statement), database_);
    check(sqlite3_reset(statement), database_);
    ++counts_[index];
    if (++pending_ >= 2048)
        flush();
}
void SessionStore::flush() {
    execute(database_, "COMMIT; BEGIN");
    pending_ = 0;
}

DataProxy::DataProxy(DataKind kind, QStringList columns, QObject *parent)
    : QAbstractTableModel(parent), kind_(kind), columns_(std::move(columns)) {
    pool_.setMaxThreadCount(1);
    debounce_.setSingleShot(true);
    connect(&debounce_, &QTimer::timeout, this, &DataProxy::query);
}
DataProxy::~DataProxy() {
    ++generation_;
    pool_.clear();
    pool_.waitForDone();
}
int DataProxy::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}
int DataProxy::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(columns_.size());
}
QVariant DataProxy::data(const QModelIndex &index, int role) const {
    const auto *row = sourceRow(index.row());
    if (!index.isValid() || !row || index.column() < 0 || index.column() >= columns_.size())
        return {};
    const bool timeColumn = (kind_ == DataKind::Packets && index.column() == 1) ||
                            ((kind_ == DataKind::Alerts || kind_ == DataKind::Dns) && index.column() == 0);
    if (timeColumn && (role == Qt::DisplayRole || role == Qt::ToolTipRole)) {
        const auto utc =
            QString::fromStdString(mininids::timestamp_utc(row->timestamp, nanosecond_resolution_));
        if (role == Qt::ToolTipRole)
            return utc + "\nUnix: " + QString::fromStdString(mininids::timestamp_string(row->timestamp)) +
                   " s";
        return utc.mid(11, utc.size() - 12) + "\n" + utc.left(10) + " UTC";
    }
    return role == Qt::DisplayRole || role == Qt::ToolTipRole ? row->cells[index.column()] : QVariant();
}
QVariant DataProxy::headerData(int section, Qt::Orientation orientation, int role) const {
    return role == Qt::DisplayRole && orientation == Qt::Horizontal && section >= 0 &&
                   section < columns_.size()
               ? QVariant(columns_[section])
               : QVariant();
}
QHash<int, QByteArray> DataProxy::roleNames() const {
    return {{Qt::DisplayRole, "display"}, {Qt::ToolTipRole, "tooltip"}};
}
int DataProxy::pageCount() const {
    return static_cast<int>(std::max<quint64>(1, (matches_ + page_size_ - 1) / page_size_));
}
void DataProxy::attach(std::shared_ptr<SessionStore> store) {
    if (store && store_ == store) {
        schedule();
        return;
    }
    ++generation_;
    debounce_.stop();
    beginResetModel();
    store_ = std::move(store);
    rows_.clear();
    total_ = matches_ = 0;
    page_ = 0;
    error_.clear();
    busy_ = false;
    endResetModel();
    emit countChanged();
    if (store_)
        schedule();
}
void DataProxy::refresh() {
    // A long current search must be allowed to finish even while new packets arrive.
    if (!busy_ && store_)
        schedule();
}
void DataProxy::schedule(int delay) {
    ++generation_;
    pool_.clear();
    busy_ = static_cast<bool>(store_);
    error_.clear();
    debounce_.start(delay);
    emit countChanged();
}
void DataProxy::setFilter(const QString &value) {
    if (filter_ == value)
        return;
    filter_ = value;
    page_ = 0;
    schedule(150);
    emit filterChanged();
}
void DataProxy::setPage(int value) {
    value = std::clamp(value, 0, pageCount() - 1);
    if (page_ == value)
        return;
    page_ = value;
    schedule();
}
void DataProxy::setPageSize(int value) {
    if (value != 100 && value != 250 && value != 500)
        return;
    if (page_size_ == value)
        return;
    page_size_ = value;
    page_ = 0;
    schedule();
}
void DataProxy::sortColumn(int column) {
    if (column < 0 || column >= columns_.size())
        return;
    descending_ = sorted_column_ == column && !descending_;
    sorted_column_ = column;
    page_ = 0;
    schedule();
}
QString DataProxy::searchHelp() const {
    QString fields;
    switch (kind_) {
    case DataKind::Packets:
        fields = "id, time, src, dst, protocol, bytes, src_port, dst_port, port";
        break;
    case DataKind::Flows:
        fields = "src, dst, protocol, packets, bytes, state, src_port, dst_port, port";
        break;
    case DataKind::Alerts:
        fields = "time, rule, severity, src, dst, count";
        break;
    case DataKind::Dns:
        fields = "time, id, direction, name, src, rcode";
        break;
    }
    return "LIVE SEARCH\nSearch runs after a brief typing pause across the entire session, including pages "
           "you have not opened. "
           "Counts show all matching records; sorting also covers every matching page.\n\n"
           "WORDS & PHRASES\nUse plain words or \"quoted phrases\". Matching ignores letter case. "
           "Dates shown in the table can be searched as text, e.g. 2024-11-23.\n\n"
           "COMBINE CONDITIONS\nSpaces and AND require every condition. OR accepts either alternative; AND "
           "binds more tightly. "
           "Prefix a word, phrase or field with - to exclude it. Parentheses and regular expressions are not "
           "supported.\n\n"
           "FIELDS\nUse field:value for contained text, field=value for exact text, and > >= < <= != for "
           "numeric comparisons. "
           "Numeric field:value means equality. Fields available in this table:\n" +
           fields +
           "\n\nTIME\nThe time field accepts Unix seconds (up to nine decimal places), or ISO 8601 "
           "timestamps with Z or an explicit UTC offset. "
           "Example: time>=2024-11-23T10:00:00.123456789Z. Table dates use UTC, not the computer's local "
           "timezone.\n\n"
           "PORTS & DETAILS\nWhere available, port checks either endpoint; src_port and dst_port check one. "
           "Text search includes row details in Flows, Alerts and DNS. Packet payload bytes are available "
           "through Inspect, outside text search. "
           "An invalid expression displays a message instead of silently showing unrelated records.";
}
QStringList DataProxy::searchExamples() const {
    switch (kind_) {
    case DataKind::Packets:
        return {"protocol=TCP bytes>=1000", "port=443 -src:192.0.2", "id>2000", "src:192.0.2 OR dst:192.0.2",
                "time>=2024-11-23T00:00:00Z"};
    case DataKind::Flows:
        return {"protocol=TCP packets>=100", "bytes>=1048576", "state:established -port=443"};
    case DataKind::Alerts:
        return {"rule:SCAN-01 severity=medium", "count>=10", "rule:DNS-02 OR rule:DNS-03"};
    case DataKind::Dns:
        return {"name:example.org direction=Response", "rcode!=0", "id=1234"};
    }
    return {};
}
void DataProxy::query() {
    if (!store_) {
        busy_ = false;
        emit countChanged();
        return;
    }
    const auto store = store_;
    const auto generation = generation_.load();
    const auto filter = filter_;
    const auto page = page_, size = page_size_, column = sorted_column_;
    const auto descending = descending_;
    const auto previousTotal = total_;
    pool_.start([this, store, generation, filter, page, size, column, descending, previousTotal] {
        QVector<DataRow> rows;
        quint64 total = previousTotal, matches = 0;
        int actualPage = page;
        QString error;
        struct Cancellation {
            std::atomic<quint64> *current;
            quint64 expected;
        } cancellation{&generation_, generation};
        try {
            if (generation_.load() != generation)
                return;
            const auto predicate = compileFilter(kind_, filter);
            Connection connection(store->fileName(), SQLITE_OPEN_READONLY);
            auto *db = connection.value;
            sqlite3_progress_handler(
                db, 2000,
                [](void *context) {
                    const auto &c = *static_cast<Cancellation *>(context);
                    return c.current->load() != c.expected ? 1 : 0;
                },
                &cancellation);
            execute(db, "PRAGMA cache_size=-4096; PRAGMA temp_store=FILE; BEGIN");
            const auto name = table(kind_);
            Statement all(db, "SELECT count(*) FROM " + name);
            check(sqlite3_step(all.value), db);
            total = sqlite3_column_int64(all.value, 0);
            matches = total;
            if (!predicate.sql.isEmpty()) {
                Statement count(db, "SELECT count(*) FROM " + name + predicate.sql);
                for (int i = 0; i < predicate.parameters.size(); ++i)
                    bind(count.value, i + 1, predicate.parameters[i]);
                check(sqlite3_step(count.value), db);
                matches = sqlite3_column_int64(count.value, 0);
            }
            actualPage = static_cast<int>(std::min<quint64>(page, matches ? (matches - 1) / size : 0));
            const bool timeColumn = (kind_ == DataKind::Packets && column == 1) ||
                                    ((kind_ == DataKind::Alerts || kind_ == DataKind::Dns) && column == 0);
            const auto order = column < 0   ? QString("seq")
                               : timeColumn ? QString("ts")
                                            : "c" + QString::number(column);
            Statement select(db, "SELECT c0,c1,c2,c3,c4,c5,detail,offset,captured,ts,sp,dp FROM " + name +
                                     predicate.sql + " ORDER BY " + order + (descending ? " DESC" : " ASC") +
                                     ",seq ASC LIMIT ? OFFSET ?");
            int i = 1;
            for (const auto &parameter : predicate.parameters)
                bind(select.value, i++, parameter);
            bind(select.value, i++, size);
            bind(select.value, i, QVariant::fromValue<qlonglong>(static_cast<qlonglong>(actualPage) * size));
            int status;
            while ((status = sqlite3_step(select.value)) == SQLITE_ROW) {
                DataRow row;
                for (int col = 0; col < 6; ++col)
                    row.cells << read(select.value, col);
                row.details = read(select.value, 6).toString();
                row.offset = sqlite3_column_int64(select.value, 7);
                row.captured = static_cast<quint32>(sqlite3_column_int64(select.value, 8));
                row.timestamp = sqlite3_column_int64(select.value, 9);
                row.source_port = read(select.value, 10).isNull() ? -1 : sqlite3_column_int(select.value, 10);
                row.destination_port =
                    read(select.value, 11).isNull() ? -1 : sqlite3_column_int(select.value, 11);
                rows << std::move(row);
            }
            check(status, db);
        } catch (const std::exception &e) {
            error = QString::fromUtf8(e.what());
        }
        if (generation_.load() != generation)
            return;
        QMetaObject::invokeMethod(
            this, [this, generation, rows = std::move(rows), total, matches, actualPage, error]() mutable {
                if (generation_.load() != generation)
                    return;
                beginResetModel();
                rows_ = std::move(rows);
                total_ = total;
                matches_ = matches;
                page_ = actualPage;
                error_ = error;
                busy_ = false;
                endResetModel();
                emit countChanged();
            });
    });
}
const DataRow *DataProxy::sourceRow(int row) const {
    return row >= 0 && row < rows_.size() ? &rows_[row] : nullptr;
}
QString DataProxy::rowSummary(int row) const {
    const auto *value = sourceRow(row);
    if (!value)
        return {};
    QStringList cells;
    for (int column = 0; column < value->cells.size(); ++column)
        cells << data(index(row, column), Qt::DisplayRole).toString().replace('\n', ' ');
    return cells.join("  |  ");
}
