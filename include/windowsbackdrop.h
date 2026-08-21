#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class WindowsBackdrop : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit WindowsBackdrop(QObject* parent = nullptr);
    ~WindowsBackdrop() override;

    static WindowsBackdrop* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static WindowsBackdrop* instance();

    Q_INVOKABLE bool applyTransientBackdrop(QObject* windowObject);
    Q_INVOKABLE void removeBackdrop(QObject* windowObject);

private:
    static WindowsBackdrop* m_instance;
};
