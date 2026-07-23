#include "AppController.h"

#include "CaptureService.h"
#include "ChatLogService.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
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
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_snapshot.message = QStringLiteral("Ready - press %1").arg(parseShortcut(m_settingsStore.settings().triggerShortcut).display());
    connect(&m_hotkeyTimer, &QTimer::timeout, this, &AppController::pollHotkeys);
    m_hotkeyTimer.setInterval(30);
    connect(&m_requestWatcher, &QFutureWatcher<CaptureRequestResult>::finished, this, [this] {
        const CaptureRequestResult result = m_requestWatcher.result();
        if (result.snapshot.state == "ANSWER") {
            rememberAnswer(result.answer, result.memoryImageB64, result.settings);
        }
        setSnapshot(result.snapshot);
    });
}

AppController::~AppController()
{
    stopHud();
    m_requestWatcher.waitForFinished();
}

SettingsStore *AppController::settingsStore() { return &m_settingsStore; }
QString AppController::state() const { return m_snapshot.state; }
QString AppController::message() const { return m_snapshot.message; }
QString AppController::visualAnswer() const { return m_snapshot.state == "ANSWER" ? m_snapshot.message : QString(); }
QString AppController::captureId() const { return m_snapshot.captureId; }
bool AppController::active() const { return m_snapshot.active; }
bool AppController::hudCollapsed() const { return m_hudCollapsed; }
bool AppController::hudRunning() const { return m_hudRunning; }
bool AppController::error() const { return m_snapshot.isError; }

void AppController::startHud()
{
    if (!saveSettings()) {
        return;
    }
    ensureOverlay();
    if (!m_hudRunning) {
        m_hudRunning = true;
        m_hotkeyTimer.start();
        emit hudRunningChanged();
    }
    clearVisualAnswer();
}

void AppController::stopHud()
{
    m_hotkeyTimer.stop();
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
    runAsyncRequest();
}

void AppController::testOllama()
{
    if (!saveSettings()) {
        return;
    }
    setSnapshot({"TESTING", "Testing Ollama model.", true, m_snapshot.captureId, false});
    const HudSettings settings = m_settingsStore.settings();
    QtConcurrent::run([this, settings] {
        RuntimeSnapshot snapshot;
        try {
            const OllamaReply reply = m_ollamaService.testModel(settings);
            snapshot = {"READY", QStringLiteral("Model replied: %1").arg(reply.answer), false, {}, false};
        } catch (const std::exception &error) {
            snapshot = {"ERROR", shortError(error), false, {}, true};
        }
        QMetaObject::invokeMethod(this, [this, snapshot] { setSnapshot(snapshot); }, Qt::QueuedConnection);
    });
}

void AppController::clearVisualAnswer()
{
    const QString ready = QStringLiteral("Ready - press %1").arg(parseShortcut(m_settingsStore.settings().triggerShortcut).display());
    setSnapshot({"READY", ready, false, m_snapshot.captureId, false});
}

void AppController::toggleHudCollapsed()
{
    m_hudCollapsed = !m_hudCollapsed;
    emit hudCollapsedChanged();
}

bool AppController::saveSettings()
{
    if (m_settingsStore.save()) {
        return true;
    }
    setSnapshot({"ERROR", m_settingsStore.lastError(), false, m_snapshot.captureId, true});
    return false;
}

CaptureRequestResult AppController::runCaptureRequest(const QImage &image, const HudSettings &settings, const QList<ChatMemory> &memories)
{
    QString retry = "none";
    QString thinking;
    QString captureId;
    try {
        captureId = CaptureService::imageFingerprint(image);
        QString initial = CaptureService::encodeJpegBase64(image, settings.screenshotMaxEdge, 85);
        OllamaReply reply;
        try {
            reply = m_ollamaService.generateFromImage(settings, initial, memories);
        } catch (const OllamaException &error) {
            thinking = error.thinking();
            if (!OllamaService::isContextLimitError(error.what())) {
                throw;
            }
            retry = "compact screenshot";
            const QString compact = CaptureService::encodeJpegBase64(image, 768, 70);
            try {
                reply = m_ollamaService.generateFromImage(settings, compact, memories);
            } catch (const OllamaException &retryError) {
                thinking = retryError.thinking();
                if (OllamaService::isContextLimitError(retryError.what())) {
                    throw OllamaException("Context too large; lower capture size.");
                }
                throw;
            }
        }
        thinking = reply.thinking;
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
        return {{"ANSWER", reply.answer, false, captureId, false}, reply.answer, memoryImageB64, settings};
    } catch (const std::exception &error) {
        const QString message = shortError(error);
        ChatLogService::write({captureId, settings.query, memories, {}, message, retry, thinking}, settings);
        return {{"ERROR", message, false, captureId, true}, {}, {}, settings};
    }
}

void AppController::setSnapshot(const RuntimeSnapshot &snapshot)
{
    m_snapshot = snapshot;
    emit snapshotChanged();
}

void AppController::runAsyncRequest()
{
    setSnapshot({"CAPTURING", "Capturing primary monitor.", true, m_snapshot.captureId, false});
    const HudSettings settings = m_settingsStore.settings();
    const QList<ChatMemory> memories = m_memories;

    QQuickWindow *overlayWindow = qobject_cast<QQuickWindow *>(m_overlay.data());
    const bool restoreOverlay = overlayWindow && overlayWindow->isVisible();
    if (restoreOverlay) {
        overlayWindow->hide();
    }

    QTimer::singleShot(80, this, [this, settings, memories, restoreOverlay] {
        captureOnGuiThread(settings, memories);
        if (restoreOverlay && m_overlay) {
            if (auto *overlayWindow = qobject_cast<QQuickWindow *>(m_overlay.data())) {
                overlayWindow->show();
                overlayWindow->raise();
                applyClickThrough(overlayWindow);
            }
        }
    });
}

void AppController::captureOnGuiThread(const HudSettings &settings, const QList<ChatMemory> &memories)
{
    QImage image;
    try {
        image = CaptureService::capturePrimaryMonitor();
    } catch (const std::exception &error) {
        setSnapshot({"ERROR", shortError(error), false, m_snapshot.captureId, true});
        return;
    }

    setSnapshot({"ASKING", "Sending screenshot to Ollama.", true, m_snapshot.captureId, false});
    m_requestWatcher.setFuture(QtConcurrent::run([this, image, settings, memories] {
        return runCaptureRequest(image, settings, memories);
    }));
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
    }
    QQmlComponent component(engine, this);
#ifdef OLLAMA_HUD_HOT_RELOAD
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(PROJECT_SOURCE_DIR "/native/qml/OllamaHud/Overlay.qml")));
#else
    component.loadFromModule("OllamaHud", "Overlay");
#endif
    QObject *created = component.createWithInitialProperties({{"appController", QVariant::fromValue(this)}});
    if (!created) {
        setSnapshot({"ERROR", component.errorString(), false, m_snapshot.captureId, true});
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
            QGuiApplication::quit();
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
    } catch (const std::exception &error) {
        setSnapshot({"ERROR", shortError(error), false, m_snapshot.captureId, true});
    }
}

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
        text = "Unknown error";
    }
    return text.size() <= 180 ? text : text.left(177) + "...";
}
