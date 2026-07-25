#pragma once

#include "DetectorSettingsStore.h"

#include <QImage>
#include <QJsonObject>
#include <QProcess>

class QNetworkReply;

class DetectorClient : public QObject
{
    Q_OBJECT
public:
    explicit DetectorClient(QObject *parent = nullptr);
    ~DetectorClient() override;
    bool prepare(QString *error = nullptr);
    QJsonObject detect(const QImage &image, const DetectorSettings &settings);
    QJsonObject startLive(const DetectorSettings &settings);
    QJsonObject stopLive();
    QJsonObject latestLive();
    static QString displayLabel(const QJsonObject &detection);
    static QString summary(const QJsonObject &result);
private:
    QJsonObject requestFor(const DetectorSettings &settings) const;
    QJsonObject get(const QString &path) const;
    QJsonObject post(const QString &path, const QJsonObject &payload) const;
    QJsonObject waitReply(QNetworkReply *reply, int timeoutMs) const;
    QProcess m_process;
    int m_port = 8765;
};
