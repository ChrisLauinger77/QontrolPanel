#include "headsetcontrolbridge.h"
#include "headsetcontrolmonitor.h"
#include "audiomanager.h"
#include "usersettings.h"
#include <QTimer>

HeadsetControlBridge* HeadsetControlBridge::m_instance = nullptr;

HeadsetControlBridge::HeadsetControlBridge(QObject *parent)
    : QObject(parent)
{
    m_instance = this;
    connect(UserSettings::instance(), &UserSettings::headsetcontrolMonitoringChanged,
            this, [this]() {
                setMonitoringEnabled(UserSettings::instance()->headsetcontrolMonitoring());
            });
    QTimer::singleShot(100, this, &HeadsetControlBridge::connectToMonitor);
}

HeadsetControlBridge::~HeadsetControlBridge()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

HeadsetControlBridge* HeadsetControlBridge::instance()
{
    if (!m_instance) {
        m_instance = new HeadsetControlBridge();
    }
    return m_instance;
}

HeadsetControlBridge* HeadsetControlBridge::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine);
    Q_UNUSED(jsEngine);

    if (!m_instance) {
        m_instance = new HeadsetControlBridge();
    }
    return m_instance;
}

void HeadsetControlBridge::connectToMonitor()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        connect(monitor, &HeadsetControlMonitor::capabilitiesChanged,
                this, &HeadsetControlBridge::onMonitorCapabilitiesChanged);
        connect(monitor, &HeadsetControlMonitor::deviceNameChanged,
                this, &HeadsetControlBridge::onMonitorDeviceNameChanged);
        connect(monitor, &HeadsetControlMonitor::batteryStatusChanged,
                this, &HeadsetControlBridge::onMonitorBatteryStatusChanged);
        connect(monitor, &HeadsetControlMonitor::batteryLevelChanged,
                this, &HeadsetControlBridge::onMonitorBatteryLevelChanged);
        connect(monitor, &HeadsetControlMonitor::chatMixChanged,
            this, &HeadsetControlBridge::onMonitorChatMixChanged);
        connect(monitor, &HeadsetControlMonitor::anyDeviceFoundChanged,
                this, &HeadsetControlBridge::onMonitorAnyDeviceFoundChanged);
        connect(monitor, &HeadsetControlMonitor::testModeEnabledChanged,
            this, &HeadsetControlBridge::onMonitorTestModeEnabledChanged);
        connect(monitor, &HeadsetControlMonitor::testProfileChanged,
            this, &HeadsetControlBridge::onMonitorTestProfileChanged);

        int fetchRateSeconds = UserSettings::instance()->headsetcontrolFetchRate();
        int fetchRateMs = fetchRateSeconds * 1000;
        QMetaObject::invokeMethod(monitor, "setFetchInterval", Qt::QueuedConnection,
                                  Q_ARG(int, fetchRateMs));

        if (UserSettings::instance()->headsetcontrolMonitoring()) {
            QMetaObject::invokeMethod(monitor, "startMonitoring", Qt::QueuedConnection);
        }

        emit capabilitiesChanged();
        emit deviceNameChanged();
        emit batteryStatusChanged();
        emit batteryLevelChanged();
        emit chatMixChanged();
        emit anyDeviceFoundChanged();
        emit testModeEnabledChanged();
        emit testProfileChanged();
    } else {
        QTimer::singleShot(200, this, &HeadsetControlBridge::connectToMonitor);
    }
}

HeadsetControlMonitor* HeadsetControlBridge::findMonitor() const
{
    AudioManager* audioManager = AudioManager::instance();
    if (audioManager) {
        AudioWorker* worker = audioManager->getWorker();
        return worker ? worker->getHeadsetControlMonitor() : nullptr;
    }
    return nullptr;
}

void HeadsetControlBridge::setMonitoringEnabled(bool enabled)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        if (enabled) {
            QMetaObject::invokeMethod(monitor, "startMonitoring", Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(monitor, "stopMonitoring", Qt::QueuedConnection);
        }
    }
}

void HeadsetControlBridge::setLights(bool enabled)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setLights", Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }
}

void HeadsetControlBridge::setRotateToMute(bool enabled)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setRotateToMute", Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }
}

void HeadsetControlBridge::setVoicePrompts(bool enabled)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setVoicePrompts", Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }
}

void HeadsetControlBridge::setSidetone(int value)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setSidetone", Qt::QueuedConnection,
                                  Q_ARG(int, value));
    }
}

void HeadsetControlBridge::setInactiveTime(int value)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setInactiveTime", Qt::QueuedConnection,
                                  Q_ARG(int, value));
    }
}

bool HeadsetControlBridge::hasSidetoneCapability() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->hasSidetoneCapability() : false;
}

bool HeadsetControlBridge::hasLightsCapability() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->hasLightsCapability() : false;
}

bool HeadsetControlBridge::hasRotateToMuteCapability() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->hasRotateToMuteCapability() : false;
}

bool HeadsetControlBridge::hasChatMixCapability() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->hasChatMixCapability() : false;
}

bool HeadsetControlBridge::hasVoicePromptsCapability() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->hasVoicePromptsCapability() : false;
}

bool HeadsetControlBridge::hasInactiveTimeCapability() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->hasInactiveTimeCapability() : false;
}

QString HeadsetControlBridge::deviceName() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->deviceName() : QString();
}

QString HeadsetControlBridge::batteryStatus() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->batteryStatus() : QString("BATTERY_UNAVAILABLE");
}

int HeadsetControlBridge::batteryLevel() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->batteryLevel() : -1;
}

int HeadsetControlBridge::chatMix() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->chatMix() : -1;
}

bool HeadsetControlBridge::anyDeviceFound() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->anyDeviceFound() : false;
}

bool HeadsetControlBridge::testModeEnabled() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->testModeEnabled() : false;
}

int HeadsetControlBridge::testProfile() const
{
    HeadsetControlMonitor* monitor = findMonitor();
    return monitor ? monitor->testProfile() : 1;
}

void HeadsetControlBridge::onMonitorCapabilitiesChanged()
{
    emit capabilitiesChanged();
}

void HeadsetControlBridge::onMonitorDeviceNameChanged()
{
    emit deviceNameChanged();
}

void HeadsetControlBridge::onMonitorBatteryStatusChanged()
{
    emit batteryStatusChanged();
}

void HeadsetControlBridge::onMonitorBatteryLevelChanged()
{
    int level = batteryLevel();

    if (level < 20 && level >= 0) {
        if (!m_lowBatteryNotificationSent && UserSettings::instance()->enableNotifications()) {
            emit lowHeadsetBattery();
            m_lowBatteryNotificationSent = true;
        }
    } else if (level >= 20 && UserSettings::instance()->enableNotifications()) {
        m_lowBatteryNotificationSent = false;
    }

    emit batteryLevelChanged();
}

void HeadsetControlBridge::onMonitorChatMixChanged()
{
    emit chatMixChanged();
}

void HeadsetControlBridge::onMonitorAnyDeviceFoundChanged()
{
    if (!anyDeviceFound()) {
        m_lowBatteryNotificationSent = false;
    }

    emit anyDeviceFoundChanged();
}

void HeadsetControlBridge::onMonitorTestModeEnabledChanged()
{
    emit testModeEnabledChanged();
}

void HeadsetControlBridge::onMonitorTestProfileChanged()
{
    emit testProfileChanged();
}

void HeadsetControlBridge::setFetchRate(int seconds)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        int intervalMs = seconds * 1000;
        QMetaObject::invokeMethod(monitor, "setFetchInterval", Qt::QueuedConnection,
                                  Q_ARG(int, intervalMs));
    }
}

void HeadsetControlBridge::setTestModeEnabled(bool enabled)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setTestModeEnabled", Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }
}

void HeadsetControlBridge::setTestProfile(int profile)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setTestProfile", Qt::QueuedConnection,
                                  Q_ARG(int, profile));
    }
}
