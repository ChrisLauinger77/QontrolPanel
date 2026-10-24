#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonArray>
#include <headsetcontrol.hpp>
#include <memory>
#include <functional>
#include <vector>

struct HeadsetControlDevice {
    QString deviceName;
    QString vendor;
    QString product;
    QString vendorId;
    QString productId;
    QString batteryStatus;  // "BATTERY_AVAILABLE", "BATTERY_CHARGING", "BATTERY_UNAVAILABLE"
    int batteryLevel;       // -1 - 100
    int timeToFullMinutes = -1; // -1 when the device supplies no estimate
    int timeToEmptyMinutes = -1;
    int chatMix;            // -1 when unavailable, otherwise 0 - 128
    int micMuted = -1;      // Last poll: -1 unknown, 0 unmuted, 1 muted
    int micAttached = -1;   // Last poll: -1 unknown, 0 detached, 1 attached
    int sidetoneLevel = -1; // Normalised 0 - 128, -1 when unknown
    QStringList capabilities;

    HeadsetControlDevice() : batteryLevel(-1), chatMix(-1) {}
};
Q_DECLARE_METATYPE(HeadsetControlDevice)

struct HeadsetControlState
{
    bool hasSidetoneCapability = false;
    bool hasLightsCapability = false;
    bool hasRotateToMuteCapability = false;
    bool hasChatMixCapability = false;
    bool hasVoicePromptsCapability = false;
    bool hasEqualizerPresetsCapability = false;
    bool hasInactiveTimeCapability = false;
    bool hasBluetoothCallVolumeCapability = false;
    bool hasBluetoothWhenPoweredOnCapability = false;
    bool hasMicMuteLedBrightnessCapability = false;
    bool hasVolumeLimiterCapability = false;
    bool hasNoiseFilterCapability = false;
    bool hasMicVolumeCapability = false;
    bool hasAncButtonModesCapability = false;
    bool hasAncStartupModeCapability = false;
    bool hasAncCapability = false;
    QString deviceName;
    QString displayName;
    QString batteryStatus = "BATTERY_UNAVAILABLE";
    int batteryLevel = -1;
    int timeToFullMinutes = -1;
    int timeToEmptyMinutes = -1;
    int chatMix = -1;
    int micMuted = -1;
    int micAttached = -1;
    int sidetoneLevel = -1;
    quint64 sidetoneRequestRevision = 0;
    QStringList equalizerPresetNames;
    bool anyDeviceFound = false;
    bool testModeEnabled = false;
    int testProfile = 1;
};

Q_DECLARE_METATYPE(HeadsetControlState)

class HeadsetControlMonitor : public QObject
{
    Q_OBJECT
public:
    using HeadsetDiscovery = std::function<std::vector<headsetcontrol::Headset>()>;
    explicit HeadsetControlMonitor(QObject *parent = nullptr, HeadsetDiscovery discover = headsetcontrol::discover);
    ~HeadsetControlMonitor();

    bool isMonitoring() const;
    HeadsetControlState snapshot() const;
    QList<HeadsetControlDevice> getCachedDevices() const { return m_cachedDevices; }

    bool hasSidetoneCapability() const { return m_hasSidetoneCapability; }
    bool hasLightsCapability() const { return m_hasLightsCapability; }
    bool hasRotateToMuteCapability() const { return m_hasRotateToMuteCapability; }
    bool hasChatMixCapability() const { return m_hasChatMixCapability; }
    bool hasVoicePromptsCapability() const { return m_hasVoicePromptsCapability; }
    bool hasInactiveTimeCapability() const { return m_hasInactiveTimeCapability; }
    bool hasBluetoothCallVolumeCapability() const { return m_hasBluetoothCallVolumeCapability; }
    bool hasBluetoothWhenPoweredOnCapability() const { return m_hasBluetoothWhenPoweredOnCapability; }
    bool hasMicMuteLedBrightnessCapability() const { return m_hasMicMuteLedBrightnessCapability; }
    bool hasVolumeLimiterCapability() const { return m_hasVolumeLimiterCapability; }
    bool hasNoiseFilterCapability() const { return m_hasNoiseFilterCapability; }
    bool hasMicVolumeCapability() const { return m_hasMicVolumeCapability; }
    bool hasAncButtonModesCapability() const { return m_hasAncButtonModesCapability; }
    bool hasAncStartupModeCapability() const { return m_hasAncStartupModeCapability; }
    bool hasAncCapability() const { return m_hasAncCapability; }
    QString deviceName() const { return m_deviceName; }
    QString displayName() const { return m_displayName; }
    QString batteryStatus() const { return m_batteryStatus; }
    int batteryLevel() const { return m_batteryLevel; }
    int timeToFullMinutes() const { return m_timeToFullMinutes; }
    int timeToEmptyMinutes() const { return m_timeToEmptyMinutes; }
    int chatMix() const { return m_chatMix; }
    int micMuted() const { return m_micMuted; }
    int micAttached() const { return m_micAttached; }
    int sidetoneLevel() const { return m_sidetoneLevel; }
    bool hasEqualizerPresetsCapability() const { return m_hasEqualizerPresetsCapability; }
    QStringList equalizerPresetNames() const { return m_equalizerPresetNames; }
    bool anyDeviceFound() const { return m_anyDeviceFound; }
    bool testModeEnabled() const { return m_testModeEnabled; }
    int testProfile() const { return m_testProfile; }

public slots:
    void setDesiredSettings(const QVariantMap& settings);
    void startMonitoring();
    void stopMonitoring();
    void requestRefresh();
    void setLights(bool enabled);
    void setRotateToMute(bool enabled);
    void setVoicePrompts(bool enabled);
    void setEqualizerPreset(int preset);
    void setSidetone(int value, quint64 requestRevision = 0);
    void setInactiveTime(int value);
    void setFetchInterval(int intervalMs);
    void setTestModeEnabled(bool enabled);
    void setTestProfile(int profile);

signals:
    void snapshotReady(const HeadsetControlState& state);
    void operationErrorChanged(const QString& error);
    void headsetDataUpdated(const QList<HeadsetControlDevice>& devices);
    void monitoringStateChanged(bool enabled);
    void capabilitiesChanged();
    void deviceNameChanged();
    void displayNameChanged();
    void batteryStatusChanged();
    void batteryLevelChanged();
    void timeToFullMinutesChanged();
    void timeToEmptyMinutesChanged();
    void chatMixChanged();
    void micMutedChanged();
    void micAttachedChanged();
    void sidetoneLevelChanged();
    void sidetoneAdopted(int value, quint64 requestRevision);
    void equalizerPresetNamesChanged();
    void anyDeviceFoundChanged();
    void testModeEnabledChanged();
    void testProfileChanged();

private slots:
    void fetchHeadsetInfo();

private:
    void fetchHeadsetInfoInternal(bool bypassRecentFetch);
    bool shouldBypassRecentFetchForManualRequest() const;
    void scheduleNextFetch();
    void applyTestDeviceConfiguration();
    void updateFetchTimerInterval(bool deviceFound);
    void updateDeviceCache();
    void updateCapabilities();
    void updateSidetoneFromDevice();
    void applyPendingSettings();
    void clearDeviceState();
    QVariantMap m_desiredSettings;
    QVariantMap m_appliedSettings;
    QMap<QString, int> m_settingAttempts;
    headsetcontrol::Headset* activeHeadset();
    QString activeHeadsetSettingsKey(const headsetcontrol::Headset& headset) const;
    QString batteryStatusToString(battery_status status) const;
    QString batteryStatusFromError(const headsetcontrol::DeviceError& error) const;
    QStringList getCapabilityList(const headsetcontrol::Headset& headset) const;

    HeadsetDiscovery m_discoverHeadsets;
    QTimer* m_fetchTimer;
    QList<HeadsetControlDevice> m_cachedDevices;
    std::vector<headsetcontrol::Headset> m_headsets;

    bool m_isMonitoring;
    int m_fetchIntervalMs;

    bool m_hasSidetoneCapability;
    bool m_hasLightsCapability;
    bool m_hasRotateToMuteCapability;
    bool m_hasChatMixCapability;
    bool m_hasVoicePromptsCapability;
    bool m_hasEqualizerPresetsCapability;
    bool m_hasInactiveTimeCapability;
    bool m_hasBluetoothCallVolumeCapability = false;
    bool m_hasBluetoothWhenPoweredOnCapability = false;
    bool m_hasMicMuteLedBrightnessCapability = false;
    bool m_hasVolumeLimiterCapability = false;
    bool m_hasNoiseFilterCapability = false;
    bool m_hasMicVolumeCapability = false;
    bool m_hasAncButtonModesCapability = false;
    bool m_hasAncStartupModeCapability = false;
    bool m_hasAncCapability = false;
    QString m_deviceName;
    QString m_displayName;
    QString m_batteryStatus;
    int m_batteryLevel;
    int m_timeToFullMinutes = -1;
    int m_timeToEmptyMinutes = -1;
    int m_chatMix;
    int m_micMuted = -1;
    int m_micAttached = -1;
    int m_sidetoneLevel = -1;
    bool m_sidetoneWriteRequested = false;
    quint64 m_sidetoneRequestRevision = 0;
    QStringList m_equalizerPresetNames;
    bool m_anyDeviceFound;
    bool m_isFetching;
    int m_activeHeadsetIndex;
    QString m_activeHeadsetSettingsKey;
    QElapsedTimer m_lastFetchCompleted;
    bool m_testModeEnabled;
    int m_testProfile;
};
