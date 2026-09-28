#include "AppController.h"

#include "CaptureService.h"
#include "ChatLogService.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QGuiApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QMetaObject>
#include <QPainter>
#include <QPen>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSaveFile>
#include <QUrl>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
enum GlobalHotkeyId {
    CaptureHotkeyId = 0x4F01,
    ExitHotkeyId,
    ClearHotkeyId,
    SimulationStartHotkeyId,
    SimulationStopHotkeyId,
    DetectorToggleHotkeyId,
    LiveDetectionToggleHotkeyId,
    EmergencyExitHotkeyId,
};

bool registerGlobalHotkey(int id, const KeyboardShortcut &shortcut)
{
    UINT modifiers = MOD_NOREPEAT;
    if (shortcut.modifiers.contains(QStringLiteral("Ctrl"))) modifiers |= MOD_CONTROL;
    if (shortcut.modifiers.contains(QStringLiteral("Alt"))) modifiers |= MOD_ALT;
    if (shortcut.modifiers.contains(QStringLiteral("Shift"))) modifiers |= MOD_SHIFT;
    return RegisterHotKey(nullptr, id, modifiers, static_cast<UINT>(shortcut.keyCode)) != FALSE;
}
#endif

void applyClickThrough(QObject *object)
{
#ifdef Q_OS_WIN
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
#else
    Q_UNUSED(object);
#endif
}

bool latchedPress(const KeyboardShortcut &shortcut, bool &armed)
{
    const bool pressed = shortcut.isPressed();
    if (!pressed) {
        armed = true;
        return false;
    }
    if (!armed) {
        return false;
    }
    armed = false;
    return true;
}

QString saveSessionScreenshot(const QString &directory, const QString &captureId, const QString &jpegBase64)
{
    if (directory.isEmpty() || captureId.isEmpty() || jpegBase64.isEmpty()) {
        return {};
    }
    const QString filename = QStringLiteral("%1-%2.jpg")
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz"), captureId);
    const QString path = QDir(directory).filePath(filename);
    const QByteArray jpeg = QByteArray::fromBase64(jpegBase64.toLatin1());
    QSaveFile file(path);
    if (jpeg.isEmpty() || !file.open(QIODevice::WriteOnly) || file.write(jpeg) != jpeg.size() || !file.commit()) {
        return {};
    }
    return path;
}

QImage annotateDetections(const QImage &image, const QJsonObject &result)
{
    if (image.isNull()) {
        return image;
    }
    QImage annotated = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&annotated);
    painter.setRenderHint(QPainter::Antialiasing);
    const QList<QColor> colors { QColor("#f6c945"), QColor("#35d0ba"), QColor("#fb7185"), QColor("#a78bfa") };
    int index = 0;
    for (const QJsonValue &value : result.value("detections").toArray()) {
        const QJsonObject detection = value.toObject();
        const QJsonArray box = detection.value("box").toArray();
        if (box.size() != 4) {
            continue;
        }
        const QRectF bounds(
            QPointF(box.at(0).toDouble(), box.at(1).toDouble()),
            QPointF(box.at(2).toDouble(), box.at(3).toDouble()));
        const QRectF clipped = bounds.normalized().intersected(QRectF(0, 0, annotated.width(), annotated.height()));
        if (clipped.isEmpty()) {
            continue;
        }
        const QColor color = colors.at(index++ % colors.size());
        painter.setPen(QPen(color, qMax(2.0, annotated.width() / 700.0)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(clipped);
        const QString label = QStringLiteral("%1  %2")
                                  .arg(DetectorClient::displayLabel(detection))
                                  .arg(QString::number(detection.value("score").toDouble(), 'f', 2));
        const QFontMetrics metrics(painter.font());
        const QRect labelRect = metrics.boundingRect(label).adjusted(-5, -3, 5, 3);
        const int labelX = qBound(0, qRound(clipped.left()), qMax(0, annotated.width() - labelRect.width()));
        const int labelY = qMax(0, qRound(clipped.top()) - labelRect.height());
        painter.fillRect(QRect(labelX, labelY, labelRect.width(), labelRect.height()), color);
        painter.setPen(Qt::black);
        painter.drawText(labelX + 5, labelY + metrics.ascent() + 3, label);
    }
    return annotated;
}

QVariantMap sessionLogEntry(
    const QString &captureId,
    const QString &question,
    const QString &answer,
    const QString &error,
    const QString &screenshotPath,
    const QString &doneReason = {},
    qint64 generatedTokens = -1,
    qint64 totalDurationNs = -1)
{
    return {
        {"timestamp", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")},
        {"captureId", captureId},
        {"question", question},
        {"answer", answer},
        {"error", error},
        {"isError", !error.isEmpty()},
        {"screenshotPath", screenshotPath},
        {"screenshotUrl", screenshotPath.isEmpty() ? QString{} : QUrl::fromLocalFile(screenshotPath).toString()},
        {"doneReason", doneReason},
        {"generatedTokens", generatedTokens},
        {"totalDurationSeconds", totalDurationNs < 0 ? -1.0 : static_cast<double>(totalDurationNs) / 1000000000.0},
    };
}
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_snapshot.message = QStringLiteral("준비됨 - %1을(를) 누르세요").arg(parseShortcut(m_settingsStore.settings().triggerShortcut).display());
    connect(&m_hotkeyTimer, &QTimer::timeout, this, &AppController::pollHotkeys);
    m_hotkeyTimer.setInterval(30);
    connect(&m_inputSimulation, &InputSimulationService::changed, this, &AppController::simulationChanged);
#ifdef Q_OS_WIN
    QCoreApplication::instance()->installNativeEventFilter(this);
    connect(&m_settingsStore, &SettingsStore::settingsChanged, this, [this] {
        if (m_hudRunning) registerHotkeys();
    });
#endif
    connect(&m_detectorPollTimer, &QTimer::timeout, this, &AppController::pollDetectorLive);
    m_detectorPollTimer.setInterval(250);
    connect(&m_requestWatcher, &QFutureWatcher<CaptureRequestResult>::finished, this, [this] {
        const CaptureRequestResult result = m_requestWatcher.result();
        if (result.snapshot.state == "답변") {
            rememberAnswer(result.answer, result.memoryImageB64, result.settings);
        }
        if (!result.detectorResult.isEmpty()) {
            updateDetectorBoxes(result.detectorResult);
        }
        if (!result.sessionLogEntry.isEmpty()) {
            m_sessionLogEntries.prepend(result.sessionLogEntry);
            emit sessionLogChanged();
        }
        m_koreanTranslation = result.koreanTranslation;
        setSnapshot(result.snapshot);
        if (result.resumeLive) {
            resumeLiveDetection();
        }
    });
}

AppController::~AppController()
{
    stopHud();
#ifdef Q_OS_WIN
    QCoreApplication::instance()->removeNativeEventFilter(this);
#endif
    m_requestWatcher.waitForFinished();
}

SettingsStore *AppController::settingsStore() { return &m_settingsStore; }
DetectorSettingsStore *AppController::detectorSettingsStore() { return &m_detectorSettingsStore; }
QString AppController::state() const { return m_snapshot.state; }
QString AppController::message() const { return m_snapshot.message; }
QString AppController::visualAnswer() const { return m_snapshot.state == "답변" ? m_snapshot.message : QString(); }
QString AppController::koreanTranslation() const { return m_koreanTranslation; }
QString AppController::captureId() const { return m_snapshot.captureId; }
bool AppController::active() const { return m_snapshot.active; }
bool AppController::hudCollapsed() const { return m_hudCollapsed; }
bool AppController::hudRunning() const { return m_hudRunning; }
bool AppController::error() const { return m_snapshot.isError; }
bool AppController::simulationRunning() const { return m_inputSimulation.running(); }
QString AppController::simulationStatus() const { return m_inputSimulation.status(); }
QString AppController::detectorStatus() const { return m_detectorStatus; }
bool AppController::liveDetection() const { return m_liveDetection; }
QVariantList AppController::detectorBoxes() const { return m_detectorBoxes; }
QVariantList AppController::sessionLogEntries() const { return m_sessionLogEntries; }

void AppController::startHud()
{
    if (!saveSettings()) {
        return;
    }
    const bool started = !m_hudRunning;
    ensureOverlay();
    if (started) {
        m_hudRunning = true;
#ifdef Q_OS_WIN
        registerHotkeys();
#else
        m_hotkeyTimer.start();
#endif
        emit hudRunningChanged();
    }
    clearVisualAnswer();
    if (started && m_detectorSettingsStore.enabled()) {
        startLiveDetection();
    }
}

void AppController::stopHud()
{
#ifdef Q_OS_WIN
    unregisterHotkeys();
#else
    m_hotkeyTimer.stop();
#endif
    m_inputSimulation.stop(QStringLiteral("HUD 중지됨"));
    stopLiveDetection();
    closeOverlay();
    if (m_hudRunning) {
        m_hudRunning = false;
        emit hudRunningChanged();
    }
}

void AppController::captureOnce()
{
    if (m_snapshot.active || m_requestWatcher.isRunning()) {
        return;
    }
    if (!saveSettings()) {
        return;
    }
    const bool detectorEnabled = m_detectorSettingsStore.enabled();
    const DetectorSettings detectorSettings = m_detectorSettingsStore.settings();
    const bool resumeLive = m_liveDetection;
    if (detectorEnabled) {
        if (!prepareDetector()) {
            return;
        }
        if (resumeLive) {
            stopLiveDetection();
        }
    }
    runAsyncRequest(resumeLive, detectorEnabled, detectorSettings);
}

void AppController::testOllama()
{
    if (!saveSettings()) {
        return;
    }
    setSnapshot({"테스트 중", "Ollama 모델을 테스트하고 있습니다.", true, m_snapshot.captureId, false});
    const HudSettings settings = m_settingsStore.settings();
    QtConcurrent::run([this, settings] {
        RuntimeSnapshot snapshot;
        try {
            const OllamaReply reply = m_ollamaService.testModel(settings);
            snapshot = {"준비", QStringLiteral("모델 응답: %1").arg(reply.answer), false, {}, false};
        } catch (const std::exception &error) {
            snapshot = {"오류", shortError(error), false, {}, true};
        }
        QMetaObject::invokeMethod(this, [this, snapshot] { setSnapshot(snapshot); }, Qt::QueuedConnection);
    });
}

void AppController::clearVisualAnswer()
{
    const QString ready = QStringLiteral("준비됨 - %1을(를) 누르세요").arg(parseShortcut(m_settingsStore.settings().triggerShortcut).display());
    setSnapshot({"준비", ready, false, m_snapshot.captureId, false});
}

void AppController::toggleHudCollapsed()
{
    m_hudCollapsed = !m_hudCollapsed;
    emit hudCollapsedChanged();
}

void AppController::stopSimulation()
{
    m_inputSimulation.stop(QStringLiteral("사용자가 중지함"));
}

bool AppController::saveSettings()
{
    if (m_settingsStore.save()) {
        return true;
    }
    setSnapshot({"오류", m_settingsStore.lastError(), false, m_snapshot.captureId, true});
    return false;
}

bool AppController::prepareDetector()
{
    if (!m_detectorSettingsStore.save()) {
        m_detectorStatus = m_detectorSettingsStore.lastError();
        emit detectorChanged();
        setSnapshot({"오류", m_detectorStatus, false, m_snapshot.captureId, true});
        return false;
    }
    QString error;
    if (!m_detectorClient.prepare(&error)) {
        m_detectorStatus = error;
        emit detectorChanged();
        setSnapshot({"오류", error, false, m_snapshot.captureId, true});
        return false;
    }
    m_detectorStatus = "워커 준비됨";
    emit detectorChanged();
    return true;
}

void AppController::startLiveDetection()
{
    if (!m_hudRunning) {
        startHud();
        if (!m_hudRunning) return;
    }
    if (m_liveDetection) {
        return;
    }
    if (!prepareDetector()) return;
    try {
        m_detectorClient.startLive(m_detectorSettingsStore.settings());
        m_liveDetection = true;
        m_detectorStatus = "실시간 감지 실행 중";
        m_detectorPollTimer.setInterval(qRound(1000.0 / qMax(0.2, m_detectorSettingsStore.liveRate())));
        m_detectorPollTimer.start();
        emit detectorChanged();
    } catch (const std::exception &error) {
        m_detectorStatus = shortError(error);
        emit detectorChanged();
        setSnapshot({"오류", m_detectorStatus, false, m_snapshot.captureId, true});
    }
}

void AppController::stopLiveDetection()
{
    m_detectorPollTimer.stop();
    if (m_liveDetection) {
        try { m_detectorClient.stopLive(); } catch (const std::exception &error) { m_detectorStatus = shortError(error); }
    }
    m_liveDetection = false;
    m_detectorBoxes.clear();
    if (m_detectorStatus == "실시간 감지 실행 중") m_detectorStatus = m_detectorSettingsStore.enabled() ? "워커 준비됨" : "사용 안 함";
    emit detectorChanged();
}

void AppController::toggleDetectorEnabled()
{
    if (m_detectorSettingsStore.enabled()) {
        stopLiveDetection();
        m_detectorSettingsStore.setEnabled(false);
        if (m_detectorSettingsStore.save()) {
            m_detectorStatus = "사용 안 함";
        } else {
            m_detectorStatus = m_detectorSettingsStore.lastError();
        }
        emit detectorChanged();
        return;
    }

    m_detectorSettingsStore.setEnabled(true);
    if (!m_detectorSettingsStore.save()) {
        m_detectorSettingsStore.setEnabled(false);
        m_detectorStatus = m_detectorSettingsStore.lastError();
        setSnapshot({"오류", m_detectorStatus, false, m_snapshot.captureId, true});
    } else {
        m_detectorStatus = "사용 중 - 다음 캡처 준비됨";
    }
    emit detectorChanged();
}

void AppController::toggleLiveDetection()
{
    if (m_liveDetection) {
        stopLiveDetection();
    } else if (!m_detectorSettingsStore.enabled()) {
        setSnapshot({"오류", "실시간 감지를 시작하기 전에 OWLv2 감지기를 사용 설정하세요.", false, m_snapshot.captureId, true});
    } else {
        startLiveDetection();
    }
}

void AppController::resumeLiveDetection()
{
    if (m_detectorSettingsStore.enabled()) startLiveDetection();
}

CaptureRequestResult AppController::runCaptureRequest(const QImage &image, const HudSettings &settings, const QList<ChatMemory> &memories, bool detectorEnabled, const DetectorSettings &detectorSettings, const QString &sessionCaptureDirectory)
{
    QString retry = "none";
    QString thinking;
    QString captureId;
    QString sessionScreenshotPath;
    QString initial;
    try {
        captureId = CaptureService::imageFingerprint(image);
        initial = CaptureService::encodeJpegBase64(image, settings.screenshotMaxEdge, 85);
        QJsonObject detectorResult;
        QString detectorContext;
        if (detectorEnabled) {
            detectorResult = m_detectorClient.detect(image, detectorSettings);
            detectorContext = DetectorClient::summary(detectorResult, detectorSettings.ollamaContextDetectionLimit);
        }
        const QImage savedImage = detectorEnabled ? annotateDetections(image, detectorResult) : image;
        sessionScreenshotPath = saveSessionScreenshot(
            sessionCaptureDirectory, captureId,
            CaptureService::encodeJpegBase64(savedImage, settings.screenshotMaxEdge, 85));
        OllamaReply reply;
        try {
            reply = m_ollamaService.generateFromImage(settings, initial, memories, detectorContext);
        } catch (const OllamaException &error) {
            thinking = error.thinking();
            if (!OllamaService::isContextLimitError(error.what())) {
                throw;
            }
            retry = "compact screenshot";
            const QString compact = CaptureService::encodeJpegBase64(image, 768, 70);
            try {
                reply = m_ollamaService.generateFromImage(settings, compact, memories, detectorContext);
            } catch (const OllamaException &retryError) {
                thinking = retryError.thinking();
                if (OllamaService::isContextLimitError(retryError.what())) {
                    throw OllamaException("컨텍스트가 너무 큽니다. 캡처 크기를 줄이세요.");
                }
                throw;
            }
        }
        thinking = reply.thinking;
        QString koreanTranslation;
        try {
            koreanTranslation = m_ollamaService.translateToKorean(settings, reply.answer).answer;
        } catch (const std::exception &translationError) {
            koreanTranslation = QStringLiteral("한국어 번역을 생성하지 못했습니다: %1").arg(shortError(translationError));
        }
        const QString memoryImageB64 = settings.memoryQaPairs > 0 ? CaptureService::encodeJpegBase64(image, 768, 70) : QString();
        ChatLogService::write({
            captureId,
            settings.query,
            memories,
            reply.answer,
            {},
            retry,
            thinking,
            reply.doneReason,
            reply.promptEvalCount,
            reply.evalCount,
            reply.totalDurationNs,
            reply.loadDurationNs,
            reply.promptEvalDurationNs,
            reply.evalDurationNs,
        }, settings);
        return {
            {"답변", reply.answer, false, captureId, false},
            reply.answer,
            memoryImageB64,
            settings,
            detectorResult,
            false,
            sessionLogEntry(captureId, settings.query, reply.answer, {}, sessionScreenshotPath, reply.doneReason, reply.evalCount, reply.totalDurationNs),
            koreanTranslation,
        };
    } catch (const std::exception &error) {
        const QString message = shortError(error);
        if (sessionScreenshotPath.isEmpty() && !initial.isEmpty()) {
            sessionScreenshotPath = saveSessionScreenshot(sessionCaptureDirectory, captureId, initial);
        }
        ChatLogService::write({captureId, settings.query, memories, {}, message, retry, thinking}, settings);
        return {
            {"오류", message, false, captureId, true},
            {},
            {},
            settings,
            {},
            false,
            sessionLogEntry(captureId, settings.query, {}, message, sessionScreenshotPath),
            {},
        };
    }
}

void AppController::setSnapshot(const RuntimeSnapshot &snapshot)
{
    if (snapshot.state != "답변") {
        m_koreanTranslation.clear();
    }
    m_snapshot = snapshot;
    emit snapshotChanged();
}

void AppController::runAsyncRequest(bool resumeLive, bool detectorEnabled, const DetectorSettings &detectorSettings)
{
    setSnapshot({"캡처 중", "주 모니터를 캡처하고 있습니다.", true, m_snapshot.captureId, false});
    const HudSettings settings = m_settingsStore.settings();
    const QList<ChatMemory> memories = m_memories;

    QQuickWindow *overlayWindow = qobject_cast<QQuickWindow *>(m_overlay.data());
    const bool restoreOverlay = overlayWindow && overlayWindow->isVisible();
    if (restoreOverlay) {
        overlayWindow->hide();
    }

    QTimer::singleShot(80, this, [this, settings, memories, restoreOverlay, resumeLive, detectorEnabled, detectorSettings] {
        captureOnGuiThread(settings, memories, resumeLive, detectorEnabled, detectorSettings);
        if (restoreOverlay && m_overlay) {
            if (auto *overlayWindow = qobject_cast<QQuickWindow *>(m_overlay.data())) {
                overlayWindow->show();
                overlayWindow->raise();
                applyClickThrough(overlayWindow);
            }
        }
    });
}

void AppController::captureOnGuiThread(const HudSettings &settings, const QList<ChatMemory> &memories, bool resumeLive, bool detectorEnabled, const DetectorSettings &detectorSettings)
{
    QImage image;
    try {
        image = CaptureService::capturePrimaryMonitor();
    } catch (const std::exception &error) {
        setSnapshot({"오류", shortError(error), false, m_snapshot.captureId, true});
        return;
    }

    setSnapshot({"질문 중", "스크린샷을 Ollama로 보내고 있습니다.", true, m_snapshot.captureId, false});
    const QString sessionCaptureDirectory = m_sessionCaptureDirectory.isValid() ? m_sessionCaptureDirectory.path() : QString{};
    m_requestWatcher.setFuture(QtConcurrent::run([this, image, settings, memories, resumeLive, detectorEnabled, detectorSettings, sessionCaptureDirectory] {
        CaptureRequestResult result = runCaptureRequest(image, settings, memories, detectorEnabled, detectorSettings, sessionCaptureDirectory);
        result.resumeLive = resumeLive;
        return result;
    }));
}

bool AppController::openLogFolder()
{
    const QString folder = QFileInfo(SettingsStore::chatLogPath()).absolutePath();
    QDir().mkpath(folder);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

bool AppController::openSessionScreenshot(const QString &path)
{
    if (!m_sessionCaptureDirectory.isValid()) {
        return false;
    }
    const QFileInfo requested(path);
    const QString sessionRoot = QFileInfo(m_sessionCaptureDirectory.path()).canonicalFilePath();
    const QString screenshot = requested.canonicalFilePath();
    const QString screenshotFolder = requested.canonicalPath();
    if (sessionRoot.isEmpty() || screenshot.isEmpty()
        || screenshotFolder.compare(sessionRoot, Qt::CaseInsensitive) != 0
        || requested.suffix().compare("jpg", Qt::CaseInsensitive) != 0) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(screenshot));
}

void AppController::updateDetectorBoxes(const QJsonObject &result)
{
    const QJsonObject capture = result.value("capture").toObject();
    const double captureWidth = qMax(1.0, capture.value("width").toDouble());
    const double captureHeight = qMax(1.0, capture.value("height").toDouble());
    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect geometry = screen ? screen->geometry() : QRect(0, 0, static_cast<int>(captureWidth), static_cast<int>(captureHeight));
    const double scaleX = geometry.width() / captureWidth;
    const double scaleY = geometry.height() / captureHeight;
    QVariantList boxes;
    for (const QJsonValue &value : result.value("detections").toArray()) {
        const QJsonObject detection = value.toObject(); const QJsonArray box = detection.value("box").toArray(); if (box.size() != 4) continue;
        const QString label = DetectorClient::displayLabel(detection);
        boxes.append(QVariantMap{{"x", geometry.x() + box.at(0).toDouble() * scaleX}, {"y", geometry.y() + box.at(1).toDouble() * scaleY}, {"width", (box.at(2).toDouble() - box.at(0).toDouble()) * scaleX}, {"height", (box.at(3).toDouble() - box.at(1).toDouble()) * scaleY}, {"label", label}, {"score", detection.value("score").toDouble()}, {"source", detection.value("source").toString()}});
    }
    m_detectorBoxes = boxes;
    const QJsonObject runtime = result.value("runtime").toObject();
    const double ms = result.value("latency").toObject().value("total_ms").toDouble();
    m_detectorStatus = QStringLiteral("상자 %1개 | %2 %3 | %4 ms").arg(boxes.size()).arg(runtime.value("device").toString()).arg(runtime.value("dtype").toString()).arg(QString::number(ms, 'f', 0));
    emit detectorChanged();
}

void AppController::pollDetectorLive()
{
    if (!m_liveDetection) return;
    try { updateDetectorBoxes(m_detectorClient.latestLive()); } catch (const std::exception &error) { m_detectorStatus = shortError(error); emit detectorChanged(); }
}

void AppController::ensureOverlay()
{
    if (m_overlay) {
        return;
    }
    auto *engine = qobject_cast<QQmlApplicationEngine *>(qmlEngine(this));
    if (!engine) {
        engine = new QQmlApplicationEngine(this);
        engine->rootContext()->setContextProperty("appController", this);
        engine->addImportPath("qrc:/qt/qml");
        engine->addImportPath("qrc:/native/qml");
#ifdef OLLAMA_HUD_HOT_RELOAD
        engine->addImportPath(QStringLiteral(OLLAMA_HUD_QML_SOURCE_DIR));
#endif
    }
    QQmlComponent component(engine, this);
#ifdef OLLAMA_HUD_HOT_RELOAD
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(OLLAMA_HUD_QML_SOURCE_DIR "/OllamaHud/Overlay.qml")));
#else
    component.loadFromModule("OllamaHud", "Overlay");
#endif
    QObject *created = component.createWithInitialProperties({{"appController", QVariant::fromValue(this)}});
    if (!created) {
        setSnapshot({"오류", component.errorString(), false, m_snapshot.captureId, true});
        return;
    }
    m_overlay = created;
    if (auto *window = qobject_cast<QQuickWindow *>(created)) {
        window->show();
        window->raise();
    }
    applyClickThrough(created);
}

void AppController::closeOverlay()
{
    if (m_overlay) {
        m_overlay->deleteLater();
        m_overlay.clear();
    }
}

void AppController::pollHotkeys()
{
    try {
        const HudSettings settings = m_settingsStore.settings();
        const KeyboardShortcut exitShortcut = parseShortcut(settings.exitShortcut);
        if (exitShortcutPressed(exitShortcut)) {
            stopHud();
            return;
        }
        const KeyboardShortcut clearShortcut = parseShortcut(settings.clearShortcut);
        if (!m_snapshot.active && latchedPress(clearShortcut, m_clearArmed)) {
            toggleHudCollapsed();
        }
        const KeyboardShortcut triggerShortcut = parseShortcut(settings.triggerShortcut);
        if (!m_snapshot.active && latchedPress(triggerShortcut, m_triggerArmed)) {
            captureOnce();
        }
        const KeyboardShortcut simulationStopShortcut = parseShortcut(settings.simulationStopShortcut);
        if (m_inputSimulation.running() && latchedPress(simulationStopShortcut, m_simulationStopArmed)) {
            m_inputSimulation.stop(QStringLiteral("긴급 중지"));
        }
        const KeyboardShortcut simulationTriggerShortcut = parseShortcut(settings.simulationTriggerShortcut);
        if (latchedPress(simulationTriggerShortcut, m_simulationTriggerArmed)) {
            m_inputSimulation.configureBackend(settings.simulationInputBackend, settings.simulationRicochetPort);
            m_inputSimulation.start(simulationTriggerShortcut);
        }
        const KeyboardShortcut detectorToggleShortcut = parseShortcut(settings.detectorToggleShortcut);
        if (latchedPress(detectorToggleShortcut, m_detectorToggleArmed)) {
            toggleDetectorEnabled();
        }
        const KeyboardShortcut liveDetectionToggleShortcut = parseShortcut(settings.liveDetectionToggleShortcut);
        if (latchedPress(liveDetectionToggleShortcut, m_liveDetectionToggleArmed)) {
            toggleLiveDetection();
        }
    } catch (const std::exception &error) {
        setSnapshot({"오류", shortError(error), false, m_snapshot.captureId, true});
    }
}

#ifdef Q_OS_WIN
bool AppController::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
    const auto *nativeMessage = static_cast<MSG *>(message);
    if (nativeMessage->message != WM_HOTKEY) {
        return false;
    }
    handleHotkey(static_cast<int>(nativeMessage->wParam));
    return true;
}

void AppController::registerHotkeys()
{
    unregisterHotkeys();
    try {
        const HudSettings settings = m_settingsStore.settings();
        const struct { int id; KeyboardShortcut shortcut; } shortcuts[] = {
            {CaptureHotkeyId, parseShortcut(settings.triggerShortcut)},
            {ExitHotkeyId, parseShortcut(settings.exitShortcut)},
            {ClearHotkeyId, parseShortcut(settings.clearShortcut)},
            {SimulationStartHotkeyId, parseShortcut(settings.simulationTriggerShortcut)},
            {SimulationStopHotkeyId, parseShortcut(settings.simulationStopShortcut)},
            {DetectorToggleHotkeyId, parseShortcut(settings.detectorToggleShortcut)},
            {LiveDetectionToggleHotkeyId, parseShortcut(settings.liveDetectionToggleShortcut)},
            {EmergencyExitHotkeyId, parseShortcut(QStringLiteral("Ctrl+`"))},
        };
        for (const auto &entry : shortcuts) {
            if (!registerGlobalHotkey(entry.id, entry.shortcut)) {
                qWarning() << "Global shortcut unavailable:" << entry.shortcut.display();
            }
        }
    } catch (const std::exception &error) {
        qWarning() << "Could not register global shortcuts:" << error.what();
    }
}

void AppController::unregisterHotkeys() const
{
    for (int id = CaptureHotkeyId; id <= EmergencyExitHotkeyId; ++id) {
        UnregisterHotKey(nullptr, id);
    }
}

void AppController::handleHotkey(int id)
{
    if (id == EmergencyExitHotkeyId || id == ExitHotkeyId) {
        stopHud();
        return;
    }
    if (!m_hudRunning) {
        return;
    }
    switch (id) {
    case CaptureHotkeyId:
        if (!m_snapshot.active) captureOnce();
        break;
    case ClearHotkeyId:
        if (!m_snapshot.active) toggleHudCollapsed();
        break;
    case SimulationStartHotkeyId: {
        const HudSettings settings = m_settingsStore.settings();
        m_inputSimulation.configureBackend(settings.simulationInputBackend, settings.simulationRicochetPort);
        m_inputSimulation.start(parseShortcut(settings.simulationTriggerShortcut));
        break;
    }
    case SimulationStopHotkeyId:
        if (m_inputSimulation.running()) m_inputSimulation.stop(QStringLiteral("긴급 중지"));
        break;
    case DetectorToggleHotkeyId:
        toggleDetectorEnabled();
        break;
    case LiveDetectionToggleHotkeyId:
        toggleLiveDetection();
        break;
    default:
        break;
    }
}
#endif

void AppController::rememberAnswer(const QString &answer, const QString &imageB64, const HudSettings &settings)
{
    if (settings.memoryQaPairs <= 0) {
        m_memories.clear();
        return;
    }
    const ChatMemory memory{settings.query, answer, imageB64};
    if (!m_memories.isEmpty() && m_memories.last().question == memory.question && m_memories.last().answer == memory.answer) {
        return;
    }
    m_memories.append(memory);
    while (m_memories.size() > settings.memoryQaPairs) {
        m_memories.removeFirst();
    }
}

QString AppController::shortError(const std::exception &error) const
{
    QString text = QString::fromUtf8(error.what()).trimmed();
    if (text.isEmpty()) {
        text = "알 수 없는 오류";
    }
    return text.size() <= 180 ? text : text.left(177) + "...";
}
