#pragma once

#include "OllamaService.h"
#include "SettingsStore.h"
#include "Shortcut.h"

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <exception>

struct RuntimeSnapshot
{
    QString state = "READY";
    QString message;
    bool active = false;
    QString captureId;
    bool isError = false;
};

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SettingsStore *settingsStore READ settingsStore CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY snapshotChanged)
    Q_PROPERTY(QString message READ message NOTIFY snapshotChanged)
    Q_PROPERTY(QString visualAnswer READ visualAnswer NOTIFY snapshotChanged)
    Q_PROPERTY(QString captureId READ captureId NOTIFY snapshotChanged)
    Q_PROPERTY(bool active READ active NOTIFY snapshotChanged)
    Q_PROPERTY(bool hudRunning READ hudRunning NOTIFY hudRunningChanged)
    Q_PROPERTY(bool error READ error NOTIFY snapshotChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    SettingsStore *settingsStore();
    QString state() const;
    QString message() const;
    QString visualAnswer() const;
    QString captureId() const;
    bool active() const;
    bool hudRunning() const;
    bool error() const;

    Q_INVOKABLE void startHud();
    Q_INVOKABLE void stopHud();
    Q_INVOKABLE void captureOnce();
    Q_INVOKABLE void testOllama();
    Q_INVOKABLE void clearVisualAnswer();
    Q_INVOKABLE bool saveSettings();

signals:
    void snapshotChanged();
    void hudRunningChanged();
    void transientMessage(const QString &message);

private:
    RuntimeSnapshot runCaptureRequest();
    void setSnapshot(const RuntimeSnapshot &snapshot);
    void runAsyncRequest();
    void ensureOverlay();
    void closeOverlay();
    void pollHotkeys();
    void rememberAnswer(const QString &answer, const QImage &image);
    QString shortError(const std::exception &error) const;

    SettingsStore m_settingsStore;
    OllamaService m_ollamaService;
    RuntimeSnapshot m_snapshot;
    QList<ChatMemory> m_memories;
    QTimer m_hotkeyTimer;
    QPointer<QObject> m_overlay;
    QFutureWatcher<RuntimeSnapshot> m_requestWatcher;
    bool m_hudRunning = false;
    bool m_triggerArmed = true;
    bool m_clearArmed = true;
};
