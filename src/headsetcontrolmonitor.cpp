#include "headsetcontrolmonitor.h"
#include "logmanager.h"

namespace
{
    constexpr int kDisconnectedFetchIntervalMs = 60000;
    constexpr int kMinimumFetchIntervalMs = 60000;
}

HeadsetControlMonitor::HeadsetControlMonitor(QObject *parent)
    : QObject(parent), m_fetchTimer(new QTimer(this)), m_isMonitoring(false), m_fetchIntervalMs(30000), m_hasSidetoneCapability(false), m_hasLightsCapability(false), m_hasRotateToMuteCapability(false), m_hasChatMixCapability(false), m_hasVoicePromptsCapability(false), m_hasEqualizerPresetsCapability(false), m_hasInactiveTimeCapability(false), m_deviceName(""), m_batteryStatus("BATTERY_UNAVAILABLE"), m_batteryLevel(-1), m_chatMix(-1), m_anyDeviceFound(false), m_isFetching(false), m_activeHeadsetIndex(-1), m_activeHeadsetSettingsKey(""), m_testModeEnabled(false), m_testProfile(3)
{
    LOG_INFO("HeadsetControlManager",
             QString("HeadsetControlMonitor initialized - HeadsetControl library version: %1")
                 .arg(QString::fromStdString(std::string(headsetcontrol::version()))));

    qRegisterMetaType<HeadsetControlState>();
    qRegisterMetaType<QList<HeadsetControlDevice>>();
    m_fetchTimer->setInterval(m_fetchIntervalMs);
    m_fetchTimer->setSingleShot(true);

    connect(m_fetchTimer, &QTimer::timeout, this, &HeadsetControlMonitor::fetchHeadsetInfo);
}

HeadsetControlMonitor::~HeadsetControlMonitor()
{
    LOG_INFO("HeadsetControlManager",
             "HeadsetControlMonitor destructor called");
    stopMonitoring();
}

void HeadsetControlMonitor::startMonitoring()
{
    if (m_isMonitoring)
    {
        LOG_INFO("HeadsetControlManager",
                 "Headset monitoring already running, ignoring start request");
        return;
    }

    LOG_INFO("HeadsetControlManager",
             QString("Starting headset monitoring (fetch interval: %1ms)").arg(m_fetchIntervalMs));

    m_isMonitoring = true;
    m_lastFetchCompleted = QElapsedTimer();
    applyTestDeviceConfiguration();
    fetchHeadsetInfoInternal(true);

    emit monitoringStateChanged(true);
}

void HeadsetControlMonitor::stopMonitoring()
{
    if (!m_isMonitoring)
    {
        return;
    }

    LOG_INFO("HeadsetControlManager",
             "Stopping headset monitoring");

    m_isMonitoring = false;
    m_fetchTimer->stop();

    clearDeviceState();
    emit operationErrorChanged({});
    emit monitoringStateChanged(false);

    LOG_INFO("HeadsetControlManager",
             "Headset monitoring stopped and data cleared");
}

bool HeadsetControlMonitor::isMonitoring() const
{
    return m_isMonitoring;
}

void HeadsetControlMonitor::setLights(bool enabled)
{
    auto desired = m_desiredSettings;
    desired["lights"] = enabled;
    setDesiredSettings(desired);
}

void HeadsetControlMonitor::setRotateToMute(bool enabled)
{
    auto desired = m_desiredSettings;
    desired["rotateToMute"] = enabled;
    setDesiredSettings(desired);
}

void HeadsetControlMonitor::setVoicePrompts(bool enabled)
{
    auto desired = m_desiredSettings;
    desired["voicePrompts"] = enabled;
    setDesiredSettings(desired);
}

void HeadsetControlMonitor::setEqualizerPreset(int preset)
{
    auto desired = m_desiredSettings;
    desired["equalizerPreset"] = preset;
    setDesiredSettings(desired);
}

void HeadsetControlMonitor::setSidetone(int value)
{
    auto desired = m_desiredSettings;
    desired["sidetone"] = value;
    setDesiredSettings(desired);
}

void HeadsetControlMonitor::setInactiveTime(int value)
{
    auto desired = m_desiredSettings;
    desired["inactiveTime"] = value;
    setDesiredSettings(desired);
}

void HeadsetControlMonitor::fetchHeadsetInfo()
{
    fetchHeadsetInfoInternal(true);
}

void HeadsetControlMonitor::fetchHeadsetInfoInternal(bool bypassRecentFetch)
{
    if (!m_isMonitoring)
    {
        return;
    }

    if (m_isFetching)
    {
        LOG_INFO("HeadsetControlManager",
                 "Headset polling already active, skipping duplicate polling request");
        return;
    }

    if (!bypassRecentFetch && m_lastFetchCompleted.isValid() && m_lastFetchCompleted.elapsed() < kMinimumFetchIntervalMs)
    {
        LOG_INFO("HeadsetControlManager",
                 "Headset polling completed during the last 60 seconds, skipping polling request");
        return;
    }

    m_isFetching = true;
    m_fetchTimer->stop();
    LOG_INFO("HeadsetControlManager",
             "Starting headset polling");

    try
    {
        applyTestDeviceConfiguration();

        // Discover headsets
        QElapsedTimer pollTimer;
        pollTimer.start();

        LOG_INFO("HeadsetControlManager", "Starting headset discovery");
        auto discoveredHeadsets = headsetcontrol::discover();
        LOG_INFO("HeadsetControlManager",
                 QString("Headset discovery returned after %1ms").arg(pollTimer.elapsed()));

        m_headsets = std::move(discoveredHeadsets);
        LOG_INFO("HeadsetControlManager",
                 QString("Headset list replacement completed after %1ms").arg(pollTimer.elapsed()));

        if (m_headsets.empty())
        {
            LOG_INFO("HeadsetControlManager",
                     "No headset devices found");

            clearDeviceState();
            updateFetchTimerInterval(false);
            m_lastFetchCompleted.start();
            m_isFetching = false;
            scheduleNextFetch();
            return;
        }

        LOG_INFO("HeadsetControlManager",
                 QString("Found %1 headset device(s)").arg(m_headsets.size()));

        updateDeviceCache();
        updateFetchTimerInterval(m_anyDeviceFound);
    }
    catch (const std::exception &e)
    {
        LOG_CRITICAL("HeadsetControlManager",
                     QString("Exception during headset discovery: %1").arg(e.what()));
        clearDeviceState();
        emit operationErrorChanged(QString::fromUtf8(e.what()));
    }

    m_lastFetchCompleted.start();
    m_isFetching = false;
    scheduleNextFetch();
}

void HeadsetControlMonitor::requestRefresh()
{
    m_settingAttempts.clear();
    applyPendingSettings();
    fetchHeadsetInfoInternal(shouldBypassRecentFetchForManualRequest());
}

bool HeadsetControlMonitor::shouldBypassRecentFetchForManualRequest() const
{
    return m_testModeEnabled || !m_anyDeviceFound || m_batteryLevel < 0 || m_batteryStatus == "BATTERY_UNAVAILABLE" || m_batteryStatus == "BATTERY_HIDERROR" || m_batteryStatus == "BATTERY_TIMEOUT";
}

void HeadsetControlMonitor::scheduleNextFetch()
{
    if (m_isMonitoring)
    {
        m_fetchTimer->start();
    }
}

void HeadsetControlMonitor::updateDeviceCache()
{
    QList<HeadsetControlDevice> newDevices;
    int activeHeadsetIndex = -1;

    for (int headsetIndex = 0; headsetIndex < static_cast<int>(m_headsets.size()); ++headsetIndex)
    {
        headsetcontrol::Headset &headset = m_headsets[headsetIndex];
        HeadsetControlDevice device;
        bool deviceReachable = true;

        device.deviceName = QString::fromStdString(std::string(headset.name()));
        device.vendorId = QString("0x%1").arg(headset.vendorId(), 4, 16, QChar('0')).toUpper();
        device.productId = QString("0x%1").arg(headset.productId(), 4, 16, QChar('0')).toUpper();
        device.vendor = device.vendorId; // Could be enhanced with vendor name lookup
        device.product = device.productId;

        device.capabilities = getCapabilityList(headset);

        // Query battery if supported
        if (headset.supports(CAP_BATTERY_STATUS))
        {
            headsetcontrol::Result<headsetcontrol::BatteryResult> batteryResult = headset.getBattery();
            if (batteryResult)
            {
                device.batteryLevel = batteryResult->level_percent;
                device.batteryStatus = batteryStatusToString(batteryResult->status);

                LOG_INFO("HeadsetControlManager",
                         QString("Device %1: Battery %2 at %3%")
                             .arg(device.deviceName, device.batteryStatus)
                             .arg(device.batteryLevel));
            }
            else
            {
                device.batteryLevel = -1;
                device.batteryStatus = batteryStatusFromError(batteryResult.error());
                deviceReachable = false;
                LOG_WARN("HeadsetControlManager",
                         QString("Failed to read battery: %1")
                             .arg(QString::fromStdString(batteryResult.error().fullMessage())));
            }
        }
        else
        {
            device.batteryStatus = "BATTERY_UNAVAILABLE";
            device.batteryLevel = -1;
        }

        if (deviceReachable && headset.supports(CAP_CHATMIX_STATUS))
        {
            headsetcontrol::Result<headsetcontrol::ChatmixResult> chatMixResult = headset.getChatmix();
            if (chatMixResult)
            {
                device.chatMix = chatMixResult->level;

                LOG_INFO("HeadsetControlManager",
                         QString("Device %1: ChatMix %2")
                             .arg(device.deviceName)
                             .arg(device.chatMix));
            }
            else
            {
                device.chatMix = -1;
                LOG_WARN("HeadsetControlManager",
                         QString("Failed to read ChatMix: %1")
                             .arg(QString::fromStdString(chatMixResult.error().fullMessage())));
            }
        }
        else
        {
            device.chatMix = -1;
        }

        LOG_INFO("HeadsetControlManager",
                 QString("Found headset device: %1 with %2 capabilities")
                     .arg(device.deviceName)
                     .arg(device.capabilities.size()));

        if (deviceReachable && activeHeadsetIndex < 0)
        {
            activeHeadsetIndex = headsetIndex;
        }
        else if (!deviceReachable)
        {
            LOG_INFO("HeadsetControlManager",
                     QString("Headset device %1 is not reachable; saved settings will not be applied until it responds")
                         .arg(device.deviceName));
        }

        newDevices.append(device);
    }

    m_cachedDevices = newDevices;
    m_activeHeadsetIndex = activeHeadsetIndex;
    updateCapabilities();

    // Update bridge battery info from the first device
    const int primaryDeviceIndex = m_activeHeadsetIndex >= 0 ? m_activeHeadsetIndex : 0;
    if (primaryDeviceIndex >= 0 && primaryDeviceIndex < m_cachedDevices.size())
    {
        const HeadsetControlDevice &primaryDevice = m_cachedDevices.at(primaryDeviceIndex);
        if (primaryDevice.batteryStatus != m_batteryStatus)
        {
            m_batteryStatus = primaryDevice.batteryStatus;
            emit batteryStatusChanged();
        }
        if (primaryDevice.batteryLevel != m_batteryLevel)
        {
            m_batteryLevel = primaryDevice.batteryLevel;
            emit batteryLevelChanged();
        }
        if (primaryDevice.chatMix != m_chatMix)
        {
            m_chatMix = primaryDevice.chatMix;
            emit chatMixChanged();
        }
    }

    emit snapshotReady(snapshot());
    emit headsetDataUpdated(m_cachedDevices);
}

headsetcontrol::Headset *HeadsetControlMonitor::activeHeadset()
{
    if (m_activeHeadsetIndex < 0 || m_activeHeadsetIndex >= static_cast<int>(m_headsets.size()))
    {
        return nullptr;
    }

    return &m_headsets[m_activeHeadsetIndex];
}

QString HeadsetControlMonitor::activeHeadsetSettingsKey(const headsetcontrol::Headset &headset) const
{
    return QString("%1:%2:%3")
        .arg(headset.vendorId(), 4, 16, QChar('0'))
        .arg(headset.productId(), 4, 16, QChar('0'))
        .arg(QString::fromStdString(std::string(headset.name())));
}

void HeadsetControlMonitor::updateCapabilities()
{
    bool newSidetoneCapability = false;
    bool newLightsCapability = false;
    bool newRotateToMuteCapability = false;
    bool newChatMixCapability = false;
    bool newVoicePromptsCapability = false;
    bool newEqualizerPresetsCapability = false;
    bool newInactivetimeCapability = false;
    QString newDeviceName = "";
    QString newActiveHeadsetSettingsKey = "";
    QStringList newEqualizerPresetNames;
    bool newAnyDeviceFound = m_activeHeadsetIndex >= 0;

    if (headsetcontrol::Headset *active = activeHeadset())
    {
        const headsetcontrol::Headset &headset = *active;
        newDeviceName = QString::fromStdString(std::string(headset.name()));
        newActiveHeadsetSettingsKey = activeHeadsetSettingsKey(headset);

        newSidetoneCapability = headset.supports(CAP_SIDETONE);
        newLightsCapability = headset.supports(CAP_LIGHTS);
        newRotateToMuteCapability = headset.supports(CAP_ROTATE_TO_MUTE);
        newChatMixCapability = headset.supports(CAP_CHATMIX_STATUS);
        newVoicePromptsCapability = headset.supports(CAP_VOICE_PROMPTS);
        newEqualizerPresetsCapability = headset.supports(CAP_EQUALIZER_PRESET);
        newInactivetimeCapability = headset.supports(CAP_INACTIVE_TIME);

        if (newEqualizerPresetsCapability)
        {
            if (const auto presets = headset.getEqualizerPresets())
            {
                for (const auto &preset : presets->presets)
                {
                    newEqualizerPresetNames.append(QString::fromStdString(preset.name));
                }
            }

            if (newEqualizerPresetNames.isEmpty())
            {
                const int presetCount = headset.getEqualizerPresetsCount();
                for (int presetIndex = 0; presetIndex < presetCount; ++presetIndex)
                {
                    newEqualizerPresetNames.append(QString("Preset %1").arg(presetIndex + 1));
                }
            }
        }

        LOG_INFO("HeadsetControlManager",
                 QString("Device capabilities: %1")
                     .arg(getCapabilityList(headset).join(", ")));
    }

    if (newSidetoneCapability != m_hasSidetoneCapability ||
        newLightsCapability != m_hasLightsCapability ||
        newRotateToMuteCapability != m_hasRotateToMuteCapability ||
        newChatMixCapability != m_hasChatMixCapability ||
        newVoicePromptsCapability != m_hasVoicePromptsCapability ||
        newEqualizerPresetsCapability != m_hasEqualizerPresetsCapability ||
        newInactivetimeCapability != m_hasInactiveTimeCapability)
    {
        m_hasSidetoneCapability = newSidetoneCapability;
        m_hasLightsCapability = newLightsCapability;
        m_hasRotateToMuteCapability = newRotateToMuteCapability;
        m_hasChatMixCapability = newChatMixCapability;
        m_hasVoicePromptsCapability = newVoicePromptsCapability;
        m_hasEqualizerPresetsCapability = newEqualizerPresetsCapability;
        m_hasInactiveTimeCapability = newInactivetimeCapability;
        emit capabilitiesChanged();
    }

    if (newEqualizerPresetNames != m_equalizerPresetNames)
    {
        m_equalizerPresetNames = newEqualizerPresetNames;
        emit equalizerPresetNamesChanged();
    }

    if (newDeviceName != m_deviceName)
    {
        m_deviceName = newDeviceName;
        emit deviceNameChanged();
    }

    if (newAnyDeviceFound != m_anyDeviceFound)
    {
        m_anyDeviceFound = newAnyDeviceFound;
        emit anyDeviceFoundChanged();
    }

    if (!newAnyDeviceFound)
    {
        m_activeHeadsetSettingsKey = "";
        m_appliedSettings.clear();
        m_settingAttempts.clear();
        return;
    }

    if (newActiveHeadsetSettingsKey != m_activeHeadsetSettingsKey)
    {
        m_activeHeadsetSettingsKey = newActiveHeadsetSettingsKey;
        m_appliedSettings.clear();
        m_settingAttempts.clear();
    }
    applyPendingSettings();
}

void HeadsetControlMonitor::setDesiredSettings(const QVariantMap& settings)
{
    for (auto it = settings.cbegin(); it != settings.cend(); ++it)
    {
        if (m_desiredSettings.value(it.key()) != it.value())
            m_settingAttempts.remove(it.key());
    }
    m_desiredSettings = settings;
    applyPendingSettings();
}

void HeadsetControlMonitor::applyPendingSettings()
        {
    auto* headset = activeHeadset();
    if (!headset || !m_isMonitoring)
        return;
    QStringList errors;
    auto apply = [&](const QString& key, bool supported, auto writeValue) {
        if (!supported || !m_desiredSettings.contains(key))
            return;
        const QVariant desired = m_desiredSettings.value(key);
        if (m_appliedSettings.value(key) == desired)
            return;
        if (m_settingAttempts.value(key) >= 3)
        {
            errors.append(key + ": retry limit reached; refresh to retry");
            return;
        }
        ++m_settingAttempts[key];
        try
        {
            const auto result = writeValue(desired);
            if (result)
            {
                m_appliedSettings[key] = desired;
                m_settingAttempts.remove(key);
        }
            else
        {
                errors.append(key + ": " + QString::fromStdString(result.error().fullMessage()));
            }
        }
        catch (const std::exception& error)
        {
            errors.append(key + ": " + QString::fromUtf8(error.what()));
    }
    };
    apply("lights", m_hasLightsCapability, [&](const QVariant& v) { return headset->setLights(v.toBool()); });
    apply("rotateToMute", m_hasRotateToMuteCapability,
          [&](const QVariant& v) { return headset->setRotateToMute(v.toBool()); });
    apply("voicePrompts", m_hasVoicePromptsCapability,
          [&](const QVariant& v) { return headset->setVoicePrompts(v.toBool()); });
    apply("sidetone", m_hasSidetoneCapability,
          [&](const QVariant& v) { return headset->setSidetone(static_cast<uint8_t>(qBound(0, v.toInt(), 128))); });
    apply("equalizerPreset", m_hasEqualizerPresetsCapability && !m_equalizerPresetNames.isEmpty(),
          [&](const QVariant& v) {
              return headset->setEqualizerPreset(
                  static_cast<uint8_t>(qBound(0, v.toInt(), int(m_equalizerPresetNames.size()) - 1)));
          });
    apply("inactiveTime", m_hasInactiveTimeCapability && m_desiredSettings.value("inactiveTime", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setInactiveTime(static_cast<uint8_t>(qBound(0, v.toInt(), 255))); });
    const QString error = errors.join("\n");
    if (!error.isEmpty())
        LOG_WARN("HeadsetControlManager", error);
    emit operationErrorChanged(error);
}

HeadsetControlState HeadsetControlMonitor::snapshot() const
{
    HeadsetControlState state;
    state.hasSidetoneCapability = hasSidetoneCapability();
    state.hasLightsCapability = hasLightsCapability();
    state.hasRotateToMuteCapability = hasRotateToMuteCapability();
    state.hasChatMixCapability = hasChatMixCapability();
    state.hasVoicePromptsCapability = hasVoicePromptsCapability();
    state.hasEqualizerPresetsCapability = hasEqualizerPresetsCapability();
    state.hasInactiveTimeCapability = hasInactiveTimeCapability();
    state.deviceName = deviceName();
    state.batteryStatus = batteryStatus();
    state.batteryLevel = batteryLevel();
    state.chatMix = chatMix();
    state.equalizerPresetNames = equalizerPresetNames();
    state.anyDeviceFound = anyDeviceFound();
    state.testModeEnabled = testModeEnabled();
    state.testProfile = testProfile();
    return state;
}

QString HeadsetControlMonitor::batteryStatusToString(battery_status status) const
{
    switch (status)
    {
    case BATTERY_AVAILABLE:
        return "BATTERY_AVAILABLE";
    case BATTERY_CHARGING:
        return "BATTERY_CHARGING";
    case BATTERY_UNAVAILABLE:
        return "BATTERY_UNAVAILABLE";
    case BATTERY_HIDERROR:
        return "BATTERY_HIDERROR";
    case BATTERY_TIMEOUT:
        return "BATTERY_TIMEOUT";
    default:
        return "BATTERY_UNAVAILABLE";
    }
}

QString HeadsetControlMonitor::batteryStatusFromError(const headsetcontrol::DeviceError &error) const
{
    switch (error.code)
    {
    case headsetcontrol::DeviceError::Code::Timeout:
        return "BATTERY_TIMEOUT";
    case headsetcontrol::DeviceError::Code::DeviceOffline:
        return "BATTERY_UNAVAILABLE";
    case headsetcontrol::DeviceError::Code::HIDError:
    case headsetcontrol::DeviceError::Code::USBError:
        return "BATTERY_HIDERROR";
    default:
        return "BATTERY_UNAVAILABLE";
    }
}

QStringList HeadsetControlMonitor::getCapabilityList(const headsetcontrol::Headset &headset) const
{
    QStringList capabilities;

    // Check all known capabilities
    for (int cap = 0; cap < NUM_CAPABILITIES; ++cap)
    {
        if (headset.supports(static_cast<enum capabilities>(cap)))
        {
            const char *capName = capability_to_enum_string(static_cast<enum capabilities>(cap));
            capabilities << QString::fromUtf8(capName);
        }
    }

    return capabilities;
}

void HeadsetControlMonitor::setFetchInterval(int intervalMs)
{
    m_fetchIntervalMs = qMax(kMinimumFetchIntervalMs, intervalMs);
    updateFetchTimerInterval(m_anyDeviceFound);

    LOG_INFO("HeadsetControlManager",
             QString("Fetch interval updated to %1ms").arg(m_fetchIntervalMs));
}

void HeadsetControlMonitor::updateFetchTimerInterval(bool deviceFound)
{
    const int effectiveInterval = deviceFound || m_testModeEnabled
                                      ? m_fetchIntervalMs
                                      : qMax(m_fetchIntervalMs, kDisconnectedFetchIntervalMs);

    if (m_fetchTimer->interval() != effectiveInterval)
    {
        m_fetchTimer->setInterval(effectiveInterval);
    }
}

void HeadsetControlMonitor::setTestModeEnabled(bool enabled)
{
    if (m_testModeEnabled == enabled)
    {
        return;
    }

    m_testModeEnabled = enabled;
    applyTestDeviceConfiguration();
    emit testModeEnabledChanged();
    emit snapshotReady(snapshot());

    if (m_isMonitoring)
    {
        fetchHeadsetInfoInternal(true);
    }
}

void HeadsetControlMonitor::setTestProfile(int profile)
{
    int sanitizedProfile = qBound(1, profile, 7);
    if (m_testProfile == sanitizedProfile)
    {
        return;
    }

    m_testProfile = sanitizedProfile;
    applyTestDeviceConfiguration();
    emit testProfileChanged();
    emit snapshotReady(snapshot());

    if (m_testModeEnabled && m_isMonitoring)
    {
        fetchHeadsetInfoInternal(true);
    }
}

void HeadsetControlMonitor::applyTestDeviceConfiguration()
{
    headsetcontrol::enableTestDevice(m_testModeEnabled);
    if (m_testModeEnabled)
    {
        headsetcontrol::setTestProfile(m_testProfile);
    }
}

void HeadsetControlMonitor::clearDeviceState()
{
    m_cachedDevices.clear();
    m_headsets.clear();
    m_hasSidetoneCapability = false;
    m_hasLightsCapability = false;
    m_hasRotateToMuteCapability = false;
    m_hasChatMixCapability = false;
    m_hasVoicePromptsCapability = false;
    m_hasEqualizerPresetsCapability = false;
    m_hasInactiveTimeCapability = false;
    m_deviceName = "";
    m_batteryStatus = "BATTERY_UNAVAILABLE";
    m_batteryLevel = -1;
    m_chatMix = -1;
    m_activeHeadsetIndex = -1;
    m_activeHeadsetSettingsKey = "";
    if (!m_equalizerPresetNames.isEmpty())
    {
        m_equalizerPresetNames.clear();
        emit equalizerPresetNamesChanged();
    }
    bool wasDeviceFound = m_anyDeviceFound;
    m_anyDeviceFound = false;

    emit capabilitiesChanged();
    emit deviceNameChanged();
    emit batteryStatusChanged();
    emit batteryLevelChanged();
    emit chatMixChanged();
    if (wasDeviceFound)
    {
        emit anyDeviceFoundChanged();
    }

    emit snapshotReady(snapshot());
    emit headsetDataUpdated(m_cachedDevices);
    m_appliedSettings.clear();
    m_settingAttempts.clear();
}
