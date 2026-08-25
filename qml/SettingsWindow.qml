pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: root
    height: 617
    minimumHeight: 500
    width: 1100
    minimumWidth: 1100
    visible: false
    transientParent: null
    flags: Qt.Window | Qt.FramelessWindowHint
    title: qsTr("QontrolPanel - Settings")
    color: "transparent"

    readonly property int maxSettingsPageIndex: 11
    property bool nativeBackdropActive: false
    property bool nativeBackdropSuppressed: false
    property int rowHeight: 35

    background: Rectangle {
        color: root.nativeBackdropActive ? "transparent" : Constants.panelColor
    }

    Component.onCompleted: Qt.callLater(initializeNativeWindow)
    onClosing: function(close) {
        if (visible) {
            close.accepted = false
            hide()
        }
    }
    onActiveChanged: updateNativeBackdrop()
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(updateNativeBackdrop)
        } else {
            WindowsBackdrop.removeBackdrop(root)
            nativeBackdropActive = false
            nativeBackdropSuppressed = false
        }
    }

    Connections {
        target: Qt.application.styleHints

        function onColorSchemeChanged() {
            // DWM does not reliably retheme Mica on a visible Qt window.
            // Keep this instance readable and retry after it is reopened.
            root.nativeBackdropSuppressed = root.visible
            root.nativeBackdropActive = false
            WindowsBackdrop.removeBackdrop(root)
        }
    }

    function updateNativeBackdrop() {
        if (!visible || nativeBackdropSuppressed) {
            nativeBackdropActive = false
            return
        }
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

    function pageComponentForIndex(index) {
        switch (index) {
        case 0:
            return generalPaneComponent
        case 1:
            return componentsPaneComponent
        case 2:
            return appearancePaneComponent
        case 3:
            return mediaOverlayPaneComponent
        case 4:
            return commAppsPaneComponent
        case 5:
            return shortcutsPaneComponent
        case 6:
            return appHotkeysPaneComponent
        case 7:
            return headsetControlPaneComponent
        case 8:
            return deviceRenamingPaneComponent
        case 9:
            return languagePaneComponent
        case 10:
            return updatePaneComponent
        case 11:
            return debugPaneComponent
        default:
            return generalPaneComponent
        }
    }

    function openPage(index, forceOpen) {
        const safeIndex = Math.max(0, Math.min(index, maxSettingsPageIndex))
        const component = pageComponentForIndex(safeIndex)
        const shouldNavigate = forceOpen
                || sidebarList.currentIndex !== safeIndex
                || !stackView.currentItem

        sidebarList.currentIndex = safeIndex
        if (!shouldNavigate) {
            return
        }

        if (stackView.depth === 0) {
            stackView.push(component)
        } else {
            stackView.replace(component)
        }
    }

    function showPreferredPane() {
        show()
        openPage(UserSettings.settingsStartupPage, true)
        raise()
        requestActivate()
    }

    function showUpdatePane() {
        show()
        openPage(10, true)
        raise()
        requestActivate()
    }

    function showHeadsetcontrolPane() {
        show()
        openPage(7, true)
        raise()
        requestActivate()
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

    DonatePopup {
        id: donatePopup
        anchors.centerIn: parent
    }

    RowLayout {
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 15
        spacing: 15

        Item {
            Layout.preferredWidth: 200
            Layout.preferredHeight: 35
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                ListView {
                    id: sidebarList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    interactive: contentHeight > height
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }
                    model: [
                        {
                            text: qsTr("General"),
                            icon: "qrc:/icons/general.svg"
                        },
                        {
                            text: qsTr("Components"),
                            icon: "qrc:/icons/component.svg"
                        },
                        {
                            text: qsTr("Appearance"),
                            icon: "qrc:/icons/wand.svg"
                        },
                        {
                            text: qsTr("Media Overlay"),
                            icon: "qrc:/icons/music.svg"
                        },
                        {
                            text: qsTr("ChatMix"),
                            icon: "qrc:/icons/chatmix.svg"
                        },
                        {
                            text: qsTr("Shortcuts"),
                            icon: "qrc:/icons/keyboard.svg"
                        },
                        {
                            text: qsTr("App Hotkeys"),
                            icon: "qrc:/icons/panel_volume_66.svg"
                        },
                        {
                            text: qsTr("HeadsetControl"),
                            icon: "qrc:/icons/headsetcontrol.svg"
                        },
                        {
                            text: qsTr("Renaming"),
                            icon: "qrc:/icons/rename.svg"
                        },
                        {
                            text: qsTr("Language"),
                            icon: "qrc:/icons/language.svg"
                        },
                        {
                            text: qsTr("Updates"),
                            icon: "qrc:/icons/update.svg"
                        },
                        {
                            text: qsTr("Debug"),
                            icon: "qrc:/icons/chip.svg"
                        }
                    ]
                    currentIndex: 0

                    Connections {
                        target: UserSettings

                        function onLanguageIndexChanged() {
                            Qt.callLater(function () {
                                root.openPage(9, true)
                            })
                        }
                    }

                    delegate: ItemDelegate {
                        id: del
                        width: sidebarList.width
                        height: 43
                        spacing: 10
                        required property var modelData
                        required property int index

                        highlighted: ListView.isCurrentItem
                        icon.source: del.modelData.icon
                        text: del.modelData.text
                        icon.width: 18
                        icon.height: 18
                        opacity: text === qsTr("Debug") && !ListView.isCurrentItem ? 0.5 : 1
                        onClicked: root.openPage(index, false)
                    }
                }

                ItemDelegate {
                    text: qsTr("Donate")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 43
                    spacing: 10
                    icon.color: "#f05670"
                    icon.source: "qrc:/icons/donate.svg"
                    icon.width: 18
                    icon.height: 18
                    onClicked: donatePopup.open()
                }
            }
        }

        StackView {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: generalPaneComponent
            readonly property bool settingsPageTransitionsEnabled: UserSettings.settingsAnimationsEnabled
            readonly property int settingsPageFadeDuration: UserSettings.settingsAnimationsEnabled ? 150 : 0
            readonly property int settingsPageSlideDuration: UserSettings.settingsAnimationsEnabled ? 300 : 0
            readonly property real settingsPageSlideDistance: Math.min(32, stackView.width * 0.3)

            popEnter: Transition {
                enabled: stackView.settingsPageTransitionsEnabled

                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: stackView.settingsPageFadeDuration
                        easing.type: Easing.InQuint
                    }
                    NumberAnimation {
                        property: "x"
                        from: (stackView.mirrored ? 1 : -1) * stackView.settingsPageSlideDistance
                        to: 0
                        duration: stackView.settingsPageSlideDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            pushEnter: Transition {
                enabled: stackView.settingsPageTransitionsEnabled

                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: stackView.settingsPageFadeDuration
                        easing.type: Easing.InQuint
                    }
                    NumberAnimation {
                        property: "x"
                        from: (stackView.mirrored ? -1 : 1) * stackView.settingsPageSlideDistance
                        to: 0
                        duration: stackView.settingsPageSlideDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            popExit: Transition {
                enabled: stackView.settingsPageTransitionsEnabled

                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: stackView.settingsPageFadeDuration
                    easing.type: Easing.OutQuint
                }
            }

            pushExit: Transition {
                enabled: stackView.settingsPageTransitionsEnabled

                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: stackView.settingsPageFadeDuration
                    easing.type: Easing.OutQuint
                }
            }

            replaceEnter: Transition {
                enabled: stackView.settingsPageTransitionsEnabled

                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: stackView.settingsPageFadeDuration
                        easing.type: Easing.InQuint
                    }
                    NumberAnimation {
                        property: "x"
                        from: (stackView.mirrored ? -1 : 1) * stackView.settingsPageSlideDistance
                        to: 0
                        duration: stackView.settingsPageSlideDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            replaceExit: Transition {
                enabled: stackView.settingsPageTransitionsEnabled

                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 1
                        to: 0
                        duration: stackView.settingsPageFadeDuration
                        easing.type: Easing.OutQuint
                    }
                    NumberAnimation {
                        property: "x"
                        from: 0
                        to: (stackView.mirrored ? 1 : -1) * stackView.settingsPageSlideDistance
                        duration: stackView.settingsPageSlideDuration
                        easing.type: Easing.InCubic
                    }
                }
            }

            Component {
                id: generalPaneComponent
                GeneralPane {}
            }

            Component {
                id: componentsPaneComponent
                ComponentsPane {}
            }

            Component {
                id: languagePaneComponent
                LanguagePane {}
            }

            Component {
                id: commAppsPaneComponent
                CommAppsPane {}
            }

            Component {
                id: shortcutsPaneComponent
                ShortcutsPane {}
            }

            Component {
                id: appHotkeysPaneComponent
                AppHotkeysPane {}
            }

            Component {
                id: appearancePaneComponent
                AppearancePane {}
            }

            Component {
                id: mediaOverlayPaneComponent
                MediaOverlayPane {}
            }

            Component {
                id: headsetControlPaneComponent
                HeadsetControlPane {}
            }

            Component {
                id: deviceRenamingPaneComponent
                DeviceRenamingPane {}
            }

            Component {
                id: updatePaneComponent
                UpdatePane {}
            }

            Component {
                id: debugPaneComponent
                DebugPane {}
            }
        }
    }
}
