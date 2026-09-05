#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include <memory>

class WindowChrome : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool maximizeButtonHovered READ maximizeButtonHovered NOTIFY maximizeButtonHoveredChanged)
    Q_PROPERTY(bool maximizeButtonPressed READ maximizeButtonPressed NOTIFY maximizeButtonPressedChanged)

private:
    explicit WindowChrome(QObject* parent = nullptr);

public:
    ~WindowChrome() override;

    static WindowChrome* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static WindowChrome* instance();

    bool maximizeButtonHovered() const;
    bool maximizeButtonPressed() const;

    Q_INVOKABLE bool installWindowChrome(
        QObject* windowObject,
        QObject* titleBarObject,
        QObject* systemMenuObject,
        QObject* minimizeButtonObject,
        QObject* maximizeButtonObject,
        QObject* closeButtonObject);
    Q_INVOKABLE void removeWindowChrome(QObject* windowObject);

    bool nativeEventFilter(
        const QByteArray& eventType,
        void* message,
        qintptr* result) override;

signals:
    void maximizeButtonHoveredChanged();
    void maximizeButtonPressedChanged();

private:
    struct Impl;

    void setMaximizeButtonHovered(bool hovered);
    void setMaximizeButtonPressed(bool pressed);

    static WindowChrome* m_instance;
    std::unique_ptr<Impl> m_impl;
    bool m_maximizeButtonHovered = false;
    bool m_maximizeButtonPressed = false;
};
