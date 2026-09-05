#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QSet>
#include <QMutex>

class LogManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum LogType {
        Info,
        Warning,
        Critical
    };
    Q_ENUM(LogType)

    static LogManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static LogManager* instance();

    // New simplified API
    Q_INVOKABLE void log(const QString &category, const QString &content);
    Q_INVOKABLE void warn(const QString &category, const QString &content);
    Q_INVOKABLE void critical(const QString &category, const QString &content);

    Q_INVOKABLE QStringList getAllCategories() const;
    Q_INVOKABLE void setQmlReady();
    Q_INVOKABLE void clearLogs();
    Q_INVOKABLE QVariantList snapshot() const;

signals:
    void bufferedLogsReady(const QVariantList &logs);
    void categoryRegistered(const QString &category);

private:
    explicit LogManager(QObject *parent = nullptr);

    void registerCategory(const QString &category);
    void emitLog(const QString &category, LogType type, const QString &content);

    QList<QPair<QString, LogType>> m_bufferedLogs;
    mutable QMutex m_mutex;
    bool m_qmlReady = false;
    bool m_deliveryPending = false;
    void publishSnapshot();

    QSet<QString> m_registeredCategories;
};

// Helper macros for concise logging
#define LOG_INFO(category, message) \
    LogManager::instance()->log(category, message)

#define LOG_WARN(category, message) \
    LogManager::instance()->warn(category, message)

#define LOG_CRITICAL(category, message) \
    LogManager::instance()->critical(category, message)
