#pragma once

#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class PowerWorker : public QObject
{
    Q_OBJECT
public slots:
    void refreshCapabilities();
    void execute(const QString& action);
    void cleanup() {}
signals:
    void capabilitiesReady(const QVariantMap& capabilities);
    void completed(const QString& action, bool success, unsigned long error);
};

class PowerBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool sleepSupported READ isSleepSupported NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hibernateSupported READ isHibernateSupported NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool uefiSupported READ isUEFISupported NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool multipleUsers READ hasMultipleUsers NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

private:
    explicit PowerBridge(QObject* parent = nullptr);

public:
    ~PowerBridge() override;

    static PowerBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static PowerBridge* instance();

    bool busy() const { return m_busy; }
    QString lastError() const { return m_lastError; }
    Q_INVOKABLE bool hasMultipleUsers();
    Q_INVOKABLE bool isSleepSupported();
    Q_INVOKABLE bool isHibernateSupported();
    Q_INVOKABLE bool isUEFISupported();

    Q_INVOKABLE bool shutdown();
    Q_INVOKABLE bool restart();
    Q_INVOKABLE bool sleep();
    Q_INVOKABLE bool hibernate();
    Q_INVOKABLE bool lockAccount();
    Q_INVOKABLE bool signOut();
    Q_INVOKABLE bool switchAccount();
    Q_INVOKABLE void restartToUEFI();

private:
    static PowerBridge* m_instance;

    bool enqueue(const QString& action);
    QThread* m_thread = nullptr;
    PowerWorker* m_worker = nullptr;
    QVariantMap m_capabilities;
    bool m_busy = false;
    QString m_lastError;
signals:
    void capabilitiesChanged();
    void busyChanged();
    void lastErrorChanged();
    void operationFailed(const QString& message);
};
