import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import QtQuick
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3

    Label {
        text: qsTr("Appearance & Position")
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
                title: qsTr("Panel position")
                description: ""

                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Top"), qsTr("Bottom"), qsTr("Left"), qsTr("Right")]
                    currentIndex: UserSettings.panelPosition
                    onActivated: {
                        UserSettings.panelPosition = currentIndex
                        currentIndex = Qt.binding(function() { return UserSettings.panelPosition })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Taskbar offset")
                description: qsTr("Windows taskbar size (Only set if using W11 and taskbar is not at screen bottom)")

                additionalControl: SpinBox {
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 160
                    from: 0
                    to: 200
                    editable: true
                    value: UserSettings.taskbarOffset
                    onValueModified: {
                        UserSettings.taskbarOffset = value
                        value = Qt.binding(function() { return UserSettings.taskbarOffset })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Panel X margin")
                description: qsTr("Control panel X axis margin")

                additionalControl: SpinBox {
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 160
                    from: 0
                    to: 200
                    editable: true
                    value: UserSettings.xAxisMargin
                    onValueModified: {
                        UserSettings.xAxisMargin = value
                        value = Qt.binding(function() { return UserSettings.xAxisMargin })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Panel Y margin")
                description: qsTr("Control panel Y axis margin")

                additionalControl: SpinBox {
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 160
                    from: 0
                    to: 200
                    editable: true
                    value: UserSettings.yAxisMargin
                    onValueModified: {
                        UserSettings.yAxisMargin = value
                        value = Qt.binding(function() { return UserSettings.yAxisMargin })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Show audio level")
                description: qsTr("Display audio level value in slider")

                additionalControl: LabeledSwitch {
                    checked: UserSettings.showAudioLevel
                    onClicked: {
                        UserSettings.showAudioLevel = checked
                        checked = Qt.binding(function() { return UserSettings.showAudioLevel })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Panel animations")
                description: qsTr("Animate panel opening and closing")

                additionalControl: LabeledSwitch {
                    checked: UserSettings.panelAnimationsEnabled
                    onClicked: {
                        UserSettings.panelAnimationsEnabled = checked
                        checked = Qt.binding(function() { return UserSettings.panelAnimationsEnabled })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Settings page animations")
                description: qsTr("Animate transitions between settings pages")

                additionalControl: LabeledSwitch {
                    checked: UserSettings.settingsAnimationsEnabled
                    onClicked: {
                        UserSettings.settingsAnimationsEnabled = checked
                        checked = Qt.binding(function() { return UserSettings.settingsAnimationsEnabled })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Panel theme")
                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Auto"), qsTr("Dark"), qsTr("Light")]
                    currentIndex: UserSettings.panelStyle
                    onActivated: {
                        UserSettings.panelStyle = currentIndex
                        currentIndex = Qt.binding(function() { return UserSettings.panelStyle })
                        Utils.setStyle(UserSettings.panelStyle)
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Tray icon style")
                description: qsTr("Choose the appearance of the system tray icon")
                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Normal"), qsTr("Filled"), qsTr("Battery"), qsTr("AppIcon")]
                    currentIndex: UserSettings.iconStyle
                    onActivated: {
                        UserSettings.iconStyle = currentIndex
                        currentIndex = Qt.binding(function() { return UserSettings.iconStyle })
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Tray icon theme")
                enabled: UserSettings.iconStyle !== 3
                description: qsTr("Choose the color of the system tray icon")
                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Auto"), qsTr("Dark"), qsTr("Light")]
                    currentIndex: UserSettings.trayIconTheme
                    onActivated: {
                        UserSettings.trayIconTheme = currentIndex
                        currentIndex = Qt.binding(function() { return UserSettings.trayIconTheme })
                    }
                }
            }
        }
    }
}
