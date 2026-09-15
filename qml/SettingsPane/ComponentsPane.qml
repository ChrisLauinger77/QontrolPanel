pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3
    Label {
        text: qsTr("Components")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }
    Label {
        Layout.fillWidth: true
        visible: UserSettings.lastError.length > 0 || AudioBridge.lastError.length > 0
        text: UserSettings.lastError.length > 0 ? UserSettings.lastError : AudioBridge.lastError
        wrapMode: Text.Wrap
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
                    title: qsTr("Enable audio device manager")
                    description: qsTr("Display audio device manager in panel")
                    enabled: UserSettings.enableApplicationMixer || UserSettings.allowBrightnessControl
                    additionalControl: LabeledSwitch {
                        enabled: UserSettings.enableApplicationMixer || UserSettings.allowBrightnessControl
                        checked: UserSettings.enableDeviceManager
                        onClicked: {
                            UserSettings.enableDeviceManager = checked
                            checked = Qt.binding(function() { return UserSettings.enableDeviceManager })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Enable application volume mixer")
                    description: qsTr("Display application volume mixer in panel")
                    enabled: UserSettings.enableDeviceManager || UserSettings.allowBrightnessControl
                    additionalControl: LabeledSwitch {
                        enabled: UserSettings.enableDeviceManager || UserSettings.allowBrightnessControl
                        checked: UserSettings.enableApplicationMixer
                        onClicked: {
                            UserSettings.enableApplicationMixer = checked
                            checked = Qt.binding(function() { return UserSettings.enableApplicationMixer })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Remember application volume levels")
                    description: qsTr("Restore each application's last volume when it starts")
                    enabled: UserSettings.enableApplicationMixer
                    additionalControl: LabeledSwitch {
                        enabled: UserSettings.enableApplicationMixer
                        checked: UserSettings.rememberApplicationVolumes
                        onClicked: {
                            UserSettings.rememberApplicationVolumes = checked
                            checked = Qt.binding(function() { return UserSettings.rememberApplicationVolumes })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    show: UserSettings.rememberApplicationVolumes
                    title: qsTr("Clear remembered application volumes")
                    description: qsTr("Forget all saved per-application volume levels")
                    additionalControl: Button {
                        text: qsTr("Clear")
                        onClicked: AudioBridge.clearRememberedApplicationVolumes()
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Enable brightness control")
                    description: qsTr("Show brightness control in panel")
                    enabled: UserSettings.enableDeviceManager || UserSettings.enableApplicationMixer
                    additionalControl: LabeledSwitch {
                        enabled: UserSettings.enableDeviceManager || UserSettings.enableApplicationMixer
                        checked: UserSettings.allowBrightnessControl
                        onClicked: {
                            UserSettings.allowBrightnessControl = checked
                            checked = Qt.binding(function() { return UserSettings.allowBrightnessControl })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Enable power menu")
                    description: qsTr("Show power button in the panel footer")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.enablePowerMenu
                        onClicked: {
                            UserSettings.enablePowerMenu = checked
                            checked = Qt.binding(function() { return UserSettings.enablePowerMenu })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Enable HeadsetControl integration")
                    description: qsTr("Monitor battery using HeadsetControl for supported devices")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolMonitoring
                        onClicked: {
                            UserSettings.headsetcontrolMonitoring = checked
                            checked = Qt.binding(function() { return UserSettings.headsetcontrolMonitoring })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Enable media session manager")
                    description: qsTr("Display currently playing media from Windows known sources")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.enableMediaSessionManager
                        onClicked: {
                            UserSettings.enableMediaSessionManager = checked
                            checked = Qt.binding(function() { return UserSettings.enableMediaSessionManager })
                        }
                    }
                }
            }
        }
    }
}
