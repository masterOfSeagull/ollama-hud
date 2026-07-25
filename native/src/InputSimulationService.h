#pragma once

#include "Shortcut.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <memory>

class InputSimulationBackend
{
public:
    virtual ~InputSimulationBackend() = default;
    virtual bool sendKey(int virtualKey, bool pressed) = 0;
};

class InputSimulationRandom
{
public:
    virtual ~InputSimulationRandom() = default;
    virtual double uniform(double minimum, double maximum) = 0;
    virtual double normal(double mean, double standardDeviation) = 0;
    virtual double generalizedNormal(double mean, double standardDeviation, double shape) = 0;
};

class InputSimulationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    explicit InputSimulationService(QObject *parent = nullptr);
    InputSimulationService(std::unique_ptr<InputSimulationBackend> backend,
        std::unique_ptr<InputSimulationRandom> random, QObject *parent = nullptr);
    ~InputSimulationService() override;

    bool running() const;
    QString status() const;
    void start(const KeyboardShortcut &activationShortcut);
    void stop(const QString &reason = QStringLiteral("Stopped"));

    // Test hooks: the production path is driven by the elapsed timer and shortcut state.
    void startImmediatelyForTest();
    void advanceForTest(qint64 milliseconds);

signals:
    void changed();

private:
    enum class Event { HorizontalUp, StartBranches, UpUp, FDown, FUp, ShiftDown, ShiftUp };

    void tick();
    void process(qint64 now);
    void schedule(Event event, qint64 delayMilliseconds);
    void beginSequence(qint64 now);
    void beginBranches(qint64 now);
    void handle(Event event, qint64 now);
    void scheduleFDown(qint64 now);
    void scheduleShiftDown(qint64 now);
    double boundedNormal(double mean, double standardDeviation, double minimum, double maximum, const QString &name);
    double boundedGeneralizedNormal(double mean, double standardDeviation, double shape, double minimum, double maximum, const QString &name);
    bool keyDown(int virtualKey);
    bool keyUp(int virtualKey);
    void releaseAll();
    void completeIfDone();
    void fail(const QString &message);
    void setStatus(const QString &status);
    void log(const QString &message) const;
    qint64 secondsToMilliseconds(double seconds) const;

    std::unique_ptr<InputSimulationBackend> m_backend;
    std::unique_ptr<InputSimulationRandom> m_random;
    QTimer m_timer;
    QElapsedTimer m_clock;
    KeyboardShortcut m_activationShortcut;
    QSet<int> m_heldKeys;
    QVector<QPair<qint64, Event>> m_events;
    QString m_status = QStringLiteral("Idle");
    bool m_running = false;
    bool m_waitingForRelease = false;
    bool m_testClock = false;
    qint64 m_testNow = 0;
    qint64 m_t2Milliseconds = 0;
    int m_horizontalKey = 0;
    qint64 m_fStartedAt = 0;
    qint64 m_shiftStartedAt = 0;
    double m_inverseT3 = 0.0;
    double m_r1 = 0.0;
    bool m_upDone = false;
    bool m_fDone = false;
    bool m_shiftDone = false;
};
