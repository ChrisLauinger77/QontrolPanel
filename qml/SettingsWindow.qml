import QtQuick
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: root
    height: 585
    minimumHeight: 360
    width: 1100
    minimumWidth: 500
    visible: false
    transientParent: null
    flags: Qt.Window | Qt.FramelessWindowHint
    title: qsTr("QontrolPanel - Settings")
    color: "transparent"

    property bool nativeBackdropActive: false

    background: Rectangle {
        color: root.nativeBackdropActive ? "transparent" : Constants.panelColor
    }

    Component.onCompleted: Qt.callLater(initializeNativeWindow)
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

    function initializeNativeWindow() {
        WindowChrome.installWindowChrome(
                    root,
                    titleBar,
                    systemMenuButton,
                    minimizeButton,
                    maximizeButton,
                    closeButton)
        updateNativeBackdrop()
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

    Item {
        id: titleBar
        z: 10
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32

        Item {
            id: systemMenuButton
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 42

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "qrc:/icons/icon.png"
                fillMode: Image.PreserveAspectFit
            }
        }

        Label {
            anchors.left: systemMenuButton.right
            anchors.right: captionButtons.left
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            elide: Text.ElideRight
            font.pixelSize: 12
        }

        Row {
            id: captionButtons
            anchors.top: parent.top
            anchors.right: parent.right
            height: parent.height

            Item {
                id: minimizeButton
                width: 46
                height: captionButtons.height

                Rectangle {
                    anchors.fill: parent
                    color: minimizeHover.hovered
                           ? (Constants.darkMode
                              ? Qt.rgba(1.0, 1.0, 1.0, 0.09)
                              : Qt.rgba(0.0, 0.0, 0.0, 0.06))
                           : "transparent"
                }

                Text {
                    anchors.centerIn: parent
                    text: "\uE921"
                    color: Constants.darkMode ? "white" : "black"
                    font.family: "Segoe Fluent Icons"
                    font.pixelSize: 10
                }

                HoverHandler {
                    id: minimizeHover
                }

                TapHandler {
                    onTapped: root.showMinimized()
                }
            }

            Item {
                id: maximizeButton
                width: 46
                height: captionButtons.height

                Rectangle {
                    anchors.fill: parent
                    color: WindowChrome.maximizeButtonPressed
                           ? (Constants.darkMode
                              ? Qt.rgba(1.0, 1.0, 1.0, 0.06)
                              : Qt.rgba(0.0, 0.0, 0.0, 0.04))
                           : WindowChrome.maximizeButtonHovered
                             ? (Constants.darkMode
                                ? Qt.rgba(1.0, 1.0, 1.0, 0.09)
                                : Qt.rgba(0.0, 0.0, 0.0, 0.06))
                             : "transparent"
                }

                Text {
                    anchors.centerIn: parent
                    text: root.visibility === Window.Maximized ? "\uE923" : "\uE922"
                    color: Constants.darkMode ? "white" : "black"
                    font.family: "Segoe Fluent Icons"
                    font.pixelSize: 10
                }
            }

            Item {
                id: closeButton
                width: 46
                height: captionButtons.height

                Rectangle {
                    anchors.fill: parent
                    color: closeTap.pressed ? "#a7190f"
                                            : closeHover.hovered ? "#c42b1c"
                                                                 : "transparent"
                }

                Text {
                    anchors.centerIn: parent
                    text: "\uE8BB"
                    color: closeHover.hovered || closeTap.pressed
                           ? "white"
                           : Constants.darkMode ? "white" : "black"
                    font.family: "Segoe Fluent Icons"
                    font.pixelSize: 10
                }

                HoverHandler {
                    id: closeHover
                }

                TapHandler {
                    id: closeTap
                    onTapped: root.close()
                }
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 520
        height: 220
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
                text: qsTr("Frameless Settings chrome proof")
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

        }
    }
}
