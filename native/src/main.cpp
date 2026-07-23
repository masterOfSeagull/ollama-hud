#include "AppController.h"
#include "OllamaService.h"
#include "SettingsStore.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>
#include <QUrl>

namespace {
int verify()
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    try {
        const HudSettings settings = SettingsStore::loadFromPath();
        SettingsStore::validate(settings);
        out << "Config: " << SettingsStore::defaultConfigPath() << "\n";
        out << "Ollama host: " << settings.host << "\n";
        out << "Model: " << settings.model << "\n";
        out << "Q/A memory pairs: " << settings.memoryQaPairs << "\n";
        out << "Chat log: " << SettingsStore::chatLogPath() << "\n";
        out << "Trigger shortcut: " << parseShortcut(settings.triggerShortcut).display() << "\n";
        out << "Exit shortcut: " << parseShortcut(settings.exitShortcut).display() << "\n";
        out << "Clear shortcut: " << parseShortcut(settings.clearShortcut).display() << "\n";
        try {
            OllamaService service;
            out << service.checkServer(settings) << "\n";
        } catch (const std::exception &error) {
            out << "Ollama server check: " << error.what() << "\n";
        }
        return 0;
    } catch (const std::exception &error) {
        err << error.what() << "\n";
        return 1;
    }
}
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("Ollama HUD");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }
    if (args.contains("--verify")) {
        QCoreApplication app(argc, argv);
        Q_UNUSED(app);
        return verify();
    }

    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption runOption("run", "Start the overlay loop directly.");
    QCommandLineOption verifyOption("verify", "Check settings and Ollama reachability without opening the UI.");
    parser.addOption(runOption);
    parser.addOption(verifyOption);
    parser.process(app);

    if (parser.isSet(verifyOption)) {
        return verify();
    }

    QQmlApplicationEngine engine;
    engine.addImportPath("qrc:/native/qml");

    AppController controller;
    engine.rootContext()->setContextProperty("appController", &controller);

    if (parser.isSet(runOption)) {
        controller.startHud();
    } else {
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
            QCoreApplication::exit(1);
        }, Qt::QueuedConnection);
#ifdef OLLAMA_HUD_HOT_RELOAD
        engine.load(QUrl::fromLocalFile(QStringLiteral(PROJECT_SOURCE_DIR "/native/qml/OllamaHud/Main.qml")));
#else
        engine.loadFromModule("OllamaHud", "Main");
#endif
    }

    return app.exec();
}
