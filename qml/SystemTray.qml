import QtQuick
import Qt.labs.platform as Platform
import QtQml.Models
import ChrisLauinger77.QontrolPanel

Platform.SystemTrayIcon {
    id: systemTray
    visible: true
    signal togglePanelRequested
    signal showIntroRequested
    signal settingsWindowRequested
    icon.source: Constants.getTrayIcon(AudioBridge.outputVolume, AudioBridge.outputMuted)
    tooltip: getTooltip()

    onActivated: function (reason) {
        if (reason === Platform.SystemTrayIcon.DoubleClick) {
            systemTray.settingsWindowRequested();
            return;
        }

        if (reason === Platform.SystemTrayIcon.Trigger) {
            HeadsetControlBridge.refreshNow();
            systemTray.togglePanelRequested();
        }
    }

    menu: Platform.Menu {
        Platform.MenuItem {
            enabled: false
            text: systemTray.truncateText(qsTr("Output: ") + (AudioBridge.isReady ? systemTray.getOutputDeviceInfo() : qsTr("Loading...")), 50)
        }
        Platform.MenuItem {
            enabled: false
            text: systemTray.truncateText(qsTr("Input: ") + (AudioBridge.isReady ? systemTray.getInputDeviceInfo() : qsTr("Loading...")), 50)
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Windows sound settings (Legacy)")
            onTriggered: Utils.openLegacySoundSettings()
        }
        Platform.MenuItem {
            text: qsTr("Windows sound settings (Modern)")
            onTriggered: Utils.openModernSoundSettings()
        }
        Platform.MenuItem {
            text: qsTr("QontrolPanel settings")
            onTriggered: systemTray.settingsWindowRequested()
        }
        Platform.Menu {
            title: qsTr("HeadsetControl")
            visible: HeadsetControlBridge.anyDeviceFound && systemTray.hasHeadsetControlMenuEntries()

            Platform.Menu {
                id: equalizerPresetMenu
                title: qsTr("Equalizer Preset")
                visible: HeadsetControlBridge.hasEqualizerPresetsCapability && equalizerPresetInstantiator.count > 0

                Instantiator {
                    id: equalizerPresetInstantiator
                    model: HeadsetControlBridge.hasEqualizerPresetsCapability ? HeadsetControlBridge.equalizerPresetNames : []
                    delegate: Platform.MenuItem {
                        required property string modelData
                        required property int index

                        text: modelData
                        checkable: true
                        checked: UserSettings.headsetcontrolEqualizerPreset === index
                        onTriggered: {
                            UserSettings.headsetcontrolEqualizerPreset = index;
                            HeadsetControlBridge.setEqualizerPreset(index);
                        }
                    }

                    onObjectAdded: function (index, object) {
                        equalizerPresetMenu.insertItem(index, object);
                    }

                    onObjectRemoved: function (index, object) {
                        equalizerPresetMenu.removeItem(object);
                    }
                }
            }

            Platform.MenuItem {
                text: qsTr("Lights")
                visible: HeadsetControlBridge.hasLightsCapability
                checkable: true
                checked: UserSettings.headsetcontrolLights
                onTriggered: {
                    UserSettings.headsetcontrolLights = !UserSettings.headsetcontrolLights;
                    HeadsetControlBridge.setLights(UserSettings.headsetcontrolLights);
                }
            }

            Platform.MenuItem {
                text: qsTr("Voice Prompts")
                visible: HeadsetControlBridge.hasVoicePromptsCapability
                checkable: true
                checked: UserSettings.headsetcontrolVoicePrompts
                onTriggered: {
                    UserSettings.headsetcontrolVoicePrompts = !UserSettings.headsetcontrolVoicePrompts;
                    HeadsetControlBridge.setVoicePrompts(UserSettings.headsetcontrolVoicePrompts);
                }
            }

            Platform.MenuItem {
                text: qsTr("Rotate-to-Mute")
                visible: HeadsetControlBridge.hasRotateToMuteCapability
                checkable: true
                checked: UserSettings.headsetcontrolRotateToMute
                onTriggered: {
                    UserSettings.headsetcontrolRotateToMute = !UserSettings.headsetcontrolRotateToMute;
                    HeadsetControlBridge.setRotateToMute(UserSettings.headsetcontrolRotateToMute);
                }
            }
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Exit")
            onTriggered: Qt.quit()
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

        if (HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE") {
            batteryText += HeadsetControlBridge.batteryIcon;
            batteryText += HeadsetControlBridge.batteryLevel + "%";
        }

        if (HeadsetControlBridge.hasChatMixCapability) {
            batteryText += " " + "🎤" + qsTr("ChatMix") + ": " + HeadsetControlBridge.chatMix;
        }

        return baseTooltip + batteryText;
    }

    function hasHeadsetControlMenuEntries() {
        return (HeadsetControlBridge.hasEqualizerPresetsCapability && HeadsetControlBridge.equalizerPresetNames.length > 0) || HeadsetControlBridge.hasLightsCapability || HeadsetControlBridge.hasVoicePromptsCapability || HeadsetControlBridge.hasRotateToMuteCapability;
    }
}
