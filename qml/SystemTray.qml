import QtQuick
import Qt.labs.platform as Platform
import ChrisLauinger77.QontrolPanel

Platform.SystemTrayIcon {
    id: systemTray
    visible: true
    signal togglePanelRequested
    signal showIntroRequested
    signal settingsWindowRequested
    icon.source: Constants.getTrayIcon(AudioBridge.outputVolume, AudioBridge.outputMuted)
    tooltip: getTooltip()

    readonly property bool mediaControlsEnabled: UserSettings.enableMediaSessionManager
                                                && MediaSessionBridge.sourceCount > 0

    onActivated: function (reason) {
        if (reason === Platform.SystemTrayIcon.Context) {
            systemTray.contextMenu.popup();
            return;
        }

        if (reason === Platform.SystemTrayIcon.DoubleClick) {
            systemTray.settingsWindowRequested();
            return;
        }

        if (reason === Platform.SystemTrayIcon.Trigger) {
            systemTray.togglePanelRequested();
        }
    }

    // The native Windows menu ignores the app's color scheme. Keep the tray
    // icon native and render its menu independently of the hidden panel window.
    property TrayMenu contextMenu: TrayMenu {
        colorScheme: Qt.application.styleHints.colorScheme
        entries: [
            { text: systemTray.truncateText(qsTr("Output: ") + (AudioBridge.isReady ? systemTray.getOutputDeviceInfo() : qsTr("Loading...")), 50),
              icon: "qrc:/icons/panel_volume_100.svg", enabled: false },
            { text: systemTray.truncateText(qsTr("Input: ") + (AudioBridge.isReady ? systemTray.getInputDeviceInfo() : qsTr("Loading...")), 50),
              icon: "qrc:/icons/mic.svg", enabled: false },
            { separator: true, visible: UserSettings.enableMediaSessionManager },
            { id: "previous", text: qsTr("Previous"), icon: "qrc:/icons/prev.png",
              visible: UserSettings.enableMediaSessionManager,
              enabled: systemTray.mediaControlsEnabled && MediaSessionBridge.canPreviousTrack },
            { id: "playPause", text: MediaSessionBridge.isMediaPlaying ? qsTr("Pause") : qsTr("Play"),
              icon: MediaSessionBridge.isMediaPlaying ? "qrc:/icons/pause.png" : "qrc:/icons/play.png",
              visible: UserSettings.enableMediaSessionManager, enabled: systemTray.mediaControlsEnabled },
            { id: "next", text: qsTr("Next"), icon: "qrc:/icons/next.png",
              visible: UserSettings.enableMediaSessionManager, enabled: systemTray.mediaControlsEnabled },
            { separator: true },
            { id: "legacySound", text: qsTr("Windows sound settings (Legacy)"), icon: "qrc:/icons/general.svg" },
            { id: "modernSound", text: qsTr("Windows sound settings (Modern)"), icon: "qrc:/icons/panel_volume_100.svg" },
            { id: "settings", text: qsTr("QontrolPanel settings"), icon: "qrc:/icons/settings.svg" },
            { id: "headset", text: qsTr("HeadsetControl"), icon: "qrc:/icons/headsetcontrol.svg",
              visible: HeadsetControlBridge.anyDeviceFound && systemTray.hasHeadsetControlMenuEntries(),
              children: [
                  { id: "fetch", text: qsTr("Fetch"), icon: "qrc:/icons/update.svg",
                    visible: systemTray.isHeadsetBatteryAvailable() },
                  { id: "equalizer", text: qsTr("Equalizer Preset"), icon: "qrc:/icons/general.svg",
                    visible: HeadsetControlBridge.hasEqualizerPresetsCapability && HeadsetControlBridge.equalizerPresetNames.length > 0,
                    children: HeadsetControlBridge.hasEqualizerPresetsCapability
                        ? HeadsetControlBridge.equalizerPresetNames.map(function(name, index) {
                            return { id: "equalizer:" + index, text: name,
                                     checked: UserSettings.headsetcontrolEqualizerPreset === index };
                          }) : [] },
                  { id: "lights", text: qsTr("Lights"), icon: "qrc:/icons/brightness.svg",
                    visible: HeadsetControlBridge.hasLightsCapability, checked: UserSettings.headsetcontrolLights },
                  { id: "voicePrompts", text: qsTr("Voice Prompts"), icon: "qrc:/icons/music.svg",
                    visible: HeadsetControlBridge.hasVoicePromptsCapability, checked: UserSettings.headsetcontrolVoicePrompts },
                  { id: "rotateToMute", text: qsTr("Rotate-to-Mute"), icon: "qrc:/icons/mic_muted.svg",
                    visible: HeadsetControlBridge.hasRotateToMuteCapability, checked: UserSettings.headsetcontrolRotateToMute }
              ] },
            { separator: true },
            { id: "exit", text: qsTr("Exit"), icon: "qrc:/icons/signout.svg" }
        ]

        onTriggered: function(actionId) {
            switch (actionId) {
            case "previous": MediaSessionBridge.previousTrack(); break;
            case "playPause": MediaSessionBridge.playPause(); break;
            case "next": MediaSessionBridge.nextTrack(); break;
            case "legacySound": Utils.openLegacySoundSettings(); break;
            case "modernSound": Utils.openModernSoundSettings(); break;
            case "settings": systemTray.settingsWindowRequested(); break;
            case "fetch": HeadsetControlBridge.refreshNow(); break;
            case "lights": UserSettings.headsetcontrolLights = !UserSettings.headsetcontrolLights; break;
            case "voicePrompts": UserSettings.headsetcontrolVoicePrompts = !UserSettings.headsetcontrolVoicePrompts; break;
            case "rotateToMute": UserSettings.headsetcontrolRotateToMute = !UserSettings.headsetcontrolRotateToMute; break;
            case "exit": Qt.quit(); break;
            default:
                if (actionId.startsWith("equalizer:"))
                    UserSettings.headsetcontrolEqualizerPreset = Number(actionId.substring(10));
                break;
            }
        }
    }

    function truncateText(text, maxLength) {
        if (text.length <= maxLength)
            return text;
        return text.substring(0, maxLength - 3) + "...";
    }

    function getOutputDeviceInfo() {
        return AudioBridge.outputDeviceDisplayName || qsTr("Unknown Device");
    }

    function getInputDeviceInfo() {
        return AudioBridge.inputDeviceDisplayName || qsTr("Unknown Device");
    }

    function getTooltip() {
        var baseTooltip = qsTr("QontrolPanel");
        var isWirelessHeadsetAvailable = HeadsetControlBridge.anyDeviceFound;
        if (!isWirelessHeadsetAvailable) {
            return baseTooltip;
        }

        var batteryText = "\n" + "🎧" + HeadsetControlBridge.deviceName + "\n";
        batteryText += HeadsetControlBridge.batteryIcon;
        if (HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE") {
            batteryText += HeadsetControlBridge.batteryLevel + "%";
        } else {
            batteryText += qsTr("Device disconnected");
        }

        if (HeadsetControlBridge.hasChatMixCapability) {
            batteryText += " " + "🎤" + qsTr("ChatMix") + ": " + HeadsetControlBridge.chatMix;
        }

        return baseTooltip + batteryText;
    }

    function isHeadsetBatteryAvailable() {
        return HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE";
    }

    function hasHeadsetControlMenuEntries() {
        return systemTray.isHeadsetBatteryAvailable() || (HeadsetControlBridge.hasEqualizerPresetsCapability && HeadsetControlBridge.equalizerPresetNames.length > 0) || HeadsetControlBridge.hasLightsCapability || HeadsetControlBridge.hasVoicePromptsCapability || HeadsetControlBridge.hasRotateToMuteCapability;
    }
}
