#include "headsetcontrolbridge.h"
#include "headsetcontrolmonitor.h"
#include "audiomanager.h"
#include "usersettings.h"
#include <QPointer>
#include <QTimer>
#include <QCoreApplication>
#include "workerthreads.h"

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
    connect(settings, &UserSettings::enableNotificationsChanged, this,
            &HeadsetControlBridge::updateLowBatteryNotificationState);
    connect(settings, &UserSettings::headsetcontrolLightsChanged, this, &HeadsetControlBridge::syncDesiredSettings);
    connect(settings, &UserSettings::headsetcontrolRotateToMuteChanged, this,
            &HeadsetControlBridge::syncDesiredSettings);
    connect(settings, &UserSettings::headsetcontrolVoicePromptsChanged, this,
            &HeadsetControlBridge::syncDesiredSettings);
    connect(settings, &UserSettings::headsetcontrolEqualizerPresetChanged, this,
            &HeadsetControlBridge::syncDesiredSettings);
    connect(settings, &UserSettings::headsetcontrolSidetoneChanged, this, &HeadsetControlBridge::syncDesiredSettings);
    connect(settings, &UserSettings::headsetcontrolInactiveTimeChanged, this,
            &HeadsetControlBridge::syncDesiredSettings);
    connect(settings, &UserSettings::headsetcontrolFetchRateChanged, this,
            [this] { setFetchRate(UserSettings::instance()->headsetcontrolFetchRate()); });
    connectToMonitor();
}

HeadsetControlBridge::~HeadsetControlBridge()
{
    shutdown();
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

HeadsetControlBridge* HeadsetControlBridge::instance()
{
    if (!m_instance) {
        m_instance = new HeadsetControlBridge(QCoreApplication::instance());
    }
    return m_instance;
}

HeadsetControlBridge* HeadsetControlBridge::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine);
    Q_UNUSED(jsEngine);
    auto* bridge = instance();
    QQmlEngine::setObjectOwnership(bridge, QQmlEngine::CppOwnership);
    return bridge;
}

void HeadsetControlBridge::connectToMonitor()
{
    if (m_monitor)
        return;
    m_thread = new QThread();
    m_monitor = new HeadsetControlMonitor();
    m_monitor->moveToThread(m_thread);
    connect(m_monitor, &HeadsetControlMonitor::snapshotReady, this, &HeadsetControlBridge::applyState);
    connect(m_monitor, &HeadsetControlMonitor::headsetDataUpdated, this, &HeadsetControlBridge::headsetDataUpdated);
    connect(m_monitor, &HeadsetControlMonitor::operationErrorChanged, this, [this](const QString& error) {
        if (m_lastError == error)
            return;
        m_lastError = error;
        emit lastErrorChanged();
                });
    m_thread->start();
    QMetaObject::invokeMethod(
        m_monitor, [monitor = m_monitor] { emit monitor->snapshotReady(monitor->snapshot()); }, Qt::QueuedConnection);
    syncDesiredSettings();
    setFetchRate(UserSettings::instance()->headsetcontrolFetchRate());
    setMonitoringEnabled(UserSettings::instance()->headsetcontrolMonitoring());
        }

void HeadsetControlBridge::shutdown()
{
    if (!m_thread)
        return;
    disconnect(m_monitor, nullptr, this, nullptr);
    retireWorkerThread(m_thread, m_monitor, "stopMonitoring");
    m_thread = nullptr;
    m_monitor = nullptr;
    }

void HeadsetControlBridge::attachAudioWorker(AudioWorker* worker)
{
    connect(this, &HeadsetControlBridge::headsetDataUpdated, worker, &AudioWorker::onHeadsetDataUpdated,
            Qt::QueuedConnection);
    // Snapshot is produced on the monitor thread and delivered through this bridge.
    if (m_monitor)
        QMetaObject::invokeMethod(
            m_monitor, [monitor = m_monitor] { emit monitor->headsetDataUpdated(monitor->getCachedDevices()); },
            Qt::QueuedConnection);
}

void HeadsetControlBridge::syncDesiredSettings()
{
    if (!m_monitor)
        return;
    auto* settings = UserSettings::instance();
    const QVariantMap desired{{"lights", settings->headsetcontrolLights()},
                              {"rotateToMute", settings->headsetcontrolRotateToMute()},
                              {"voicePrompts", settings->headsetcontrolVoicePrompts()},
                              {"equalizerPreset", settings->headsetcontrolEqualizerPreset()},
                              {"sidetone", settings->headsetcontrolSidetone()},
                              {"inactiveTime", settings->headsetcontrolInactiveTime()}};
    QMetaObject::invokeMethod(
        m_monitor, [monitor = m_monitor, desired] { monitor->setDesiredSettings(desired); }, Qt::QueuedConnection);
}

HeadsetControlMonitor* HeadsetControlBridge::findMonitor() const
{
    return m_monitor;
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
    UserSettings::instance()->setHeadsetcontrolLights(enabled);
}

void HeadsetControlBridge::setRotateToMute(bool enabled)
{
    UserSettings::instance()->setHeadsetcontrolRotateToMute(enabled);
}

void HeadsetControlBridge::setVoicePrompts(bool enabled)
{
    UserSettings::instance()->setHeadsetcontrolVoicePrompts(enabled);
}

void HeadsetControlBridge::setEqualizerPreset(int preset)
{
    UserSettings::instance()->setHeadsetcontrolEqualizerPreset(preset);
}

void HeadsetControlBridge::setSidetone(int value)
{
    UserSettings::instance()->setHeadsetcontrolSidetone(value);
}

void HeadsetControlBridge::setInactiveTime(int value)
{
    UserSettings::instance()->setHeadsetcontrolInactiveTime(value);
}

void HeadsetControlBridge::refreshNow()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "requestRefresh", Qt::QueuedConnection);
    }
}

bool HeadsetControlBridge::hasSidetoneCapability() const
{
    return m_cachedState.hasSidetoneCapability;
}

bool HeadsetControlBridge::hasLightsCapability() const
{
    return m_cachedState.hasLightsCapability;
}

bool HeadsetControlBridge::hasRotateToMuteCapability() const
{
    return m_cachedState.hasRotateToMuteCapability;
}

bool HeadsetControlBridge::hasChatMixCapability() const
{
    return m_cachedState.hasChatMixCapability;
}

bool HeadsetControlBridge::hasVoicePromptsCapability() const
{
    return m_cachedState.hasVoicePromptsCapability;
}

bool HeadsetControlBridge::hasEqualizerPresetsCapability() const
{
    return m_cachedState.hasEqualizerPresetsCapability;
}

bool HeadsetControlBridge::hasInactiveTimeCapability() const
{
    return m_cachedState.hasInactiveTimeCapability;
}

QString HeadsetControlBridge::deviceName() const
{
    return m_cachedState.deviceName;
}

QString HeadsetControlBridge::batteryStatus() const
{
    return m_cachedState.batteryStatus;
}

int HeadsetControlBridge::batteryLevel() const
{
    return m_cachedState.batteryLevel;
}

QString HeadsetControlBridge::batteryIcon() const
{
    const int level = batteryLevel();
    const QString status = batteryStatus();
    const int lowBatteryThreshold = UserSettings::instance()->headsetcontrolLowBatteryThreshold();

    if (status == "BATTERY_UNAVAILABLE" || level < 0) {
        return QString::fromUtf8("❌");
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
    return m_cachedState.chatMix;
}

QStringList HeadsetControlBridge::equalizerPresetNames() const
{
    return m_cachedState.equalizerPresetNames;
}

bool HeadsetControlBridge::anyDeviceFound() const
{
    return m_cachedState.anyDeviceFound;
}

bool HeadsetControlBridge::testModeEnabled() const
{
    return m_cachedState.testModeEnabled;
}

int HeadsetControlBridge::testProfile() const
{
    return m_cachedState.testProfile;
}

void HeadsetControlBridge::updateLowBatteryNotificationState()
{
    const int level = batteryLevel();
    const int lowBatteryThreshold = UserSettings::instance()->headsetcontrolLowBatteryThreshold();

    if (batteryStatus() != "BATTERY_AVAILABLE" || level < 0 || level > lowBatteryThreshold ||
        !UserSettings::instance()->enableNotifications())
    {
        m_lowBatteryNotificationSent = false;
        return;
    }

    if (!m_lowBatteryNotificationSent) {
        emit lowHeadsetBattery();
        m_lowBatteryNotificationSent = true;
    }
}

void HeadsetControlBridge::applyState(const HeadsetControlState& state)
{
    const HeadsetControlState previous = m_cachedState;
    m_cachedState = state;

    const bool capabilitiesChangedNow =
        previous.hasSidetoneCapability != m_cachedState.hasSidetoneCapability ||
        previous.hasLightsCapability != m_cachedState.hasLightsCapability ||
        previous.hasRotateToMuteCapability != m_cachedState.hasRotateToMuteCapability ||
        previous.hasChatMixCapability != m_cachedState.hasChatMixCapability ||
        previous.hasVoicePromptsCapability != m_cachedState.hasVoicePromptsCapability ||
        previous.hasEqualizerPresetsCapability != m_cachedState.hasEqualizerPresetsCapability ||
        previous.hasInactiveTimeCapability != m_cachedState.hasInactiveTimeCapability;

    if (capabilitiesChangedNow)
    {
        emit capabilitiesChanged();
    }
    if (previous.deviceName != m_cachedState.deviceName)
    {
        emit deviceNameChanged();
    }
    if (previous.batteryStatus != m_cachedState.batteryStatus)
    {
        emit batteryStatusChanged();
        emit batteryIconChanged();
    }
    if (previous.batteryLevel != m_cachedState.batteryLevel)
    {
        emit batteryLevelChanged();
        emit batteryIconChanged();
    }
    if (previous.chatMix != m_cachedState.chatMix)
    {
        emit chatMixChanged();
    }
    if (previous.equalizerPresetNames != m_cachedState.equalizerPresetNames)
    {
        emit equalizerPresetNamesChanged();
    }
    if (previous.anyDeviceFound != m_cachedState.anyDeviceFound)
    {
        if (!m_cachedState.anyDeviceFound)
        {
            m_lowBatteryNotificationSent = false;
        }
        emit anyDeviceFoundChanged();
    }
    if (previous.testModeEnabled != m_cachedState.testModeEnabled)
    {
        emit testModeEnabledChanged();
    }
    if (previous.testProfile != m_cachedState.testProfile)
    {
        emit testProfileChanged();
    }
    if (previous.deviceName != state.deviceName)
        m_lowBatteryNotificationSent = false;
    updateLowBatteryNotificationState();
}

void HeadsetControlBridge::setFetchRate(int seconds)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        int intervalMs = qBound(60, seconds, 86400) * 1000;
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
