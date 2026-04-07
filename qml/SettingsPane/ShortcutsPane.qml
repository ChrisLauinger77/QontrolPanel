pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    id: lyt
    spacing: 3

    Label {
        text: qsTr("Global Shortcuts")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
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
                    title: qsTr("Enable global shortcuts")
                    description: qsTr("Allow QontrolPanel to respond to keyboard shortcuts globally")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.globalShortcutsEnabled
                        onClicked: {
                            UserSettings.globalShortcutsEnabled = checked
                            KeyboardShortcutManager.manageGlobalShortcuts(checked)
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Notification on ChatMix toggle")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.chatMixShortcutNotification
                        onClicked: UserSettings.chatMixShortcutNotification = checked
                    }
                }

                Card {
                    enabled: UserSettings.globalShortcutsEnabled
                    Layout.fillWidth: true
                    title: qsTr("Show/Hide Panel")
                    description: qsTr("Shortcut to toggle the main panel visibility")

                    additionalControl: RowLayout {
                        spacing: 15

                        Label {
                            text: Context.toggleShortcut
                            opacity: 0.7
                        }

                        Button {
                            text: qsTr("Change")
                            onClicked: {
                                shortcutDialog.openForPanel()
                            }
                        }
                    }
                }

                Card {
                    enabled: UserSettings.globalShortcutsEnabled
                    Layout.fillWidth: true
                    title: qsTr("Toggle ChatMix")
                    description: qsTr("Shortcut to enable/disable ChatMix feature")

                    additionalControl: RowLayout {
                        spacing: 15
                        Label {
                            text: Context.chatMixShortcut
                            opacity: 0.7
                        }

                        Button {
                            text: qsTr("Change")
                            onClicked: {
                                shortcutDialog.openForChatMix()
                            }
                        }
                    }
                }

                Card {
                    enabled: UserSettings.globalShortcutsEnabled
                    Layout.fillWidth: true
                    title: qsTr("Toggle Microphone Mute")
                    description: qsTr("Shortcut to mute/unmute microphone")

                    additionalControl: RowLayout {
                        spacing: 15
                        Label {
                            text: Context.micMuteShortcut
                            opacity: 0.7
                        }

                        Button {
                            text: qsTr("Change")
                            onClicked: {
                                shortcutDialog.openForMicMute()
                            }
                        }
                    }
                }
            }
        }

        Dialog {
            id: shortcutDialog
            modal: true
            width: 400
            anchors.centerIn: parent

            property string dialogTitle: ""
            property string shortcutType: ""
            property int tempModifiers: Qt.ControlModifier | Qt.ShiftModifier
            property int tempKey: Qt.Key_S

            title: dialogTitle

            onOpened: {
                KeyboardShortcutManager.globalShortcutsSuspended = true
                Qt.callLater(function() {
                    inputRect.forceActiveFocus()
                })
            }

            onClosed: {
                KeyboardShortcutManager.globalShortcutsSuspended = false
            }

            function openForPanel() {
                dialogTitle = qsTr("Set Panel Shortcut")
                shortcutType = "panel"
                tempModifiers = UserSettings.panelShortcutModifiers
                tempKey = UserSettings.panelShortcutKey
                open()
            }

            function openForChatMix() {
                dialogTitle = qsTr("Set ChatMix Shortcut")
                shortcutType = "chatmix"
                tempModifiers = UserSettings.chatMixShortcutModifiers
                tempKey = UserSettings.chatMixShortcutKey
                open()
            }

            function openForMicMute() {
                dialogTitle = qsTr("Set Microphone Mute Shortcut")
                shortcutType = "micmute"
                tempModifiers = UserSettings.micMuteShortcutModifiers
                tempKey = UserSettings.micMuteShortcutKey
                open()
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 15

                Label {
                    text: qsTr("Press the desired key combination")
                    Layout.fillWidth: true
                }

                Rectangle {
                    id: inputRect
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: Constants.footerColor
                    radius: 5
                    focus: true

                    Rectangle {
                        opacity: parent.focus ? 1 : 0.15
                        border.width: 1
                        border.color: parent.focus? palette.accent : Constants.footerBorderColor
                        color: Constants.footerColor
                        radius: 5
                        anchors.fill: parent

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutQuad
                            }
                        }

                        Behavior on border.color {
                            ColorAnimation {
                                duration: 200
                                easing.type: Easing.OutQuad
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: Context.getShortcutText(shortcutDialog.tempModifiers, shortcutDialog.tempKey)
                        font.family: "Consolas, monospace"
                        font.pixelSize: 16
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: parent.forceActiveFocus()
                    }

                    Keys.onPressed: function(event) {
                        let modifiers = 0
                        if (event.modifiers & Qt.ControlModifier) modifiers |= Qt.ControlModifier
                        if (event.modifiers & Qt.ShiftModifier) modifiers |= Qt.ShiftModifier
                        if (event.modifiers & Qt.AltModifier) modifiers |= Qt.AltModifier

                        shortcutDialog.tempModifiers = modifiers
                        shortcutDialog.tempKey = event.key
                        event.accepted = true
                    }
                }

                RowLayout {
                    spacing: 15
                    Button {
                        text: qsTr("Cancel")
                        onClicked: shortcutDialog.close()
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Apply")
                        highlighted: true
                        Layout.fillWidth: true
                        onClicked: {
                            if (shortcutDialog.shortcutType === "panel") {
                                UserSettings.panelShortcutModifiers = shortcutDialog.tempModifiers
                                UserSettings.panelShortcutKey = shortcutDialog.tempKey
                            } else if (shortcutDialog.shortcutType === "chatmix") {
                                UserSettings.chatMixShortcutModifiers = shortcutDialog.tempModifiers
                                UserSettings.chatMixShortcutKey = shortcutDialog.tempKey
                            } else if (shortcutDialog.shortcutType === "micmute") {
                                UserSettings.micMuteShortcutModifiers = shortcutDialog.tempModifiers
                                UserSettings.micMuteShortcutKey = shortcutDialog.tempKey
                            }
                            // Re-register hotkeys with new settings
                            if (UserSettings.globalShortcutsEnabled) {
                                KeyboardShortcutManager.manageGlobalShortcuts(true)
                            }
                            shortcutDialog.close()
                        }
                    }
                }
            }
        }
    }
}
