import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3

    Label {
        text: qsTr("General Settings")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }

    CustomScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        ColumnLayout {
            width: parent.width
            spacing: 3

            Card {
                Layout.fillWidth: true
                title: qsTr("Run at system startup")
                description: qsTr("QontrolPanel will boot up when your computer starts")

                additionalControl: LabeledSwitch {
                    checked: StartupShortcutBridge.getShortcutState()
                    onClicked: StartupShortcutBridge.setStartupShortcut(checked)
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Settings startup page")
                description: qsTr("Choose which settings page opens when the settings window is shown")
                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [
                        qsTr("General"),
                        qsTr("Components"),
                        qsTr("Appearance"),
                        qsTr("Media Overlay"),
                        qsTr("ChatMix"),
                        qsTr("Shortcuts"),
                        qsTr("App Hotkeys"),
                        qsTr("HeadsetControl"),
                        qsTr("Renaming"),
                        qsTr("Language"),
                        qsTr("Updates"),
                        qsTr("Debug")
                    ]
                    currentIndex: UserSettings.settingsStartupPage
                    onActivated: {
                        UserSettings.settingsStartupPage = currentIndex
                        currentIndex = Qt.binding(function() { return UserSettings.settingsStartupPage })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Show power action confirmation")
                description: qsTr("Show a confirmation dialog when selecting a power action from the system tray menu")
                additionalControl: LabeledSwitch {
                    checked: UserSettings.showPowerDialogConfirmation
                    onClicked: {
                        UserSettings.showPowerDialogConfirmation = checked
                        checked = Qt.binding(function() { return UserSettings.showPowerDialogConfirmation })
                    }
                }
            }

            Card {
                enabled: UserSettings.showPowerDialogConfirmation
                Layout.fillWidth: true
                title: qsTr("Power action confirmation timeout (seconds)")
                description: qsTr("Time before the power action confirmation dialog automatically triggers the selected action")
                additionalControl: SpinBox {
                    from: 1
                    to: 120
                    value: UserSettings.powerDialogTimeout
                    onValueModified: {
                        UserSettings.powerDialogTimeout = value
                        value = Qt.binding(function() { return UserSettings.powerDialogTimeout })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Slider wheel sensivity")
                additionalControl: SpinBox {
                    from: 1
                    to: 10
                    value: UserSettings.sliderWheelSensivity
                    onValueModified: {
                        UserSettings.sliderWheelSensivity = value
                        value = Qt.binding(function() { return UserSettings.sliderWheelSensivity })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("DDC/CI brightness update rate")
                description: qsTr("Controls how frequently brightness commands are sent to external monitors")
                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Normal"), qsTr("Fast"), qsTr("Faster"), qsTr("Lightspeed")]
                    function acceptedIndex() {
                        switch(UserSettings.ddcciQueueDelay) {
                            case 500: return 0
                            case 250: return 1
                            case 100: return 2
                            case 1: return 3
                            default: return 0
                        }
                    }
                    currentIndex: acceptedIndex()
                    onActivated: {
                        switch(currentIndex) {
                            case 0: UserSettings.ddcciQueueDelay = 500; break
                            case 1: UserSettings.ddcciQueueDelay = 250; break
                            case 2: UserSettings.ddcciQueueDelay = 100; break
                            case 3: UserSettings.ddcciQueueDelay = 1; break
                        }
                        currentIndex = Qt.binding(acceptedIndex)
                    }
                }
            }
        }
    }
}
