import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls.FluentWinUI3
import QtQuick.Controls.impl
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    id: mediaLayout
    opacity: 0
    spacing: 10

    property real displayedPositionMs: 0
    property real positionBaselineMs: 0
    property real positionBaselineTimestampMs: 0
    property bool positionBaselinePlaying: false
    property real positionBaselinePlaybackRate: 1
    property bool timelineIdentityValid: false
    property string timelineSourceName: ""
    property string timelineTitle: ""
    property string timelineArtist: ""
    property real timelineDurationMs: 0
    readonly property real timelineCorrectionToleranceMs: 1500
    property bool seekGestureActive: false
    property string seekSourceName: ""
    property string seekTitle: ""
    property string seekArtist: ""
    property real seekDurationMs: 0

    function clampPosition(positionMs) {
        return Math.max(0, Math.min(MediaSessionBridge.mediaDurationMs, positionMs))
    }

    function setDisplayedPosition(positionMs) {
        displayedPositionMs = clampPosition(positionMs)
        if (!timelineSlider.pressed) {
            timelineSlider.value = displayedPositionMs
        }
    }

    function formatMediaTime(positionMs) {
        const totalSeconds = Math.floor(Math.max(0, positionMs) / 1000)
        const seconds = totalSeconds % 60
        const totalMinutes = Math.floor(totalSeconds / 60)
        const minutes = totalMinutes % 60
        const hours = Math.floor(totalMinutes / 60)
        const paddedSeconds = seconds < 10 ? "0" + seconds : seconds.toString()
        if (hours > 0) {
            const paddedMinutes = minutes < 10 ? "0" + minutes : minutes.toString()
            return hours + ":" + paddedMinutes + ":" + paddedSeconds
        }
        return totalMinutes + ":" + paddedSeconds
    }

    function projectedPosition(timestampMs) {
        let projectedPosition = positionBaselineMs
        if (positionBaselinePlaying) {
            projectedPosition += (timestampMs - positionBaselineTimestampMs)
                * positionBaselinePlaybackRate
        }
        return clampPosition(projectedPosition)
    }

    function synchronizeTimeline() {
        if (seekGestureActive) {
            return
        }

        const timestampMs = Date.now()
        const nativePositionMs = clampPosition(MediaSessionBridge.mediaPositionMs)
        const sameTimeline = timelineIdentityValid
            && MediaSessionBridge.hasMediaTimeline
            && MediaSessionBridge.sourceName === timelineSourceName
            && MediaSessionBridge.mediaTitle === timelineTitle
            && MediaSessionBridge.mediaArtist === timelineArtist
            && MediaSessionBridge.mediaDurationMs === timelineDurationMs

        let synchronizedPositionMs = nativePositionMs
        if (sameTimeline && positionBaselinePlaying && MediaSessionBridge.isMediaPlaying) {
            const projectedPositionMs = projectedPosition(timestampMs)
            if (Math.abs(nativePositionMs - projectedPositionMs)
                    <= timelineCorrectionToleranceMs) {
                synchronizedPositionMs = projectedPositionMs
            }
        }

        timelineIdentityValid = MediaSessionBridge.hasMediaTimeline
        timelineSourceName = MediaSessionBridge.sourceName
        timelineTitle = MediaSessionBridge.mediaTitle
        timelineArtist = MediaSessionBridge.mediaArtist
        timelineDurationMs = MediaSessionBridge.mediaDurationMs
        positionBaselineMs = synchronizedPositionMs
        positionBaselineTimestampMs = timestampMs
        positionBaselinePlaying = MediaSessionBridge.isMediaPlaying
        positionBaselinePlaybackRate = Number.isFinite(MediaSessionBridge.mediaPlaybackRate)
            ? MediaSessionBridge.mediaPlaybackRate : 1
        setDisplayedPosition(positionBaselineMs)
    }

    function updateDisplayedPosition() {
        if (!MediaSessionBridge.hasMediaTimeline || seekGestureActive) {
            return
        }
        setDisplayedPosition(projectedPosition(Date.now()))
    }

    function beginSeek() {
        seekGestureActive = true
        seekSourceName = MediaSessionBridge.sourceName
        seekTitle = MediaSessionBridge.mediaTitle
        seekArtist = MediaSessionBridge.mediaArtist
        seekDurationMs = MediaSessionBridge.mediaDurationMs
    }

    function finishSeek(positionMs) {
        if (!seekGestureActive) {
            return
        }
        seekGestureActive = false

        const timelineUnchanged = MediaSessionBridge.hasMediaTimeline
            && MediaSessionBridge.canSeek
            && MediaSessionBridge.sourceName === seekSourceName
            && MediaSessionBridge.mediaTitle === seekTitle
            && MediaSessionBridge.mediaArtist === seekArtist
            && MediaSessionBridge.mediaDurationMs === seekDurationMs
        if (!timelineUnchanged) {
            synchronizeTimeline()
            return
        }

        const targetPosition = Math.max(MediaSessionBridge.mediaMinimumSeekMs,
            Math.min(MediaSessionBridge.mediaMaximumSeekMs, positionMs))
        positionBaselineMs = targetPosition
        positionBaselineTimestampMs = Date.now()
        positionBaselinePlaying = MediaSessionBridge.isMediaPlaying
        positionBaselinePlaybackRate = Number.isFinite(MediaSessionBridge.mediaPlaybackRate)
            ? MediaSessionBridge.mediaPlaybackRate : 1
        setDisplayedPosition(targetPosition)
        MediaSessionBridge.seekTo(Math.round(targetPosition))
    }

    function finishOpacityAnimation() {
        opacityAnimation.stop()
        opacity = 1
    }

    Component.onCompleted: synchronizeTimeline()

    Connections {
        target: MediaSessionBridge

        function onMediaInfoChanged() {
            mediaLayout.synchronizeTimeline()
        }
    }

    Timer {
        interval: 250
        repeat: true
        running: mediaLayout.Window.window !== null
            && mediaLayout.Window.window.visible
            && MediaSessionBridge.hasMediaTimeline
            && MediaSessionBridge.isMediaPlaying
            && !mediaLayout.seekGestureActive
        onTriggered: mediaLayout.updateDisplayedPosition()
    }

    Behavior on opacity {
        enabled: UserSettings.panelAnimationsEnabled
        NumberAnimation {
            id: opacityAnimation
            duration: 300
            easing.type: Easing.OutQuad
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 24
        spacing: 8

        Item {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18

            Image {
                anchors.fill: parent
                source: MediaSessionBridge.sourceIcon || ""
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: MediaSessionBridge.sourceIcon !== ""
            }

            IconImage {
                anchors.fill: parent
                source: "qrc:/icons/music.svg"
                color: palette.text
                opacity: 0.7
                visible: MediaSessionBridge.sourceIcon === ""
            }
        }

        Label {
            text: MediaSessionBridge.sourceName || ""
            font.pixelSize: 12
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        ToolButton {
            id: sourceSwitchButton
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            visible: MediaSessionBridge.sourceCount > 1

            contentItem: Text {
                text: "\uE76C"
                font.family: "Segoe Fluent Icons"
                font.pixelSize: 12
                color: sourceSwitchButton.palette.buttonText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: MediaSessionBridge.nextSource()
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
                color: Constants.cardColor
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
            id: timelineRow
            objectName: "mediaTimelineRow"
            Layout.fillWidth: true
            visible: MediaSessionBridge.hasMediaTimeline
                && MediaSessionBridge.mediaDurationMs > 0
            spacing: 8

            Label {
                objectName: "mediaElapsedTime"
                text: mediaLayout.formatMediaTime(mediaLayout.displayedPositionMs)
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
                Layout.minimumWidth: 36
            }

            Slider {
                id: timelineSlider
                objectName: "mediaTimelineSlider"
                focusPolicy: Qt.NoFocus
                enabled: MediaSessionBridge.canSeek
                from: enabled ? MediaSessionBridge.mediaMinimumSeekMs : 0
                to: enabled ? MediaSessionBridge.mediaMaximumSeekMs
                            : MediaSessionBridge.mediaDurationMs
                Layout.fillWidth: true

                onMoved: mediaLayout.displayedPositionMs = value
                onPressedChanged: {
                    if (pressed) {
                        mediaLayout.beginSeek()
                    } else {
                        mediaLayout.finishSeek(value)
                    }
                }
            }

            Label {
                objectName: "mediaRemainingTime"
                text: "-" + mediaLayout.formatMediaTime(
                    MediaSessionBridge.mediaDurationMs - mediaLayout.displayedPositionMs)
                font.pixelSize: 11
                Layout.minimumWidth: 42
            }
        }

        RowLayout {
            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                objectName: "mediaPreviousButton"
                icon.source: "qrc:/icons/prev.png"
                enabled: MediaSessionBridge.canPreviousTrack
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
