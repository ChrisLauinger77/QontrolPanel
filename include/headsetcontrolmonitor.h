#pragma once
#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <headsetcontrol.hpp>
#include <memory>
#include <vector>

struct HeadsetControlDevice {
    QString deviceName;
    QString vendor;
    QString product;
    QString vendorId;
    QString productId;
    QString batteryStatus;  // "BATTERY_AVAILABLE", "BATTERY_CHARGING", "BATTERY_UNAVAILABLE"
    int batteryLevel;       // -1 - 100
    int chatMix;            // -1 when unavailable, otherwise 0 - 128
    QStringList capabilities;

    HeadsetControlDevice() : batteryLevel(-1), chatMix(-1) {}
};
Q_DECLARE_METATYPE(HeadsetControlDevice)

class HeadsetControlMonitor : public QObject
{
    Q_OBJECT
public:
    explicit HeadsetControlMonitor(QObject *parent = nullptr);
    ~HeadsetControlMonitor();

    bool isMonitoring() const;
    QList<HeadsetControlDevice> getCachedDevices() const { return m_cachedDevices; }

    bool hasSidetoneCapability() const { return m_hasSidetoneCapability; }
    bool hasLightsCapability() const { return m_hasLightsCapability; }
    bool hasRotateToMuteCapability() const { return m_hasRotateToMuteCapability; }
    bool hasChatMixCapability() const { return m_hasChatMixCapability; }
    bool hasVoicePromptsCapability() const { return m_hasVoicePromptsCapability; }
    bool hasInactiveTimeCapability() const { return m_hasInactiveTimeCapability; }
    QString deviceName() const { return m_deviceName; }
    QString batteryStatus() const { return m_batteryStatus; }
    int batteryLevel() const { return m_batteryLevel; }
    int chatMix() const { return m_chatMix; }
    bool anyDeviceFound() const { return m_anyDeviceFound; }
    bool testModeEnabled() const { return m_testModeEnabled; }
    int testProfile() const { return m_testProfile; }

public slots:
    void startMonitoring();
    void stopMonitoring();
    void setLights(bool enabled);
    void setRotateToMute(bool enabled);
    void setVoicePrompts(bool enabled);
    void setSidetone(int value);
    void setInactiveTime(int value);
    void setFetchInterval(int intervalMs);
    void setTestModeEnabled(bool enabled);
    void setTestProfile(int profile);

signals:
    void headsetDataUpdated(const QList<HeadsetControlDevice>& devices);
    void monitoringStateChanged(bool enabled);
    void capabilitiesChanged();
    void deviceNameChanged();
    void batteryStatusChanged();
    void batteryLevelChanged();
    void chatMixChanged();
    void anyDeviceFoundChanged();
    void testModeEnabledChanged();
    void testProfileChanged();

private slots:
    void fetchHeadsetInfo();

private:
    void applyTestDeviceConfiguration();
    void updateDeviceCache();
    void updateCapabilities();
    QString batteryStatusToString(battery_status status) const;
    QStringList getCapabilityList(const headsetcontrol::Headset& headset) const;

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
    bool m_hasInactiveTimeCapability;
    QString m_deviceName;
    QString m_batteryStatus;
    int m_batteryLevel;
    int m_chatMix;
    bool m_anyDeviceFound;
    bool m_isFetching;
    bool m_testModeEnabled;
    int m_testProfile;
};
