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
    return QString::fromStdString(value.dump(2));
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
nlohmann::json dnsDetails(const DecodedPacket &p) {
    const auto &dns = *p.dns;
    auto questions = nlohmann::json::array(), records = nlohmann::json::array();
    for (const auto &q : dns.questions)
        questions.push_back({{"name", q.name}, {"type", q.type}, {"class", q.klass}});
    for (const auto &r : dns.records)
        records.push_back({{"name", r.name},
                           {"type", r.type},
                           {"class", r.klass},
                           {"ttl", r.ttl},
                           {"value", r.value},
                           {"section", r.section}});
    return {{"id", dns.id},       {"response", dns.response}, {"flags", dns.flags}, {"opcode", dns.opcode},
            {"rcode", dns.rcode}, {"questions", questions},   {"records", records}};
}
QString packetDetails(const DecodedPacket &p) {
    const char *statuses[] = {"ok", "truncated", "unsupported", "malformed"};
    nlohmann::json details{{"packet", p.index},
                           {"timestamp", timestamp_string(p.timestamp)},
                           {"protocol", p.protocol_name},
                           {"captured_length", p.captured_length},
                           {"original_length", p.original_length},
                           {"capture_truncated", p.capture_truncated},
                           {"parse_status", statuses[static_cast<unsigned>(p.status)]},
                           {"diagnostic", p.diagnostic}};
    if (p.captured_length >= 14)
        details["ethernet"] = {{"source", mac_string(p.src_mac)},
                               {"destination", mac_string(p.dst_mac)},
                               {"type", p.ether_type},
                               {"vlans", p.vlans}};
    if (p.arp) {
        const auto &arp = *p.arp;
        details["arp"] = {{"opcode", arp.opcode},
                          {"hardware_type", arp.hardware},
                          {"protocol_type", arp.protocol},
                          {"sender_ip", ip_string(arp.sender_ip)},
                          {"target_ip", ip_string(arp.target_ip)},
                          {"sender_mac", mac_string(arp.sender_mac)},
                          {"target_mac", mac_string(arp.target_mac)}};
    }
    if (p.ipv4)
        details["ipv4"] = {{"source", ip_string(p.src)},
                           {"destination", ip_string(p.dst)},
                           {"protocol", p.protocol},
                           {"ttl", p.ttl},
                           {"ihl", p.ip_header_length},
                           {"total_length", p.ip_length},
                           {"fragment_offset", p.fragment_offset},
                           {"more_fragments", p.more_fragments},
                           {"checksum", p.ip_checksum}};
    if (p.transport) {
        nlohmann::json transport{{"source_port", p.src_port},
                                 {"destination_port", p.dst_port},
                                 {"checksum", p.transport_checksum},
                                 {"payload_length", p.payload_length}};
        if (p.protocol == 6) {
            transport["sequence"] = p.seq;
            transport["acknowledgment"] = p.ack;
            transport["flags"] = p.tcp_flags;
            transport["window"] = p.tcp_window;
        } else
            transport["length"] = p.udp_length;
        details[p.protocol == 6 ? "tcp" : "udp"] = std::move(transport);
    }
    if (p.protocol == 1 && p.status == ParseStatus::Ok)
        details["icmp"] = {{"type", p.icmp_type}, {"code", p.icmp_code}, {"checksum", p.transport_checksum}};
    if (p.dns)
        details["dns"] = dnsDetails(p);
    return detail_text(details);
}
struct SessionData final : IEventSink {
    std::shared_ptr<SessionStore> store;
    AnalysisSummary summary;
    Evaluator evaluator;
    std::unique_ptr<ReportWriter> report;
    nlohmann::json metrics;
    QString error, report_path, capture_name;
    explicit SessionData(std::vector<GroundTruthEvent> truth) : evaluator(std::move(truth)) {}
    void packet(const DecodedPacket &p) override {
        const auto src = p.arp    ? ip_string(p.arp->sender_ip)
                         : p.ipv4 ? ip_string(p.src)
                                  : mac_string(p.src_mac);
        const auto dst = p.arp    ? ip_string(p.arp->target_ip)
                         : p.ipv4 ? ip_string(p.dst)
                                  : mac_string(p.dst_mac);
        DataRow row{{QVariant::fromValue<qulonglong>(p.index), qs(timestamp_string(p.timestamp)), qs(src),
                     qs(dst), qs(p.protocol_name), p.original_length},
                    {},
                    p.file_offset,
                    p.captured_length};
        row.timestamp = p.timestamp;
        if (p.transport) {
            row.source_port = p.src_port;
            row.destination_port = p.dst_port;
        }
        store->append(DataKind::Packets, row);
    }
    void flow(const FlowRecord &f) override {
        report->flow(f);
        const auto details = nlohmann::json{{"source", ip_string(f.src)},
                                            {"source_port", f.src_port},
                                            {"destination_port", f.dst_port},
                                            {"protocol", f.protocol},
                                            {"packets", f.packets},
                                            {"bytes", f.bytes},
                                            {"state", state_string(f.state)},
                                            {"destination", ip_string(f.dst)},
                                            {"forward_packets", f.forward},
                                            {"reverse_packets", f.reverse},
                                            {"retransmissions", f.retransmissions},
                                            {"first", timestamp_string(f.first)},
                                            {"last", timestamp_string(f.last)},
                                            {"termination", f.termination},
                                            {"tcp_flag_counts", f.flag_counts}};
        DataRow row{{qs(ip_string(f.src)) + ":" + QString::number(f.src_port),
                     qs(ip_string(f.dst)) + ":" + QString::number(f.dst_port),
                     f.protocol == 6 ? "TCP" : "UDP", QVariant::fromValue<qulonglong>(f.packets),
                     QVariant::fromValue<qulonglong>(f.bytes), qs(state_string(f.state))},
                    detail_text(details)};
        row.source_port = f.src_port;
        row.destination_port = f.dst_port;
        store->append(DataKind::Flows, row);
    }
    void alert(const Alert &a) override {
        report->alert(a);
        evaluator.observe(a);
        const auto detail = nlohmann::json{{"rule_id", a.rule_id},
                                           {"attack", a.attack},
                                           {"source", a.src},
                                           {"destination", a.dst},
                                           {"severity", a.severity},
                                           {"confidence", a.confidence},
                                           {"packet_index", a.packet_index},
                                           {"first_timestamp", timestamp_string(a.timestamp)},
                                           {"last_timestamp", timestamp_string(a.last)},
                                           {"occurrences", a.occurrences},
                                           {"evidence", a.evidence}};
        DataRow row{{qs(timestamp_string(a.timestamp)), qs(a.rule_id), qs(a.severity), qs(a.src), qs(a.dst),
                     QVariant::fromValue<qulonglong>(a.occurrences)},
                    detail_text(detail)};
        row.timestamp = a.timestamp;
        store->append(DataKind::Alerts, row);
    }
    void dns(const DecodedPacket &p, const DnsObservation &o) override {
        report->dns(p, o);
        auto details = dnsDetails(p);
        details["source"] = ip_string(p.src);
        details["destination"] = ip_string(p.dst);
        details["source_port"] = p.src_port;
        details["destination_port"] = p.dst_port;
        details["timestamp"] = timestamp_string(p.timestamp);
        details["matched"] = o.matched;
        details["unmatched"] = o.unmatched;
        details["conflicting"] = o.conflicting;
        details["arp_correlated"] = o.arp_correlated;
        details["layer_mismatch"] = o.layer_mismatch;
        const auto name = p.dns->questions.empty() ? "(no question)" : p.dns->questions[0].name;
        DataRow row{{qs(timestamp_string(p.timestamp)), p.dns->id, p.dns->response ? "Response" : "Query",
                     qs(name), qs(ip_string(p.src)), p.dns->rcode},
                    detail_text(details)};
        row.timestamp = p.timestamp;
        store->append(DataKind::Dns, row);
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
    : QObject(parent),
      packet_proxy(DataKind::Packets, {"#", "Capture time", "Source", "Destination", "Protocol", "Bytes"},
                   this),
      flow_proxy(DataKind::Flows,
                 {"Source endpoint", "Destination endpoint", "Protocol", "Packets", "Bytes", "TCP state"},
                 this),
      alert_proxy(DataKind::Alerts, {"Capture time", "Rule", "Severity", "Source", "Destination", "Count"},
                  this),
      dns_proxy(DataKind::Dns, {"Capture time", "ID", "Direction", "Question", "Source", "Rcode"}, this) {
    QSettings settings;
    output_root_ =
        settings
            .value("output_root",
                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/MiniNIDS")
            .toString();
    recent_ = settings.value("recent").toStringList();
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
        emit archiveEntriesChanged();
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
                emit archiveEntriesChanged();
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
                emit archiveEntriesChanged();
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
    const auto capture_name =
        QFileInfo(original).fileName() +
        (archiveInput() ? " / " + archive_entries_[prepared_entry_].toMap()["name"].toString() : QString());
    summary_.clear();
    evaluation_.clear();
    timeline_.clear();
    protocols_.clear();
    report_path_.clear();
    analyzed_input_.clear();
    analyzed_directory_.reset();
    packet_proxy.attach({});
    flow_proxy.attach({});
    alert_proxy.attach({});
    dns_proxy.attach({});
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
    launch([this, input, original, directory, root, truth, config, provenance, capture_name] {
        auto result = std::make_shared<SessionData>(std::vector<GroundTruthEvent>{});
        result->capture_name = capture_name;
        try {
            result->store = std::make_shared<SessionStore>();
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
            QMetaObject::invokeMethod(this, [this, store = result->store, input, directory, before] {
                analyzed_input_ = input;
                analyzed_directory_ = directory;
                input_size_ = before.size();
                input_modified_ = before.lastModified().toMSecsSinceEpoch();
                packet_proxy.attach(store);
                flow_proxy.attach(store);
                alert_proxy.attach(store);
                dns_proxy.attach(store);
            });
            Progress observer;
            observer.callback = [this, result](auto bytes, auto total, auto count) {
                result->store->flush();
                QMetaObject::invokeMethod(this, [this, bytes, total, count] {
                    progress_ = total ? static_cast<double>(bytes) / static_cast<double>(total) : 0;
                    status_ = QString("Analyzing %1 packets").arg(count);
                    packet_proxy.refresh();
                    flow_proxy.refresh();
                    alert_proxy.refresh();
                    dns_proxy.refresh();
                    emit changed();
                });
            };
            result->summary = NidsEngine(config).analyze(reader, *result, &cancel_, &observer);
            result->store->flush();
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
                packet_proxy.attach({});
                flow_proxy.attach({});
                alert_proxy.attach({});
                dns_proxy.attach({});
            } else {
                packet_proxy.attach(result->store);
                flow_proxy.attach(result->store);
                alert_proxy.attach(result->store);
                dns_proxy.attach(result->store);
                summary_ = variant(result->summary.json());
                summary_["capture_name"] = result->capture_name;
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
            const auto packetBytes = file.read(data->captured);
            if (packetBytes.size() != data->captured)
                return "Capture is no longer readable.";
            const PacketView view{data->timestamp, data->cells[0].toULongLong(), data->offset,
                                  data->cells[5].toUInt(),
                                  Bytes(reinterpret_cast<const std::uint8_t *>(packetBytes.constData()),
                                        static_cast<std::size_t>(packetBytes.size()))};
            detail = packetDetails(ProtocolDispatcher().decode(view));
            const auto bytes = packetBytes.first(std::min<qsizetype>(packetBytes.size(), 4096));
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
