#pragma once
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <QQmlEngine>

class HeadsetControlMonitor;

class HeadsetControlBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasSidetoneCapability READ hasSidetoneCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasLightsCapability READ hasLightsCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasRotateToMuteCapability READ hasRotateToMuteCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasChatMixCapability READ hasChatMixCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasVoicePromptsCapability READ hasVoicePromptsCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasEqualizerPresetsCapability READ hasEqualizerPresetsCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasInactiveTimeCapability READ hasInactiveTimeCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY batteryStatusChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(QString batteryIcon READ batteryIcon NOTIFY batteryIconChanged)
    Q_PROPERTY(int chatMix READ chatMix NOTIFY chatMixChanged)
    Q_PROPERTY(QStringList equalizerPresetNames READ equalizerPresetNames NOTIFY equalizerPresetNamesChanged)
    Q_PROPERTY(bool anyDeviceFound READ anyDeviceFound NOTIFY anyDeviceFoundChanged)
    Q_PROPERTY(bool testModeEnabled READ testModeEnabled NOTIFY testModeEnabledChanged)
    Q_PROPERTY(int testProfile READ testProfile NOTIFY testProfileChanged)

public:
    explicit HeadsetControlBridge(QObject *parent = nullptr);
    ~HeadsetControlBridge();

    static HeadsetControlBridge* instance();
    static HeadsetControlBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    Q_INVOKABLE void setMonitoringEnabled(bool enabled);
    Q_INVOKABLE void setLights(bool enabled);
    Q_INVOKABLE void setRotateToMute(bool enabled);
    Q_INVOKABLE void setVoicePrompts(bool enabled);
    Q_INVOKABLE void setEqualizerPreset(int preset);
    Q_INVOKABLE void setSidetone(int value);
    Q_INVOKABLE void setInactiveTime(int value);
    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE void setFetchRate(int seconds);
    Q_INVOKABLE void setTestModeEnabled(bool enabled);
    Q_INVOKABLE void setTestProfile(int profile);

    bool hasSidetoneCapability() const;
    bool hasLightsCapability() const;
    bool hasRotateToMuteCapability() const;
    bool hasChatMixCapability() const;
    bool hasVoicePromptsCapability() const;
    bool hasEqualizerPresetsCapability() const;
    bool hasInactiveTimeCapability() const;
    QString deviceName() const;
    QString batteryStatus() const;
    int batteryLevel() const;
    QString batteryIcon() const;
    int chatMix() const;
    QStringList equalizerPresetNames() const;
    bool anyDeviceFound() const;
    bool testModeEnabled() const;
    int testProfile() const;

signals:
    void capabilitiesChanged();
    void deviceNameChanged();
    void batteryStatusChanged();
    void batteryLevelChanged();
    void batteryIconChanged();
    void chatMixChanged();
    void equalizerPresetNamesChanged();
    void anyDeviceFoundChanged();
    void testModeEnabledChanged();
    void testProfileChanged();
    void lowHeadsetBattery();

private slots:
    void onMonitorCapabilitiesChanged();
    void onMonitorDeviceNameChanged();
    void onMonitorBatteryStatusChanged();
    void onMonitorBatteryLevelChanged();
    void onMonitorChatMixChanged();
    void onMonitorEqualizerPresetNamesChanged();
    void onMonitorAnyDeviceFoundChanged();
    void onMonitorTestModeEnabledChanged();
    void onMonitorTestProfileChanged();

private:
    static HeadsetControlBridge* m_instance;
    HeadsetControlMonitor* findMonitor() const;
    void connectToMonitor();
    void updateLowBatteryNotificationState();

    bool m_lowBatteryNotificationSent = false;
};
