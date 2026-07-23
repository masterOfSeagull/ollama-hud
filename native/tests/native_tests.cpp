#include "AppController.h"
#include "ChatLogService.h"
#include "OllamaService.h"
#include "SettingsStore.h"
#include "Shortcut.h"

#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class NativeTests : public QObject
{
    Q_OBJECT

private slots:
    void settingsLoadSaveValidation();
    void chatPayloadGeneration();
    void chatLogIncludesCompletionMetadata();
    void memorySelectionCollapsesDuplicates();
    void shortcutParsing();
    void hudCollapseTogglesWithoutDiscardingText();
    void qmlOverlayLoads();
    void qmlOverlayModuleLoads();
    void qmlMainLoads();
};

void NativeTests::settingsLoadSaveValidation()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("settings.yaml");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"(host: http://127.0.0.1:9999
model: test-model
trigger_shortcut: Alt+`
exit_shortcut: Ctrl+`
clear_shortcut: Alt+2
screenshot_max_edge: 512
timeout_seconds: 3
memory_qa_pairs: 2
instruction: 'Answer briefly.'
screenshot_context: 'Use the newest image.'
keep_alive: 1m
think: false
query: 'Where now?'
options:
  temperature: 0.1
  num_ctx: 4096
)");
    file.close();

    HudSettings settings = SettingsStore::loadFromPath(path);
    QCOMPARE(settings.host, QString("http://127.0.0.1:9999"));
    QCOMPARE(settings.model, QString("test-model"));
    QCOMPARE(settings.memoryQaPairs, 2);
    QCOMPARE(settings.screenshotContext, QString("Use the newest image."));
    QCOMPARE(settings.think, false);
    QCOMPARE(settings.options.value("num_ctx").toInt(), 4096);

    SettingsStore store;
    store.setSettings(settings);
    QCOMPARE(store.keepAliveMinutes(), QString("1"));
    store.setKeepAliveMinutes("10");
    QCOMPARE(store.settings().keepAlive, QString("10m"));
    store.setNumCtx("8192");
    QCOMPARE(store.settings().options.value("num_ctx").toInt(), 8192);
    store.setTopP("0.75");
    QCOMPARE(store.settings().options.value("top_p").toDouble(), 0.75);

    HudSettings invalid;
    invalid.host = "";
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SettingsStore::validate(invalid));

    const QString savedPath = dir.filePath("saved.yaml");
    SettingsStore::saveToPath(settings, savedPath);
    QVERIFY(QFile::exists(savedPath));
    HudSettings reloaded = SettingsStore::loadFromPath(savedPath);
    QCOMPARE(reloaded.query, QString("Where now?"));
}

void NativeTests::chatPayloadGeneration()
{
    HudSettings settings;
    settings.model = "vision-model";
    settings.query = "Where is the exit?";
    settings.screenshotContext = "Use the current image.";
    settings.memoryQaPairs = 1;
    QList<ChatMemory> memories = {
        {"Old question", "Old answer", "old-image"},
        {"Recent question", "Recent answer", "recent-image"},
    };

    QJsonObject payload = OllamaService::buildChatPayload(settings, "current-image", memories);
    QCOMPARE(payload.value("model").toString(), QString("vision-model"));
    QCOMPARE(payload.value("think").toBool(), true);
    QJsonArray messages = payload.value("messages").toArray();
    QCOMPARE(messages.size(), 5);
    QCOMPARE(messages.at(0).toObject().value("role").toString(), QString("system"));
    QCOMPARE(messages.at(1).toObject().value("content").toString(), QString("Use the current image."));
    QCOMPARE(messages.at(2).toObject().value("content").toString(), QString("Recent question"));
    QCOMPARE(messages.at(4).toObject().value("images").toArray().at(0).toString(), QString("current-image"));

    settings.screenshotContext.clear();
    messages = OllamaService::buildChatPayload(settings, "current-image", memories).value("messages").toArray();
    QCOMPARE(messages.size(), 4);
    QCOMPARE(messages.at(1).toObject().value("content").toString(), QString("Recent question"));
}

void NativeTests::chatLogIncludesCompletionMetadata()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("chat.log");
    HudSettings settings;
    ChatLogService::write({"capture", "Where now?", {}, "Go left.", {}, "none", {}, "length", 120, 42, 3000000000, 500000000, 1000000000, 1500000000}, settings, path);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString log = QString::fromUtf8(file.readAll());
    QVERIFY(log.contains("Done reason: length"));
    QVERIFY(log.contains("Prompt tokens: 120"));
    QVERIFY(log.contains("Generated tokens: 42"));
    QVERIFY(log.contains("Total duration: 3.000 s"));
}

void NativeTests::memorySelectionCollapsesDuplicates()
{
    QList<ChatMemory> memories = {
        {"Q", "A", "one"},
        {"Q", "A", "two"},
        {"Q2", "A2", "three"},
    };
    QList<ChatMemory> selected = OllamaService::selectPromptMemories(memories, 3);
    QCOMPARE(selected.size(), 2);
    QCOMPARE(selected.at(0).imageB64, QString("two"));
    QCOMPARE(selected.at(1).answer, QString("A2"));
}

void NativeTests::shortcutParsing()
{
    QCOMPARE(parseShortcut("Alt+`").display(), QString("Alt+`"));
    QCOMPARE(parseShortcut("control+escape").display(), QString("Ctrl+Esc"));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parseShortcut("Alt+1+2"));
}

void NativeTests::hudCollapseTogglesWithoutDiscardingText()
{
    AppController controller;
    QSignalSpy spy(&controller, &AppController::hudCollapsedChanged);
    const QString originalMessage = controller.message();

    controller.toggleHudCollapsed();
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.hudCollapsed());
    QCOMPARE(controller.state(), QString("READY"));
    QCOMPARE(controller.message(), originalMessage);

    controller.toggleHudCollapsed();
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.hudCollapsed());
    QCOMPARE(controller.message(), originalMessage);
}

void NativeTests::qmlOverlayLoads()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(PROJECT_SOURCE_DIR) + "/native/qml");
    AppController controller;
    engine.rootContext()->setContextProperty("appController", &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(PROJECT_SOURCE_DIR) + "/native/qml/OllamaHud/Overlay.qml"));
    QObject *object = component.createWithInitialProperties({{"appController", QVariant::fromValue(&controller)}});
    QVERIFY2(object, qPrintable(component.errorString()));
    delete object;
}

void NativeTests::qmlOverlayModuleLoads()
{
    QQmlEngine engine;
    engine.addImportPath("qrc:/qt/qml");
    engine.addImportPath(QStringLiteral(PROJECT_SOURCE_DIR) + "/native/qml");
    AppController controller;
    engine.rootContext()->setContextProperty("appController", &controller);
    QQmlComponent component(&engine);
    component.loadFromModule("OllamaHud", "Overlay");
    QObject *object = component.createWithInitialProperties({{"appController", QVariant::fromValue(&controller)}});
    QVERIFY2(object, qPrintable(component.errorString()));
    delete object;
}

void NativeTests::qmlMainLoads()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(PROJECT_SOURCE_DIR) + "/native/qml");
    AppController controller;
    engine.rootContext()->setContextProperty("appController", &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(PROJECT_SOURCE_DIR) + "/native/qml/OllamaHud/Main.qml"));
    QObject *object = component.create();
    QVERIFY2(object, qPrintable(component.errorString()));
    delete object;
}

int main(int argc, char **argv)
{
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    NativeTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "native_tests.moc"
