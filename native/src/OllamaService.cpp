#include "OllamaService.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <algorithm>

namespace {
constexpr auto ScreenshotContext =
    "Prior screenshots and Q&A turns are stale context. Use the current screenshot as the "
    "source of truth, and use prior turns only when the current screenshot supports them.";

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

QString compact(const QString &text, int limit = 220)
{
    const QString cleaned = oneLine(text);
    if (cleaned.size() <= limit) {
        return cleaned;
    }
    return cleaned.left(limit - 3) + "...";
}

QString errorText(QNetworkReply *reply, const QByteArray &body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject() && document.object().contains("error")) {
        return document.object().value("error").toVariant().toString();
    }
    const QString text = QString::fromUtf8(body).trimmed();
    return text.isEmpty() ? QStringLiteral("HTTP %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()) : text;
}
}

OllamaException::OllamaException(const QString &message, int statusCode, const QString &thinking)
    : std::runtime_error(message.toStdString())
    , m_statusCode(statusCode)
    , m_thinking(thinking)
{
}

int OllamaException::statusCode() const { return m_statusCode; }
QString OllamaException::thinking() const { return m_thinking; }

OllamaService::OllamaService(QObject *parent)
    : QObject(parent)
{
}

QList<ChatMemory> OllamaService::selectPromptMemories(const QList<ChatMemory> &memories, int maxMemories)
{
    if (maxMemories <= 0) {
        return {};
    }
    QList<ChatMemory> selected;
    QSet<QString> seen;
    for (auto it = memories.crbegin(); it != memories.crend(); ++it) {
        const QString key = oneLine(it->question).toLower() + "\n" + oneLine(it->answer).toLower();
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        selected.prepend(*it);
        if (selected.size() >= maxMemories) {
            break;
        }
    }
    return selected;
}

QJsonArray OllamaService::buildChatMessages(
    const QString &query,
    const QString &instruction,
    const QString &imageB64,
    const QList<ChatMemory> &memories,
    int maxMemories)
{
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", instruction}});
    messages.append(QJsonObject{{"role", "system"}, {"content", ScreenshotContext}});
    for (const ChatMemory &memory : selectPromptMemories(memories, maxMemories)) {
        messages.append(QJsonObject{
            {"role", "user"},
            {"content", compact(memory.question)},
            {"images", QJsonArray{memory.imageB64}},
        });
        messages.append(QJsonObject{{"role", "assistant"}, {"content", compact(memory.answer)}});
    }
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", query},
        {"images", QJsonArray{imageB64}},
    });
    return messages;
}

QJsonObject OllamaService::buildChatPayload(
    const HudSettings &settings,
    const QString &imageB64,
    const QList<ChatMemory> &memories,
    int maxMemories)
{
    QJsonObject options;
    for (auto it = settings.options.constBegin(); it != settings.options.constEnd(); ++it) {
        options.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }

    return {
        {"model", settings.model},
        {"messages", buildChatMessages(settings.query, settings.instruction, imageB64, memories, maxMemories < 0 ? settings.memoryQaPairs : maxMemories)},
        {"stream", false},
        {"think", settings.think},
        {"keep_alive", settings.keepAlive},
        {"options", options},
    };
}

QString OllamaService::buildMessagePreview(
    const QString &query,
    const QString &instruction,
    const QList<ChatMemory> &memories,
    int maxMemories)
{
    QStringList parts = {
        "system: " + instruction,
        QStringLiteral("system: %1").arg(ScreenshotContext),
    };
    for (const ChatMemory &memory : selectPromptMemories(memories, maxMemories)) {
        parts.append("user: " + compact(memory.question) + " [screenshot omitted]");
        parts.append("assistant: " + compact(memory.answer));
    }
    parts.append("user: " + query + " [screenshot omitted]");
    return parts.join('\n');
}

bool OllamaService::isContextLimitError(const QString &message)
{
    const QString text = message.toLower();
    static const QStringList markers = {
        "context length",
        "context window",
        "context is full",
        "maximum context",
        "num_ctx",
        "too many tokens",
        "input is too long",
        "payload too large",
        "request entity too large",
        "image too large",
        "413",
    };
    return std::any_of(markers.cbegin(), markers.cend(), [&](const QString &marker) {
        return text.contains(marker);
    });
}

QString OllamaService::checkServer(const HudSettings &settings)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(settings.normalizedHost() + "/api/tags"));
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(static_cast<int>(std::min(settings.timeoutSeconds, 10.0) * 1000));
    loop.exec();
    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
    }
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();
    const QString replyErrorString = reply->errorString();
    reply->deleteLater();
    if (error != QNetworkReply::NoError) {
        throw OllamaException(QStringLiteral("Could not reach Ollama at %1: %2").arg(settings.host, replyErrorString));
    }
    if (status >= 400) {
        throw OllamaException(QStringLiteral("Ollama returned HTTP %1 from /api/tags.").arg(status), status);
    }
    Q_UNUSED(body);
    return "Ollama server is reachable.";
}

OllamaReply OllamaService::generateFromImage(const HudSettings &settings, const QString &imageB64, const QList<ChatMemory> &memories)
{
    const QJsonObject data = postChat(settings, buildChatPayload(settings, imageB64, memories));
    const QJsonObject message = data.value("message").toObject();
    if (message.isEmpty()) {
        throw OllamaException("Ollama did not return a chat message.");
    }
    const QString thinking = oneLine(message.value("thinking").toString());
    const QString answer = oneLine(message.value("content").toString());
    if (answer.isEmpty()) {
        throw OllamaException("Ollama returned an empty response.", 0, thinking);
    }
    return {answer, thinking};
}

OllamaReply OllamaService::testModel(const HudSettings &settings)
{
    QJsonObject options;
    for (auto it = settings.options.constBegin(); it != settings.options.constEnd(); ++it) {
        options.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    options.insert("temperature", 0);
    const QJsonObject payload{
        {"model", settings.model},
        {"messages", QJsonArray{
            QJsonObject{{"role", "system"}, {"content", settings.instruction}},
            QJsonObject{{"role", "user"}, {"content", "Reply with OK."}},
        }},
        {"stream", false},
        {"think", settings.think},
        {"keep_alive", settings.keepAlive},
        {"options", options},
    };
    const QJsonObject data = postChat(settings, payload);
    const QJsonObject message = data.value("message").toObject();
    if (message.isEmpty()) {
        throw OllamaException("Ollama did not return a chat message.");
    }
    const QString thinking = oneLine(message.value("thinking").toString());
    QString answer = oneLine(message.value("content").toString());
    if (answer.isEmpty()) {
        answer = "OK";
    }
    return {answer, thinking};
}

QJsonObject OllamaService::postChat(const HudSettings &settings, const QJsonObject &payload)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(settings.normalizedHost() + "/api/chat"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(static_cast<int>(settings.timeoutSeconds * 1000));
    loop.exec();
    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
    }

    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const QString httpErrorText = status >= 400 ? errorText(reply, body) : QString();
    reply->deleteLater();
    if (networkError != QNetworkReply::NoError) {
        throw OllamaException(QStringLiteral("Could not reach Ollama at %1: %2").arg(settings.host, networkErrorText));
    }
    if (status >= 400) {
        throw OllamaException(httpErrorText, status);
    }
    QJsonParseError parseError {};
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        throw OllamaException("Ollama returned invalid JSON.");
    }
    const QJsonObject object = document.object();
    if (object.contains("error")) {
        throw OllamaException(object.value("error").toVariant().toString());
    }
    return object;
}
