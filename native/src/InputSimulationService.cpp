#include "InputSimulationService.h"

#include "SettingsStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <random>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int vkLeft = 0x25;
constexpr int vkUp = 0x26;
constexpr int vkRight = 0x27;
constexpr int vkF = 0x46;
constexpr int vkLeftShift = 0xA0;

bool requiresExtendedScanCode(int virtualKey)
{
    switch (virtualKey) {
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
        return true;
    default:
        return false;
    }
}

class NativeInputBackend final : public InputSimulationBackend
{
public:
    bool sendKey(int virtualKey, bool pressed) override
    {
#ifdef Q_OS_WIN
        // Emit every simulated key as a physical scan code.  The explicit
        // Left Shift virtual key maps to scan code 0x2A, avoiding ambiguous
        // generic Shift input on games that read raw keyboard events.
        const UINT scanCode = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
        if (scanCode == 0) {
            return false;
        }
        INPUT input {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 0;
        input.ki.wScan = static_cast<WORD>(scanCode & 0xff);
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
        // MapVirtualKeyW may omit the E0 prefix on some layouts.  Navigation
        // keys must still be marked extended or Windows treats them as keypad
        // 4/6/8/2 rather than cursor keys.
        if (requiresExtendedScanCode(virtualKey) || (scanCode & 0xff00) == 0xe000) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        if (!pressed) {
            input.ki.dwFlags |= KEYEVENTF_KEYUP;
        }
        return SendInput(1, &input, sizeof(input)) == 1;
#else
        Q_UNUSED(virtualKey);
        Q_UNUSED(pressed);
        return false;
#endif
    }
};

class NativeInputRandom final : public InputSimulationRandom
{
public:
    double uniform(double minimum, double maximum) override
    {
        std::uniform_real_distribution<double> distribution(minimum, maximum);
        return distribution(m_engine);
    }

    double normal(double mean, double standardDeviation) override
    {
        std::normal_distribution<double> distribution(mean, standardDeviation);
        return distribution(m_engine);
    }

    double generalizedNormal(double mean, double standardDeviation, double shape) override
    {
        // f(x) is proportional to exp(-(|x-mean|/alpha)^shape).  This alpha
        // makes standardDeviation the actual standard deviation of the draw.
        const double alpha = standardDeviation * std::sqrt(std::tgamma(1.0 / shape) / std::tgamma(3.0 / shape));
        std::gamma_distribution<double> magnitude(1.0 / shape, 1.0);
        std::bernoulli_distribution sign;
        const double offset = alpha * std::pow(magnitude(m_engine), 1.0 / shape);
        return mean + (sign(m_engine) ? offset : -offset);
    }

private:
    std::mt19937_64 m_engine {std::random_device {}()};
};
}

InputSimulationService::InputSimulationService(QObject *parent)
    : InputSimulationService(std::make_unique<NativeInputBackend>(), std::make_unique<NativeInputRandom>(), parent)
{
}

InputSimulationService::InputSimulationService(std::unique_ptr<InputSimulationBackend> backend,
    std::unique_ptr<InputSimulationRandom> random, QObject *parent)
    : QObject(parent)
    , m_backend(std::move(backend))
    , m_random(std::move(random))
{
    m_timer.setInterval(10);
    connect(&m_timer, &QTimer::timeout, this, &InputSimulationService::tick);
}

InputSimulationService::~InputSimulationService()
{
    stop(QStringLiteral("종료됨"));
}

bool InputSimulationService::running() const { return m_running; }
QString InputSimulationService::status() const { return m_status; }

void InputSimulationService::start(const KeyboardShortcut &activationShortcut)
{
    if (m_running) {
        log(QStringLiteral("ignored trigger while active"));
        return;
    }
    m_testClock = false;
    m_activationShortcut = activationShortcut;
    m_running = true;
    m_waitingForRelease = true;
    m_clock.restart();
    setStatus(QStringLiteral("%1 키를 놓는 중").arg(activationShortcut.display()));
    log(QStringLiteral("activation received; waiting for shortcut release"));
    m_timer.start();
}

void InputSimulationService::startImmediatelyForTest()
{
    if (m_running) {
        return;
    }
    m_testClock = true;
    m_testNow = 0;
    m_running = true;
    m_waitingForRelease = false;
    beginSequence(0);
}

void InputSimulationService::advanceForTest(qint64 milliseconds)
{
    if (!m_testClock || milliseconds < 0) {
        return;
    }
    m_testNow += milliseconds;
    process(m_testNow);
}

void InputSimulationService::stop(const QString &reason)
{
    if (!m_running && m_heldKeys.isEmpty()) {
        return;
    }
    m_timer.stop();
    m_events.clear();
    releaseAll();
    m_running = false;
    m_waitingForRelease = false;
    setStatus(reason);
    log(QStringLiteral("%1").arg(reason));
}

void InputSimulationService::tick()
{
    if (!m_running) {
        return;
    }
    if (m_waitingForRelease) {
        if (m_activationShortcut.isPressed()) {
            return;
        }
        m_waitingForRelease = false;
        beginSequence(m_clock.elapsed());
        return;
    }
    process(m_clock.elapsed());
}

void InputSimulationService::process(qint64 now)
{
    if (!m_running || m_waitingForRelease) {
        return;
    }
    while (m_running) {
        auto next = std::min_element(m_events.begin(), m_events.end(), [](const auto &left, const auto &right) {
            return left.first < right.first;
        });
        if (next == m_events.end() || next->first > now) {
            break;
        }
        const qint64 eventTime = next->first;
        const Event event = next->second;
        m_events.erase(next);
        if (m_testClock) {
            m_testNow = eventTime;
        }
        handle(event, eventTime);
    }
    if (m_testClock) {
        m_testNow = now;
    }
}

void InputSimulationService::schedule(Event event, qint64 delayMilliseconds)
{
    const qint64 now = m_testClock ? m_testNow : m_clock.elapsed();
    m_events.append({now + std::max<qint64>(0, delayMilliseconds), event});
}

void InputSimulationService::beginSequence(qint64 now)
{
    Q_UNUSED(now);
    m_horizontalKey = m_random->uniform(0.0, 1.0) < 0.52 ? vkLeft : vkRight;
    if (!keyDown(m_horizontalKey)) {
        return;
    }
    setStatus(QStringLiteral("입력 시뮬레이션 실행 중"));
    const qint64 t1 = secondsToMilliseconds(m_random->uniform(0.0, 1.54));
    schedule(Event::HorizontalUp, t1);
    log(QStringLiteral("started horizontal=%1 t1=%2ms").arg(m_horizontalKey == vkLeft ? "Left" : "Right").arg(t1));
}

void InputSimulationService::beginBranches(qint64 now)
{
    const double t2 = boundedNormal(5.2, 2.2, 0.0, 10.5, QStringLiteral("t2"));
    if (!m_running) {
        return;
    }
    m_t2Milliseconds = secondsToMilliseconds(t2);
    m_upDone = m_fDone = m_shiftDone = false;
    if (!keyDown(vkUp)) {
        return;
    }
    schedule(Event::UpUp, m_t2Milliseconds);

    m_fStartedAt = now;
    const double t3 = boundedNormal(5.0, 2.0, 2.02, 10.12, QStringLiteral("t3"));
    if (!m_running) {
        return;
    }
    m_r1 = m_random->uniform(0.05, 0.15);
    m_inverseT3 = 1.0 / t3;
    scheduleFDown(now);

    m_shiftStartedAt = now;
    scheduleShiftDown(now);
    log(QStringLiteral("parallel branches started t2=%1ms t3=%2 r1=%3").arg(m_t2Milliseconds).arg(t3).arg(m_r1));
}

void InputSimulationService::handle(Event event, qint64 now)
{
    switch (event) {
    case Event::HorizontalUp:
        if (keyUp(m_horizontalKey)) {
            schedule(Event::StartBranches, 0);
        }
        break;
    case Event::StartBranches:
        beginBranches(now);
        break;
    case Event::UpUp:
        if (keyUp(vkUp)) {
            m_upDone = true;
            completeIfDone();
        }
        break;
    case Event::FDown:
        if (keyDown(vkF)) {
            const double t5 = boundedGeneralizedNormal(0.18, 0.1, 5.0, 0.062, 0.32, QStringLiteral("t5"));
            if (m_running) schedule(Event::FUp, secondsToMilliseconds(t5));
        }
        break;
    case Event::FUp:
        if (keyUp(vkF)) {
            if (now - m_fStartedAt > m_t2Milliseconds) {
                m_fDone = true;
                completeIfDone();
            } else {
                scheduleFDown(now);
            }
        }
        break;
    case Event::ShiftDown:
        if (keyDown(vkLeftShift)) {
            const double t7 = boundedGeneralizedNormal(0.16, 0.09, 4.0, 0.052, 0.28, QStringLiteral("t7"));
            if (m_running) schedule(Event::ShiftUp, secondsToMilliseconds(t7));
        }
        break;
    case Event::ShiftUp:
        if (keyUp(vkLeftShift)) {
            if (now - m_shiftStartedAt > m_t2Milliseconds) {
                m_shiftDone = true;
                completeIfDone();
            } else {
                scheduleShiftDown(now);
            }
        }
        break;
    }
}

void InputSimulationService::scheduleFDown(qint64 now)
{
    Q_UNUSED(now);
    const double t4 = boundedNormal(m_inverseT3, m_inverseT3 * m_r1, 0.5 * m_inverseT3, 2.0 * m_inverseT3, QStringLiteral("t4"));
    if (m_running) schedule(Event::FDown, secondsToMilliseconds(t4));
}

void InputSimulationService::scheduleShiftDown(qint64 now)
{
    Q_UNUSED(now);
    const double t6 = boundedGeneralizedNormal(1.0, 0.2, 3.0, 0.4, 1.6, QStringLiteral("t6"));
    if (m_running) schedule(Event::ShiftDown, secondsToMilliseconds(t6));
}

double InputSimulationService::boundedNormal(double mean, double standardDeviation, double minimum, double maximum, const QString &name)
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        const double value = m_random->normal(mean, standardDeviation);
        if (value >= minimum && value <= maximum) return value;
    }
    log(QStringLiteral("warning: %1 exceeded bounded sampling retries; using uniform fallback").arg(name));
    return m_random->uniform(minimum, maximum);
}

double InputSimulationService::boundedGeneralizedNormal(double mean, double standardDeviation, double shape, double minimum, double maximum, const QString &name)
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        const double value = m_random->generalizedNormal(mean, standardDeviation, shape);
        if (value >= minimum && value <= maximum) return value;
    }
    log(QStringLiteral("warning: %1 exceeded bounded sampling retries; using uniform fallback").arg(name));
    return m_random->uniform(minimum, maximum);
}

bool InputSimulationService::keyDown(int virtualKey)
{
    if (!m_backend->sendKey(virtualKey, true)) {
        fail(QStringLiteral("가상 키 %1을(를) 누르는 입력 주입에 실패했습니다").arg(virtualKey));
        return false;
    }
    m_heldKeys.insert(virtualKey);
    return true;
}

bool InputSimulationService::keyUp(int virtualKey)
{
    if (!m_heldKeys.contains(virtualKey)) {
        return true;
    }
    if (!m_backend->sendKey(virtualKey, false)) {
        fail(QStringLiteral("가상 키 %1을(를) 놓는 입력 주입에 실패했습니다").arg(virtualKey));
        return false;
    }
    m_heldKeys.remove(virtualKey);
    return true;
}

void InputSimulationService::releaseAll()
{
    const QSet<int> keys = m_heldKeys;
    m_heldKeys.clear();
    for (const int key : keys) {
        if (!m_backend->sendKey(key, false)) {
            log(QStringLiteral("Input injection failed while releasing virtual key %1 during cleanup").arg(key));
        }
    }
}

void InputSimulationService::completeIfDone()
{
    if (m_upDone && m_fDone && m_shiftDone) {
        m_timer.stop();
        m_running = false;
        setStatus(QStringLiteral("완료"));
        log(QStringLiteral("complete"));
    }
}

void InputSimulationService::fail(const QString &message)
{
    log(message);
    m_timer.stop();
    m_events.clear();
    releaseAll();
    m_running = false;
    m_waitingForRelease = false;
    setStatus(QStringLiteral("실패: %1").arg(message));
}

void InputSimulationService::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit changed();
}

void InputSimulationService::log(const QString &message) const
{
    const QString path = QDir(SettingsStore::projectRoot()).filePath(QStringLiteral("logs/input-simulation.log"));
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << message << "\n";
}

qint64 InputSimulationService::secondsToMilliseconds(double seconds) const
{
    return qRound64(qMax(0.0, seconds) * 1000.0);
}
