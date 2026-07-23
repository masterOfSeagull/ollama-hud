#pragma once

#include <QObject>
#include <QVariantMap>

struct HudSettings
{
    QString host = "http://127.0.0.1:11434";
    QString model = "huihui_ai/qwen3-vl-abliterated:8b-instruct";
    QString triggerShortcut = "Alt+1";
    QString exitShortcut = "Esc";
    QString clearShortcut = "Alt+2";
    int screenshotMaxEdge = 1280;
    double timeoutSeconds = 120.0;
    int memoryQaPairs = 3;
    QString instruction = "Answer in one short sentence. No chain of thought. Give the best direction/action only.";
    QString screenshotContext = "Prior screenshots and Q&A turns are stale context. Use the current screenshot as the source of truth, and use prior turns only when the current screenshot supports them.";
    QString query = "In this RPG dungeon screenshot, identify the entrance, portal, exit, or door I should use next. Which direction or action should I take?";
    QString keepAlive = "30m";
    bool think = true;
    QVariantMap options = {
        {"temperature", 0.2},
        {"top_p", 0.8},
        {"num_predict", 2048},
        {"num_ctx", 32768},
        {"repeat_penalty", 1.1},
        {"repeat_last_n", 64},
    };

    QString normalizedHost() const;
};

class SettingsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY settingsChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY settingsChanged)
    Q_PROPERTY(QString triggerShortcut READ triggerShortcut WRITE setTriggerShortcut NOTIFY settingsChanged)
    Q_PROPERTY(QString exitShortcut READ exitShortcut WRITE setExitShortcut NOTIFY settingsChanged)
    Q_PROPERTY(QString clearShortcut READ clearShortcut WRITE setClearShortcut NOTIFY settingsChanged)
    Q_PROPERTY(int screenshotMaxEdge READ screenshotMaxEdge WRITE setScreenshotMaxEdge NOTIFY settingsChanged)
    Q_PROPERTY(double timeoutSeconds READ timeoutSeconds WRITE setTimeoutSeconds NOTIFY settingsChanged)
    Q_PROPERTY(int memoryQaPairs READ memoryQaPairs WRITE setMemoryQaPairs NOTIFY settingsChanged)
    Q_PROPERTY(QString instruction READ instruction WRITE setInstruction NOTIFY settingsChanged)
    Q_PROPERTY(QString screenshotContext READ screenshotContext WRITE setScreenshotContext NOTIFY settingsChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY settingsChanged)
    Q_PROPERTY(QString keepAlive READ keepAlive WRITE setKeepAlive NOTIFY settingsChanged)
    Q_PROPERTY(QString keepAliveMinutes READ keepAliveMinutes WRITE setKeepAliveMinutes NOTIFY settingsChanged)
    Q_PROPERTY(bool think READ think WRITE setThink NOTIFY settingsChanged)
    Q_PROPERTY(QString optionsText READ optionsText WRITE setOptionsText NOTIFY settingsChanged)
    Q_PROPERTY(QString numCtx READ numCtx WRITE setNumCtx NOTIFY settingsChanged)
    Q_PROPERTY(QString numPredict READ numPredict WRITE setNumPredict NOTIFY settingsChanged)
    Q_PROPERTY(QString repeatLastN READ repeatLastN WRITE setRepeatLastN NOTIFY settingsChanged)
    Q_PROPERTY(QString repeatPenalty READ repeatPenalty WRITE setRepeatPenalty NOTIFY settingsChanged)
    Q_PROPERTY(QString temperature READ temperature WRITE setTemperature NOTIFY settingsChanged)
    Q_PROPERTY(QString topP READ topP WRITE setTopP NOTIFY settingsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit SettingsStore(QObject *parent = nullptr);

    static QString projectRoot();
    static QString defaultConfigPath();
    static QString userConfigPath();
    static QString chatLogPath();
    static HudSettings loadFromPath(const QString &path = {});
    static void saveToPath(const HudSettings &settings, const QString &path = {});
    static void validate(const HudSettings &settings);

    HudSettings settings() const;
    void setSettings(const HudSettings &settings);

    Q_INVOKABLE bool load();
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool resetToDefaults();

    QString host() const;
    void setHost(const QString &value);
    QString model() const;
    void setModel(const QString &value);
    QString triggerShortcut() const;
    void setTriggerShortcut(const QString &value);
    QString exitShortcut() const;
    void setExitShortcut(const QString &value);
    QString clearShortcut() const;
    void setClearShortcut(const QString &value);
    int screenshotMaxEdge() const;
    void setScreenshotMaxEdge(int value);
    double timeoutSeconds() const;
    void setTimeoutSeconds(double value);
    int memoryQaPairs() const;
    void setMemoryQaPairs(int value);
    QString instruction() const;
    void setInstruction(const QString &value);
    QString screenshotContext() const;
    void setScreenshotContext(const QString &value);
    QString query() const;
    void setQuery(const QString &value);
    QString keepAlive() const;
    void setKeepAlive(const QString &value);
    QString keepAliveMinutes() const;
    void setKeepAliveMinutes(const QString &value);
    bool think() const;
    void setThink(bool value);
    QString optionsText() const;
    void setOptionsText(const QString &value);
    QString numCtx() const;
    void setNumCtx(const QString &value);
    QString numPredict() const;
    void setNumPredict(const QString &value);
    QString repeatLastN() const;
    void setRepeatLastN(const QString &value);
    QString repeatPenalty() const;
    void setRepeatPenalty(const QString &value);
    QString temperature() const;
    void setTemperature(const QString &value);
    QString topP() const;
    void setTopP(const QString &value);
    QString lastError() const;

signals:
    void settingsChanged();
    void lastErrorChanged();

private:
    void setLastError(const QString &message);
    QString optionText(const QString &key) const;
    void setOptionText(const QString &key, const QString &value);
    HudSettings m_settings;
    QString m_lastError;
};
