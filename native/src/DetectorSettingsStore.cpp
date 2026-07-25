#include "DetectorSettingsStore.h"
#include "SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <stdexcept>

namespace {
QJsonArray targetArray(const QString &value) { return QJsonDocument::fromJson(value.toUtf8()).array(); }
QString targetText(const QJsonArray &items) { return QString::fromUtf8(QJsonDocument(items).toJson(QJsonDocument::Compact)); }
bool safeTargetName(const QString &name) { return !name.trimmed().isEmpty() && !name.contains('/') && !name.contains('\\') && !name.contains(".."); }
bool validPromptThreshold(const QString &prompt) {
    const int separator = prompt.lastIndexOf(':');
    if (separator < 0) return true;
    bool validNumber = false;
    const double threshold = prompt.mid(separator + 1).trimmed().toDouble(&validNumber);
    return !prompt.left(separator).trimmed().isEmpty() && validNumber && threshold >= 0.0 && threshold <= 1.0;
}
bool containsGuideImage(const QString &folder) {
    const QDir directory(folder);
    if (folder.trimmed().isEmpty() || !directory.exists()) return false;
    QDirIterator it(folder, {"*.png", "*.jpg", "*.jpeg", "*.webp"}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext();
}
QJsonObject settingsObject(const DetectorSettings &settings) {
    return {{"enabled", settings.enabled}, {"model", settings.model}, {"device", settings.device}, {"dtype", settings.dtype}, {"text_threshold", settings.textThreshold}, {"guide_threshold", settings.guideThreshold}, {"targets", QJsonDocument::fromJson(settings.targetsJson.toUtf8()).array()}, {"positive_guide_folder", settings.positiveGuideFolder}, {"negative_guide_folder", settings.negativeGuideFolder}, {"live_rate", settings.liveRate}};
}
}

DetectorSettingsStore::DetectorSettingsStore(QObject *parent) : QObject(parent) { load(); }
QString DetectorSettingsStore::configPath() { return QDir(SettingsStore::projectRoot()).filePath("config/detector-settings.json"); }
QUrl DetectorSettingsStore::settingsFolder() const { return QUrl::fromLocalFile(QFileInfo(configPath()).absolutePath()); }
DetectorSettings DetectorSettingsStore::settings() const { return m_settings; }
void DetectorSettingsStore::validate(const DetectorSettings &s) {
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(s.targetsJson.toUtf8(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isArray()) throw std::invalid_argument("Detector targets must be a JSON array.");
    const QJsonArray targets = document.array();
    if (targets.isEmpty()) throw std::invalid_argument("Configure at least one detector target.");
    for (const QJsonValue &value : targets) {
        const QJsonObject target = value.toObject();
        const QString name = target.value("name").toString().trimmed();
        if (!safeTargetName(name)) throw std::invalid_argument("Each detector target needs a name without path characters.");
        const bool imageTarget = target.value("type").toString() == "image" || target.value("guide_only").toBool();
        const QJsonArray prompts = target.value("prompts").toArray();
        bool hasPrompt = false;
        for (const QJsonValue &prompt : prompts) {
            if (!prompt.isString()) throw std::invalid_argument("Detector prompts must be text.");
            if (!validPromptThreshold(prompt.toString())) throw std::invalid_argument("Prompt thresholds must use prompt:0.0 through prompt:1.0.");
            hasPrompt = hasPrompt || !prompt.toString().trimmed().isEmpty();
        }
        if (!imageTarget && !hasPrompt) {
            throw std::invalid_argument(QStringLiteral("Target '%1' needs at least one prompt or alias.").arg(name).toStdString());
        }
        if (imageTarget) {
            const bool useDefaultDirectories = target.contains("use_default_guide_directories")
                ? target.value("use_default_guide_directories").toBool()
                : target.value("guide_dir").toString().isEmpty();
            const QString positiveFolder = useDefaultDirectories
                ? QDir(s.positiveGuideFolder).filePath(name)
                : target.value("positive_guide_dir").toString();
            if (!containsGuideImage(positiveFolder)) {
                throw std::invalid_argument(QStringLiteral("Image target '%1' needs a positive guide folder containing at least one PNG, JPG, JPEG, or WEBP image.").arg(name).toStdString());
            }
        }
    }
    if (s.liveRate < .2 || s.liveRate > 30) throw std::invalid_argument("Live detection rate must be between 0.2 and 30 Hz.");
}
bool DetectorSettingsStore::load() {
    QFile file(configPath()); if (!file.exists()) return true;
    return loadFromFile(configPath());
}
bool DetectorSettingsStore::save() { return saveToFile(configPath()); }
bool DetectorSettingsStore::saveToFile(const QString &path) {
    try {
        const QString selectedPath = path.trimmed();
        if (selectedPath.isEmpty()) throw std::invalid_argument("Choose a detector settings file.");
        validate(m_settings);
        QDir().mkpath(QFileInfo(selectedPath).absolutePath());
        QFile file(selectedPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) throw std::runtime_error(QStringLiteral("Could not write detector settings: %1").arg(selectedPath).toStdString());
        file.write(QJsonDocument(settingsObject(m_settings)).toJson(QJsonDocument::Indented));
        setLastError({});
        return true;
    } catch (const std::exception &e) { setLastError(e.what()); return false; }
}
bool DetectorSettingsStore::loadFromFile(const QString &path) {
    try {
        const QString selectedPath = path.trimmed();
        if (selectedPath.isEmpty()) throw std::invalid_argument("Choose a detector settings file.");
        QFile file(selectedPath);
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(QStringLiteral("Could not read detector settings: %1").arg(selectedPath).toStdString());
        QJsonParseError parse;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject()) throw std::invalid_argument("Detector settings must be a JSON object.");
        const QJsonObject object = document.object();
        if (object.contains("targets") && !object.value("targets").isArray()) throw std::invalid_argument("Detector settings targets must be an array.");
        DetectorSettings loaded;
        loaded.enabled = object.value("enabled").toBool(loaded.enabled); loaded.model = object.value("model").toString(loaded.model); loaded.device = object.value("device").toString(loaded.device); loaded.dtype = object.value("dtype").toString(loaded.dtype); loaded.textThreshold = object.value("text_threshold").toDouble(loaded.textThreshold); loaded.guideThreshold = object.value("guide_threshold").toDouble(loaded.guideThreshold); if (object.contains("targets")) loaded.targetsJson = QString::fromUtf8(QJsonDocument(object.value("targets").toArray()).toJson(QJsonDocument::Compact)); loaded.positiveGuideFolder = object.value("positive_guide_folder").toString(loaded.positiveGuideFolder); loaded.negativeGuideFolder = object.value("negative_guide_folder").toString(loaded.negativeGuideFolder); loaded.liveRate = object.value("live_rate").toDouble(loaded.liveRate);
        validate(loaded);
        m_settings = loaded;
        setLastError({});
        emit changed();
        return true;
    } catch (const std::exception &e) { setLastError(e.what()); return false; }
}
bool DetectorSettingsStore::resetToDefaults() { m_settings=DetectorSettings{}; emit changed(); return save(); }
bool DetectorSettingsStore::enabled() const{return m_settings.enabled;} void DetectorSettingsStore::setEnabled(bool v){m_settings.enabled=v;emit changed();}
QString DetectorSettingsStore::model() const{return m_settings.model;} void DetectorSettingsStore::setModel(const QString &v){m_settings.model=v;emit changed();}
QString DetectorSettingsStore::device() const{return m_settings.device;} void DetectorSettingsStore::setDevice(const QString &v){m_settings.device=v;emit changed();}
QString DetectorSettingsStore::dtype() const{return m_settings.dtype;} void DetectorSettingsStore::setDtype(const QString &v){m_settings.dtype=v;emit changed();}
double DetectorSettingsStore::textThreshold() const{return m_settings.textThreshold;} void DetectorSettingsStore::setTextThreshold(double v){m_settings.textThreshold=v;emit changed();}
double DetectorSettingsStore::guideThreshold() const{return m_settings.guideThreshold;} void DetectorSettingsStore::setGuideThreshold(double v){m_settings.guideThreshold=v;emit changed();}
QString DetectorSettingsStore::targetsJson() const{return m_settings.targetsJson;} void DetectorSettingsStore::setTargetsJson(const QString &v){m_settings.targetsJson=v;emit changed();}
QVariantList DetectorSettingsStore::targets() const {
    QVariantList result;
    for (const QJsonValue &value : targetArray(m_settings.targetsJson)) {
        const QJsonObject target = value.toObject();
        QStringList prompts;
        for (const QJsonValue &prompt : target.value("prompts").toArray()) prompts.append(prompt.toString());
        const bool imageTarget = target.value("type").toString() == "image" || target.value("guide_only").toBool();
        const bool useDefaultDirectories = target.contains("use_default_guide_directories")
            ? target.value("use_default_guide_directories").toBool()
            : target.value("guide_dir").toString().isEmpty();
        result.append(QVariantMap{{"name", target.value("name").toString()}, {"prompts", prompts.join(", ")}, {"type", imageTarget ? "image" : "text"}, {"useDefaultGuideDirectories", useDefaultDirectories}, {"positiveGuideDir", target.value("positive_guide_dir").toString(target.value("guide_dir").toString())}, {"negativeGuideDir", target.value("negative_guide_dir").toString()}});
    }
    return result;
}
QStringList DetectorSettingsStore::defaultGuideTargetNames() const {
    QStringList names;
    const QDir root(m_settings.positiveGuideFolder);
    if (!root.exists()) return names;
    for (const QFileInfo &entry : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
        if (containsGuideImage(entry.absoluteFilePath())) names.append(entry.fileName());
    }
    return names;
}
int DetectorSettingsStore::defaultGuideTargetIndex(const QString &name) const { return defaultGuideTargetNames().indexOf(name); }
QString DetectorSettingsStore::positiveGuideFolder() const{return m_settings.positiveGuideFolder;} void DetectorSettingsStore::setPositiveGuideFolder(const QString &v){m_settings.positiveGuideFolder=v;emit changed();}
QString DetectorSettingsStore::negativeGuideFolder() const{return m_settings.negativeGuideFolder;} void DetectorSettingsStore::setNegativeGuideFolder(const QString &v){m_settings.negativeGuideFolder=v;emit changed();}
double DetectorSettingsStore::liveRate() const{return m_settings.liveRate;} void DetectorSettingsStore::setLiveRate(double v){m_settings.liveRate=v;emit changed();}
QString DetectorSettingsStore::lastError() const{return m_lastError;} void DetectorSettingsStore::setLastError(const QString &v){if(m_lastError==v)return;m_lastError=v;emit lastErrorChanged();}
void DetectorSettingsStore::addTextTarget() { QJsonArray items=targetArray(m_settings.targetsJson); items.append(QJsonObject{{"type", "text"}, {"name", ""}, {"prompts", QJsonArray{}}}); m_settings.targetsJson=targetText(items); emit changed(); }
void DetectorSettingsStore::addImageTarget() { QJsonArray items=targetArray(m_settings.targetsJson); items.append(QJsonObject{{"type", "image"}, {"name", ""}, {"use_default_guide_directories", true}}); m_settings.targetsJson=targetText(items); emit changed(); }
void DetectorSettingsStore::removeTarget(int index) { QJsonArray items=targetArray(m_settings.targetsJson); if (index < 0 || index >= items.size()) return; items.removeAt(index); m_settings.targetsJson=targetText(items); emit changed(); }
void DetectorSettingsStore::updateTextTarget(int index, const QString &name, const QString &prompts) {
    QJsonArray items=targetArray(m_settings.targetsJson); if (index < 0 || index >= items.size()) return; QJsonArray promptArray; for (const QString &prompt : prompts.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts)) promptArray.append(prompt.trimmed()); items.replace(index, QJsonObject{{"type", "text"}, {"name", name.trimmed()}, {"prompts", promptArray}}); m_settings.targetsJson=targetText(items); emit changed();
}
void DetectorSettingsStore::updateImageTarget(int index, const QString &name, bool useDefaultGuideDirectories, const QString &positiveGuideDir, const QString &negativeGuideDir) {
    QJsonArray items=targetArray(m_settings.targetsJson); if (index < 0 || index >= items.size()) return; QJsonObject item{{"type", "image"}, {"name", name.trimmed()}, {"use_default_guide_directories", useDefaultGuideDirectories}}; if (!useDefaultGuideDirectories) { item.insert("positive_guide_dir", positiveGuideDir.trimmed()); item.insert("negative_guide_dir", negativeGuideDir.trimmed()); } items.replace(index, item); m_settings.targetsJson=targetText(items); emit changed();
}
void DetectorSettingsStore::setImageTargetGuideFolder(int index, bool positive, const QString &folder) {
    QJsonArray items=targetArray(m_settings.targetsJson); if (index < 0 || index >= items.size()) return; QJsonObject item=items.at(index).toObject(); if (item.value("type").toString() != "image") return; item.insert("use_default_guide_directories", false); item.insert(positive ? "positive_guide_dir" : "negative_guide_dir", folder.trimmed()); items.replace(index, item); m_settings.targetsJson=targetText(items); emit changed();
}
