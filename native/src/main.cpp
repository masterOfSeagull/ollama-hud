#include "AppController.h"
#include "HotReloadController.h"
#include "OllamaService.h"
#include "SettingsStore.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
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
        out << "Simulation trigger shortcut: " << parseShortcut(settings.simulationTriggerShortcut).display() << "\n";
        out << "Simulation stop shortcut: " << parseShortcut(settings.simulationStopShortcut).display() << "\n";
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
#ifdef OLLAMA_HUD_RELEASE_BRANDING
    // Applies to the taskbar and the title-bar icon of every Release window.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/packaging/ollama-hud-release.png")));
#endif
#ifdef OLLAMA_HUD_HOT_RELOAD
    QGuiApplication::setQuitOnLastWindowClosed(false);
    qputenv("QML_DISABLE_DISK_CACHE", "1");
#endif
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
#ifdef OLLAMA_HUD_HOT_RELOAD
    engine.addImportPath(QStringLiteral(OLLAMA_HUD_QML_SOURCE_DIR));
#endif

    AppController controller;
    engine.rootContext()->setContextProperty("appController", &controller);
#ifdef OLLAMA_HUD_HOT_RELOAD
    const QUrl mainUrl = QUrl::fromLocalFile(QStringLiteral(OLLAMA_HUD_QML_SOURCE_DIR "/OllamaHud/Main.qml"));
    HotReloadController hotReload(&engine, mainUrl, &app);
    engine.rootContext()->setContextProperty("HotReload", &hotReload);
    engine.rootContext()->setContextProperty("hotReloadEnabled", true);
#else
    engine.rootContext()->setContextProperty("hotReloadEnabled", false);
#endif

    if (parser.isSet(runOption)) {
        controller.startHud();
    } else {
        const QMetaObject::Connection initialLoadFailure = QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
            QCoreApplication::exit(1);
        });
#ifdef OLLAMA_HUD_HOT_RELOAD
        engine.load(mainUrl);
#else
        engine.loadFromModule("OllamaHud", "Main");
#endif
        QObject::disconnect(initialLoadFailure);
    }

    return app.exec();
}
