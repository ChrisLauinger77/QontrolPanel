import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import QtQuick
import ChrisLauinger77.QontrolPanel

Dialog {
    id: executableRenameDialog
    title: qsTr("Rename Application")
    modal: true
    width: 300
    dim: false
    property string originalName: ""

    function setNameFieldText(text) {
        executableCustomNameField.text = text
        executableCustomNameField.forceActiveFocus()
        executableCustomNameField.selectAll()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 15

        Label {
            text: qsTr("Original name: ") + executableRenameDialog.originalName
            font.bold: true
        }

        Label {
            text: qsTr("Custom name:")
        }

        TextField {
            id: executableCustomNameField
            Layout.fillWidth: true
            placeholderText: executableRenameDialog.originalName

            Keys.onReturnPressed: {
                if (AudioBridge.setCustomExecutableName(
                    executableRenameDialog.originalName,
                    executableCustomNameField.text.trim()
                ))
                    executableRenameDialog.close()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: AudioBridge.lastError.length > 0
            text: AudioBridge.lastError
            wrapMode: Text.Wrap
        }

        RowLayout {
            spacing: 15
            Layout.topMargin: 10

            Button {
                text: qsTr("Cancel")
                onClicked: executableRenameDialog.close()
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Save")
                highlighted: true
                Layout.fillWidth: true
                onClicked: {
                    if (AudioBridge.setCustomExecutableName(
                        executableRenameDialog.originalName,
                        executableCustomNameField.text.trim()
                    ))
                        executableRenameDialog.close()
                }
            }
        }
    }
}
