#pragma once

#include "OllamaService.h"
#include "SettingsStore.h"

struct ChatLogEntry
{
    QString captureId;
    QString question;
    QList<ChatMemory> memories;
    QString answer;
    QString error;
    QString retry = "none";
    QString thinking;
    QString doneReason;
    qint64 promptEvalCount = -1;
    qint64 evalCount = -1;
    qint64 totalDurationNs = -1;
    qint64 loadDurationNs = -1;
    qint64 promptEvalDurationNs = -1;
    qint64 evalDurationNs = -1;
};

class ChatLogService
{
public:
    static void write(const ChatLogEntry &entry, const HudSettings &settings, const QString &path = {});
};
