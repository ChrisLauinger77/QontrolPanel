#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include <memory>

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
    Q_INVOKABLE bool applyMainWindowBackdrop(QObject* windowObject);
    Q_INVOKABLE void removeBackdrop(QObject* windowObject);

private:
    struct Impl;

    bool applyBackdrop(QObject* windowObject, bool mainWindow);

    static WindowsBackdrop* m_instance;
    std::unique_ptr<Impl> m_impl;
};
