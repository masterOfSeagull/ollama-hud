#include "DetectorClient.h"
#include "SettingsStore.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <stdexcept>

namespace { QString endpoint(int port, const QString &path) { return QStringLiteral("http://127.0.0.1:%1%2").arg(port).arg(path); }
void requireOk(const QJsonObject &object) { if (!object.value("ok").toBool()) throw std::runtime_error(object.value("error").toString("Detector worker failed.").toStdString()); } }

DetectorClient::DetectorClient(QObject *parent) : QObject(parent) { m_process.setProcessChannelMode(QProcess::MergedChannels); }
DetectorClient::~DetectorClient() { if (m_process.state() != QProcess::NotRunning) { m_process.terminate(); if (!m_process.waitForFinished(1500)) m_process.kill(); } }
bool DetectorClient::prepare(QString *error) {
    if (m_process.state() == QProcess::NotRunning) {
        const QString root = QDir(SettingsStore::projectRoot()).absoluteFilePath("../owlv2-visual-detector");
        const QString python = QDir(root).filePath(".venv/Scripts/python.exe");
        if (!QFileInfo::exists(python)) { if (error) *error = QStringLiteral("OWLv2 worker Python was not found: %1").arg(python); return false; }
        m_process.setWorkingDirectory(root);
        m_process.start(python, {"-m", "owlv2_detector.worker", "--host", "127.0.0.1", "--port", QString::number(m_port)});
        if (!m_process.waitForStarted(4000)) { if (error) *error = QStringLiteral("Could not start OWLv2 worker: %1").arg(m_process.errorString()); return false; }
    }
    return true;
}
QJsonObject DetectorClient::waitReply(QNetworkReply *reply, int timeoutMs) const {
    QEventLoop loop; QTimer timer; timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit); connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit); timer.start(timeoutMs); loop.exec();
    if (!timer.isActive()) { reply->abort(); reply->deleteLater(); throw std::runtime_error("OWLv2 worker timed out."); }
    const QByteArray body=reply->readAll(); const auto error=reply->error(); const QString errorText=reply->errorString(); reply->deleteLater();
    QJsonParseError parse; const QJsonDocument document=QJsonDocument::fromJson(body,&parse);
    if (parse.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonObject object=document.object(); requireOk(object); return object;
    }
    if (error != QNetworkReply::NoError) throw std::runtime_error(QStringLiteral("Could not reach OWLv2 worker: %1").arg(errorText).toStdString());
    throw std::runtime_error("OWLv2 worker returned invalid JSON.");
}
QJsonObject DetectorClient::get(const QString &path) const { QNetworkAccessManager manager; return waitReply(manager.get(QNetworkRequest(QUrl(endpoint(m_port,path)))), 5000); }
QJsonObject DetectorClient::post(const QString &path, const QJsonObject &payload) const { QNetworkAccessManager manager; QNetworkRequest request(QUrl(endpoint(m_port,path))); request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json"); return waitReply(manager.post(request,QJsonDocument(payload).toJson(QJsonDocument::Compact)), 180000); }
QJsonObject DetectorClient::requestFor(const DetectorSettings &s) const {
    QJsonParseError parse; const QJsonDocument targets=QJsonDocument::fromJson(s.targetsJson.toUtf8(),&parse); if (parse.error != QJsonParseError::NoError || !targets.isArray()) throw std::runtime_error("Detector targets are invalid JSON.");
    return {{"model",QJsonObject{{"name",s.model}}},{"runtime",QJsonObject{{"device",s.device},{"dtype",s.dtype},{"text_threshold",s.textThreshold},{"guide_threshold",s.guideThreshold}}},{"targets",targets.array()},{"guide_folders",QJsonObject{{"positive",s.positiveGuideFolder},{"negative",s.negativeGuideFolder}}},{"live_rate",s.liveRate}};
}
QJsonObject DetectorClient::detect(const QImage &image, const DetectorSettings &settings) {
    QJsonObject request=requestFor(settings); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); if (!image.save(&buffer,"PNG")) throw std::runtime_error("Could not encode screenshot for OWLv2."); request.insert("image_base64",QString::fromLatin1(bytes.toBase64())); request.insert("capture",QJsonObject{{"left",0},{"top",0},{"width",image.width()},{"height",image.height()}}); return post("/detect",request);
}
QJsonObject DetectorClient::startLive(const DetectorSettings &settings) { return post("/live/start",requestFor(settings)); }
QJsonObject DetectorClient::stopLive() { return post("/live/stop",{}); }
QJsonObject DetectorClient::latestLive() { return get("/live/latest"); }
QString DetectorClient::displayLabel(const QJsonObject &detection) {
    const QString target = detection.value("target").toString();
    const QString guide = detection.value("guide").toString();
    if (!target.isEmpty() && !guide.isEmpty()) {
        QString normalizedGuide = guide;
        normalizedGuide.replace('\\', '/');
        const QString targetDirectory = QStringLiteral("/%1/").arg(target);
        const int targetStart = normalizedGuide.indexOf(targetDirectory, 0, Qt::CaseInsensitive);
        if (targetStart >= 0) {
            const QString relativeGuide = normalizedGuide.mid(targetStart + targetDirectory.size());
            if (!relativeGuide.isEmpty()) return QStringLiteral("%1:%2").arg(target, relativeGuide);
        }
    }
    const QString prompt = detection.value("matched_prompt").toString();
    return prompt.isEmpty() ? target : QStringLiteral("%1:%2").arg(target, prompt);
}
QString DetectorClient::summary(const QJsonObject &result) {
    const QJsonArray items=result.value("detections").toArray(); if (items.isEmpty()) return "OWLv2 detector: no configured targets detected.";
    QStringList lines{"OWLv2 detector results for this exact screenshot:"}; int count=0; for(const QJsonValue &value:items) { if (++count>8) break; const QJsonObject item=value.toObject(); const QJsonArray box=item.value("box").toArray(); if (box.size() != 4) continue; QString line=QStringLiteral("- %1 (%2, score %3) at [%4, %5, %6, %7]").arg(displayLabel(item)).arg(item.value("source").toString()).arg(QString::number(item.value("score").toDouble(),'f',3)).arg(QString::number(box.at(0).toDouble(),'f',0)).arg(QString::number(box.at(1).toDouble(),'f',0)).arg(QString::number(box.at(2).toDouble(),'f',0)).arg(QString::number(box.at(3).toDouble(),'f',0)); if (item.contains("raw_logit")) line += QStringLiteral(" raw logit %1").arg(item.value("raw_logit").toDouble(), 0, 'f', 3); lines.append(line); }
    return lines.join('\n');
}
