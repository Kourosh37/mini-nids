#include "AppController.hpp"
#include "Fixtures.hpp"
#include "ZipFixtures.hpp"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace fixtures;
class GuiTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory;
  private slots:
    void initTestCase() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
        QCoreApplication::setOrganizationName("MiniNIDSTests");
        QCoreApplication::setApplicationName("ControllerValidation");
    }
    void localDatasetFullSession() {
        const auto input = qEnvironmentVariable("MININIDS_GUI_AUDIT_CAPTURE");
        if (input.isEmpty())
            QSKIP("Set MININIDS_GUI_AUDIT_CAPTURE for a full local dataset audit.");
        const auto expected = qEnvironmentVariable("MININIDS_GUI_AUDIT_PACKETS").toULongLong();
        QTemporaryDir output;
        AppController controller;
        controller.setInputFile(input);
        controller.setOutputRoot(output.path());
        QElapsedTimer duration, heartbeat;
        duration.start();
        heartbeat.start();
        qint64 longestGap = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this,
                [&] { longestGap = std::max(longestGap, heartbeat.restart()); });
        timer.start(20);
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 300000);
        QVERIFY2(!controller.summary().isEmpty(), qPrintable(controller.status()));
        QCOMPARE(controller.summary()["packets"].toULongLong(), expected);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.packet_proxy.queryBusy(), 30000);
        QCOMPARE(controller.packet_proxy.totalCount(), expected);
        controller.packet_proxy.setPage(controller.packet_proxy.pageCount() - 1);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.packet_proxy.queryBusy(), 30000);
        const auto last = controller.packet_proxy.count() - 1;
        QCOMPARE(controller.packet_proxy.sourceRow(last)->cells[0].toULongLong(), expected);
        QVERIFY(controller.inspect(2, last).contains("Captured bytes"));
        controller.packet_proxy.setFilter("id>2000");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.packet_proxy.queryBusy(), 30000);
        QCOMPARE(controller.packet_proxy.matchCount(), expected - 2000);
        controller.packet_proxy.sortColumn(0);
        controller.packet_proxy.sortColumn(0);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.packet_proxy.queryBusy(), 30000);
        QCOMPARE(controller.packet_proxy.sourceRow(0)->cells[0].toULongLong(), expected);
        const auto expectedRows =
            QJsonDocument::fromJson(qgetenv("MININIDS_GUI_AUDIT_ROWS")).toVariant().toMap();
        const QList<QPair<DataProxy *, QString>> tables{{&controller.flow_proxy, "flows.csv"},
                                                        {&controller.alert_proxy, "alerts.csv"},
                                                        {&controller.dns_proxy, "dns_log.csv"}};
        for (const auto &[model, name] : tables) {
            QTRY_VERIFY_WITH_TIMEOUT(!model->queryBusy(), 30000);
            if (expectedRows.contains(name))
                QCOMPARE(model->totalCount(), expectedRows[name].toULongLong());
            model->setPage(model->pageCount() - 1);
            QTRY_VERIFY_WITH_TIMEOUT(!model->queryBusy(), 30000);
            if (model->totalCount() > 0) {
                QVERIFY(model->count() > 0);
                QVERIFY(!model->sourceRow(model->count() - 1)->details.isEmpty());
            }
        }
        qulonglong protocolTotal = 0;
        for (const auto &protocol : controller.protocols())
            protocolTotal += protocol.toMap()["count"].toULongLong();
        QCOMPARE(protocolTotal, expected);
        const auto protocolName = controller.protocols().first().toMap()["name"].toString();
        const auto protocolCount = controller.protocols().first().toMap()["count"].toULongLong();
        controller.packet_proxy.setFilter(protocolName);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.packet_proxy.queryBusy(), 30000);
        QCOMPARE(controller.packet_proxy.matchCount(), protocolCount);
        qInfo() << "Full GUI session:" << expected << "packets; milliseconds:" << duration.elapsed()
                << "; longest UI timer gap:" << longestGap;
        QVERIFY2(longestGap < 1000, "The UI event loop stalled for a second or more.");
    }
    void workerReportsFilteringAndInspection() {
        TempDir temp;
        Capture capture;
        capture.add(tcp());
        capture.add(arp(1));
        capture.add(arp(2));
        capture.add(arp(2));
        capture.save(temp.path / "input.pcap");
        AppController controller;
        controller.setInputFile(QString::fromStdString((temp.path / "input.pcap").string()));
        controller.setOutputRoot(QString::fromStdString((temp.path / "reports").string()));
        QSignalSpy done(&controller, &AppController::analysisFinished);
        controller.start();
        QVERIFY(controller.busy());
        QVERIFY(done.wait(30000));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.summary()["packets"].toULongLong(), 4ULL);
        QTRY_COMPARE(controller.packet_proxy.rowCount(), 4);
        controller.packet_proxy.setFilter("TCP");
        QTRY_COMPARE(controller.packet_proxy.rowCount(), 1);
        QTRY_VERIFY(controller.inspect(2, 0).contains("Captured bytes"));
        QVERIFY(controller.inspect(2, 0).contains("\"tcp\""));
        controller.packet_proxy.setFilter("ARP");
        QTRY_COMPARE(controller.packet_proxy.rowCount(), 3);
        const auto arpDetails = controller.inspect(2, 0);
        QVERIFY(arpDetails.contains("\"arp\""));
        QVERIFY(!arpDetails.contains("\"ipv4\""));
        QVERIFY(!arpDetails.contains("\"tcp\""));
        QVERIFY(QFile::exists(controller.reportPath() + "/alerts.csv"));
        QTRY_VERIFY(controller.alert_proxy.rowCount() > 0);
        QVERIFY(!controller.applyConfig("{\"max_flows\":-1}"));
    }
    void cancellationAndErrors() {
        TempDir temp;
        Capture capture;
        for (unsigned i = 0; i < 10000; ++i)
            capture.add(tcp());
        capture.save(temp.path / "input.pcap");
        AppController controller;
        controller.setOutputRoot(QString::fromStdString((temp.path / "reports").string()));
        controller.setInputFile(QString::fromStdString((temp.path / "input.pcap").string()));
        QSignalSpy done(&controller, &AppController::analysisFinished);
        controller.start();
        controller.cancel();
        QVERIFY(done.wait(30000));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.summary()["status"].toString(), QString("cancelled"));
        controller.setInputFile(QString::fromStdString((temp.path / "missing.pcap").string()));
        controller.start();
        QVERIFY(done.wait(30000));
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.status().contains("failed"));
        QTRY_COMPARE(controller.packet_proxy.count(), 0);
        QCOMPARE(controller.packet_proxy.totalCount(), 0ULL);
        QVERIFY(controller.summary().isEmpty());
        QVERIFY(controller.evaluation().isEmpty());
        QVERIFY(controller.timeline().isEmpty());
        QVERIFY(controller.protocols().isEmpty());
    }
    void allRecordsAreSearchableAcrossPagesAndLateAlerts() {
        TempDir temp;
        Capture capture;
        for (unsigned i = 0; i < 2000; ++i)
            capture.add(tcp(40000, 80, 0x10));
        for (unsigned port = 1; port <= 20; ++port)
            capture.add(tcp(40000, static_cast<std::uint16_t>(port), 2, port));
        capture.save(temp.path / "large.pcap");
        AppController controller;
        auto config = mininids::EngineConfig::parse_text(controller.configText().toStdString()).json();
        config["max_ui_rows"] = 2000;
        config["scan_ports"] = 20;
        QVERIFY(controller.applyConfig(QString::fromStdString(config.dump())));
        controller.setInputFile(QString::fromStdString((temp.path / "large.pcap").string()));
        controller.setOutputRoot(QString::fromStdString((temp.path / "reports").string()));
        QSignalSpy done(&controller, &AppController::analysisFinished);
        controller.start();
        QVERIFY(done.wait(30000));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.summary()["packets"].toULongLong(), 2020ULL);
        QTRY_VERIFY(!controller.packet_proxy.queryBusy());
        QCOMPARE(controller.packet_proxy.count(), 100);
        QCOMPARE(controller.packet_proxy.totalCount(), 2020ULL);
        QVERIFY(controller.summary()["capture_name"].toString().contains("large.pcap"));
        controller.alert_proxy.setFilter("SCAN-01");
        QTRY_VERIFY(controller.alert_proxy.count() > 0);
        controller.packet_proxy.setPage(20);
        QTRY_VERIFY(!controller.packet_proxy.queryBusy());
        QCOMPARE(controller.packet_proxy.count(), 20);
        QCOMPARE(controller.packet_proxy.sourceRow(0)->cells[0].toULongLong(), 2001ULL);
        QVERIFY(controller.inspect(2, 19).contains("Captured bytes"));
        controller.packet_proxy.setFilter("protocol:TCP id>2000 dst_port>=10");
        QTRY_VERIFY(!controller.packet_proxy.queryBusy());
        QCOMPARE(controller.packet_proxy.matchCount(), 11ULL);
        QCOMPARE(controller.packet_proxy.count(), 11);
        controller.packet_proxy.setFilter("");
        controller.packet_proxy.setPageSize(500);
        controller.packet_proxy.sortColumn(0);
        controller.packet_proxy.sortColumn(0);
        QTRY_VERIFY(!controller.packet_proxy.queryBusy());
        QCOMPARE(controller.packet_proxy.count(), 500);
        QCOMPARE(controller.packet_proxy.pageCount(), 5);
        QCOMPARE(controller.packet_proxy.sourceRow(0)->cells[0].toULongLong(), 2020ULL);
        controller.packet_proxy.setFilter("UDP");
        QTRY_COMPARE(controller.packet_proxy.count(), 0);
        QCOMPARE(controller.packet_proxy.matchCount(), 0ULL);
        QCOMPARE(controller.packet_proxy.totalCount(), 2020ULL);
        controller.setInputFile(QString::fromStdString((temp.path / "missing.pcap").string()));
        controller.start();
        QCOMPARE(controller.packet_proxy.count(), 0);
        QVERIFY(done.wait(30000));
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.summary().isEmpty());
        QCOMPARE(controller.alert_proxy.totalCount(), 0ULL);
    }
    void globalSearchSortingAndQueryErrors() {
        auto store = std::make_shared<SessionStore>();
        for (const qlonglong timestamp : {10LL, 2LL, 1LL}) {
            DataRow row;
            row.cells = {timestamp,   QString::number(timestamp) + ".000000000",
                         "192.0.2.1", "198.51.100.1",
                         "TCP",       timestamp * 100};
            row.details = QString::number(timestamp);
            row.timestamp = timestamp * 1000000000LL;
            row.destination_port = timestamp == 10 ? 443 : 80;
            store->append(DataKind::Packets, row);
        }
        store->flush();
        DataProxy proxy(DataKind::Packets,
                        {"#", "Capture time", "Source", "Destination", "Protocol", "Bytes"});
        proxy.attach(store);
        proxy.sortColumn(1);
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.sourceRow(0)->details, QString("1"));
        QCOMPARE(proxy.sourceRow(1)->details, QString("2"));
        QCOMPARE(proxy.sourceRow(2)->details, QString("10"));
        proxy.sortColumn(1);
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.sourceRow(0)->details, QString("10"));
        proxy.setFilter("protocol=TCP AND -port:443 bytes>=200");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.count(), 1);
        QCOMPARE(proxy.totalCount(), 3ULL);
        QCOMPARE(proxy.sourceRow(0)->details, QString("2"));
        proxy.setFilter("time>=2.000000000 time<10.000000000");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.count(), 1);
        QCOMPARE(proxy.sourceRow(0)->details, QString("2"));
        proxy.setFilter("port!=443");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.matchCount(), 2ULL);
        proxy.setFilter("id=10 OR id=1");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.matchCount(), 2ULL);
        proxy.setFilter("src:\"192.0.2\" -id:10");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.matchCount(), 2ULL);
        proxy.setFilter("unknown:value");
        QTRY_VERIFY(!proxy.queryBusy());
        QVERIFY(!proxy.queryError().isEmpty());
        QCOMPARE(proxy.count(), 0);
        // Superseded searches must never overwrite the newest result.
        proxy.setFilter("TCP");
        proxy.setFilter("id=2");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.count(), 1);
        QCOMPARE(proxy.sourceRow(0)->details, QString("2"));
        proxy.setFilter("' OR 1=1 --");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.count(), 0);
        proxy.setFilter("\"unclosed");
        QTRY_VERIFY(!proxy.queryBusy());
        QVERIFY(!proxy.queryError().isEmpty());
        DataRow precise;
        precise.cells = {99, "1732345678.123456789", "192.0.2.1", "198.51.100.1", "TCP", 64};
        precise.timestamp = 1732345678123456789LL;
        store->append(DataKind::Packets, precise);
        store->flush();
        proxy.setFilter("time=1732345678.123456789");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.count(), 1);
        QCOMPARE(proxy.sourceRow(0)->cells[0].toInt(), 99);
        QCOMPARE(proxy.data(proxy.index(0, 1), Qt::DisplayRole).toString(),
                 QString("07:07:58.123456789\n2024-11-23 UTC"));
        QVERIFY(proxy.data(proxy.index(0, 1), Qt::ToolTipRole).toString().contains("1732345678.123456789 s"));
        proxy.setFilter("time=2024-11-23T07:07:58.123456789Z");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.matchCount(), 1ULL);
        proxy.setFilter("time=2024-11-23T10:37:58.123456789+03:30");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.matchCount(), 1ULL);
        proxy.setFilter("2024-11-23");
        QTRY_VERIFY(!proxy.queryBusy());
        QCOMPARE(proxy.matchCount(), 1ULL);
        proxy.setNanosecondResolution(false);
        QCOMPARE(proxy.data(proxy.index(0, 1), Qt::DisplayRole).toString(),
                 QString("07:07:58.123456\n2024-11-23 UTC"));
        proxy.setFilter("time=2024-02-30T10:00:00Z");
        QTRY_VERIFY(!proxy.queryBusy());
        QVERIFY(!proxy.queryError().isEmpty());
    }
    void curatedSettingsPreserveActiveBudgetsAndConvertDurations() {
        AppController controller;
        auto config = mininids::EngineConfig::parse_text(controller.configText().toStdString()).json();
        config["max_flows"] = 43210;
        config["max_ui_rows"] = 2000;
        QVERIFY(controller.applyConfig(QString::fromStdString(config.dump())));
        QSignalSpy changed(&controller, &AppController::settingsChanged);
        QVERIFY(controller.applySettings({{"window_s", 0.001001}, {"syn_incomplete_ratio", 0.75}}));
        QCOMPARE(changed.count(), 1);
        const auto current = mininids::EngineConfig::parse_text(controller.configText().toStdString());
        QCOMPARE(current.max_flows, std::size_t(43210));
        QCOMPARE(current.window, mininids::Timestamp(1001000));
        QVERIFY(!controller.analysisSettings().contains("max_ui_rows"));
        QVERIFY(!controller.applySettings({{"max_ui_rows", 3000}}));
        QVERIFY(!controller.applySettings({{"window_s", 0.0001}}));
        QCOMPARE(controller.formatDuration(0.001001), QString("1.001 ms"));
        QVERIFY(controller.applySettings({{"window_s", 1.001}}));
        QCOMPARE(mininids::EngineConfig::parse_text(controller.configText().toStdString()).window,
                 mininids::Timestamp(1001000000));
    }
    void flowsAlertsAndDnsAlsoIncludeRecordsAfter2000() {
        TempDir temp;
        Capture capture;
        for (unsigned i = 1; i <= 2101; ++i) {
            // Real link identity mismatch, rather than missing-query visibility alone.
            capture.add(arp(2, 1, 0xc0000000U + i));
            auto message = fixtures::dns(true);
            put16(message, 0, static_cast<std::uint16_t>(i));
            capture.add(udp(message, 53, 45000, 0xc0000000U + i));
        }
        capture.save(temp.path / "dns-responses.pcap");
        AppController controller;
        controller.setInputFile(QString::fromStdString((temp.path / "dns-responses.pcap").string()));
        controller.setOutputRoot(QString::fromStdString((temp.path / "reports").string()));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        const QList<DataProxy *> models{&controller.packet_proxy, &controller.flow_proxy,
                                        &controller.alert_proxy, &controller.dns_proxy};
        for (auto *model : models) {
            QTRY_VERIFY(!model->queryBusy());
            const auto expected = model == &controller.packet_proxy ? 4202ULL : 2101ULL;
            QCOMPARE(model->totalCount(), expected);
            QCOMPARE(model->count(), 100);
            model->setPage(model->pageCount() - 1);
            QTRY_VERIFY(!model->queryBusy());
            QCOMPARE(model->count(), static_cast<int>(expected % 100));
        }
        controller.dns_proxy.setFilter("id=2101 direction:Response");
        controller.flow_proxy.setFilter("src:192.0.8.53 src_port=53");
        controller.alert_proxy.setFilter("src=192.0.8.53 rule:DNS-02");
        for (auto *model : {&controller.flow_proxy, &controller.alert_proxy, &controller.dns_proxy}) {
            QTRY_VERIFY(!model->queryBusy());
            QCOMPARE(model->matchCount(), 1ULL);
            QCOMPARE(model->count(), 1);
        }
        const auto dnsDetails = nlohmann::json::parse(controller.inspect(5, 0).toStdString());
        QCOMPARE(dnsDetails["id"].get<unsigned>(), 2101U);
        QVERIFY(dnsDetails["questions"][0]["name"] == "example.org");
        QVERIFY(dnsDetails["unmatched"] == true);
    }
    void finalAlertUpdatesKeepOneRowAndRefreshSearchIndex() {
        auto store = std::make_shared<SessionStore>();
        DataRow first{{"1.000000000", "ARP-01", "high", "source", "target", 1}, "initial evidence"};
        first.timestamp = second;
        store->append(DataKind::Alerts, first, 1);
        store->flush();
        DataProxy model(DataKind::Alerts, {"Time", "Rule", "Severity", "Source", "Target", "Count"});
        model.attach(store);
        model.setFilter("initial");
        QTRY_VERIFY(!model.queryBusy());
        QCOMPARE(model.matchCount(), 1ULL);
        auto final = first;
        final.cells[5] = 7;
        final.details = "final aggregate evidence";
        store->append(DataKind::Alerts, final, 1);
        store->flush();
        model.refresh();
        QTRY_VERIFY(!model.queryBusy());
        QCOMPARE(model.totalCount(), 1ULL);
        QCOMPARE(model.matchCount(), 0ULL);
        model.setFilter("count>=7 final");
        QTRY_VERIFY(!model.queryBusy());
        QCOMPARE(model.matchCount(), 1ULL);
        QCOMPARE(model.sourceRow(0)->cells[5].toInt(), 7);
    }
    void zipSelectionValidationAnalysisAndLifetime() {
        TempDir temp;
        Capture capture;
        capture.add(tcp());
        capture.add(arp());
        const auto file = temp.path / "dataset.zip";
        auto ng = pcapng_bytes();
        ng[40] = 1;
        ng[41] = 0;
        const auto more = pcapng_bytes(false);
        ng.insert(ng.end(), more.begin(), more.end());
        save_bytes(file, zip_bytes({{"a.pcap", capture.bytes}, {"b.pcap", ng}, {"bad.pcap", {1}}}));
        AppController controller;
        controller.setOutputRoot(QString::fromStdString((temp.path / "reports").string()));
        controller.setInputFile(QString::fromStdString(file.string()));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        QCOMPARE(controller.archiveEntries().size(), 3);
        QSignalSpy entriesChanged(&controller, &AppController::archiveEntriesChanged);
        controller.setGroundTruth("reference.json");
        controller.setGroundTruth("");
        QCOMPARE(entriesChanged.count(), 0);
        QVERIFY(controller.archiveInput());
        QVERIFY(!controller.canAnalyze());
        controller.prepareArchiveEntry(2);
        QVERIFY(!controller.canAnalyze());
        controller.prepareArchiveEntry(1);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        QCOMPARE(controller.preparedEntry(), 1);
        QCOMPARE(entriesChanged.count(), 1);
        QVERIFY(controller.canAnalyze());
        QVERIFY(!controller.archiveEntries()[1].toMap()["warning"].toString().isEmpty());
        auto configuration = mininids::EngineConfig::parse_text(controller.configText().toStdString()).json();
        configuration["redact"] = true;
        QVERIFY(controller.applyConfig(QString::fromStdString(configuration.dump())));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        QCOMPARE(controller.summary()["packets"].toULongLong(), 2ULL);
        QVERIFY(QFile::exists(controller.reportPath() + "/input_source.json"));
        QFile provenance(controller.reportPath() + "/input_source.json");
        QVERIFY(provenance.open(QIODevice::ReadOnly));
        const auto source = nlohmann::json::parse(provenance.readAll().toStdString());
        QVERIFY(source["archive"] == "[redacted]");
        QCOMPARE(source["snaplen_exceeded_packets"].get<unsigned>(), 1U);
        QCOMPARE(controller.recentFiles().first(), QString::fromStdString(file.string()));
        // Changing input must keep the analyzed temporary capture alive for packet inspection.
        controller.setInputFile("other.pcap");
        QTRY_VERIFY(controller.inspect(2, 0).contains("Captured bytes"));
        controller.setInputFile(QString::fromStdString(file.string()));
        controller.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        QVERIFY(!controller.canAnalyze());
        controller.setInputFile(QString::fromStdString((temp.path / "missing.zip").string()));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        QVERIFY(controller.status().contains("failed"));
        QVERIFY(!controller.canAnalyze());
    }
};
QTEST_GUILESS_MAIN(GuiTest)
#include "GuiTest.moc"
