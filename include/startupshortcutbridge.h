#pragma once

#include <QString>
#include <QObject>
#include <QQmlEngine>

class StartupShortcutBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

private:
    explicit StartupShortcutBridge(QObject* parent = nullptr);

public:
    ~StartupShortcutBridge() override;

    static StartupShortcutBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static StartupShortcutBridge* instance();

    Q_INVOKABLE bool getShortcutState();
    Q_INVOKABLE void setStartupShortcut(bool enabled);

private:
    static StartupShortcutBridge* m_instance;

    bool isShortcutPresent(QString shortcutName);
    void manageShortcut(bool state, QString shortcutName);
};
