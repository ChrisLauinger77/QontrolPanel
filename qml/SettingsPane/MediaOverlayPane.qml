import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3

    Label {
        text: qsTr("Media Overlay")
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
                title: qsTr("Enable media overlay")
                description: qsTr("Show a notification overlay when the currently playing song changes")

                additionalControl: LabeledSwitch {
                    checked: UserSettings.enableMediaOverlay
                    onClicked: UserSettings.enableMediaOverlay = checked
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Overlay size")
                description: qsTr("Choose the size of the media overlay")

                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Tiny"), qsTr("Normal"), qsTr("Big")]
                    currentIndex: UserSettings.mediaOverlaySize
                    onActivated: UserSettings.mediaOverlaySize = currentIndex
                    enabled: UserSettings.enableMediaOverlay
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: width * (7 / 16)
                color: Constants.cardColor
                radius: 5

                Rectangle {
                    anchors.fill: parent
                    border.width: 1
                    color: Constants.cardColor
                    border.color: Constants.cardBorderColor
                    opacity: 1
                    radius: 5
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    Label {
                        text: qsTr("Overlay position")
                        font.pixelSize: 14
                    }

                    Label {
                        text: qsTr("Choose where the media overlay appears on screen")
                        opacity: 0.6
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: -10
                        Layout.rightMargin: -10
                        Layout.bottomMargin: -10
                        color: Constants.footerColor
                        bottomLeftRadius: 5
                        bottomRightRadius: 5

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 15
                            rows: 3
                            columns: 3
                            rowSpacing: 8
                            columnSpacing: 8

                            // Row 1: Top positions
                            RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 0
                            onClicked: UserSettings.mediaOverlayPosition = 0
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Top Left")
                        }

                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 1
                            onClicked: UserSettings.mediaOverlayPosition = 1
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Top Center")
                        }

                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 2
                            onClicked: UserSettings.mediaOverlayPosition = 2
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Top Right")
                        }

                        // Row 2: Middle positions
                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 3
                            onClicked: UserSettings.mediaOverlayPosition = 3
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Left")
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                        }

                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 4
                            onClicked: UserSettings.mediaOverlayPosition = 4
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Right")
                        }

                        // Row 3: Bottom positions
                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 5
                            onClicked: UserSettings.mediaOverlayPosition = 5
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Bottom Left")
                        }

                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 6
                            onClicked: UserSettings.mediaOverlayPosition = 6
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Bottom Center")
                        }

                        RadioButton {
                            Layout.alignment: Qt.AlignCenter
                            checked: UserSettings.mediaOverlayPosition === 7
                            onClicked: UserSettings.mediaOverlayPosition = 7
                            enabled: UserSettings.enableMediaOverlay
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Bottom Right")
                        }
                        }
                    }
                }
            }
        }
    }
}
