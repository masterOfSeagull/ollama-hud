#include "SettingsStore.h"

#include "Shortcut.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <stdexcept>

namespace {
QVariant parseScalar(QString value)
{
    value = value.trimmed();
    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
        value = value.mid(1, value.size() - 2);
        return value.replace("''", "'");
    }
    const QString lowered = value.toLower();
    if (lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    bool ok = false;
    const int intValue = value.toInt(&ok);
    if (ok && !value.contains('.')) {
        return intValue;
    }
    const double doubleValue = value.toDouble(&ok);
    if (ok) {
        return doubleValue;
    }
    return value;
}

QString unindentYamlLine(QString line)
{
    if (line.startsWith("  ")) {
        line.remove(0, 2);
    }
    return line;
}

QVariantMap readYamlLike(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    QVariantMap values;
    QVariantMap options;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        line.remove('\r');
        if (line.trimmed().isEmpty() || line.trimmed().startsWith('#') || line.startsWith(' ')) {
            continue;
        }
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        QString value = line.mid(colon + 1).trimmed();

        if (key == "options") {
            while (i + 1 < lines.size() && lines.at(i + 1).startsWith("  ")) {
                QString optionLine = lines.at(++i);
                optionLine.remove('\r');
                const int optionColon = optionLine.indexOf(':');
                if (optionColon > 0) {
                    const QString optionKey = optionLine.left(optionColon).trimmed();
                    options.insert(optionKey, parseScalar(optionLine.mid(optionColon + 1)));
                }
            }
            continue;
        }

        if (value == ">-" || value == ">" || value == "|" || value == "|-") {
            QStringList block;
            while (i + 1 < lines.size() && (lines.at(i + 1).startsWith("  ") || lines.at(i + 1).trimmed().isEmpty())) {
                QString blockLine = lines.at(++i);
                blockLine.remove('\r');
                block.append(unindentYamlLine(blockLine).trimmed());
            }
            values.insert(key, value.startsWith('>') ? block.join(' ').simplified() : block.join('\n'));
            continue;
        }

        if (value.startsWith('\'') && !(value.size() > 1 && value.endsWith('\''))) {
            QStringList block;
            block.append(value.mid(1));
            while (i + 1 < lines.size()) {
                QString blockLine = lines.at(++i);
                blockLine.remove('\r');
                blockLine = unindentYamlLine(blockLine);
                if (blockLine.endsWith('\'')) {
                    blockLine.chop(1);
                    block.append(blockLine);
                    break;
                }
                block.append(blockLine);
            }
            values.insert(key, block.join('\n').replace("''", "'").trimmed());
            continue;
        }

        values.insert(key, parseScalar(value));
    }

    if (!options.isEmpty()) {
        values.insert("options", options);
    }
    return values;
}

QString quoteYaml(const QString &text)
{
    return QStringLiteral("'%1'").arg(QString(text).replace("'", "''"));
}

QString variantToYaml(const QVariant &value)
{
    if (value.typeId() == QMetaType::Bool) {
        return value.toBool() ? "true" : "false";
    }
    if ((value.typeId() == QMetaType::Int || value.typeId() == QMetaType::Double || value.typeId() == QMetaType::LongLong)
        && !value.toString().isEmpty()) {
        return value.toString();
    }
    return quoteYaml(value.toString());
}

QString optionsToText(const QVariantMap &options)
{
    QJsonObject object;
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        object.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QVariantMap optionsFromText(const QString &text, const QVariantMap &fallback)
{
    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && document.isObject()) {
        return document.object().toVariantMap();
    }

    QVariantMap parsed;
    const QStringList lines = text.split('\n');
    for (const QString &line : lines) {
        const int colon = line.indexOf(':');
        if (colon > 0) {
            parsed.insert(line.left(colon).trimmed(), parseScalar(line.mid(colon + 1)));
        }
    }
    return parsed.isEmpty() ? fallback : parsed;
}

QString stringValue(const QVariantMap &data, const QString &key, const QString &fallback)
{
    const QString value = data.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

int intValue(const QVariantMap &data, const QString &key, int fallback)
{
    bool ok = false;
    const int value = data.value(key).toInt(&ok);
    return ok ? value : fallback;
}

double doubleValue(const QVariantMap &data, const QString &key, double fallback)
{
    bool ok = false;
    const double value = data.value(key).toDouble(&ok);
    return ok ? value : fallback;
}
}

QString HudSettings::normalizedHost() const
{
    QString normalized = host.trimmed();
    while (normalized.endsWith('/')) {
        normalized.chop(1);
    }
    return normalized;
}

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
    load();
}

QString SettingsStore::projectRoot()
{
    const QDir sourceRoot(QStringLiteral(PROJECT_SOURCE_DIR));
    if (sourceRoot.exists("config/default.yaml")) {
        return sourceRoot.absolutePath();
    }
    QDir appDir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 5; ++i) {
        if (appDir.exists("config/default.yaml")) {
            return appDir.absolutePath();
        }
        appDir.cdUp();
    }
    return sourceRoot.absolutePath();
}

QString SettingsStore::defaultConfigPath()
{
    return QDir(projectRoot()).filePath("config/default.yaml");
}

QString SettingsStore::userConfigPath()
{
    return QDir(projectRoot()).filePath("config/settings.yaml");
}

QString SettingsStore::chatLogPath()
{
    return QDir(projectRoot()).filePath("logs/chat.log");
}

HudSettings SettingsStore::loadFromPath(const QString &path)
{
    const QString selectedPath = path.isEmpty()
        ? (QFileInfo::exists(userConfigPath()) ? userConfigPath() : defaultConfigPath())
        : path;
    const QVariantMap data = readYamlLike(selectedPath);
    HudSettings settings;
    QVariantMap options = settings.options;
    const QVariantMap rawOptions = data.value("options").toMap();
    for (auto it = rawOptions.constBegin(); it != rawOptions.constEnd(); ++it) {
        options.insert(it.key(), it.value());
    }
    settings.host = stringValue(data, "host", settings.host);
    settings.model = stringValue(data, "model", settings.model);
    settings.triggerShortcut = stringValue(data, "trigger_shortcut", settings.triggerShortcut);
    settings.exitShortcut = stringValue(data, "exit_shortcut", settings.exitShortcut);
    settings.clearShortcut = stringValue(data, "clear_shortcut", settings.clearShortcut);
    settings.screenshotMaxEdge = std::max(64, intValue(data, "screenshot_max_edge", settings.screenshotMaxEdge));
    settings.timeoutSeconds = std::max(1.0, doubleValue(data, "timeout_seconds", settings.timeoutSeconds));
    settings.memoryQaPairs = std::max(0, intValue(data, "memory_qa_pairs", settings.memoryQaPairs));
    settings.instruction = stringValue(data, "instruction", settings.instruction);
    settings.query = stringValue(data, "query", settings.query);
    settings.keepAlive = stringValue(data, "keep_alive", settings.keepAlive);
    if (data.contains("think")) {
        settings.think = data.value("think").toBool();
    }
    settings.options = options;
    return settings;
}

void SettingsStore::saveToPath(const HudSettings &settings, const QString &path)
{
    validate(settings);
    const QString selectedPath = path.isEmpty() ? userConfigPath() : path;
    QDir().mkpath(QFileInfo(selectedPath).absolutePath());
    QFile file(selectedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw std::runtime_error(QStringLiteral("Could not write settings: %1").arg(selectedPath).toStdString());
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "host: " << settings.host << "\n";
    out << "model: " << settings.model << "\n";
    out << "trigger_shortcut: " << quoteYaml(settings.triggerShortcut) << "\n";
    out << "exit_shortcut: " << quoteYaml(settings.exitShortcut) << "\n";
    out << "clear_shortcut: " << quoteYaml(settings.clearShortcut) << "\n";
    out << "screenshot_max_edge: " << settings.screenshotMaxEdge << "\n";
    out << "timeout_seconds: " << settings.timeoutSeconds << "\n";
    out << "memory_qa_pairs: " << settings.memoryQaPairs << "\n";
    out << "instruction: " << quoteYaml(settings.instruction) << "\n";
    out << "keep_alive: " << quoteYaml(settings.keepAlive) << "\n";
    out << "think: " << (settings.think ? "true" : "false") << "\n";
    out << "query: " << quoteYaml(settings.query) << "\n";
    out << "options:\n";
    for (auto it = settings.options.constBegin(); it != settings.options.constEnd(); ++it) {
        out << "  " << it.key() << ": " << variantToYaml(it.value()) << "\n";
    }
}

void SettingsStore::validate(const HudSettings &settings)
{
    if (settings.host.trimmed().isEmpty()) {
        throw std::invalid_argument("Ollama host is required.");
    }
    if (settings.model.trimmed().isEmpty()) {
        throw std::invalid_argument("Model is required.");
    }
    parseShortcut(settings.triggerShortcut);
    parseShortcut(settings.exitShortcut);
    parseShortcut(settings.clearShortcut);
    if (settings.screenshotMaxEdge < 64) {
        throw std::invalid_argument("Screenshot max edge must be at least 64.");
    }
    if (settings.timeoutSeconds < 1) {
        throw std::invalid_argument("Timeout seconds must be at least 1.");
    }
    if (settings.memoryQaPairs < 0 || settings.memoryQaPairs > 20) {
        throw std::invalid_argument("Q/A memory pairs must be between 0 and 20.");
    }
    if (settings.instruction.trimmed().isEmpty()) {
        throw std::invalid_argument("Instruction is required.");
    }
    if (settings.query.trimmed().isEmpty()) {
        throw std::invalid_argument("Query is required.");
    }
    if (settings.keepAlive.trimmed().isEmpty()) {
        throw std::invalid_argument("Keep alive is required.");
    }
}

HudSettings SettingsStore::settings() const { return m_settings; }

void SettingsStore::setSettings(const HudSettings &settings)
{
    m_settings = settings;
    emit settingsChanged();
}

bool SettingsStore::load()
{
    try {
        setSettings(loadFromPath());
        setLastError({});
        return true;
    } catch (const std::exception &error) {
        setLastError(error.what());
        return false;
    }
}

bool SettingsStore::save()
{
    try {
        m_settings.options = optionsFromText(optionsText(), m_settings.options);
        saveToPath(m_settings);
        setLastError({});
        return true;
    } catch (const std::exception &error) {
        setLastError(error.what());
        return false;
    }
}

bool SettingsStore::resetToDefaults()
{
    try {
        setSettings(loadFromPath(defaultConfigPath()));
        setLastError({});
        return true;
    } catch (const std::exception &error) {
        setLastError(error.what());
        return false;
    }
}

QString SettingsStore::host() const { return m_settings.host; }
void SettingsStore::setHost(const QString &value) { m_settings.host = value; emit settingsChanged(); }
QString SettingsStore::model() const { return m_settings.model; }
void SettingsStore::setModel(const QString &value) { m_settings.model = value; emit settingsChanged(); }
QString SettingsStore::triggerShortcut() const { return m_settings.triggerShortcut; }
void SettingsStore::setTriggerShortcut(const QString &value) { m_settings.triggerShortcut = value; emit settingsChanged(); }
QString SettingsStore::exitShortcut() const { return m_settings.exitShortcut; }
void SettingsStore::setExitShortcut(const QString &value) { m_settings.exitShortcut = value; emit settingsChanged(); }
QString SettingsStore::clearShortcut() const { return m_settings.clearShortcut; }
void SettingsStore::setClearShortcut(const QString &value) { m_settings.clearShortcut = value; emit settingsChanged(); }
int SettingsStore::screenshotMaxEdge() const { return m_settings.screenshotMaxEdge; }
void SettingsStore::setScreenshotMaxEdge(int value) { m_settings.screenshotMaxEdge = value; emit settingsChanged(); }
double SettingsStore::timeoutSeconds() const { return m_settings.timeoutSeconds; }
void SettingsStore::setTimeoutSeconds(double value) { m_settings.timeoutSeconds = value; emit settingsChanged(); }
int SettingsStore::memoryQaPairs() const { return m_settings.memoryQaPairs; }
void SettingsStore::setMemoryQaPairs(int value) { m_settings.memoryQaPairs = value; emit settingsChanged(); }
QString SettingsStore::instruction() const { return m_settings.instruction; }
void SettingsStore::setInstruction(const QString &value) { m_settings.instruction = value; emit settingsChanged(); }
QString SettingsStore::query() const { return m_settings.query; }
void SettingsStore::setQuery(const QString &value) { m_settings.query = value; emit settingsChanged(); }
QString SettingsStore::keepAlive() const { return m_settings.keepAlive; }
void SettingsStore::setKeepAlive(const QString &value) { m_settings.keepAlive = value; emit settingsChanged(); }
bool SettingsStore::think() const { return m_settings.think; }
void SettingsStore::setThink(bool value) { m_settings.think = value; emit settingsChanged(); }
QString SettingsStore::optionsText() const { return optionsToText(m_settings.options); }
void SettingsStore::setOptionsText(const QString &value)
{
    m_settings.options = optionsFromText(value, m_settings.options);
    emit settingsChanged();
}
QString SettingsStore::lastError() const { return m_lastError; }

void SettingsStore::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}
