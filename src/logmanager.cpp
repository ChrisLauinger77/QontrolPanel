#include "logmanager.h"
#include <QDebug>
#include <QDateTime>


LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
}

LogManager* LogManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    auto* logger = instance();
    QQmlEngine::setObjectOwnership(logger, QQmlEngine::CppOwnership);
    return logger;
}

LogManager* LogManager::instance()
{
    // Deliberately process-owned: logging remains valid during native shutdown.
    static LogManager* logger = new LogManager();
    return logger;
}

void LogManager::registerCategory(const QString &category)
{
    bool added;
    {
        QMutexLocker lock(&m_mutex);
        added = !m_registeredCategories.contains(category);
        m_registeredCategories.insert(category);
    }
    if (added)
        emit categoryRegistered(category);
}

void LogManager::log(const QString &category, const QString &content)
{
    registerCategory(category);
    emitLog(category, Info, content);
}

void LogManager::warn(const QString &category, const QString &content)
{
    registerCategory(category);
    emitLog(category, Warning, content);
}

void LogManager::critical(const QString &category, const QString &content)
{
    registerCategory(category);
    emitLog(category, Critical, content);
}

QStringList LogManager::getAllCategories() const
{
    QMutexLocker lock(&m_mutex);
    QStringList categories = m_registeredCategories.values();
    categories.sort();
    return categories;
}

void LogManager::emitLog(const QString &category, LogType type, const QString &content)
{
    QString typeStr;
    QString colorCode;

    switch (type) {
    case Info:
        typeStr = "INFO";
        colorCode = "\033[32m";
        break;
    case Warning:
        typeStr = "WARN";
        colorCode = "\033[33m";
        break;
    case Critical:
        typeStr = "CRITICAL";
        colorCode = "\033[31m";
        break;
    }

    const QString resetCode = "\033[0m";
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

    QString consoleMessage = QString("[%1] %2 [%3%4%5] - %6")
                                 .arg(timestamp, category, colorCode, typeStr, resetCode, content);

    QString plainMessage = QString("[%1] %2 [%3] - %4")
                               .arg(timestamp, category, typeStr, content);

    qDebug().noquote() << consoleMessage;

    QMutexLocker lock(&m_mutex);
        m_bufferedLogs.append(qMakePair(plainMessage, type));
    while (m_bufferedLogs.size() > 500)
        m_bufferedLogs.removeFirst();
    if (m_qmlReady && !m_deliveryPending)
    {
        m_deliveryPending = true;
        QMetaObject::invokeMethod(this, &LogManager::publishSnapshot, Qt::QueuedConnection);
    }
}

QVariantList LogManager::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    QVariantList logs;
    for (const auto& entry : m_bufferedLogs)
    {
        logs.append(QVariantMap{{"message", entry.first}, {"type", static_cast<int>(entry.second)}});
    }
    return logs;
}

void LogManager::publishSnapshot()
{
    {
        QMutexLocker lock(&m_mutex);
        m_deliveryPending = false;
    }
    emit bufferedLogsReady(snapshot());
}

void LogManager::setQmlReady()
{
    {
        QMutexLocker lock(&m_mutex);
    m_qmlReady = true;
    }
    publishSnapshot();
    }

void LogManager::clearLogs()
{
    {
        QMutexLocker lock(&m_mutex);
    m_bufferedLogs.clear();
}
    publishSnapshot();
}
