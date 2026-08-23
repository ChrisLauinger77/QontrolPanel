import QtQuick
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: root
    height: 585
    minimumHeight: 585
    width: 1100
    minimumWidth: 1100
    visible: false
    transientParent: null
    flags: Qt.Window | Qt.FramelessWindowHint
    title: qsTr("QontrolPanel - Settings")
    color: "transparent"

    property bool nativeBackdropActive: false

    background: Rectangle {
        color: root.nativeBackdropActive ? "transparent" : Constants.panelColor
    }

    Component.onCompleted: Qt.callLater(updateNativeBackdrop)
    onActiveChanged: updateNativeBackdrop()
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(updateNativeBackdrop)
        }
    }

    Connections {
        target: Qt.application.styleHints

        function onColorSchemeChanged() {
            root.updateNativeBackdrop()
        }
    }

    function updateNativeBackdrop() {
        nativeBackdropActive = WindowsBackdrop.applyMainWindowBackdrop(root)
    }

    function showPrototype() {
        show()
        raise()
        requestActivate()
        Qt.callLater(updateNativeBackdrop)
    }

    function showPreferredPane() {
        showPrototype()
    }

    function showUpdatePane() {
        showPrototype()
    }

    function showHeadsetcontrolPane() {
        showPrototype()
    }

    Rectangle {
        anchors.centerIn: parent
        width: 520
        height: 260
        radius: 10
        color: Constants.darkMode ? Qt.rgba(0.16, 0.16, 0.16, 0.72)
                                  : Qt.rgba(1.0, 1.0, 1.0, 0.72)
        border.color: Constants.darkMode ? Qt.rgba(1.0, 1.0, 1.0, 0.08)
                                         : Qt.rgba(0.0, 0.0, 0.0, 0.08)

        Column {
            anchors.centerIn: parent
            spacing: 16

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Frameless Settings Mica proof")
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.nativeBackdropActive
                      ? qsTr("Windows accepted the Mica backdrop request")
                      : qsTr("Opaque fallback is active")
                opacity: 0.8
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
