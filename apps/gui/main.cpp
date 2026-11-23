#include "AppController.hpp"
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MiniNIDS");
    app.setApplicationName("Mini-NIDS");
    const bool validating = app.arguments().contains("--smoke-test") || app.arguments().contains("--review");
    if (validating)
        app.setApplicationName("Mini-NIDS Validation");
    QQuickStyle::setStyle("Basic");
    AppController controller;
    if (validating) {
        controller.setDark(true);
        controller.setPersian(false);
    }
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
    QObject::connect(&controller, &AppController::languageChanged, &engine,
                     &QQmlApplicationEngine::retranslate);
    engine.loadFromModule("MiniNids", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    QTimer review;
    int step = 0;
    const auto arguments = app.arguments();
    if (arguments.contains("--smoke-test")) {
        review.setInterval(100);
        QObject::connect(&review, &QTimer::timeout, &app, [&] {
            if (step < 9)
                engine.rootObjects()[0]->setProperty("page", step++);
            else if (step++ == 9) {
                controller.setDark(false);
                engine.rootObjects()[0]->setProperty("width", 420);
                controller.setPersian(true);
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
            if (step > 0 && step <= 12) {
                const auto filename = directory + QString("/screen-%1.png").arg(step, 2, 10, QChar('0'));
                if (!window->grabWindow().save(filename)) {
                    app.exit(1);
                    return;
                }
            }
            if (step < 9)
                window->setProperty("page", step);
            else if (step == 9) {
                window->setProperty("page", 0);
                controller.setDark(false);
            } else if (step == 10) {
                window->setWidth(420);
                window->setHeight(860);
            } else if (step == 11) {
                controller.setPersian(true);
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
    return app.exec();
}
