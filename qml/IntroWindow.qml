import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: introWindow
    width: mainLyt.implicitWidth + 40
    height: mainLyt.implicitHeight + 40
    visible: false
    flags: Qt.FramelessWindowHint | Qt.ToolTip
    color: "#00000000"
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    transientParent: null

    function showIntro() {
        visible = true
    }

    function closeIntro() {
        visible = false
        UserSettings.firstRun = false
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        color: palette.window
        radius: 12

        Rectangle {
            anchors.fill: parent
            color: "#00000000"
            radius: 12
            border.width: 1
            border.color: Constants.separatorColor
            opacity: 0.2
        }

        ColumnLayout {
            id: mainLyt
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20

            Label {
                text: qsTr("Welcome to QontrolPanel!")
                font.pixelSize: 22
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            MenuSeparator { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 40
                Label {
                    text: qsTr("Automatic application update")
                    Layout.fillWidth: true
                }

                LabeledSwitch {
                    checked: UserSettings.autoFetchForAppUpdates
                    onClicked: UserSettings.autoFetchForAppUpdates = checked
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 40
                Label {
                    text: qsTr("Automatic translations update")
                    Layout.fillWidth: true
                }

                LabeledSwitch {
                    checked: UserSettings.autoUpdateTranslations
                    onClicked: UserSettings.autoUpdateTranslations = checked
                }
            }

            MenuSeparator { Layout.fillWidth: true }

            Button {
                text: qsTr("Get Started")
                highlighted: true
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                onClicked: introWindow.closeIntro()
            }
        }
    }
}
