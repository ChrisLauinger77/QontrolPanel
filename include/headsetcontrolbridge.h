#pragma once
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <QQmlEngine>

#include "headsetcontrolmonitor.h"
#include <QThread>
class AudioWorker;

class HeadsetControlBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasSidetoneCapability READ hasSidetoneCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasHeadsetVolumeCapability READ hasHeadsetVolumeCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canReadHeadsetVolume READ canReadHeadsetVolume NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasLightsCapability READ hasLightsCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasRotateToMuteCapability READ hasRotateToMuteCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasChatMixCapability READ hasChatMixCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasVoicePromptsCapability READ hasVoicePromptsCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasEqualizerPresetsCapability READ hasEqualizerPresetsCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasInactiveTimeCapability READ hasInactiveTimeCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasBluetoothCallVolumeCapability READ hasBluetoothCallVolumeCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasBluetoothWhenPoweredOnCapability READ hasBluetoothWhenPoweredOnCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasMicMuteLedBrightnessCapability READ hasMicMuteLedBrightnessCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasVolumeLimiterCapability READ hasVolumeLimiterCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasNoiseFilterCapability READ hasNoiseFilterCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasMicVolumeCapability READ hasMicVolumeCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasAncButtonModesCapability READ hasAncButtonModesCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasAncStartupModeCapability READ hasAncStartupModeCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasAncCapability READ hasAncCapability NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY batteryStatusChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(int timeToFullMinutes READ timeToFullMinutes NOTIFY timeToFullMinutesChanged)
    Q_PROPERTY(int timeToEmptyMinutes READ timeToEmptyMinutes NOTIFY timeToEmptyMinutesChanged)
    Q_PROPERTY(QString batteryIcon READ batteryIcon NOTIFY batteryIconChanged)
    Q_PROPERTY(int chatMix READ chatMix NOTIFY chatMixChanged)
    Q_PROPERTY(int micMuted READ micMuted NOTIFY micMutedChanged)
    Q_PROPERTY(int micAttached READ micAttached NOTIFY micAttachedChanged)
    Q_PROPERTY(int sidetoneLevel READ sidetoneLevel NOTIFY sidetoneLevelChanged)
    Q_PROPERTY(int pendingSidetoneLevel READ pendingSidetoneLevel NOTIFY pendingSidetoneLevelChanged)
    Q_PROPERTY(int headsetVolumeLevel READ headsetVolumeLevel NOTIFY headsetVolumeLevelChanged)
    Q_PROPERTY(int pendingHeadsetVolumeLevel READ pendingHeadsetVolumeLevel NOTIFY pendingHeadsetVolumeLevelChanged)
    Q_PROPERTY(QStringList equalizerPresetNames READ equalizerPresetNames NOTIFY equalizerPresetNamesChanged)
    Q_PROPERTY(bool anyDeviceFound READ anyDeviceFound NOTIFY anyDeviceFoundChanged)
    Q_PROPERTY(bool testModeEnabled READ testModeEnabled NOTIFY testModeEnabledChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int testProfile READ testProfile NOTIFY testProfileChanged)

private:
    explicit HeadsetControlBridge(QObject* parent = nullptr);

public:
    ~HeadsetControlBridge();

    static HeadsetControlBridge* instance();
    static HeadsetControlBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    void attachAudioWorker(AudioWorker* worker);
    void shutdown();
    Q_INVOKABLE void setMonitoringEnabled(bool enabled);
    Q_INVOKABLE void setLights(bool enabled);
    Q_INVOKABLE void setRotateToMute(bool enabled);
    Q_INVOKABLE void setVoicePrompts(bool enabled);
    Q_INVOKABLE void setEqualizerPreset(int preset);
    Q_INVOKABLE void setSidetone(int value);
    Q_INVOKABLE void setHeadsetVolume(int value);
    Q_INVOKABLE void setInactiveTime(int value);
    Q_INVOKABLE void setBluetoothCallVolume(int value);
    Q_INVOKABLE void setBluetoothWhenPoweredOn(int value);
    Q_INVOKABLE void setMicMuteLedBrightness(int value);
    Q_INVOKABLE void setVolumeLimiter(int value);
    Q_INVOKABLE void setNoiseFilter(int value);
    Q_INVOKABLE void setMicVolume(int value);
    Q_INVOKABLE void setAncButtonModes(int value);
    Q_INVOKABLE void setAncStartupMode(int value);
    Q_INVOKABLE void setAnc(int value);
    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE void setFetchRate(int seconds);
    Q_INVOKABLE void setTestModeEnabled(bool enabled);
    Q_INVOKABLE void setTestProfile(int profile);

    bool hasSidetoneCapability() const;
    bool hasHeadsetVolumeCapability() const;
    bool canReadHeadsetVolume() const;
    bool hasLightsCapability() const;
    bool hasRotateToMuteCapability() const;
    bool hasChatMixCapability() const;
    bool hasVoicePromptsCapability() const;
    bool hasEqualizerPresetsCapability() const;
    bool hasInactiveTimeCapability() const;
    bool hasBluetoothCallVolumeCapability() const;
    bool hasBluetoothWhenPoweredOnCapability() const;
    bool hasMicMuteLedBrightnessCapability() const;
    bool hasVolumeLimiterCapability() const;
    bool hasNoiseFilterCapability() const;
    bool hasMicVolumeCapability() const;
    bool hasAncButtonModesCapability() const;
    bool hasAncStartupModeCapability() const;
    bool hasAncCapability() const;
    QString deviceName() const;
    QString displayName() const;
    QString batteryStatus() const;
    int batteryLevel() const;
    int timeToFullMinutes() const;
    int timeToEmptyMinutes() const;
    QString batteryIcon() const;
    int chatMix() const;
    int micMuted() const;
    int micAttached() const;
    int sidetoneLevel() const;
    int pendingSidetoneLevel() const { return m_pendingSidetoneLevel; }
    int headsetVolumeLevel() const;
    int pendingHeadsetVolumeLevel() const { return m_pendingHeadsetVolumeLevel; }
    QStringList equalizerPresetNames() const;
    bool anyDeviceFound() const;
    bool testModeEnabled() const;
    int testProfile() const;
    QString lastError() const { return m_lastError; }

signals:
    void capabilitiesChanged();
    void deviceNameChanged();
    void displayNameChanged();
    void batteryStatusChanged();
    void batteryLevelChanged();
    void timeToFullMinutesChanged();
    void timeToEmptyMinutesChanged();
    void batteryIconChanged();
    void chatMixChanged();
    void micMutedChanged();
    void micAttachedChanged();
    void sidetoneLevelChanged();
    void pendingSidetoneLevelChanged();
    void headsetVolumeLevelChanged();
    void pendingHeadsetVolumeLevelChanged();
    void equalizerPresetNamesChanged();
    void anyDeviceFoundChanged();
    void testModeEnabledChanged();
    void testProfileChanged();
    void lowHeadsetBattery();
    void lastErrorChanged();
    void headsetDataUpdated(const QList<HeadsetControlDevice>& devices);

private slots:
    void applyState(const HeadsetControlState& state);

private:
    static HeadsetControlBridge* m_instance;
    HeadsetControlMonitor* findMonitor() const;
    void connectToMonitor();
    void syncDesiredSettings();
    void requestSidetone(int value);
    void requestHeadsetVolume(int value);
    QThread* m_thread = nullptr;
    HeadsetControlMonitor* m_monitor = nullptr;
    void updateLowBatteryNotificationState();

    bool m_lowBatteryNotificationSent = false;
    bool m_adoptingSidetone = false;
    quint64 m_sidetoneRequestRevision = 0;
    int m_pendingSidetoneLevel = -1;
    bool m_adoptingHeadsetVolume = false;
    quint64 m_headsetVolumeRequestRevision = 0;
    int m_pendingHeadsetVolumeLevel = -1;
    HeadsetControlState m_cachedState;
    QString m_lastError;
};
