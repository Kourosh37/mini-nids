#include "AppController.hpp"
#include "Fixtures.hpp"
#include "ZipFixtures.hpp"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace fixtures;
class GuiTest : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("MiniNIDSTests");
        QCoreApplication::setApplicationName("ControllerValidation");
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
        QCOMPARE(controller.packet_proxy.rowCount(), 4);
        controller.packet_proxy.setFilter("TCP");
        QCOMPARE(controller.packet_proxy.rowCount(), 1);
        QVERIFY(controller.inspect(2, 0).contains("Captured bytes"));
        controller.packet_proxy.setFilter("ARP");
        QCOMPARE(controller.packet_proxy.rowCount(), 3);
        QVERIFY(QFile::exists(controller.reportPath() + "/alerts.csv"));
        QVERIFY(controller.alert_proxy.rowCount() > 0);
        controller.setDark(false);
        QVERIFY(!controller.dark());
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
        QVERIFY(controller.archiveInput());
        QVERIFY(!controller.canAnalyze());
        controller.prepareArchiveEntry(2);
        QVERIFY(!controller.canAnalyze());
        controller.prepareArchiveEntry(1);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 30000);
        QCOMPARE(controller.preparedEntry(), 1);
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
        QVERIFY(controller.inspect(2, 0).contains("Captured bytes"));
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
