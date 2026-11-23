#include "AppController.hpp"
#include "Fixtures.hpp"
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
        controller.setPersian(true);
        QVERIFY(controller.persian());
        controller.setPersian(false);
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
};
QTEST_GUILESS_MAIN(GuiTest)
#include "GuiTest.moc"
