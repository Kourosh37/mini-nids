#include "AppController.hpp"
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <mininids/capture/PcapReader.hpp>
#include <mininids/reporting/Evaluation.hpp>
#include <mininids/reporting/ReportWriter.hpp>
#include <sstream>

using namespace mininids;
namespace {
QString qs(const std::string &value) {
    return QString::fromStdString(value);
}
QString detail_text(const nlohmann::json &value) {
    auto text = QString::fromStdString(value.dump(2));
    return text.size() > 16384 ? text.left(16384) + "\n[detail truncated at 16384 characters]" : text;
}
std::filesystem::path path(const QString &value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toStdString());
#endif
}
QString local(const QString &value) {
    const QUrl url(value);
    return url.isLocalFile() ? url.toLocalFile() : value;
}
QVariantMap variant(const nlohmann::json &j) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).toVariant().toMap();
}
struct SessionData final : IEventSink {
    QVector<DataRow> packets, flows, alerts, dns_rows;
    AnalysisSummary summary;
    Evaluator evaluator;
    std::unique_ptr<ReportWriter> report;
    EngineConfig config;
    nlohmann::json metrics;
    QString error, report_path;
    explicit SessionData(EngineConfig c, std::vector<GroundTruthEvent> truth)
        : evaluator(std::move(truth)), config(c) {}
    void packet(const DecodedPacket &p) override {
        if (static_cast<std::size_t>(packets.size()) >= config.max_ui_rows)
            return;
        const auto src = p.arp    ? ip_string(p.arp->sender_ip)
                         : p.ipv4 ? ip_string(p.src)
                                  : mac_string(p.src_mac);
        const auto dst = p.arp    ? ip_string(p.arp->target_ip)
                         : p.ipv4 ? ip_string(p.dst)
                                  : mac_string(p.dst_mac);
        const nlohmann::json details = {{"packet", p.index},
                                        {"timestamp", timestamp_string(p.timestamp)},
                                        {"ethernet",
                                         {{"source", mac_string(p.src_mac)},
                                          {"destination", mac_string(p.dst_mac)},
                                          {"type", p.ether_type},
                                          {"vlans", p.vlans}}},
                                        {"ipv4",
                                         {{"source", src},
                                          {"destination", dst},
                                          {"ttl", p.ttl},
                                          {"ihl", p.ip_header_length},
                                          {"total_length", p.ip_length},
                                          {"fragment_offset", p.fragment_offset},
                                          {"more_fragments", p.more_fragments},
                                          {"checksum", p.ip_checksum}}},
                                        {"transport",
                                         {{"source_port", p.src_port},
                                          {"destination_port", p.dst_port},
                                          {"sequence", p.seq},
                                          {"acknowledgment", p.ack},
                                          {"flags", p.tcp_flags},
                                          {"window", p.tcp_window},
                                          {"checksum", p.transport_checksum},
                                          {"payload_length", p.payload_length}}},
                                        {"captured_length", p.captured_length},
                                        {"original_length", p.original_length},
                                        {"capture_truncated", p.capture_truncated},
                                        {"parse_status", static_cast<int>(p.status)},
                                        {"diagnostic", p.diagnostic}};
        packets.push_back({{QVariant::fromValue<qulonglong>(p.index), qs(timestamp_string(p.timestamp)),
                            qs(src), qs(dst), qs(p.protocol_name), p.original_length},
                           detail_text(details),
                           p.file_offset,
                           p.captured_length});
    }
    void flow(const FlowRecord &f) override {
        report->flow(f);
        if (static_cast<std::size_t>(flows.size()) >= config.max_ui_rows)
            return;
        const auto details =
            nlohmann::json{{"source", ip_string(f.src)},           {"destination", ip_string(f.dst)},
                           {"forward_packets", f.forward},         {"reverse_packets", f.reverse},
                           {"retransmissions", f.retransmissions}, {"first", timestamp_string(f.first)},
                           {"last", timestamp_string(f.last)},     {"termination", f.termination},
                           {"tcp_flag_counts", f.flag_counts}};
        flows.push_back({{qs(ip_string(f.src)) + ":" + QString::number(f.src_port),
                          qs(ip_string(f.dst)) + ":" + QString::number(f.dst_port),
                          f.protocol == 6 ? "TCP" : "UDP", QVariant::fromValue<qulonglong>(f.packets),
                          QVariant::fromValue<qulonglong>(f.bytes), qs(state_string(f.state))},
                         detail_text(details)});
    }
    void alert(const Alert &a) override {
        report->alert(a);
        evaluator.observe(a);
        if (static_cast<std::size_t>(alerts.size()) >= config.max_ui_rows)
            return;
        const auto detail = nlohmann::json{{"rule_id", a.rule_id},
                                           {"severity", a.severity},
                                           {"confidence", a.confidence},
                                           {"packet_index", a.packet_index},
                                           {"first_timestamp", timestamp_string(a.timestamp)},
                                           {"last_timestamp", timestamp_string(a.last)},
                                           {"occurrences", a.occurrences},
                                           {"evidence", a.evidence}};
        alerts.push_back({{qs(timestamp_string(a.timestamp)), qs(a.rule_id), qs(a.severity), qs(a.src),
                           qs(a.dst), QVariant::fromValue<qulonglong>(a.occurrences)},
                          detail_text(detail)});
    }
    void dns(const DecodedPacket &p, const DnsObservation &o) override {
        report->dns(p, o);
        if (static_cast<std::size_t>(dns_rows.size()) >= config.max_ui_rows)
            return;
        nlohmann::json records = nlohmann::json::array();
        for (const auto &r : p.dns->records)
            records.push_back({{"name", r.name},
                               {"type", r.type},
                               {"ttl", r.ttl},
                               {"value", r.value},
                               {"section", r.section}});
        const auto name = p.dns->questions.empty() ? "(no question)" : p.dns->questions[0].name;
        dns_rows.push_back(
            {{qs(timestamp_string(p.timestamp)), p.dns->id, p.dns->response ? "Response" : "Query", qs(name),
              qs(ip_string(p.src)), p.dns->rcode},
             detail_text(nlohmann::json{{"records", records},
                                        {"matched", o.matched},
                                        {"conflicting", o.conflicting},
                                        {"arp_correlated", o.arp_correlated}})});
    }
};
class Progress final : public IProgressObserver {
  public:
    std::function<void(std::uint64_t, std::uint64_t, std::uint64_t)> callback;
    void progress(std::uint64_t bytes, std::uint64_t total, std::uint64_t packets) override {
        callback(bytes, total, packets);
    }
};
} // namespace
AppController::AppController(QObject *parent)
    : QObject(parent), packets({"#", "Capture time", "Source", "Destination", "Protocol", "Bytes"}, this),
      flows({"Source endpoint", "Destination endpoint", "Protocol", "Packets", "Bytes", "TCP state"}, this),
      alerts({"Capture time", "Rule", "Severity", "Source", "Destination", "Count"}, this),
      dns({"Capture time", "ID", "Direction", "Question", "Source", "Rcode"}, this),
      packet_proxy(&packets, this), flow_proxy(&flows, this), alert_proxy(&alerts, this),
      dns_proxy(&dns, this) {
    QSettings settings;
    output_root_ =
        settings
            .value("output_root",
                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/MiniNIDS")
            .toString();
    recent_ = settings.value("recent").toStringList();
    dark_ = settings.value("dark", true).toBool();
    const auto saved = settings.value("config").toString();
    if (!saved.isEmpty()) {
        try {
            config_ = EngineConfig::parse_text(saved.toStdString());
        } catch (const std::exception &) {
            status_ = "Saved configuration invalid; defaults restored";
        }
    }
}
AppController::~AppController() {
    cancel_.store(true);
    if (worker_) {
        worker_->quit();
        worker_->wait();
    }
}
QString AppController::configText() const {
    return qs(config_.json().dump(2));
}
void AppController::setInputFile(const QString &value) {
    if (!busy_) {
        input_ = local(value);
        archive_.reset();
        archive_entries_.clear();
        prepared_directory_.reset();
        prepared_path_.clear();
        prepared_entry_ = -1;
        progress_ = 0;
        if (archiveInput()) {
            scanArchive();
            return;
        }
        status_ = "Ready for offline analysis";
        emit changed();
    }
}
void AppController::launch(std::function<void()> task) {
    busy_ = true;
    cancel_.store(false);
    emit changed();
    worker_ = QThread::create(std::move(task));
    auto *thread = worker_;
    connect(thread, &QThread::finished, this, [this, thread] {
        if (worker_ == thread)
            worker_ = nullptr;
        busy_ = false;
        emit changed();
        thread->deleteLater();
    });
    worker_->start();
}
void AppController::scanArchive() {
    status_ = "Inspecting ZIP capture headers...";
    const auto input = input_;
    ZipLimits limits;
    limits.max_packet = config_.max_packet;
    launch([this, input, limits] {
        try {
            auto archive = std::make_shared<ZipArchive>(path(input), limits);
            const auto captures = archive->inspect(&cancel_);
            QVariantList entries;
            for (const auto &capture : captures) {
                entries.push_back(QVariantMap{
                    {"index", capture.index},
                    {"name", qs(capture.name)},
                    {"bytes", QVariant::fromValue<qulonglong>(capture.bytes)},
                    {"supported", capture.supported},
                    {"pcapng", capture.pcapng},
                    {"reason", qs(capture.reason)},
                    {"snaplen", capture.metadata.snaplen},
                    {"precision", capture.metadata.nanosecond_resolution ? "nanoseconds" : "microseconds"}});
            }
            const auto compatible =
                std::count_if(captures.begin(), captures.end(), [](const auto &c) { return c.supported; });
            QMetaObject::invokeMethod(this, [this, archive, entries, compatible] {
                archive_ = archive;
                archive_entries_ = entries;
                status_ = entries.isEmpty()
                              ? "No .pcap or .pcapng files found in ZIP"
                              : QString("%1 compatible capture(s) found. Choose a file and prepare it.")
                                    .arg(compatible);
                emit changed();
                emit importFinished();
            });
        } catch (const std::exception &error) {
            const auto message = cancel_.load() ? QString("ZIP inspection cancelled")
                                                : "ZIP import failed: " + qs(error.what());
            QMetaObject::invokeMethod(this, [this, message] {
                status_ = message;
                emit changed();
                emit importFinished();
            });
        }
    });
}
void AppController::prepareArchiveEntry(int row) {
    if (busy_ || !archive_ || row < 0 || row >= archive_entries_.size())
        return;
    const auto entry = archive_entries_[row].toMap();
    if (!entry["supported"].toBool())
        return;
    prepared_directory_.reset();
    prepared_path_.clear();
    prepared_entry_ = -1;
    progress_ = 0;
    status_ = "Extracting selected capture and checking integrity...";
    auto directory = std::make_shared<QTemporaryDir>(QDir::tempPath() + "/mininids-import-XXXXXX");
    if (!directory->isValid()) {
        status_ = "ZIP import failed: cannot create temporary directory";
        emit changed();
        return;
    }
    const auto archive = archive_;
    launch([this, archive, directory, entry, row] {
        try {
            auto last = std::chrono::steady_clock::now();
            const auto prepared = archive->prepare(
                entry["index"].toUInt(), path(directory->path()), &cancel_,
                [this, &last](bool validating, auto current, auto total) {
                    const auto now = std::chrono::steady_clock::now();
                    if (current != total && now - last < std::chrono::milliseconds(100))
                        return;
                    last = now;
                    const auto fraction =
                        total ? static_cast<double>(current) / static_cast<double>(total) : 0;
                    QMetaObject::invokeMethod(this, [this, validating, fraction] {
                        progress_ = (validating ? 0.7 : 0.0) + fraction * (validating ? 0.3 : 0.7);
                        status_ = validating ? "Validating every PCAP record..."
                                             : "Extracting selected capture and checking CRC...";
                        emit changed();
                    });
                });
            QMetaObject::invokeMethod(this, [this, directory, entry, row, prepared] {
                auto updated = entry;
                updated["validated_packets"] = QVariant::fromValue<qulonglong>(prepared.packets);
                updated["snaplen_exceeded_packets"] =
                    QVariant::fromValue<qulonglong>(prepared.snaplen_exceeded_packets);
                updated["warning"] = prepared.snaplen_exceeded_packets
                                         ? QString("Warning: %1 packets exceed the declared "
                                                   "snaplen; all bounded packet bytes preserved.")
                                               .arg(prepared.snaplen_exceeded_packets)
                                         : QString();
                archive_entries_[row] = updated;
                prepared_directory_ = directory;
                prepared_path_ = QDir(directory->path()).filePath("capture.pcap");
                prepared_entry_ = row;
                progress_ = 1;
                status_ = QString("Ready: %1 | %2 packets validated. Start analysis to inspect traffic.")
                              .arg(entry["name"].toString())
                              .arg(prepared.packets);
                if (prepared.snaplen_exceeded_packets)
                    status_ += " See metadata warning above.";
                emit changed();
                emit importFinished();
            });
        } catch (const std::exception &error) {
            const auto message = cancel_.load() ? QString("ZIP import cancelled; no capture imported")
                                                : "ZIP import failed: " + qs(error.what());
            QMetaObject::invokeMethod(this, [this, message] {
                status_ = message;
                progress_ = 0;
                emit changed();
                emit importFinished();
            });
        }
    });
}
void AppController::setOutputRoot(const QString &value) {
    if (!busy_) {
        output_root_ = local(value);
        QSettings().setValue("output_root", output_root_);
        emit changed();
    }
}
void AppController::setGroundTruth(const QString &value) {
    if (!busy_) {
        ground_truth_ = local(value);
        emit changed();
    }
}
void AppController::setDark(bool value) {
    dark_ = value;
    QSettings().setValue("dark", value);
    emit changed();
}
void AppController::cancel() {
    cancel_.store(true);
    status_ = "Cancellation requested";
    emit changed();
}
bool AppController::applyConfig(const QString &text) {
    if (busy_)
        return false;
    try {
        config_ = EngineConfig::parse_text(text.toStdString());
        QSettings().setValue("config", configText());
        status_ = "Configuration applied";
        emit changed();
        return true;
    } catch (const std::exception &e) {
        status_ = "Configuration: " + qs(e.what());
        emit changed();
        return false;
    }
}
void AppController::start() {
    if (busy_)
        return;
    if (!canAnalyze()) {
        status_ = archiveInput() ? "Prepare a compatible capture from the ZIP first"
                                 : "Choose a PCAP or ZIP file first";
        emit changed();
        return;
    }
    progress_ = 0;
    status_ = "Analyzing capture...";
    const auto input = archiveInput() ? prepared_path_ : input_, original = input_, root = output_root_,
               truth = ground_truth_;
    const auto directory = prepared_directory_;
    const auto config = config_;
    nlohmann::json provenance;
    if (archiveInput()) {
        const auto entry = archive_entries_[prepared_entry_].toMap();
        provenance = {{"kind", "zip"},
                      {"archive", config.redact ? "[redacted]" : original.toStdString()},
                      {"entry", config.redact ? "[redacted]" : entry["name"].toString().toStdString()},
                      {"source_format", entry["pcapng"].toBool() ? "pcapng" : "pcap"},
                      {"validated_packets", entry["validated_packets"].toULongLong()},
                      {"snaplen_exceeded_packets", entry["snaplen_exceeded_packets"].toULongLong()},
                      {"integrity", "zip_crc_and_capture_structure_checked"}};
    }
    emit changed();
    launch([this, input, original, directory, root, truth, config, provenance] {
        auto result = std::make_shared<SessionData>(config, std::vector<GroundTruthEvent>{});
        try {
            if (!truth.isEmpty())
                result->evaluator = Evaluator(Evaluator::load(path(truth)));
            const auto destination = QDir(root).filePath(
                "session-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") + "-" +
                QUuid::createUuid().toString(QUuid::Id128).left(8));
            const QFileInfo before(input);
            PcapReader reader(path(input), config.max_packet);
            result->report = std::make_unique<ReportWriter>(path(destination), config);
            result->report_path = destination;
            if (!provenance.is_null())
                result->report->input_metadata(provenance);
            Progress observer;
            observer.callback = [this](auto bytes, auto total, auto count) {
                QMetaObject::invokeMethod(this, [this, bytes, total, count] {
                    progress_ = total ? static_cast<double>(bytes) / static_cast<double>(total) : 0;
                    status_ = QString("Analyzing %1 packets").arg(count);
                    emit changed();
                });
            };
            result->summary = NidsEngine(config).analyze(reader, *result, &cancel_, &observer);
            const QFileInfo after(input);
            if (before.size() != after.size() || before.lastModified() != after.lastModified())
                throw Error("input file changed during analysis");
            result->metrics = result->evaluator.result(!truth.isEmpty());
            if (result->summary.cancelled)
                result->metrics["status"] = "partial_cancelled";
            result->report->finish(result->summary, result->metrics);
        } catch (const std::exception &e) {
            result->error = qs(e.what());
        }
        QMetaObject::invokeMethod(this, [this, result, input, original, directory] {
            if (!result->error.isEmpty()) {
                status_ = "Analysis failed: " + result->error;
                report_path_ = result->report_path;
            } else {
                packets.replace(std::move(result->packets));
                flows.replace(std::move(result->flows));
                alerts.replace(std::move(result->alerts));
                dns.replace(std::move(result->dns_rows));
                summary_ = variant(result->summary.json());
                evaluation_ = variant(result->metrics);
                summary_["throughput"] = result->summary.performance()["packets_per_second"].get<double>();
                timeline_.clear();
                protocols_.clear();
                for (const auto &[time, count] : result->summary.timeline)
                    timeline_.push_back(QVariantMap{{"time", static_cast<double>(time) / second},
                                                    {"count", QVariant::fromValue<qulonglong>(count)}});
                for (const auto &[name, count] : result->summary.protocols)
                    protocols_.push_back(
                        QVariantMap{{"name", qs(name)}, {"count", QVariant::fromValue<qulonglong>(count)}});
                report_path_ = result->report_path;
                analyzed_input_ = input;
                analyzed_directory_ = directory;
                QFileInfo info(input);
                input_size_ = info.size();
                input_modified_ = info.lastModified().toMSecsSinceEpoch();
                recent_.removeAll(original);
                recent_.prepend(original);
                while (recent_.size() > 8)
                    recent_.removeLast();
                QSettings().setValue("recent", recent_);
                status_ = result->summary.cancelled ? "Cancelled; partial reports saved"
                                                    : "Analysis complete; reports saved";
                progress_ = result->summary.cancelled ? progress_ : 1;
            }
            emit changed();
            emit analysisFinished();
        });
    });
}
QString AppController::inspect(int page, int row) const {
    const DataProxy *model = page == 2   ? &packet_proxy
                             : page == 3 ? &flow_proxy
                             : page == 4 ? &alert_proxy
                                         : &dns_proxy;
    const auto *data = model->sourceRow(row);
    if (!data)
        return {};
    QString detail = data->details;
    if (page == 2) {
        const QFileInfo info(analyzed_input_);
        if (info.size() != input_size_ || info.lastModified().toMSecsSinceEpoch() != input_modified_)
            return detail + "\n\nCapture changed; hex inspection unavailable.";
        QFile file(analyzed_input_);
        if (file.open(QIODevice::ReadOnly) && file.seek(static_cast<qint64>(data->offset))) {
            const auto bytes = file.read(std::min<qint64>(data->captured, 4096));
            detail += "\n\nCaptured bytes (maximum 4096, read on demand):\n";
            for (qsizetype i = 0; i < bytes.size(); i += 16)
                detail += QString::number(i, 16).rightJustified(4, '0') + "  " +
                          QString::fromLatin1(bytes.mid(i, 16).toHex(' ')) + "\n";
        } else
            detail += "\n\nCapture is no longer readable.";
    }
    return detail;
}
void AppController::openReports() {
    if (!report_path_.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(report_path_));
}
