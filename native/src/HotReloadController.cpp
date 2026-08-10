#include "HotReloadController.h"

#include <QDebug>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QtAlgorithms>

#include <utility>

HotReloadController::HotReloadController(QQmlApplicationEngine *engine, QUrl rootUrl, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_rootUrl(std::move(rootUrl))
{
    Q_ASSERT(m_engine);
}

void HotReloadController::reload()
{
    if (m_reloadInFlight) {
        qInfo() << "QML hot reload ignored: a reload is already in progress.";
        return;
    }

    m_reloadInFlight = true;
    qInfo() << "QML hot reload requested for" << m_rootUrl;
    QTimer::singleShot(0, this, &HotReloadController::performReload);
}

void HotReloadController::performReload()
{
    const auto roots = m_engine->rootObjects();
    qDeleteAll(roots);

    // QQmlEngine requires QML-created objects to be gone before clearing
    // singleton and component caches.
    m_engine->clearSingletons();
    m_engine->clearComponentCache();

    bool created = false;
    QString error;
    const QMetaObject::Connection createdConnection = connect(
        m_engine,
        &QQmlApplicationEngine::objectCreated,
        this,
        [&created, &error, this](QObject *object, const QUrl &url) {
            if (url != m_rootUrl) {
                return;
            }
            created = object != nullptr;
            if (!created) {
                error = QStringLiteral("%1을(를) 다시 불러오지 못했습니다").arg(m_rootUrl.toString());
            }
        });

    m_engine->load(m_rootUrl);
    disconnect(createdConnection);
    m_reloadInFlight = false;

    if (created) {
        qInfo() << "QML hot reload succeeded for" << m_rootUrl;
        emit reloadSucceeded();
        return;
    }

    if (error.isEmpty()) {
        error = QStringLiteral("%1을(를) 다시 불러오는 중 루트 객체가 생성되지 않았습니다").arg(m_rootUrl.toString());
    }
    qWarning().noquote() << "QML hot reload failed:" << error;
    emit reloadFailed(error);
}
