import QtQuick
import QtQuick.Controls.FluentWinUI3
import QtQuick.Window
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: mediaPanelWindow
    objectName: "mainMediaWindow"
    width: 360
    height: mediaContent.implicitHeight + 30
    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#00000000"
    transientParent: null

    readonly property bool available: UserSettings.enableMediaSessionManager
                                      && MediaSessionBridge.mediaTitle !== ""
    property bool nativeBackdropActive: false
    property alias contentOpacity: mediaContent.opacity
    signal hideRequested()

    Component.onCompleted: updateNativeBackdrop()

    function updateNativeBackdrop() {
        nativeBackdropActive = WindowsBackdrop.applyTransientBackdrop(mediaPanelWindow)
    }

    Connections {
        target: Qt.application.styleHints

        function onColorSchemeChanged() {
            mediaPanelWindow.updateNativeBackdrop()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: mediaPanelWindow.hideRequested()
    }

    Rectangle {
        anchors.fill: parent
        color: mediaPanelWindow.nativeBackdropActive ? "transparent" : Constants.panelColor
        radius: 12

        Rectangle {
            anchors.fill: parent
            color: "#00000000"
            radius: 12
            border.width: 1
            border.color: "#E3E3E3"
            opacity: 0.2
        }
    }

    MediaFlyoutContent {
        id: mediaContent
        anchors.fill: parent
        anchors.margins: 15
    }
}
