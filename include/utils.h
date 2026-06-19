#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

class Utils : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Utils(QObject* parent = nullptr);
    ~Utils() override;

    static Utils* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static Utils* instance();

    Q_INVOKABLE void openLegacySoundSettings();
    Q_INVOKABLE void openModernSoundSettings();
    Q_INVOKABLE int getAvailableDesktopWidth() const;
    Q_INVOKABLE int getAvailableDesktopHeight() const;
    Q_INVOKABLE QVariantMap getCursorScreenAvailableGeometry() const;
    Q_INVOKABLE void playFeedbackSound();
    Q_INVOKABLE void playNotificationSound(bool enabled);
    Q_INVOKABLE void setStyle(int style);

private:
    static Utils* m_instance;

    QByteArray m_successSound;
    QByteArray m_failureSound;
};
