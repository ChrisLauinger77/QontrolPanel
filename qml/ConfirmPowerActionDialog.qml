import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

Dialog {
    id: dialog
    closePolicy: Popup.NoAutoClose
    property int minimumDialogWidth: 320
    property int maximumDialogWidth: Math.min(520, parent ? parent.width - 32 : 520)
    property int contentHorizontalPadding: 48
    title: {
        switch (action) {
        case 0:
            return qsTr("Hibernate")
        case 1:
            return qsTr("Restart")
        case 2:
            return qsTr("Shutdown")
        case 3:
            return qsTr("Sign Out")
        case 4:
            return qsTr("Restart to UEFI")
        default:
            return qsTr("Action")
        }
    }
    width: Math.max(minimumDialogWidth,
                    Math.min(maximumDialogWidth,
                             Math.max(messageLabel.implicitWidth + contentHorizontalPadding,
                                      actionButtons.implicitWidth + contentHorizontalPadding)))
    signal requestClose()
    property int countdownSeconds: 30
    property string text: {
        switch (action) {
        case 0:
            return qsTr("System will hibernate in %1 seconds").arg(countdownTimer.remaining)
        case 1:
            return qsTr("System will restart in %1 seconds").arg(countdownTimer.remaining)
        case 2:
            return qsTr("System will shutdown in %1 seconds").arg(countdownTimer.remaining)
        case 3:
            return qsTr("You will be signed out in %1 seconds").arg(countdownTimer.remaining)
        case 4:
            return qsTr("System will restart to UEFI in %1 seconds").arg(countdownTimer.remaining)
        default:
            return ""
        }
    }
    property int action: 0
    property string actionText: {
        switch (action) {
        case 0:
            return qsTr("Hibernate")
        case 1:
            return qsTr("Restart")
        case 2:
            return qsTr("Shutdown")
        case 3:
            return qsTr("Sign Out")
        default:
            return qsTr("OK")
        }
    }
    Timer {
        id: countdownTimer
        property int remaining: dialog.countdownSeconds
        interval: 1000
        repeat: true
        running: dialog.visible
        onTriggered: {
            remaining--
            if (remaining <= 0) {
                stop()
                dialog.close()
                dialog.performAction()
            }
        }
        onRunningChanged: {
            if (running) {
                remaining = dialog.countdownSeconds
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            id: messageLabel
            text: dialog.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.preferredWidth: dialog.width - dialog.contentHorizontalPadding
        }

        ProgressBar {
            from: 0
            to: dialog.countdownSeconds
            value: countdownTimer.remaining
            Layout.fillWidth: true

            Behavior on value {
                NumberAnimation {
                    duration: 1000
                    easing.type: Easing.Linear
                }
            }
        }
    }

    function performAction() {
        switch (action) {
        case 0:
            PowerBridge.hibernate()
            break
        case 1:
            PowerBridge.restart()
            break
        case 2:
            PowerBridge.shutdown()
            break
        case 3:
            PowerBridge.signOut()
            break
        case 4:
            PowerBridge.restartToUEFI()
            break
        default:
            break
        }
    }
    footer: DialogButtonBox {
        id: actionButtons
        spacing: 10
        Button {
            text: qsTr("Cancel")
            onClicked: {
                countdownTimer.stop()
                dialog.requestClose()
            }
        }
        Button {
            text: dialog.actionText
            highlighted: true
            onClicked: {
                countdownTimer.stop()
                dialog.close()
                dialog.requestClose()
                dialog.performAction()
            }
        }
    }
}
