#include "ChatLogService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
QString oneLine(const QString &text)
{
    QStringList parts;
    for (const QString &part : text.split('\n')) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }
    return parts.join(' ').trimmed();
}

QString countText(qint64 count)
{
    return count >= 0 ? QString::number(count) : QStringLiteral("(not returned)");
}

QString durationText(qint64 nanoseconds)
{
    if (nanoseconds < 0) {
        return QStringLiteral("(not returned)");
    }
    return QString::number(static_cast<double>(nanoseconds) / 1000000000.0, 'f', 3) + " s";
}
}

void ChatLogService::write(const ChatLogEntry &entry, const HudSettings &settings, const QString &path)
{
    const QString selectedPath = path.isEmpty() ? SettingsStore::chatLogPath() : path;
    QDir().mkpath(QFileInfo(selectedPath).absolutePath());
    QFile file(selectedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }

    const QList<ChatMemory> sentMemories = OllamaService::selectPromptMemories(entry.memories, settings.memoryQaPairs);
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QString(80, '=') << "\n";
    out << "Timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "Model: " << settings.model << "\n";
    out << "Host: " << settings.host << "\n";
    out << "Capture ID: " << entry.captureId << "\n";
    out << "Screenshot attached: yes; payload omitted from this text log\n";
    out << "Screenshot max edge: " << settings.screenshotMaxEdge << "\n";
    out << "Q/A memory configured: " << settings.memoryQaPairs << "\n";
    out << "Q/A memory available: " << entry.memories.size() << "\n";
    out << "Q/A memory sent: " << sentMemories.size() << "\n";
    out << "Think: " << (settings.think ? "yes" : "no") << "\n";
    out << "Retry: " << entry.retry << "\n\n";
    out << "Ollama completion:\n";
    out << "Done reason: " << (entry.doneReason.isEmpty() ? "(not returned)" : entry.doneReason) << "\n";
    out << "Prompt tokens: " << countText(entry.promptEvalCount) << "\n";
    out << "Generated tokens: " << countText(entry.evalCount) << "\n";
    out << "Total duration: " << durationText(entry.totalDurationNs) << "\n";
    out << "Load duration: " << durationText(entry.loadDurationNs) << "\n";
    out << "Prompt evaluation duration: " << durationText(entry.promptEvalDurationNs) << "\n";
    out << "Generation duration: " << durationText(entry.evalDurationNs) << "\n\n";
    out << "Question:\n" << entry.question << "\n\n";
    out << "Included Memory:\n";
    if (sentMemories.isEmpty()) {
        out << "(none)\n";
    } else {
        for (int i = 0; i < sentMemories.size(); ++i) {
            out << i + 1 << ". Q: " << oneLine(sentMemories.at(i).question) << "\n";
            out << "   A: " << oneLine(sentMemories.at(i).answer) << "\n";
        }
    }
    out << "\nAnswer:\n" << (entry.answer.isEmpty() ? "(none)" : entry.answer) << "\n\n";
    out << "Thinking:\n" << (entry.thinking.isEmpty() ? "(none)" : entry.thinking) << "\n";
    if (!entry.error.isEmpty()) {
        out << "\nError:\n" << entry.error << "\n";
    }
    out << "\nMessage Preview Sent:\n";
    out << OllamaService::buildMessagePreview(entry.question, settings.instruction, settings.screenshotContext, sentMemories, settings.memoryQaPairs) << "\n\n";
}
