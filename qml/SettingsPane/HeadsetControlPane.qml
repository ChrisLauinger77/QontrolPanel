pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3
    readonly property var testProfiles: [qsTr("1 - Error conditions"), qsTr("2 - Charging battery"), qsTr("3 - Basic battery"), qsTr("4 - Battery unavailable"), qsTr("5 - Timeout"), qsTr("6 - Full battery"), qsTr("7 - Low battery")]

    Label {
        text: HeadsetControlBridge.deviceName
        font.pixelSize: 22
        font.bold: true
        visible: HeadsetControlBridge.anyDeviceFound
    }

    ProgressBar {
        Layout.bottomMargin: 15
        Layout.fillWidth: true
        from: 0
        to: 100
        value: HeadsetControlBridge.batteryLevel
        indeterminate: HeadsetControlBridge.batteryStatus === "BATTERY_CHARGING"
        visible: HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE" && HeadsetControlBridge.anyDeviceFound
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        CustomScrollView {
            anchors.fill: parent
            ColumnLayout {
                width: parent.width
                spacing: 3

                Card {
                    Layout.fillWidth: true
                    visible: !HeadsetControlBridge.anyDeviceFound && UserSettings.headsetcontrolMonitoring && !HeadsetControlBridge.testModeEnabled
                    title: qsTr("No compatible device found.")
                    description: qsTr("Enable test mode below to simulate a supported headset and validate the HeadsetControl UI.")
                }

                Card {
                    Layout.fillWidth: true
                    visible: UserSettings.headsetcontrolMonitoring && (!HeadsetControlBridge.anyDeviceFound || HeadsetControlBridge.testModeEnabled)
                    title: qsTr("HeadsetControl test mode")
                    description: qsTr("Simulate a supported headset for testing. This stays enabled until the app closes.")

                    additionalControl: LabeledSwitch {
                        checked: HeadsetControlBridge.testModeEnabled
                        onClicked: HeadsetControlBridge.setTestModeEnabled(checked)
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: UserSettings.headsetcontrolMonitoring && (!HeadsetControlBridge.anyDeviceFound || HeadsetControlBridge.testModeEnabled)
                    title: qsTr("Test headset profile")
                    description: qsTr("Choose which synthetic headset scenario HeadsetControl should simulate.")
                    enabled: HeadsetControlBridge.testModeEnabled

                    additionalControl: CustomComboBox {
                        Layout.preferredHeight: 35
                        model: testProfiles
                        currentIndex: Math.max(0, HeadsetControlBridge.testProfile - 1)
                        enabled: HeadsetControlBridge.testModeEnabled
                        onActivated: HeadsetControlBridge.setTestProfile(currentIndex + 1)
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Device battery")
                    visible: HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE" && HeadsetControlBridge.anyDeviceFound
                    description: qsTr("Current battery level of the connected headset")
                    additionalControl: Label {
                        text: HeadsetControlBridge.batteryIcon + HeadsetControlBridge.batteryLevel + "%"
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Notification on low battery")
                    description: qsTr("Notify when the connected headset reaches %1% or lower.").arg(UserSettings.headsetcontrolLowBatteryThreshold)
                    additionalControl: RowLayout {
                        spacing: 8

                        SpinBox {
                            Layout.preferredHeight: 35
                            Layout.preferredWidth: 100
                            from: 1
                            to: 30
                            value: UserSettings.headsetcontrolLowBatteryThreshold
                            editable: true
                            onValueModified: UserSettings.headsetcontrolLowBatteryThreshold = value
                        }

                        Label {
                            text: "%"
                            opacity: 0.7
                        }

                        LabeledSwitch {
                            checked: UserSettings.enableNotifications
                            onClicked: {
                                UserSettings.enableNotifications = checked;
                            }
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Show battery status in panel footer")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.displayBatteryFooter
                        onClicked: {
                            UserSettings.displayBatteryFooter = checked;
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("ChatMix")
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasChatMixCapability
                    description: qsTr("ChatMix value of the connected headset")
                    additionalControl: Label {
                        text: HeadsetControlBridge.chatMix
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasEqualizerPresetsCapability
                    enabled: HeadsetControlBridge.hasEqualizerPresetsCapability
                    Layout.fillWidth: true
                    title: qsTr("Equalizer Preset")
                    description: qsTr("Set the equalizer preset for your headset")
                    additionalControl: CustomComboBox {
                        Layout.preferredHeight: 35
                        model: HeadsetControlBridge.equalizerPresetNames
                        enabled: count > 0
                        currentIndex: {
                            if (count <= 0) {
                                return -1;
                            }

                            return Math.min(Math.max(0, UserSettings.headsetcontrolEqualizerPreset), count - 1);
                        }
                        onActivated: {
                            UserSettings.headsetcontrolEqualizerPreset = currentIndex;
                            HeadsetControlBridge.setEqualizerPreset(currentIndex);
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasInactiveTimeCapability
                    enabled: HeadsetControlBridge.hasInactiveTimeCapability
                    Layout.fillWidth: true
                    title: qsTr("Inactive time (minutes)")
                    description: qsTr("Set the time of inactivity after which your headset will enter power-saving mode")
                    additionalControl: RowLayout {
                        spacing: 12

                        NFSlider {
                            id: inactiveTimeSlider
                            from: 0
                            to: 90
                            value: Math.max(0, UserSettings.headsetcontrolInactiveTime)
                            Layout.preferredWidth: 180
                            onPressedChanged: {
                                if (!pressed) {
                                    UserSettings.headsetcontrolInactiveTime = Math.round(value);
                                    HeadsetControlBridge.setInactiveTime(Math.round(value));
                                }
                            }
                            onWheelChanged: {
                                UserSettings.headsetcontrolInactiveTime = Math.round(value);
                                HeadsetControlBridge.setInactiveTime(Math.round(value));
                            }
                        }

                        Label {
                            text: Math.round(inactiveTimeSlider.value).toString()
                            opacity: 0.7
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasLightsCapability
                    enabled: HeadsetControlBridge.hasLightsCapability
                    Layout.fillWidth: true
                    title: qsTr("Lights")
                    description: qsTr("Toggle RGB lights on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolLights
                        onClicked: {
                            UserSettings.headsetcontrolLights = checked;
                            HeadsetControlBridge.setLights(checked);
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasRotateToMuteCapability
                    enabled: HeadsetControlBridge.hasRotateToMuteCapability
                    Layout.fillWidth: true
                    title: qsTr("Rotate-to-Mute")
                    description: qsTr("Toggle rotate-to-mute feature on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolRotateToMute
                        onClicked: {
                            UserSettings.headsetcontrolRotateToMute = checked;
                            HeadsetControlBridge.setRotateToMute(checked);
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasSidetoneCapability
                    enabled: HeadsetControlBridge.hasSidetoneCapability
                    Layout.fillWidth: true
                    title: qsTr("Sidetone")
                    description: qsTr("Adjust your voice feedback level")
                    additionalControl: RowLayout {
                        spacing: 12

                        NFSlider {
                            id: sidetoneSlider
                            from: 0
                            to: 128
                            value: UserSettings.headsetcontrolSidetone
                            Layout.preferredWidth: 180
                            onPressedChanged: {
                                if (!pressed) {
                                    UserSettings.headsetcontrolSidetone = Math.round(value);
                                    HeadsetControlBridge.setSidetone(Math.round(value));
                                }
                            }
                            onWheelChanged: {
                                UserSettings.headsetcontrolSidetone = Math.round(value);
                                HeadsetControlBridge.setSidetone(Math.round(value));
                            }
                        }

                        Label {
                            text: Math.round(sidetoneSlider.value).toString()
                            opacity: 0.7
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasVoicePromptsCapability
                    enabled: HeadsetControlBridge.hasVoicePromptsCapability
                    Layout.fillWidth: true
                    title: qsTr("Voice Prompts")
                    description: qsTr("Toggle voice prompts on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolVoicePrompts
                        onClicked: {
                            UserSettings.headsetcontrolVoicePrompts = checked;
                            HeadsetControlBridge.setVoicePrompts(checked);
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Fetch rate (seconds)")
                    description: qsTr("How often the status from your headset is fetched. Lower values may increase battery usage.")
                    additionalControl: RowLayout {
                        spacing: 8

                        Button {
                            text: qsTr("Fetch")
                            onClicked: HeadsetControlBridge.refreshNow()
                        }

                        SpinBox {
                            from: 10
                            to: 3600
                            value: UserSettings.headsetcontrolFetchRate
                            editable: true
                            stepSize: 5
                            onValueModified: {
                                UserSettings.headsetcontrolFetchRate = value;
                                HeadsetControlBridge.setFetchRate(value);
                            }
                        }
                    }
                }

                Card {
                    visible: UserSettings.headsetcontrolMonitoring
                    Layout.fillWidth: true
                    title: qsTr("GitHub repository")
                    description: "https://github.com/Sapd/HeadsetControl"

                    additionalControl: Button {
                        text: qsTr("View on GitHub")
                        onClicked: Qt.openUrlExternally("https://github.com/Sapd/HeadsetControl#headsetcontrol")
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            opacity: 0.5
            text: qsTr("HeadsetControl monitoring is disabled\nYou can enable it in the Components tab.")
            visible: !UserSettings.headsetcontrolMonitoring
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
