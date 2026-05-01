pragma ComponentBehavior: Bound

import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import QtQuick
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: root
    height: 585
    minimumHeight: 585
    width: 1100
    minimumWidth: 1100
    visible: false
    transientParent: null
    title: qsTr("QontrolPanel - Settings")

    readonly property int maxSettingsPageIndex: 11

    function pageComponentForIndex(index) {
        switch (index) {
        case 0:
            return generalPaneComponent;
        case 1:
            return componentsPaneComponent;
        case 2:
            return appearancePaneComponent;
        case 3:
            return mediaOverlayPaneComponent;
        case 4:
            return commAppsPaneComponent;
        case 5:
            return shortcutsPaneComponent;
        case 6:
            return appHotkeysPaneComponent;
        case 7:
            return headsetControlPaneComponent;
        case 8:
            return deviceRenamingPaneComponent;
        case 9:
            return languagePaneComponent;
        case 10:
            return updatePaneComponent;
        case 11:
            return debugPaneComponent;
        default:
            return generalPaneComponent;
        }
    }

    function openPage(index, forceOpen) {
        const safeIndex = Math.max(0, Math.min(index, maxSettingsPageIndex));
        const component = pageComponentForIndex(safeIndex);
        const shouldNavigate = forceOpen || sidebarList.currentIndex !== safeIndex || !stackView.currentItem;

        sidebarList.currentIndex = safeIndex;

        if (!shouldNavigate) {
            return;
        }

        if (stackView.depth === 0) {
            stackView.push(component);
        } else {
            stackView.replace(component);
        }
    }

    function showPreferredPane() {
        show();
        openPage(UserSettings.settingsStartupPage, true);
    }

    function showUpdatePane() {
        show();
        openPage(10, true);
    }

    function showHeadsetcontrolPane() {
        show();
        openPage(7, true);
    }

    property int rowHeight: 35

    DonatePopup {
        id: donatePopup
        anchors.centerIn: parent
    }

    Popup {
        id: easterEggDialog
        anchors.centerIn: parent
        width: mainPopupLyt.implicitWidth + 50
        height: implicitHeight + 20
        modal: true

        Connections {
            target: Context
            function onEasterEggRequested() {
                easterEggDialog.open();
            }
        }

        ColumnLayout {
            id: mainPopupLyt
            spacing: 15
            anchors.fill: parent

            Label {
                text: qsTr("You just lost the game")
                font.bold: true
                font.pixelSize: 18
                Layout.alignment: Qt.AlignCenter
            }

            Button {
                text: qsTr("Too bad")
                onClicked: easterEggDialog.close()
                Layout.alignment: Qt.AlignCenter
            }
        }
    }

    RowLayout {
        anchors.fill: parent
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
                    interactive: false
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
                                root.openPage(9, true);
                            });
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
                        onClicked: {
                            root.openPage(index, false);
                        }
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

            popEnter: Transition {
                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 150
                        easing.type: Easing.InQuint
                    }
                    NumberAnimation {
                        property: "y"
                        from: (stackView.mirrored ? -0.3 : 0.3) * -stackView.width
                        to: 0
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }
            }

            pushEnter: Transition {
                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 150
                        easing.type: Easing.InQuint
                    }
                    NumberAnimation {
                        property: "y"
                        from: (stackView.mirrored ? -0.3 : 0.3) * stackView.width
                        to: 0
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }
            }

            popExit: Transition {
                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: 150
                    easing.type: Easing.OutQuint
                }
            }

            pushExit: Transition {
                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: 150
                    easing.type: Easing.OutQuint
                }
            }

            replaceEnter: Transition {
                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 150
                        easing.type: Easing.InQuint
                    }
                    NumberAnimation {
                        property: "y"
                        from: (stackView.mirrored ? -0.3 : 0.3) * stackView.width
                        to: 0
                        duration: 300
                        easing.type: Easing.OutCubic
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
