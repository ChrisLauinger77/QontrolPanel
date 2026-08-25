#include "logmanager.h"
#include <QDebug>
#include <QDateTime>

LogManager* LogManager::m_instance = nullptr;

LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
}

LogManager* LogManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

LogManager* LogManager::instance()
{
    if (!m_instance) {
        m_instance = new LogManager();
    }
    return m_instance;
}

void LogManager::registerCategory(const QString &category)
{
    if (!m_registeredCategories.contains(category)) {
        m_registeredCategories.insert(category);
        emit categoryRegistered(category);
    }
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
    QStringList categories = m_registeredCategories.values();
    categories.sort();
    return categories;
}

QString LogManager::formatMessage(const QString &category, LogType type, const QString &content) const
{
    QString typeStr;
    switch (type) {
    case Info:
        typeStr = "INFO";
        break;
    case Warning:
        typeStr = "WARN";
        break;
    case Critical:
        typeStr = "CRITICAL";
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    return QString("[%1] %2 [%3] - %4")
        .arg(timestamp, category, typeStr, content);
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

    if (m_qmlReady) {
        emit logReceived(plainMessage, type);
    } else {
        m_bufferedLogs.append(qMakePair(plainMessage, type));
    }
}

void LogManager::setQmlReady()
{
    if (m_qmlReady) return;

    m_qmlReady = true;

    QVariantList bufferedLogs;
    for (const auto& log : m_bufferedLogs) {
        QVariantMap logEntry;
        logEntry["message"] = log.first;
        logEntry["type"] = static_cast<int>(log.second);
        bufferedLogs.append(logEntry);
    }

    if (!bufferedLogs.isEmpty()) {
        emit bufferedLogsReady(bufferedLogs);
    }

    m_bufferedLogs.clear();
}
