#pragma once

#include "OllamaService.h"
#include "DetectorClient.h"
#include "DetectorSettingsStore.h"
#include "InputSimulationService.h"
#include "SettingsStore.h"
#include "Shortcut.h"

#include <QFutureWatcher>
#include <QAbstractNativeEventFilter>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantList>
#include <exception>

struct RuntimeSnapshot
{
    QString state = "준비";
    QString message;
    bool active = false;
    QString captureId;
    bool isError = false;
};

struct CaptureRequestResult
{
    RuntimeSnapshot snapshot;
    QString answer;
    QString memoryImageB64;
    HudSettings settings;
    QJsonObject detectorResult;
    bool resumeLive = false;
    QVariantMap sessionLogEntry;
    QString koreanTranslation;
};

class AppController : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(SettingsStore *settingsStore READ settingsStore CONSTANT)
    Q_PROPERTY(DetectorSettingsStore *detectorSettingsStore READ detectorSettingsStore CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY snapshotChanged)
    Q_PROPERTY(QString message READ message NOTIFY snapshotChanged)
    Q_PROPERTY(QString visualAnswer READ visualAnswer NOTIFY snapshotChanged)
    Q_PROPERTY(QString koreanTranslation READ koreanTranslation NOTIFY snapshotChanged)
    Q_PROPERTY(QString captureId READ captureId NOTIFY snapshotChanged)
    Q_PROPERTY(bool active READ active NOTIFY snapshotChanged)
    Q_PROPERTY(bool hudCollapsed READ hudCollapsed NOTIFY hudCollapsedChanged)
    Q_PROPERTY(bool hudRunning READ hudRunning NOTIFY hudRunningChanged)
    Q_PROPERTY(bool error READ error NOTIFY snapshotChanged)
    Q_PROPERTY(bool simulationRunning READ simulationRunning NOTIFY simulationChanged)
    Q_PROPERTY(QString simulationStatus READ simulationStatus NOTIFY simulationChanged)
    Q_PROPERTY(QString detectorStatus READ detectorStatus NOTIFY detectorChanged)
    Q_PROPERTY(bool liveDetection READ liveDetection NOTIFY detectorChanged)
    Q_PROPERTY(QVariantList detectorBoxes READ detectorBoxes NOTIFY detectorChanged)
    Q_PROPERTY(QVariantList sessionLogEntries READ sessionLogEntries NOTIFY sessionLogChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    SettingsStore *settingsStore();
    DetectorSettingsStore *detectorSettingsStore();
    QString state() const;
    QString message() const;
    QString visualAnswer() const;
    QString koreanTranslation() const;
    QString captureId() const;
    bool active() const;
    bool hudCollapsed() const;
    bool hudRunning() const;
    bool error() const;
    bool simulationRunning() const;
    QString simulationStatus() const;
    QString detectorStatus() const;
    bool liveDetection() const;
    QVariantList detectorBoxes() const;
    QVariantList sessionLogEntries() const;

    Q_INVOKABLE void startHud();
    Q_INVOKABLE void stopHud();
    Q_INVOKABLE void captureOnce();
    Q_INVOKABLE void testOllama();
    Q_INVOKABLE void clearVisualAnswer();
    Q_INVOKABLE void toggleHudCollapsed();
    Q_INVOKABLE void stopSimulation();
    Q_INVOKABLE bool saveSettings();
    Q_INVOKABLE void startLiveDetection();
    Q_INVOKABLE void stopLiveDetection();
    Q_INVOKABLE void toggleDetectorEnabled();
    Q_INVOKABLE void toggleLiveDetection();
    Q_INVOKABLE bool openLogFolder();
    Q_INVOKABLE bool openSessionScreenshot(const QString &path);

#ifdef Q_OS_WIN
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

signals:
    void snapshotChanged();
    void hudCollapsedChanged();
    void hudRunningChanged();
    void transientMessage(const QString &message);
    void simulationChanged();
    void detectorChanged();
    void sessionLogChanged();

private:
    CaptureRequestResult runCaptureRequest(const QImage &image, const HudSettings &settings, const QList<ChatMemory> &memories, bool detectorEnabled, const DetectorSettings &detectorSettings, const QString &sessionCaptureDirectory);
    void setSnapshot(const RuntimeSnapshot &snapshot);
    void runAsyncRequest(bool resumeLive, bool detectorEnabled, const DetectorSettings &detectorSettings);
    void captureOnGuiThread(const HudSettings &settings, const QList<ChatMemory> &memories, bool resumeLive, bool detectorEnabled, const DetectorSettings &detectorSettings);
    bool prepareDetector();
    void resumeLiveDetection();
    void updateDetectorBoxes(const QJsonObject &result);
    void pollDetectorLive();
    void ensureOverlay();
    void closeOverlay();
    void pollHotkeys();
#ifdef Q_OS_WIN
    void registerHotkeys();
    void unregisterHotkeys() const;
    void handleHotkey(int id);
#endif
    void rememberAnswer(const QString &answer, const QString &imageB64, const HudSettings &settings);
    QString shortError(const std::exception &error) const;

    SettingsStore m_settingsStore;
    DetectorSettingsStore m_detectorSettingsStore;
    OllamaService m_ollamaService;
    DetectorClient m_detectorClient;
    InputSimulationService m_inputSimulation;
    RuntimeSnapshot m_snapshot;
    QString m_koreanTranslation;
    QList<ChatMemory> m_memories;
    QTimer m_hotkeyTimer;
    QTimer m_detectorPollTimer;
    QPointer<QObject> m_overlay;
    QFutureWatcher<CaptureRequestResult> m_requestWatcher;
    bool m_hudRunning = false;
    bool m_hudCollapsed = false;
    bool m_triggerArmed = true;
    bool m_clearArmed = true;
    bool m_simulationTriggerArmed = true;
    bool m_simulationStopArmed = true;
    bool m_detectorToggleArmed = true;
    bool m_liveDetectionToggleArmed = true;
    bool m_liveDetection = false;
    QString m_detectorStatus = "사용 안 함";
    QVariantList m_detectorBoxes;
    QVariantList m_sessionLogEntries;
    QTemporaryDir m_sessionCaptureDirectory;
};
