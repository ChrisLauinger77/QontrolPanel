#include "usersettings.h"
#include <QSettings>
#include "logmanager.h"

namespace {
constexpr int kMaxSettingsStartupPage = 11;
constexpr int kMinHeadsetcontrolLowBatteryThreshold = 1;
constexpr int kMaxHeadsetcontrolLowBatteryThreshold = 30;
constexpr int kMinHeadsetcontrolFetchRate = 60;
}

UserSettings* UserSettings::m_instance = nullptr;

UserSettings::UserSettings(QObject *parent)
    : QObject(parent)
{
    initProperties();
}

UserSettings* UserSettings::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    auto* settings = instance();
    QQmlEngine::setObjectOwnership(settings, QQmlEngine::CppOwnership);
    return settings;
}

UserSettings* UserSettings::instance()
{
    if (!m_instance) {
        m_instance = new UserSettings();
    }
    return m_instance;
}

bool UserSettings::saveValue(const QString& key, const QVariant& value)
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
    settings.setValue(key, value);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        LOG_WARN("Settings", QString("Could not persist setting: %1").arg(key));
        m_lastError = tr("Could not save settings. Check access to your user profile.");
        emit lastErrorChanged();
        emit saveFailed(m_lastError);
        return false;
    }
    if (!m_lastError.isEmpty())
    {
        m_lastError.clear();
        emit lastErrorChanged();
    }
    return true;
}

void UserSettings::initProperties()
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");

    m_enableDeviceManager = settings.value("enableDeviceManager", true).toBool();
    m_enableApplicationMixer = settings.value("enableApplicationMixer", true).toBool();
    m_enableMediaSessionManager = settings.value("enableMediaSessionManager", true).toBool();
    m_panelPosition = qBound(0, settings.value("panelPosition", 1).toInt(), 3);
    m_taskbarOffset = qBound(0, settings.value("taskbarOffset", 0).toInt(), 200);
    m_xAxisMargin = qBound(0, settings.value("xAxisMargin", 12).toInt(), 200);
    m_yAxisMargin = qBound(0, settings.value("yAxisMargin", 12).toInt(), 200);
    m_languageIndex = settings.value("languageIndex", 0).toInt();

    m_chatMixValue = qBound(0, settings.value("chatMixValue", 50).toInt(), 100);
    m_chatMixEnabled = settings.value("chatMixEnabled", false).toBool();
    m_activateChatmix = settings.value("activateChatmix", false).toBool();
    m_showAudioLevel = settings.value("showAudioLevel", true).toBool();
    m_chatmixRestoreVolume = qBound(0, settings.value("chatmixRestoreVolume", 80).toInt(), 100);

    m_globalShortcutsEnabled = settings.value("globalShortcutsEnabled", true).toBool();
    m_panelShortcutKey = settings.value("panelShortcutKey", 83).toInt();
    m_panelShortcutModifiers = settings.value("panelShortcutModifiers", 117440512).toInt();
    m_chatMixShortcutKey = settings.value("chatMixShortcutKey", 77).toInt();
    m_chatMixShortcutModifiers = settings.value("chatMixShortcutModifiers", 117440512).toInt();
    m_chatMixShortcutNotification = settings.value("chatMixShortcutNotification", true).toBool();
    m_micMuteShortcutKey = settings.value("micMuteShortcutKey", 75).toInt();
    m_micMuteShortcutModifiers = settings.value("micMuteShortcutModifiers", 117440512).toInt();
    m_autoUpdateTranslations = settings.value("autoUpdateTranslations", false).toBool();
    m_firstRun = settings.value("firstRun", true).toBool();
    m_settingsStartupPage = qBound(0, settings.value("settingsStartupPage", 0).toInt(), kMaxSettingsStartupPage);
    m_settingsAnimationsEnabled = settings.value("settingsAnimationsEnabled", true).toBool();

    m_trayIconTheme = qBound(0, settings.value("trayIconTheme", 0).toInt(), 2);
    m_iconStyle = qBound(0, settings.value("iconStyle", 1).toInt(), 2);

    m_autoFetchForAppUpdates = settings.value("autoFetchForAppUpdates", false).toBool();
    m_headsetcontrolMonitoring = settings.value("headsetcontrolMonitoring", true).toBool();
    m_headsetcontrolLights = settings.value("headsetcontrolLights", true).toBool();
    m_headsetcontrolRotateToMute = settings.value("headsetcontrolRotateToMute", true).toBool();
    m_headsetcontrolVoicePrompts = settings.value("headsetcontrolVoicePrompts", true).toBool();
    m_headsetcontrolEqualizerPreset = qMax(0, settings.value("headsetcontrolEqualizerPreset", 0).toInt());
    m_headsetcontrolInactiveTime = qBound(0, settings.value("headsetcontrolInactiveTime", 30).toInt(), 90);
    m_headsetcontrolSidetone = qBound(0, settings.value("headsetcontrolSidetone", 0).toInt(), 128);
    m_allowBrightnessControl = settings.value("allowBrightnessControl", true).toBool();
    m_avoidApplicationsOverflow = settings.value("avoidApplicationsOverflow", false).toBool();
    m_ddcciQueueDelay = qBound(1, settings.value("ddcciQueueDelay", 500).toInt(), 5000);

    m_enablePowerMenu = settings.value("enablePowerMenu", true).toBool();
    m_showPowerDialogConfirmation = settings.value("showPowerDialogConfirmation", true).toBool();
    m_powerDialogTimeout = qBound(1, settings.value("powerDialogTimeout", 30).toInt(), 120);

    m_ddcciBrightness = qBound(0, settings.value("ddcciBrightness", 100).toInt(), 100);
    m_displayBatteryFooter = settings.value("displayBatteryFooter", true).toBool();
    m_panelStyle = qBound(0, settings.value("panelStyle", 0).toInt(), 2);
    m_headsetcontrolFetchRate =
        qBound(60,
               qMax(kMinHeadsetcontrolFetchRate,
                    settings.value("headsetcontrolFetchRate", kMinHeadsetcontrolFetchRate).toInt()),
               3600);
    m_enableNotifications = settings.value("enableNotifications", false).toBool();
    m_headsetcontrolLowBatteryThreshold = qBound(
        kMinHeadsetcontrolLowBatteryThreshold,
        settings.value("headsetcontrolLowBatteryThreshold", 25).toInt(),
        kMaxHeadsetcontrolLowBatteryThreshold);

    m_enableMediaOverlay = settings.value("enableMediaOverlay", false).toBool();
    m_mediaOverlayPosition = qBound(0, settings.value("mediaOverlayPosition", 1).toInt(), 8); // Default: top-center
    m_mediaOverlaySize = qBound(0, settings.value("mediaOverlaySize", 1).toInt(), 2);         // Default: normal

    m_sliderWheelSensivity = qBound(1, settings.value("sliderWheelSensivity", 2).toInt(), 10);
}

// Setters
void UserSettings::setEnableDeviceManager(bool value)
{
    if (m_enableDeviceManager != value) {
        if (!saveValue("enableDeviceManager", value))
            return;
        m_enableDeviceManager = value;
        emit enableDeviceManagerChanged();
    }
}

void UserSettings::setEnableApplicationMixer(bool value)
{
    if (m_enableApplicationMixer != value) {
        if (!saveValue("enableApplicationMixer", value))
            return;
        m_enableApplicationMixer = value;
        emit enableApplicationMixerChanged();
    }
}

void UserSettings::setEnableMediaSessionManager(bool value)
{
    if (m_enableMediaSessionManager != value) {
        if (!saveValue("enableMediaSessionManager", value))
            return;
        m_enableMediaSessionManager = value;
        emit enableMediaSessionManagerChanged();
    }
}

void UserSettings::setPanelPosition(int value)
{
    value = qBound(0, value, 3);
    if (m_panelPosition != value) {
        if (!saveValue("panelPosition", value))
            return;
        m_panelPosition = value;
        emit panelPositionChanged();
    }
}

void UserSettings::setTaskbarOffset(int value)
{
    value = qBound(0, value, 200);
    if (m_taskbarOffset != value) {
        if (!saveValue("taskbarOffset", value))
            return;
        m_taskbarOffset = value;
        emit taskbarOffsetChanged();
    }
}

void UserSettings::setXAxisMargin(int value)
{
    value = qBound(0, value, 200);
    if (m_xAxisMargin != value) {
        if (!saveValue("xAxisMargin", value))
            return;
        m_xAxisMargin = value;
        emit xAxisMarginChanged();
    }
}

void UserSettings::setYAxisMargin(int value)
{
    value = qBound(0, value, 200);
    if (m_yAxisMargin != value) {
        if (!saveValue("yAxisMargin", value))
            return;
        m_yAxisMargin = value;
        emit yAxisMarginChanged();
    }
}

void UserSettings::setLanguageIndex(int value)
{
    if (m_languageIndex != value) {
        if (!saveValue("languageIndex", value))
            return;
        m_languageIndex = value;
        emit languageIndexChanged();
    }
}

void UserSettings::setChatMixValue(int value)
{
    value = qBound(0, value, 100);
    if (m_chatMixValue != value) {
        if (!saveValue("chatMixValue", value))
            return;
        m_chatMixValue = value;
        emit chatMixValueChanged();
    }
}

void UserSettings::setChatMixEnabled(bool value)
{
    if (m_chatMixEnabled != value) {
        if (!saveValue("chatMixEnabled", value))
            return;
        m_chatMixEnabled = value;
        emit chatMixEnabledChanged();
    }
}

void UserSettings::setActivateChatmix(bool value)
{
    if (m_activateChatmix != value) {
        if (!saveValue("activateChatmix", value))
            return;
        m_activateChatmix = value;
        emit activateChatmixChanged();
    }
}

void UserSettings::setShowAudioLevel(bool value)
{
    if (m_showAudioLevel != value) {
        if (!saveValue("showAudioLevel", value))
            return;
        m_showAudioLevel = value;
        emit showAudioLevelChanged();
    }
}

void UserSettings::setChatmixRestoreVolume(int value)
{
    value = qBound(0, value, 100);
    if (m_chatmixRestoreVolume != value) {
        if (!saveValue("chatmixRestoreVolume", value))
            return;
        m_chatmixRestoreVolume = value;
        emit chatmixRestoreVolumeChanged();
    }
}

void UserSettings::setGlobalShortcutsEnabled(bool value)
{
    if (m_globalShortcutsEnabled != value) {
        if (!saveValue("globalShortcutsEnabled", value))
            return;
        m_globalShortcutsEnabled = value;
        emit globalShortcutsEnabledChanged();
    }
}

void UserSettings::setPanelShortcutKey(int value)
{
    if (m_panelShortcutKey != value) {
        if (!saveValue("panelShortcutKey", value))
            return;
        m_panelShortcutKey = value;
        emit panelShortcutKeyChanged();
    }
}

void UserSettings::setPanelShortcutModifiers(int value)
{
    if (m_panelShortcutModifiers != value) {
        if (!saveValue("panelShortcutModifiers", value))
            return;
        m_panelShortcutModifiers = value;
        emit panelShortcutModifiersChanged();
    }
}

void UserSettings::setChatMixShortcutKey(int value)
{
    if (m_chatMixShortcutKey != value) {
        if (!saveValue("chatMixShortcutKey", value))
            return;
        m_chatMixShortcutKey = value;
        emit chatMixShortcutKeyChanged();
    }
}

void UserSettings::setChatMixShortcutModifiers(int value)
{
    if (m_chatMixShortcutModifiers != value) {
        if (!saveValue("chatMixShortcutModifiers", value))
            return;
        m_chatMixShortcutModifiers = value;
        emit chatMixShortcutModifiersChanged();
    }
}

void UserSettings::setChatMixShortcutNotification(bool value)
{
    if (m_chatMixShortcutNotification != value) {
        if (!saveValue("chatMixShortcutNotification", value))
            return;
        m_chatMixShortcutNotification = value;
        emit chatMixShortcutNotificationChanged();
    }
}

void UserSettings::setMicMuteShortcutKey(int value)
{
    if (m_micMuteShortcutKey != value) {
        if (!saveValue("micMuteShortcutKey", value))
            return;
        m_micMuteShortcutKey = value;
        emit micMuteShortcutKeyChanged();
    }
}

void UserSettings::setMicMuteShortcutModifiers(int value)
{
    if (m_micMuteShortcutModifiers != value) {
        if (!saveValue("micMuteShortcutModifiers", value))
            return;
        m_micMuteShortcutModifiers = value;
        emit micMuteShortcutModifiersChanged();
    }
}

void UserSettings::setAutoUpdateTranslations(bool value)
{
    if (m_autoUpdateTranslations != value) {
        if (!saveValue("autoUpdateTranslations", value))
            return;
        m_autoUpdateTranslations = value;
        emit autoUpdateTranslationsChanged();
    }
}

void UserSettings::setFirstRun(bool value)
{
    if (m_firstRun != value) {
        if (!saveValue("firstRun", value))
            return;
        m_firstRun = value;
        emit firstRunChanged();
    }
}

void UserSettings::setSettingsStartupPage(int value)
{
    value = qBound(0, value, kMaxSettingsStartupPage);

    if (m_settingsStartupPage != value) {
        if (!saveValue("settingsStartupPage", value))
            return;
        m_settingsStartupPage = value;
        emit settingsStartupPageChanged();
    }
}

void UserSettings::setSettingsAnimationsEnabled(bool value)
{
    if (m_settingsAnimationsEnabled != value) {
        if (!saveValue("settingsAnimationsEnabled", value))
            return;
        m_settingsAnimationsEnabled = value;
        emit settingsAnimationsEnabledChanged();
    }
}

void UserSettings::setTrayIconTheme(int value)
{
    value = qBound(0, value, 2);
    if (m_trayIconTheme != value) {
        if (!saveValue("trayIconTheme", value))
            return;
        m_trayIconTheme = value;
        emit trayIconThemeChanged();
    }
}

void UserSettings::setIconStyle(int value)
{
    value = qBound(0, value, 2);
    if (m_iconStyle != value) {
        if (!saveValue("iconStyle", value))
            return;
        m_iconStyle = value;
        emit iconStyleChanged();
    }
}

void UserSettings::setAutoFetchForAppUpdates(bool value)
{
    if (m_autoFetchForAppUpdates != value) {
        if (!saveValue("autoFetchForAppUpdates", value))
            return;
        m_autoFetchForAppUpdates = value;
        emit autoFetchForAppUpdatesChanged();
    }
}

void UserSettings::setHeadsetcontrolMonitoring(bool value)
{
    if (m_headsetcontrolMonitoring != value) {
        if (!saveValue("headsetcontrolMonitoring", value))
            return;
        m_headsetcontrolMonitoring = value;
        emit headsetcontrolMonitoringChanged();
    }
}

void UserSettings::setHeadsetcontrolLights(bool value)
{
    if (m_headsetcontrolLights != value) {
        if (!saveValue("headsetcontrolLights", value))
            return;
        m_headsetcontrolLights = value;
        emit headsetcontrolLightsChanged();
    }
}

void UserSettings::setHeadsetcontrolRotateToMute(bool value)
{
    if (m_headsetcontrolRotateToMute != value) {
        if (!saveValue("headsetcontrolRotateToMute", value))
            return;
        m_headsetcontrolRotateToMute = value;
        emit headsetcontrolRotateToMuteChanged();
    }
}

void UserSettings::setHeadsetcontrolVoicePrompts(bool value)
{
    if (m_headsetcontrolVoicePrompts != value) {
        if (!saveValue("headsetcontrolVoicePrompts", value))
            return;
        m_headsetcontrolVoicePrompts = value;
        emit headsetcontrolVoicePromptsChanged();
    }
}

void UserSettings::setHeadsetcontrolEqualizerPreset(int value)
{
    value = qMax(0, value);

    if (m_headsetcontrolEqualizerPreset != value) {
        if (!saveValue("headsetcontrolEqualizerPreset", value))
            return;
        m_headsetcontrolEqualizerPreset = value;
        emit headsetcontrolEqualizerPresetChanged();
    }
}

void UserSettings::setHeadsetcontrolInactiveTime(int value)
{
    value = qBound(0, value, 90);
    value = qBound(0, value, 128);

    if (m_headsetcontrolInactiveTime != value) {
        if (!saveValue("headsetcontrolInactiveTime", value))
            return;
        m_headsetcontrolInactiveTime = value;
        emit headsetcontrolInactiveTimeChanged();
    }
}

void UserSettings::setHeadsetcontrolSidetone(int value)
{
    value = qBound(0, value, 128);
    if (m_headsetcontrolSidetone != value) {
        if (!saveValue("headsetcontrolSidetone", value))
            return;
        m_headsetcontrolSidetone = value;
        emit headsetcontrolSidetoneChanged();
    }
}

void UserSettings::setAllowBrightnessControl(bool value)
{
    if (m_allowBrightnessControl != value) {
        if (!saveValue("allowBrightnessControl", value))
            return;
        m_allowBrightnessControl = value;
        emit allowBrightnessControlChanged();
    }
}

void UserSettings::setAvoidApplicationsOverflow(bool value)
{
    if (m_avoidApplicationsOverflow != value) {
        if (!saveValue("avoidApplicationsOverflow", value))
            return;
        m_avoidApplicationsOverflow = value;
        emit avoidApplicationsOverflowChanged();
    }
}

void UserSettings::setDdcciQueueDelay(int value)
{
    value = qBound(1, value, 5000);
    if (m_ddcciQueueDelay != value) {
        if (!saveValue("ddcciQueueDelay", value))
            return;
        m_ddcciQueueDelay = value;
        emit ddcciQueueDelayChanged();
    }
}

void UserSettings::setEnablePowerMenu(bool value)
{
    if (m_enablePowerMenu != value) {
        if (!saveValue("enablePowerMenu", value))
            return;
        m_enablePowerMenu = value;
        emit enablePowerMenuChanged();
    }
}

void UserSettings::setShowPowerDialogConfirmation(bool value)
{
    if (m_showPowerDialogConfirmation != value) {
        if (!saveValue("showPowerDialogConfirmation", value))
            return;
        m_showPowerDialogConfirmation = value;
        emit showPowerDialogConfirmationChanged();
    }
}

void UserSettings::setPowerDialogTimeout(int value)
{
    value = qBound(1, value, 120);
    if (m_powerDialogTimeout != value) {
        if (!saveValue("powerDialogTimeout", value))
            return;
        m_powerDialogTimeout = value;
        emit powerDialogTimeoutChanged();
    }
}

void UserSettings::setDdcciBrightness(int value)
{
    value = qBound(0, value, 100);
    if (m_ddcciBrightness != value) {
        if (!saveValue("ddcciBrightness", value))
            return;
        m_ddcciBrightness = value;
        emit ddcciBrightnessChanged();
    }
}

void UserSettings::setDisplayBatteryFooter(bool value)
{
    if (m_displayBatteryFooter != value) {
        if (!saveValue("displayBatteryFooter", value))
            return;
        m_displayBatteryFooter = value;
        emit displayBatteryFooterChanged();
    }
}

void UserSettings::setPanelStyle(int value)
{
    value = qBound(0, value, 2);
    if (m_panelStyle != value) {
        if (!saveValue("panelStyle", value))
            return;
        m_panelStyle = value;
        emit panelStyleChanged();
    }
}

void UserSettings::setHeadsetcontrolFetchRate(int value)
{
    value = qBound(60, value, 3600);
    value = qMax(kMinHeadsetcontrolFetchRate, value);
    if (m_headsetcontrolFetchRate != value) {
        if (!saveValue("headsetcontrolFetchRate", value))
            return;
        m_headsetcontrolFetchRate = value;
        emit headsetcontrolFetchRateChanged();
    }
}

void UserSettings::setEnableNotifications(bool value)
{
    if (m_enableNotifications != value) {
        if (!saveValue("enableNotifications", value))
            return;
        m_enableNotifications = value;
        emit enableNotificationsChanged();
    }
}

void UserSettings::setHeadsetcontrolLowBatteryThreshold(int value)
{
    value = qBound(kMinHeadsetcontrolLowBatteryThreshold,
                   value,
                   kMaxHeadsetcontrolLowBatteryThreshold);

    if (m_headsetcontrolLowBatteryThreshold != value) {
        if (!saveValue("headsetcontrolLowBatteryThreshold", value))
            return;
        m_headsetcontrolLowBatteryThreshold = value;
        emit headsetcontrolLowBatteryThresholdChanged();
    }
}

void UserSettings::setEnableMediaOverlay(bool value)
{
    if (m_enableMediaOverlay != value) {
        if (!saveValue("enableMediaOverlay", value))
            return;
        m_enableMediaOverlay = value;
        emit enableMediaOverlayChanged();
    }
}

void UserSettings::setMediaOverlayPosition(int value)
{
    value = qBound(0, value, 8);
    if (m_mediaOverlayPosition != value) {
        if (!saveValue("mediaOverlayPosition", value))
            return;
        m_mediaOverlayPosition = value;
        emit mediaOverlayPositionChanged();
    }
}

void UserSettings::setMediaOverlaySize(int value)
{
    value = qBound(0, value, 2);
    if (m_mediaOverlaySize != value) {
        if (!saveValue("mediaOverlaySize", value))
            return;
        m_mediaOverlaySize = value;
        emit mediaOverlaySizeChanged();
    }
}

void UserSettings::setSliderWheelSensivity(int value)
{
    value = qBound(1, value, 10);
    if (m_sliderWheelSensivity != value) {
        if (!saveValue("sliderWheelSensivity", value))
            return;
        m_sliderWheelSensivity = value;
        emit sliderWheelSensivityChanged();
    }
}
