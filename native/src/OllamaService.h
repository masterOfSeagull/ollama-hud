#pragma once

#include "SettingsStore.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <stdexcept>

struct ChatMemory
{
    QString question;
    QString answer;
    QString imageB64;
};

struct OllamaReply
{
    QString answer;
    QString thinking;
    QString doneReason;
    qint64 promptEvalCount = -1;
    qint64 evalCount = -1;
    qint64 totalDurationNs = -1;
    qint64 loadDurationNs = -1;
    qint64 promptEvalDurationNs = -1;
    qint64 evalDurationNs = -1;
};

class OllamaException : public std::runtime_error
{
public:
    explicit OllamaException(const QString &message, int statusCode = 0, const QString &thinking = {});
    int statusCode() const;
    QString thinking() const;

private:
    int m_statusCode = 0;
    QString m_thinking;
};

class OllamaService : public QObject
{
    Q_OBJECT

public:
    explicit OllamaService(QObject *parent = nullptr);

    static QJsonArray buildChatMessages(
        const QString &query,
        const QString &instruction,
        const QString &screenshotContext,
        const QString &imageB64,
        const QList<ChatMemory> &memories,
        int maxMemories);
    static QJsonObject buildChatPayload(
        const HudSettings &settings,
        const QString &imageB64,
        const QList<ChatMemory> &memories,
        int maxMemories = -1);
    static QList<ChatMemory> selectPromptMemories(const QList<ChatMemory> &memories, int maxMemories);
    static QString buildMessagePreview(
        const QString &query,
        const QString &instruction,
        const QString &screenshotContext,
        const QList<ChatMemory> &memories,
        int maxMemories);
    static bool isContextLimitError(const QString &message);

    QString checkServer(const HudSettings &settings);
    OllamaReply generateFromImage(const HudSettings &settings, const QString &imageB64, const QList<ChatMemory> &memories);
    OllamaReply testModel(const HudSettings &settings);

private:
    QJsonObject postChat(const HudSettings &settings, const QJsonObject &payload);
};
