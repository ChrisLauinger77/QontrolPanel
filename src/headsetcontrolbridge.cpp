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
        connect(monitor, &HeadsetControlMonitor::equalizerPresetNamesChanged,
            this, &HeadsetControlBridge::onMonitorEqualizerPresetNamesChanged);
        connect(monitor, &HeadsetControlMonitor::anyDeviceFoundChanged,
                this, &HeadsetControlBridge::onMonitorAnyDeviceFoundChanged);
        connect(monitor, &HeadsetControlMonitor::testModeEnabledChanged,
            this, &HeadsetControlBridge::onMonitorTestModeEnabledChanged);
        connect(monitor, &HeadsetControlMonitor::testProfileChanged,
            this, &HeadsetControlBridge::onMonitorTestProfileChanged);
        connect(monitor, &HeadsetControlMonitor::headsetDataUpdated,
                this, [this](const QList<HeadsetControlDevice>&) {
                    HeadsetControlMonitor* monitor = findMonitor();
                    if (monitor) {
                        queueCacheRefresh(monitor);
                    }
                });

        int fetchRateSeconds = UserSettings::instance()->headsetcontrolFetchRate();
        int fetchRateMs = fetchRateSeconds * 1000;
        QMetaObject::invokeMethod(monitor, "setFetchInterval", Qt::QueuedConnection,
                                  Q_ARG(int, fetchRateMs));

        if (UserSettings::instance()->headsetcontrolMonitoring()) {
            QMetaObject::invokeMethod(monitor, "startMonitoring", Qt::QueuedConnection);
        }

        queueCacheRefresh(monitor);
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

void HeadsetControlBridge::setEqualizerPreset(int preset)
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        QMetaObject::invokeMethod(monitor, "setEqualizerPreset", Qt::QueuedConnection,
                                  Q_ARG(int, preset));
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

void HeadsetControlBridge::onMonitorCapabilitiesChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorDeviceNameChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorBatteryStatusChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorBatteryLevelChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorChatMixChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorEqualizerPresetNamesChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorAnyDeviceFoundChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
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
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::onMonitorTestProfileChanged()
{
    HeadsetControlMonitor* monitor = findMonitor();
    if (monitor) {
        queueCacheRefresh(monitor);
        return;
    }
    resetCachedStateToDefaults();
}

void HeadsetControlBridge::queueCacheRefresh(HeadsetControlMonitor* monitor)
{
    if (!monitor) {
        return;
    }

    QMetaObject::invokeMethod(
        monitor,
        [this, monitor]() {
            CachedState state;
            state.hasSidetoneCapability = monitor->hasSidetoneCapability();
            state.hasLightsCapability = monitor->hasLightsCapability();
            state.hasRotateToMuteCapability = monitor->hasRotateToMuteCapability();
            state.hasChatMixCapability = monitor->hasChatMixCapability();
            state.hasVoicePromptsCapability = monitor->hasVoicePromptsCapability();
            state.hasEqualizerPresetsCapability = monitor->hasEqualizerPresetsCapability();
            state.hasInactiveTimeCapability = monitor->hasInactiveTimeCapability();
            state.deviceName = monitor->deviceName();
            state.batteryStatus = monitor->batteryStatus();
            state.batteryLevel = monitor->batteryLevel();
            state.chatMix = monitor->chatMix();
            state.equalizerPresetNames = monitor->equalizerPresetNames();
            state.anyDeviceFound = monitor->anyDeviceFound();
            state.testModeEnabled = monitor->testModeEnabled();
            state.testProfile = monitor->testProfile();

            QMetaObject::invokeMethod(this, [this, state]() {
                const CachedState previous = m_cachedState;
                m_cachedState = state;

                const bool capabilitiesChangedNow =
                    previous.hasSidetoneCapability != m_cachedState.hasSidetoneCapability ||
                    previous.hasLightsCapability != m_cachedState.hasLightsCapability ||
                    previous.hasRotateToMuteCapability != m_cachedState.hasRotateToMuteCapability ||
                    previous.hasChatMixCapability != m_cachedState.hasChatMixCapability ||
                    previous.hasVoicePromptsCapability != m_cachedState.hasVoicePromptsCapability ||
                    previous.hasEqualizerPresetsCapability != m_cachedState.hasEqualizerPresetsCapability ||
                    previous.hasInactiveTimeCapability != m_cachedState.hasInactiveTimeCapability;

                if (capabilitiesChangedNow) {
                    emit capabilitiesChanged();
                }
                if (previous.deviceName != m_cachedState.deviceName) {
                    emit deviceNameChanged();
                }
                if (previous.batteryStatus != m_cachedState.batteryStatus) {
                    emit batteryStatusChanged();
                    emit batteryIconChanged();
                }
                if (previous.batteryLevel != m_cachedState.batteryLevel) {
                    updateLowBatteryNotificationState();
                    emit batteryLevelChanged();
                    emit batteryIconChanged();
                }
                if (previous.chatMix != m_cachedState.chatMix) {
                    emit chatMixChanged();
                }
                if (previous.equalizerPresetNames != m_cachedState.equalizerPresetNames) {
                    emit equalizerPresetNamesChanged();
                }
                if (previous.anyDeviceFound != m_cachedState.anyDeviceFound) {
                    if (!m_cachedState.anyDeviceFound) {
                        m_lowBatteryNotificationSent = false;
                    }
                    emit anyDeviceFoundChanged();
                }
                if (previous.testModeEnabled != m_cachedState.testModeEnabled) {
                    emit testModeEnabledChanged();
                }
                if (previous.testProfile != m_cachedState.testProfile) {
                    emit testProfileChanged();
                }
            }, Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void HeadsetControlBridge::resetCachedStateToDefaults()
{
    const CachedState previous = m_cachedState;
    m_cachedState = CachedState{};

    const bool capabilitiesChangedNow =
        previous.hasSidetoneCapability != m_cachedState.hasSidetoneCapability ||
        previous.hasLightsCapability != m_cachedState.hasLightsCapability ||
        previous.hasRotateToMuteCapability != m_cachedState.hasRotateToMuteCapability ||
        previous.hasChatMixCapability != m_cachedState.hasChatMixCapability ||
        previous.hasVoicePromptsCapability != m_cachedState.hasVoicePromptsCapability ||
        previous.hasEqualizerPresetsCapability != m_cachedState.hasEqualizerPresetsCapability ||
        previous.hasInactiveTimeCapability != m_cachedState.hasInactiveTimeCapability;

    if (capabilitiesChangedNow) {
        emit capabilitiesChanged();
    }
    if (previous.deviceName != m_cachedState.deviceName) {
        emit deviceNameChanged();
    }
    if (previous.batteryStatus != m_cachedState.batteryStatus) {
        emit batteryStatusChanged();
        emit batteryIconChanged();
    }
    if (previous.batteryLevel != m_cachedState.batteryLevel) {
        updateLowBatteryNotificationState();
        emit batteryLevelChanged();
        emit batteryIconChanged();
    }
    if (previous.chatMix != m_cachedState.chatMix) {
        emit chatMixChanged();
    }
    if (previous.equalizerPresetNames != m_cachedState.equalizerPresetNames) {
        emit equalizerPresetNamesChanged();
    }
    if (previous.anyDeviceFound != m_cachedState.anyDeviceFound) {
        if (!m_cachedState.anyDeviceFound) {
            m_lowBatteryNotificationSent = false;
        }
        emit anyDeviceFoundChanged();
    }
    if (previous.testModeEnabled != m_cachedState.testModeEnabled) {
        emit testModeEnabledChanged();
    }
    if (previous.testProfile != m_cachedState.testProfile) {
        emit testProfileChanged();
    }
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
