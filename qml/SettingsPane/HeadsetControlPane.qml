pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3
    readonly property var testProfiles: [
        qsTr("1 - Error conditions"),
        qsTr("2 - Charging battery"),
        qsTr("3 - Basic battery"),
        qsTr("4 - Battery unavailable"),
        qsTr("5 - Timeout"),
        qsTr("6 - Full battery"),
        qsTr("7 - Low battery")
    ]

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
                    description: qsTr("Current battery level of the connected headset") +
                                 (HeadsetControlBridge.batteryStatus === "BATTERY_CHARGING" ? "\n" + "⚡︎" + qsTr("(Charging)") : "")
                    additionalControl: Label {
                        text: "🔋" + HeadsetControlBridge.batteryLevel + "%"
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("ChatMix")
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasChatMixCapability
                    description: qsTr("ChatMix value of the connected headset")
                    additionalControl: Label {
                        text:  HeadsetControlBridge.chatMix
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Show battery status in panel footer")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.displayBatteryFooter
                        onClicked:{
                            UserSettings.displayBatteryFooter = checked
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    enabled: HeadsetControlBridge.hasLightsCapability
                    Layout.fillWidth: true
                    title: qsTr("Headset Lighting")
                    description: qsTr("Toggle RGB lights on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolLights
                        onClicked:{
                            UserSettings.headsetcontrolLights = checked
                            HeadsetControlBridge.setLights(checked)
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    enabled: HeadsetControlBridge.hasRotateToMuteCapability
                    Layout.fillWidth: true
                    title: qsTr("Headset Rotate-to-Mute")
                    description: qsTr("Toggle rotate-to-mute feature on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolRotateToMute
                        onClicked:{
                            UserSettings.headsetcontrolRotateToMute = checked
                            HeadsetControlBridge.setRotateToMute(checked)
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    enabled: HeadsetControlBridge.hasSidetoneCapability
                    Layout.fillWidth: true
                    title: qsTr("Microphone Sidetone")
                    description: qsTr("Adjust your voice feedback level")
                    additionalControl: NFSlider {
                        from: 0
                        to: 128
                        value: UserSettings.headsetcontrolSidetone
                        onPressedChanged: {
                            if (!pressed) {
                                UserSettings.headsetcontrolSidetone = Math.round(value)
                                HeadsetControlBridge.setSidetone(Math.round(value))
                            }
                        }
                        onWheelChanged: {
                            UserSettings.headsetcontrolSidetone = Math.round(value)
                            HeadsetControlBridge.setSidetone(Math.round(value))
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Notification on low battery")
                    additionalControl: Switch {
                        checked: UserSettings.enableNotifications
                        onClicked: {
                            UserSettings.enableNotifications = checked
                            HeadsetControlBridge.notificationsEnabled = checked
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Fetch rate (seconds)")
                    additionalControl: SpinBox {
                        from: 10
                        to: 600
                        value: UserSettings.headsetcontrolFetchRate
                        editable: true
                        stepSize: 5
                        onValueModified: {
                            UserSettings.headsetcontrolFetchRate = (value)
                            HeadsetControlBridge.setFetchRate(value)
                        }
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            opacity: 0.5
            text: qsTr("HeadsetControl monitoring is disabled\nYou can enable it in the General tab.")
            visible: !UserSettings.headsetcontrolMonitoring
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
