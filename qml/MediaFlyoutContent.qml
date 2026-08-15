import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import QtQuick.Controls.impl
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    id: mediaLayout
    opacity: 0
    spacing: 10

    Behavior on opacity {
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutQuad
        }
    }

    ColumnLayout {
        Layout.fillWidth: true

        RowLayout {
            id: infosLyt
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                Layout.alignment: Qt.AlignVCenter
                color: "#2a2a2a"
                radius: 3

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: MediaSessionBridge.mediaArt || ""
                    fillMode: Image.PreserveAspectCrop
                    visible: MediaSessionBridge.mediaArt !== ""
                    smooth: true
                }

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/icons/music.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                    color: palette.text
                    opacity: 0.3
                    visible: MediaSessionBridge.mediaArt === ""
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter

                Label {
                    text: MediaSessionBridge.mediaTitle || ""
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: MediaSessionBridge.mediaArtist || ""
                    font.pixelSize: 12
                    opacity: 0.7
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                icon.source: "qrc:/icons/prev.png"
                onClicked: MediaSessionBridge.previousTrack()
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
            }

            ToolButton {
                icon.source: MediaSessionBridge.isMediaPlaying ? "qrc:/icons/pause.png" : "qrc:/icons/play.png"
                onClicked: MediaSessionBridge.playPause()
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
            }

            ToolButton {
                icon.source: "qrc:/icons/next.png"
                onClicked: MediaSessionBridge.nextTrack()
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }
}
