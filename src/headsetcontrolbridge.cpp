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
    UserSettings* settings = UserSettings::instance();

    connect(settings, &UserSettings::headsetcontrolMonitoringChanged,
            this, [this]() {
                setMonitoringEnabled(UserSettings::instance()->headsetcontrolMonitoring());
            });
    connect(settings, &UserSettings::headsetcontrolLowBatteryThresholdChanged,
            this, [this]() {
                updateLowBatteryNotificationState();
                emit batteryIconChanged();
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
        emit batteryIconChanged();
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

QString HeadsetControlBridge::batteryIcon() const
{
    const int level = batteryLevel();
    const QString status = batteryStatus();
    const int lowBatteryThreshold = UserSettings::instance()->headsetcontrolLowBatteryThreshold();

    if (status == "BATTERY_UNAVAILABLE" || level < 0) {
        return QString();
    }

    QString icon;
    if (status == "BATTERY_CHARGING") {
        icon += QString::fromUtf8("⚡︎");
    }

    icon += level <= lowBatteryThreshold ? QString::fromUtf8("🪫") : QString::fromUtf8("🔋");
    return icon;
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
    emit batteryIconChanged();
}

void HeadsetControlBridge::onMonitorBatteryLevelChanged()
{
    updateLowBatteryNotificationState();
    emit batteryLevelChanged();
    emit batteryIconChanged();
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

void HeadsetControlBridge::updateLowBatteryNotificationState()
{
    const int level = batteryLevel();
    const int lowBatteryThreshold = UserSettings::instance()->headsetcontrolLowBatteryThreshold();

    if (level < 0 || level > lowBatteryThreshold || !UserSettings::instance()->enableNotifications()) {
        m_lowBatteryNotificationSent = false;
        return;
    }

    if (!m_lowBatteryNotificationSent) {
        emit lowHeadsetBattery();
        m_lowBatteryNotificationSent = true;
    }
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
