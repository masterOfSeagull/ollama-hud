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
};

class ChatLogService
{
public:
    static void write(const ChatLogEntry &entry, const HudSettings &settings, const QString &path = {});
};
