#include "AppController.h"
#include "ChatLogService.h"
#include "DetectorClient.h"
#include "DetectorSettingsStore.h"
#include "HotReloadController.h"
#include "InputSimulationService.h"
#include "OllamaService.h"
#include "SettingsStore.h"
#include "Shortcut.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <deque>

namespace {
class FakeInputBackend final : public InputSimulationBackend
{
public:
    bool sendKey(int key, bool pressed) override
    {
        events.append({key, pressed});
        ++calls;
        return calls != failAt;
    }
    QVector<QPair<int, bool>> events;
    int calls = 0;
    int failAt = -1;
};

class FakeInputRandom final : public InputSimulationRandom
{
public:
    double uniform(double minimum, double maximum) override { return take(uniforms, (minimum + maximum) / 2.0); }
    double normal(double mean, double) override { return take(normals, mean); }
    double generalizedNormal(double mean, double, double) override { return take(generalized, mean); }

    std::deque<double> uniforms;
    std::deque<double> normals;
    std::deque<double> generalized;

private:
    static double take(std::deque<double> &values, double fallback)
    {
        if (values.empty()) return fallback;
        const double value = values.front();
        values.pop_front();
        return value;
    }
};
}

class NativeTests : public QObject
{
    Q_OBJECT

private slots:
    void settingsLoadSaveValidation();
    void promptSettingsImportExport();
    void chatPayloadGeneration();
    void koreanTranslationPayloadIsIndependent();
    void detectorSettingsAndContext();
    void detectorTargetRowEditing();
    void detectorSettingsImportExport();
    void chatLogIncludesCompletionMetadata();
    void memorySelectionCollapsesDuplicates();
    void shortcutParsing();
    void inputSimulationSequenceAndCleanup();
    void inputSimulationFailureCleansUp();
    void hudCollapseTogglesWithoutDiscardingText();
    void qmlOverlayLoads();
    void qmlOverlayModuleLoads();
    void qmlMainLoads();
    void qmlUiResourcesAreAdopted();
    void hotReloadRecreatesSingleton();
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
simulation_trigger_shortcut: Alt+3
simulation_stop_shortcut: Alt+4
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
    QCOMPARE(settings.simulationTriggerShortcut, QString("Alt+3"));
    QCOMPARE(settings.simulationStopShortcut, QString("Alt+4"));
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
    invalid = HudSettings {};
    invalid.simulationStopShortcut = invalid.simulationTriggerShortcut;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SettingsStore::validate(invalid));

    const QString savedPath = dir.filePath("saved.yaml");
    SettingsStore::saveToPath(settings, savedPath);
    QVERIFY(QFile::exists(savedPath));
    HudSettings reloaded = SettingsStore::loadFromPath(savedPath);
    QCOMPARE(reloaded.query, QString("Where now?"));
}

void NativeTests::promptSettingsImportExport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("prompt-profile.yaml");

    SettingsStore source;
    source.setInstruction("Use concise directions.");
    source.setScreenshotContext("Trust only the current frame.");
    source.setQuery("Which doorway is next?");
    QVERIFY(source.savePromptToFile(path));
    QCOMPARE(source.localFilePath(QUrl::fromLocalFile(path)), QDir::fromNativeSeparators(path));
    QCOMPARE(source.settingsFolder().toLocalFile(), QFileInfo(SettingsStore::userConfigPath()).absolutePath());

    SettingsStore loaded;
    loaded.setHost("http://127.0.0.1:9999");
    QVERIFY(loaded.loadPromptFromFile(path));
    QCOMPARE(loaded.instruction(), QString("Use concise directions."));
    QCOMPARE(loaded.screenshotContext(), QString("Trust only the current frame."));
    QCOMPARE(loaded.query(), QString("Which doorway is next?"));
    QCOMPARE(loaded.host(), QString("http://127.0.0.1:9999"));

    QFile incomplete(path);
    QVERIFY(incomplete.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    incomplete.write("instruction: 'Missing the other prompt fields.'\n");
    incomplete.close();
    QVERIFY(!loaded.loadPromptFromFile(path));
    QCOMPARE(loaded.instruction(), QString("Use concise directions."));
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

void NativeTests::koreanTranslationPayloadIsIndependent()
{
    HudSettings settings;
    settings.model = "translation-model";
    settings.keepAlive = "5m";
    settings.think = true;
    settings.options.insert("temperature", 0.8);
    const QJsonObject payload = OllamaService::buildKoreanTranslationPayload(settings, "Turn left at the gate.");

    QCOMPARE(payload.value("model").toString(), QString("translation-model"));
    QCOMPARE(payload.value("think").toBool(), false);
    QCOMPARE(payload.value("options").toObject().value("temperature").toDouble(), 0.0);
    const QJsonArray messages = payload.value("messages").toArray();
    QCOMPARE(messages.size(), 2);
    QCOMPARE(messages.at(0).toObject().value("role").toString(), QString("system"));
    QCOMPARE(messages.at(1).toObject().value("role").toString(), QString("user"));
    QCOMPARE(messages.at(1).toObject().value("content").toString(), QString("Turn left at the gate."));
    QVERIFY(!messages.at(1).toObject().contains("images"));
}

void NativeTests::detectorSettingsAndContext()
{
    DetectorSettings settings;
    QVERIFY(settings.enabled == false);
    QVERIFY(QJsonDocument::fromJson(settings.targetsJson.toUtf8()).isArray());
    const QJsonObject result{{"detections", QJsonArray{QJsonObject{{"target", "Entrance"}, {"matched_prompt", "portal"}, {"source", "guides"}, {"score", 0.8}, {"raw_logit", 0.8}, {"box", QJsonArray{1, 2, 3, 4}}}}}};
    const QString context = DetectorClient::summary(result);
    QVERIFY(context.contains("Entrance:portal"));
    QVERIFY(context.contains("raw logit 0.800"));
    const QJsonObject guidedDetection{{"target", "Entrance"}, {"source", "guides"}, {"guide", "C:\\guides\\Entrance\\variants\\portal.png"}};
    QCOMPARE(DetectorClient::displayLabel(guidedDetection), QString("Entrance:variants/portal.png"));
    HudSettings hud;
    hud.query += "\n\nUse this structured detector context together with the screenshot; do not invent detections:\n" + context;
    const QJsonArray messages = OllamaService::buildChatPayload(hud, "image", {}).value("messages").toArray();
    QVERIFY(messages.last().toObject().value("content").toString().contains("OWLv2 detector results"));
}

void NativeTests::detectorTargetRowEditing()
{
    DetectorSettingsStore store;
    const int initialCount = store.targets().size();
    store.addTextTarget();
    QCOMPARE(store.targets().size(), initialCount + 1);
    const int index = store.targets().size() - 1;
    QVERIFY(!store.save());
    QVERIFY(store.lastError().contains("이름"));
    store.updateTextTarget(index, "Portal", "gate, doorway");
    const QVariantMap target = store.targets().at(index).toMap();
    QCOMPARE(target.value("name").toString(), QString("Portal"));
    QCOMPARE(target.value("prompts").toString(), QString("gate, doorway"));
    QCOMPARE(target.value("type").toString(), QString("text"));
    store.removeTarget(index);
    QCOMPARE(store.targets().size(), initialCount);

    store.addTextTarget();
    const int thresholdIndex = store.targets().size() - 1;
    store.updateTextTarget(thresholdIndex, "Portal", "entrance:1.2");
    QVERIFY(!store.save());
    QVERIFY(store.lastError().contains("임계값"));
    store.removeTarget(thresholdIndex);

    store.addImageTarget();
    const int imageIndex = store.targets().size() - 1;
    store.updateImageTarget(imageIndex, "Portal images", false, "C:/guides/Portal/positive", "C:/guides/Portal/negative");
    const QVariantMap imageTarget = store.targets().at(imageIndex).toMap();
    QCOMPARE(imageTarget.value("type").toString(), QString("image"));
    QVERIFY(!imageTarget.value("useDefaultGuideDirectories").toBool());
    QCOMPARE(imageTarget.value("positiveGuideDir").toString(), QString("C:/guides/Portal/positive"));
    QCOMPARE(imageTarget.value("negativeGuideDir").toString(), QString("C:/guides/Portal/negative"));
    QVERIFY(!store.save());
    QVERIFY(store.lastError().contains("양성 가이드 폴더"));
    store.removeTarget(imageIndex);

    QTemporaryDir guideRoot;
    QVERIFY(guideRoot.isValid());
    const QString nestedGuideDirectory = guideRoot.filePath("Portal/variants");
    QVERIFY(QDir().mkpath(nestedGuideDirectory));
    QFile guideFile(QDir(nestedGuideDirectory).filePath("example.png"));
    QVERIFY(guideFile.open(QIODevice::WriteOnly));
    guideFile.write("discovery-only test image");
    guideFile.close();
    store.setPositiveGuideFolder(guideRoot.path());
    QCOMPARE(store.defaultGuideTargetNames(), QStringList{"Portal"});
}

void NativeTests::detectorSettingsImportExport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("detector-snapshot.json");
    DetectorSettingsStore source;
    source.setModel("snapshot-model");
    source.setTextThreshold(0.35);
    source.updateTextTarget(0, "Snapshot target", "entrance:0.3, portal:0.6");
    QVERIFY(source.saveToFile(path));
    QCOMPARE(DetectorSettingsStore().settingsFolder().toLocalFile(), QFileInfo(DetectorSettingsStore::configPath()).absolutePath());
    QCOMPARE(source.localFilePath(QUrl::fromLocalFile(path)), QDir::fromNativeSeparators(path));
    QVERIFY(source.localFilePath(QUrl("https://example.com/detector-settings.json")).isEmpty());

    DetectorSettingsStore loaded;
    QVERIFY(loaded.loadFromFile(path));
    QCOMPARE(loaded.model(), QString("snapshot-model"));
    QCOMPARE(loaded.textThreshold(), 0.35);
    QCOMPARE(loaded.targets().first().toMap().value("prompts").toString(), QString("entrance:0.3, portal:0.6"));

    QFile invalid(path);
    QVERIFY(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalid.write("not JSON");
    invalid.close();
    QVERIFY(!loaded.loadFromFile(path));
    QCOMPARE(loaded.model(), QString("snapshot-model"));
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

void NativeTests::inputSimulationSequenceAndCleanup()
{
    auto backend = std::make_unique<FakeInputBackend>();
    auto *recordingBackend = backend.get();
    auto random = std::make_unique<FakeInputRandom>();
    random->uniforms = {0.51, 0.0, 0.05}; // Left, t1, r1
    random->normals = {1.0, 5.0, 0.2, 0.2}; // t2, t3, first two t4 values
    random->generalized = {0.4, 0.1, 0.052}; // t6, t5, t7
    InputSimulationService service(std::move(backend), std::move(random));

    service.startImmediatelyForTest();
    service.advanceForTest(0); // release horizontal and launch all three branches
    const QPair<int, bool> leftDown(0x25, true);
    const QPair<int, bool> leftUp(0x25, false);
    const QPair<int, bool> upDown(0x26, true);
    const QPair<int, bool> fDown(0x46, true);
    const QPair<int, bool> shiftDown(0xA0, true);
    const QPair<int, bool> upUp(0x26, false);
    const QPair<int, bool> shiftUp(0xA0, false);
    QCOMPARE(recordingBackend->events.at(0), leftDown);
    QCOMPARE(recordingBackend->events.at(1), leftUp);
    QCOMPARE(recordingBackend->events.at(2), upDown);

    service.advanceForTest(200);
    QVERIFY(recordingBackend->events.contains(fDown));
    service.advanceForTest(200);
    QVERIFY(recordingBackend->events.contains(shiftDown));
    QVERIFY(service.running());

    service.stop(QStringLiteral("test stop"));
    QVERIFY(!service.running());
    QVERIFY(recordingBackend->events.contains(upUp));
    QVERIFY(recordingBackend->events.contains(shiftUp));
}

void NativeTests::inputSimulationFailureCleansUp()
{
    auto backend = std::make_unique<FakeInputBackend>();
    auto *recordingBackend = backend.get();
    auto random = std::make_unique<FakeInputRandom>();
    random->uniforms = {0.51, 0.0, 0.05};
    random->normals = {1.0, 5.0, 0.2};
    random->generalized = {0.4};
    recordingBackend->failAt = 4; // Left down/up, Up down, then F down fails.
    InputSimulationService service(std::move(backend), std::move(random));

    service.startImmediatelyForTest();
    service.advanceForTest(0);
    service.advanceForTest(200);
    QVERIFY(!service.running());
    QVERIFY(service.status().startsWith("실패:"));
    const QPair<int, bool> upUp(0x26, false);
    QVERIFY(recordingBackend->events.contains(upUp));
}

void NativeTests::hudCollapseTogglesWithoutDiscardingText()
{
    AppController controller;
    QVERIFY(controller.sessionLogEntries().isEmpty());
    QVERIFY(!controller.openSessionScreenshot(SettingsStore::chatLogPath()));
    QSignalSpy spy(&controller, &AppController::hudCollapsedChanged);
    const QString originalMessage = controller.message();

    controller.toggleHudCollapsed();
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.hudCollapsed());
    QCOMPARE(controller.state(), QString("준비"));
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
    engine.rootContext()->setContextProperty("hotReloadEnabled", false);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(PROJECT_SOURCE_DIR) + "/native/qml/OllamaHud/Main.qml"));
    QObject *object = component.create();
    QVERIFY2(object, qPrintable(component.errorString()));
    delete object;
}

void NativeTests::qmlUiResourcesAreAdopted()
{
    QVERIFY(QFile::exists(":/native/qml/OllamaHud/UI/qmldir"));
    QVERIFY(QFile::exists(":/native/qml/OllamaHud/UI/Colors.qml"));
    QVERIFY(QFile::exists(":/native/qml/OllamaHud/UI/fonts/Pretendard-Regular.otf"));
    QVERIFY(QFile::exists(":/native/qml/OllamaHud/UI/fonts/Pretendard-Medium.otf"));
    QVERIFY(QFile::exists(":/native/qml/OllamaHud/UI/fonts/Pretendard-SemiBold.otf"));
    QVERIFY(QFile::exists(":/native/qml/OllamaHud/UI/fonts/Pretendard-Bold.otf"));
    QVERIFY(!QFile::exists(":/native/qml/GenyDL/qmldir"));
}

void NativeTests::hotReloadRecreatesSingleton()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString moduleDirectory = dir.filePath("Test/Ui");
    QVERIFY(QDir().mkpath(moduleDirectory));

    auto writeFile = [](const QString &path, const QByteArray &contents) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        return file.write(contents) == contents.size();
    };
    QVERIFY(writeFile(moduleDirectory + "/qmldir", "module Test.Ui\nsingleton Palette 1.0 Palette.qml\n"));
    QVERIFY(writeFile(moduleDirectory + "/Palette.qml", "pragma Singleton\nimport QtQml\nQtObject { readonly property string value: \"initial\" }\n"));
    const QString rootPath = dir.filePath("ReloadRoot.qml");
    QVERIFY(writeFile(rootPath, "import QtQml\nimport Test.Ui\nQtObject { property string observed: Palette.value }\n"));

    QQmlApplicationEngine engine;
    engine.addImportPath(dir.path());
    const QUrl rootUrl = QUrl::fromLocalFile(rootPath);
    engine.load(rootUrl);
    QCOMPARE(engine.rootObjects().size(), 1);
    QCOMPARE(engine.rootObjects().constFirst()->property("observed").toString(), QString("initial"));

    QVERIFY(writeFile(moduleDirectory + "/Palette.qml", "pragma Singleton\nimport QtQml\nQtObject { readonly property string value: \"updated\" }\n"));
    HotReloadController controller(&engine, rootUrl);
    QSignalSpy succeeded(&controller, &HotReloadController::reloadSucceeded);
    QSignalSpy failed(&controller, &HotReloadController::reloadFailed);
    controller.reload();
    QTRY_COMPARE(succeeded.count(), 1);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(engine.rootObjects().size(), 1);
    QCOMPARE(engine.rootObjects().constFirst()->property("observed").toString(), QString("updated"));
}

int main(int argc, char **argv)
{
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    NativeTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "native_tests.moc"
