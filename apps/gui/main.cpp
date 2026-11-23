#include "AppController.hpp"
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QGuiApplication app(argc, argv);
    app.setLayoutDirection(Qt::LeftToRight);
    app.setOrganizationName("MiniNIDS");
    app.setApplicationName("Mini-NIDS");
    app.setApplicationVersion(QStringLiteral(MININIDS_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/org.mininids.MiniNIDS.png")));
    const bool validating = app.arguments().contains("--smoke-test") ||
                            app.arguments().contains("--review") || app.arguments().contains("--review-zip");
    if (validating)
        app.setApplicationName("Mini-NIDS Validation");
    QQuickStyle::setStyle("Basic");
    AppController controller;
    QQmlApplicationEngine engine;
    int qml_warnings = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qml_warnings += static_cast<int>(warnings.size());
    });
    engine.rootContext()->setContextProperty("controller", &controller);
    engine.rootContext()->setContextProperty("packetModel", &controller.packet_proxy);
    engine.rootContext()->setContextProperty("flowModel", &controller.flow_proxy);
    engine.rootContext()->setContextProperty("alertModel", &controller.alert_proxy);
    engine.rootContext()->setContextProperty("dnsModel", &controller.dns_proxy);
    engine.loadFromModule("MiniNids", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
    if (!mainWindow)
        return 1;
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = app.primaryScreen();
    if (screen) {
        mainWindow->setScreen(screen);
        const auto area = screen->availableGeometry().adjusted(16, 16, -16, -16);
        mainWindow->setMinimumSize(mainWindow->minimumSize().boundedTo(area.size()));
        mainWindow->resize(mainWindow->size().boundedTo(area.size()));
        mainWindow->setPosition(area.x() + (area.width() - mainWindow->width()) / 2,
                                area.y() + (area.height() - mainWindow->height()) / 2);
    }
    mainWindow->show();
    QTimer review;
    int step = 0;
    const auto arguments = app.arguments();
    if (arguments.contains("--smoke-test")) {
        if (app.windowIcon().pixmap(32, 32).isNull()) {
            qCritical() << "Application icon is missing";
            return 1;
        }
        review.setInterval(100);
        QObject::connect(&review, &QTimer::timeout, &app, [&] {
            if (step < 18) {
                if (step == 9)
                    mainWindow->setWidth(420);
                mainWindow->setProperty("page", step++ % 9);
            } else
                app.exit(qml_warnings ? 1 : 0);
        });
        review.start();
    }
    if (arguments.size() == 5 && arguments[1] == "--review") {
        controller.setInputFile(arguments[2]);
        controller.setOutputRoot(arguments[3]);
        const auto directory = arguments[4];
        QDir().mkpath(directory);
        review.setInterval(250);
        QObject::connect(&review, &QTimer::timeout, &app, [&, directory] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
            if (!window) {
                app.exit(1);
                return;
            }
            if (step > 0 && step <= 11) {
                const auto filename = directory + QString("/screen-%1.png").arg(step, 2, 10, QChar('0'));
                if (!window->grabWindow().save(filename)) {
                    app.exit(1);
                    return;
                }
            }
            if (step < 9)
                window->setProperty("page", step);
            else if (step == 9) {
                window->setProperty("page", 1);
            } else if (step == 10) {
                window->setWidth(420);
                window->setHeight(860);
            } else {
                app.exit(qml_warnings ? 1 : 0);
                return;
            }
            ++step;
        });
        QObject::connect(&controller, &AppController::analysisFinished, &app, [&] {
            if (controller.status().contains("failed")) {
                qWarning() << controller.status();
                app.exit(1);
            } else
                review.start();
        });
        QTimer::singleShot(0, &controller, &AppController::start);
    }
    if (arguments.size() == 6 && arguments[1] == "--review-zip") {
        controller.setOutputRoot(arguments[4]);
        controller.setInputFile(arguments[2]);
        const auto directory = arguments[5], entry = arguments[3];
        QDir().mkpath(directory);
        engine.rootObjects()[0]->setProperty("page", 1);
        review.setInterval(250);
        QObject::connect(&review, &QTimer::timeout, &app, [&, directory, entry] {
            if (controller.busy())
                return;
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
            if (!window || controller.status().contains("failed")) {
                qWarning() << controller.status();
                app.exit(1);
                return;
            }
            const auto save = [&](const QString &name) {
                if (!window->grabWindow().save(directory + "/" + name + ".png"))
                    app.exit(1);
            };
            if (step == 0) {
                save("zip-list-dark");
                int row = -1;
                const auto entries = controller.archiveEntries();
                for (qsizetype i = 0; i < entries.size(); ++i)
                    if (entries[i].toMap()["name"].toString() == entry)
                        row = static_cast<int>(i);
                if (row < 0) {
                    app.exit(1);
                    return;
                }
                controller.prepareArchiveEntry(row);
            } else if (step == 1) {
                if (!controller.canAnalyze()) {
                    qWarning() << controller.status();
                    app.exit(1);
                    return;
                }
                save("zip-ready-dark");
                controller.start();
            } else if (step == 2) {
                if (controller.summary().isEmpty()) {
                    app.exit(1);
                    return;
                }
                window->setProperty("page", 0);
            } else if (step == 3) {
                save("zip-analyzed-dark");
                window->setProperty("page", 1);
            } else if (step == 4) {
                save("zip-complete-dark");
                window->setWidth(420);
                window->setHeight(860);
            } else if (step == 5) {
                save("zip-list-compact");
                app.exit(qml_warnings ? 1 : 0);
            }
            ++step;
        });
        QTimer::singleShot(120000, &app, [&] { app.exit(1); });
        review.start();
    }
    return app.exec();
}
