#include "headsetcontrolmonitor.h"
#include "logmanager.h"
#include "sidetonepolicy.h"

namespace
{
    constexpr int kDisconnectedFetchIntervalMs = 60000;
    constexpr int kMinimumFetchIntervalMs = 60000;
}

HeadsetControlMonitor::HeadsetControlMonitor(QObject *parent, HeadsetDiscovery discover)
    : QObject(parent), m_discoverHeadsets(std::move(discover)), m_fetchTimer(new QTimer(this)), m_isMonitoring(false), m_fetchIntervalMs(30000), m_hasSidetoneCapability(false), m_hasLightsCapability(false), m_hasRotateToMuteCapability(false), m_hasChatMixCapability(false), m_hasVoicePromptsCapability(false), m_hasEqualizerPresetsCapability(false), m_hasInactiveTimeCapability(false), m_deviceName(""), m_batteryStatus("BATTERY_UNAVAILABLE"), m_batteryLevel(-1), m_chatMix(-1), m_anyDeviceFound(false), m_isFetching(false), m_activeHeadsetIndex(-1), m_activeHeadsetSettingsKey(""), m_testModeEnabled(false), m_testProfile(3)
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

void HeadsetControlMonitor::setSidetone(int value, quint64 requestRevision)
{
    m_settingAttempts.remove("sidetone");
    m_sidetoneWriteRequested = true;
    m_sidetoneRequestRevision = requestRevision;
    setDesiredSettings({{"sidetone", qBound(0, value, 128)}});
    // Acknowledge completion even for failed/skipped writes so optimistic UI can settle.
    emit snapshotReady(snapshot());
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
        auto discoveredHeadsets = m_discoverHeadsets();
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
    const bool retryPending = !m_settingAttempts.isEmpty();
    m_settingAttempts.clear();
    // Read before retrying writes so an external sidetone change can be adopted.
    fetchHeadsetInfoInternal(retryPending || shouldBypassRecentFetchForManualRequest());
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
                // Keep the synthetic error profile selectable so its failing writes and
                // bounded retries can be exercised through the settings pane.
                deviceReachable = m_testModeEnabled && headset.vendorId() == 0xF00B && headset.productId() == 0xA00C;
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

        if (deviceReachable && headset.supports(CAP_SIDETONE_STATUS))
        {
            const auto sidetone = headset.getSidetone();
            if (sidetone && sidetone->current_level <= 128)
                device.sidetoneLevel = sidetone->current_level;
            else
                LOG_WARN("HeadsetControlManager", sidetone
                    ? QString("Invalid sidetone level: %1").arg(sidetone->current_level)
                    : QString("Failed to read sidetone: %1").arg(QString::fromStdString(sidetone.error().fullMessage())));
        }

        // Opening a device can refine its product name (e.g. a paired Jabra headset).
        // Read the names after the battery and ChatMix queries have opened it.
        const auto vendorName = headset.vendorName();
        const auto productName = headset.productName();
        device.vendor = vendorName.empty() ? device.vendorId
            : QString::fromUtf8(vendorName.data(), static_cast<qsizetype>(vendorName.size()));
        device.product = productName.empty() ? device.productId
            : QString::fromUtf8(productName.data(), static_cast<qsizetype>(productName.size()));

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
    updateSidetoneFromDevice();
    applyPendingSettings();

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
    bool newBluetoothCallVolumeCapability = false;
    bool newBluetoothWhenPoweredOnCapability = false;
    bool newMicMuteLedBrightnessCapability = false;
    bool newVolumeLimiterCapability = false;
    bool newNoiseFilterCapability = false;
    bool newMicVolumeCapability = false;
    QString newDeviceName = "";
    QString newDisplayName;
    QString newActiveHeadsetSettingsKey = "";
    QStringList newEqualizerPresetNames;
    bool newAnyDeviceFound = m_activeHeadsetIndex >= 0;

    if (headsetcontrol::Headset *active = activeHeadset())
    {
        const headsetcontrol::Headset &headset = *active;
        newDeviceName = QString::fromStdString(std::string(headset.name()));
        const auto productName = headset.productName();
        newDisplayName = productName.empty() ? newDeviceName
            : QString::fromUtf8(productName.data(), static_cast<qsizetype>(productName.size()));
        newActiveHeadsetSettingsKey = activeHeadsetSettingsKey(headset);

        newSidetoneCapability = headset.supports(CAP_SIDETONE);
        newLightsCapability = headset.supports(CAP_LIGHTS);
        newRotateToMuteCapability = headset.supports(CAP_ROTATE_TO_MUTE);
        newChatMixCapability = headset.supports(CAP_CHATMIX_STATUS);
        newVoicePromptsCapability = headset.supports(CAP_VOICE_PROMPTS);
        newEqualizerPresetsCapability = headset.supports(CAP_EQUALIZER_PRESET);
        newInactivetimeCapability = headset.supports(CAP_INACTIVE_TIME);
        newBluetoothCallVolumeCapability = headset.supports(CAP_BT_CALL_VOLUME);
        newBluetoothWhenPoweredOnCapability = headset.supports(CAP_BT_WHEN_POWERED_ON);
        newMicMuteLedBrightnessCapability = headset.supports(CAP_MICROPHONE_MUTE_LED_BRIGHTNESS);
        newVolumeLimiterCapability = headset.supports(CAP_VOLUME_LIMITER);
        newNoiseFilterCapability = headset.supports(CAP_NOISE_FILTER);
        newMicVolumeCapability = headset.supports(CAP_MICROPHONE_VOLUME);

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
        newMicVolumeCapability != m_hasMicVolumeCapability ||
        newNoiseFilterCapability != m_hasNoiseFilterCapability ||
        newVolumeLimiterCapability != m_hasVolumeLimiterCapability ||
        newMicMuteLedBrightnessCapability != m_hasMicMuteLedBrightnessCapability ||
        newBluetoothWhenPoweredOnCapability != m_hasBluetoothWhenPoweredOnCapability ||
        newBluetoothCallVolumeCapability != m_hasBluetoothCallVolumeCapability ||
        newInactivetimeCapability != m_hasInactiveTimeCapability)
    {
        m_hasSidetoneCapability = newSidetoneCapability;
        m_hasLightsCapability = newLightsCapability;
        m_hasRotateToMuteCapability = newRotateToMuteCapability;
        m_hasChatMixCapability = newChatMixCapability;
        m_hasVoicePromptsCapability = newVoicePromptsCapability;
        m_hasEqualizerPresetsCapability = newEqualizerPresetsCapability;
        m_hasInactiveTimeCapability = newInactivetimeCapability;
        m_hasBluetoothCallVolumeCapability = newBluetoothCallVolumeCapability;
        m_hasBluetoothWhenPoweredOnCapability = newBluetoothWhenPoweredOnCapability;
        m_hasMicMuteLedBrightnessCapability = newMicMuteLedBrightnessCapability;
        m_hasVolumeLimiterCapability = newVolumeLimiterCapability;
        m_hasNoiseFilterCapability = newNoiseFilterCapability;
        m_hasMicVolumeCapability = newMicVolumeCapability;
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

    if (newDisplayName != m_displayName)
    {
        m_displayName = newDisplayName;
        emit displayNameChanged();
    }

    if (newAnyDeviceFound != m_anyDeviceFound)
    {
        m_anyDeviceFound = newAnyDeviceFound;
        emit anyDeviceFoundChanged();
    }

    if (!newAnyDeviceFound)
    {
        m_activeHeadsetSettingsKey = "";
        m_sidetoneWriteRequested = false;
        m_appliedSettings.clear();
        m_settingAttempts.clear();
        return;
    }

    if (newActiveHeadsetSettingsKey != m_activeHeadsetSettingsKey)
    {
        m_activeHeadsetSettingsKey = newActiveHeadsetSettingsKey;
        m_sidetoneWriteRequested = false;
        m_appliedSettings.clear();
        m_settingAttempts.clear();
    }
}

void HeadsetControlMonitor::updateSidetoneFromDevice()
{
    auto* headset = activeHeadset();
    const int observed = headset ? m_cachedDevices.at(m_activeHeadsetIndex).sidetoneLevel : -1;
    if (m_sidetoneLevel != observed)
    {
        m_sidetoneLevel = observed;
        emit sidetoneLevelChanged();
    }
    if (headset && SidetonePolicy::decide(headset->supports(CAP_SIDETONE_STATUS), observed,
            m_appliedSettings.value("sidetone", -1).toInt(), m_desiredSettings.value("sidetone", 0).toInt(),
            m_sidetoneWriteRequested) == SidetonePolicy::Action::Adopt)
    {
        // A read establishes the baseline too; its settings echo must never become a write.
        m_desiredSettings["sidetone"] = observed;
        m_appliedSettings["sidetone"] = observed;
        m_settingAttempts.remove("sidetone");
        m_sidetoneWriteRequested = false;
        emit sidetoneAdopted(observed, m_sidetoneRequestRevision);
    }
}

void HeadsetControlMonitor::setDesiredSettings(const QVariantMap& settings)
{
    for (auto it = settings.cbegin(); it != settings.cend(); ++it)
    {
        if (m_desiredSettings.value(it.key()) != it.value())
            m_settingAttempts.remove(it.key());
        m_desiredSettings[it.key()] = it.value();
        // Relinquishing a write-only preference invalidates its old acknowledgement.
        // A later explicit selection must write again, even if it matches that old value.
        if (it.value().toInt() < 0)
            m_appliedSettings.remove(it.key());
    }
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
                if (key == "sidetone")
                {
                    m_sidetoneWriteRequested = false;
                    emit sidetoneLevelChanged();
                    emit snapshotReady(snapshot());
                }
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
    const bool applySidetone = SidetonePolicy::decide(headset->supports(CAP_SIDETONE_STATUS), -1,
        m_appliedSettings.value("sidetone", -1).toInt(), m_desiredSettings.value("sidetone", 0).toInt(),
        m_sidetoneWriteRequested) == SidetonePolicy::Action::Apply;
    apply("sidetone", m_hasSidetoneCapability && applySidetone,
          [&](const QVariant& v) {
              const auto result = headset->setSidetone(static_cast<uint8_t>(qBound(0, v.toInt(), 128)));
              if (result)
                  m_sidetoneLevel = result->current_level <= 128 ? result->current_level : -1;
              return result;
          });
    apply("equalizerPreset", m_hasEqualizerPresetsCapability && !m_equalizerPresetNames.isEmpty(),
          [&](const QVariant& v) {
              return headset->setEqualizerPreset(
                  static_cast<uint8_t>(qBound(0, v.toInt(), int(m_equalizerPresetNames.size()) - 1)));
          });
    apply("inactiveTime", m_hasInactiveTimeCapability && m_desiredSettings.value("inactiveTime", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setInactiveTime(static_cast<uint8_t>(qBound(0, v.toInt(), 255))); });
    apply("micVolume", m_hasMicVolumeCapability && m_desiredSettings.value("micVolume", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setMicVolume(static_cast<uint8_t>(qBound(0, v.toInt(), 128))); });
    apply("noiseFilter", m_hasNoiseFilterCapability && m_desiredSettings.value("noiseFilter", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setNoiseFilter(static_cast<uint8_t>(qBound(0, v.toInt(), 2))); });
    apply("volumeLimiter", m_hasVolumeLimiterCapability && m_desiredSettings.value("volumeLimiter", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setVolumeLimiter(qBound(0, v.toInt(), 1) != 0); });
    apply("micMuteLedBrightness", m_hasMicMuteLedBrightnessCapability && m_desiredSettings.value("micMuteLedBrightness", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setMicMuteLedBrightness(static_cast<uint8_t>(qBound(0, v.toInt(), 3))); });
    apply("bluetoothWhenPoweredOn", m_hasBluetoothWhenPoweredOnCapability && m_desiredSettings.value("bluetoothWhenPoweredOn", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setBluetoothWhenPoweredOn(qBound(0, v.toInt(), 1) != 0); });
    apply("bluetoothCallVolume", m_hasBluetoothCallVolumeCapability && m_desiredSettings.value("bluetoothCallVolume", -1).toInt() >= 0,
          [&](const QVariant& v) { return headset->setBluetoothCallVolume(static_cast<uint8_t>(qBound(0, v.toInt(), 100))); });
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
    state.hasBluetoothCallVolumeCapability = hasBluetoothCallVolumeCapability();
    state.hasBluetoothWhenPoweredOnCapability = hasBluetoothWhenPoweredOnCapability();
    state.hasMicMuteLedBrightnessCapability = hasMicMuteLedBrightnessCapability();
    state.hasVolumeLimiterCapability = hasVolumeLimiterCapability();
    state.hasNoiseFilterCapability = hasNoiseFilterCapability();
    state.hasMicVolumeCapability = hasMicVolumeCapability();
    state.deviceName = deviceName();
    state.displayName = displayName();
    state.batteryStatus = batteryStatus();
    state.batteryLevel = batteryLevel();
    state.chatMix = chatMix();
    state.sidetoneLevel = sidetoneLevel();
    state.sidetoneRequestRevision = m_sidetoneRequestRevision;
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
    m_hasBluetoothCallVolumeCapability = false;
    m_hasBluetoothWhenPoweredOnCapability = false;
    m_hasMicMuteLedBrightnessCapability = false;
    m_hasVolumeLimiterCapability = false;
    m_hasNoiseFilterCapability = false;
    m_hasMicVolumeCapability = false;
    m_deviceName = "";
    m_displayName.clear();
    m_batteryStatus = "BATTERY_UNAVAILABLE";
    m_batteryLevel = -1;
    m_chatMix = -1;
    m_sidetoneLevel = -1;
    m_sidetoneWriteRequested = false;
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
    emit displayNameChanged();
    emit batteryStatusChanged();
    emit batteryLevelChanged();
    emit chatMixChanged();
    emit sidetoneLevelChanged();
    if (wasDeviceFound)
    {
        emit anyDeviceFoundChanged();
    }

    emit snapshotReady(snapshot());
    emit headsetDataUpdated(m_cachedDevices);
    m_appliedSettings.clear();
    m_settingAttempts.clear();
}
