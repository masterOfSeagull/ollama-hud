#pragma once

#include <QObject>
#include <QUrl>

class QQmlApplicationEngine;

class HotReloadController final : public QObject
{
    Q_OBJECT

public:
    explicit HotReloadController(QQmlApplicationEngine *engine, QUrl rootUrl, QObject *parent = nullptr);

    Q_INVOKABLE void reload();

signals:
    void reloadSucceeded();
    void reloadFailed(const QString &error);

private:
    void performReload();

    QQmlApplicationEngine *m_engine;
    QUrl m_rootUrl;
    bool m_reloadInFlight = false;
};
