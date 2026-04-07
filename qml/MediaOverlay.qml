import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import QtQuick.Controls.impl
import QtQuick.Window
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: mediaOverlayWindow
    width: calculateWidth()
    height: calculateHeight()
    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#00000000"

    property bool isAnimatingIn: false
    property bool isAnimatingOut: false
    property string previousTitle: ""
    property string previousArtist: ""
    property bool hasReceivedFirstUpdate: false

    transientParent: null

    function calculateWidth() {
        // Size multipliers: tiny=0.8, normal=1.0, big=1.2
        const sizeMultipliers = [0.8, 1.0, 1.2]
        const multiplier = sizeMultipliers[UserSettings.mediaOverlaySize] || 1.0
        return Math.round(300 * multiplier)
    }

    function calculateHeight() {
        const sizeMultipliers = [0.8, 1.0, 1.2]
        const multiplier = sizeMultipliers[UserSettings.mediaOverlaySize] || 1.0
        return Math.round(80 * multiplier)
    }

    Component.onCompleted: {
        positionWindow()
    }

    Connections {
        target: UserSettings

        function onMediaOverlayPositionChanged() {
            positionWindow()
        }

        function onMediaOverlaySizeChanged() {
            width = calculateWidth()
            height = calculateHeight()
            positionWindow()
        }
    }

    Connections {
        target: MediaSessionBridge

        function onMediaInfoChanged() {
            if (!UserSettings.enableMediaOverlay || !UserSettings.enableMediaSessionManager) {
                return
            }

            const newTitle = MediaSessionBridge.mediaTitle || ""
            const newArtist = MediaSessionBridge.mediaArtist || ""

            // First update after component creation - just store the values, don't show overlay
            if (!hasReceivedFirstUpdate) {
                previousTitle = newTitle
                previousArtist = newArtist
                hasReceivedFirstUpdate = true
                return
            }

            // Only show overlay if song changed (title or artist is different)
            if (newTitle !== "" && (newTitle !== previousTitle || newArtist !== previousArtist)) {
                previousTitle = newTitle
                previousArtist = newArtist
                showOverlay()
            }
        }
    }

    Timer {
        id: autoHideTimer
        interval: 3000
        repeat: false
        onTriggered: mediaOverlayWindow.hideOverlay()
    }

    PropertyAnimation {
        id: showAnimation
        target: contentTransform
        duration: 300
        easing.type: Easing.OutQuad
        onStarted: {
            mediaOverlayWindow.isAnimatingIn = true
            contentOpacityAnimation.start()
        }
        onFinished: {
            mediaOverlayWindow.isAnimatingIn = false
            autoHideTimer.start()
        }
    }

    PropertyAnimation {
        id: hideAnimation
        target: contentTransform
        duration: 250
        easing.type: Easing.OutQuad
        onStarted: {
            mediaOverlayWindow.isAnimatingOut = true
            hideOpacityAnimation.start()
        }
        onFinished: {
            mediaOverlayWindow.visible = false
            mediaOverlayWindow.isAnimatingOut = false
            resetTransform()
        }
    }

    PropertyAnimation {
        id: contentOpacityAnimation
        target: overlayRect
        property: "opacity"
        duration: 200
        easing.type: Easing.OutQuad
        from: 0
        to: 1
    }

    PropertyAnimation {
        id: hideOpacityAnimation
        target: overlayRect
        property: "opacity"
        duration: 150
        easing.type: Easing.InQuad
        from: 1
        to: 0
    }

    Translate {
        id: contentTransform
        x: 0
        y: 0
    }

    function showOverlay() {
        if (visible || isAnimatingIn) {
            autoHideTimer.restart()
            return
        }

        positionWindow()

        if (isAnimatingOut) {
            hideAnimation.stop()
            hideOpacityAnimation.stop()
            isAnimatingOut = false

            animateFrom(contentTransform.x, contentTransform.y)
            return
        }

        setInitialTransform()
        overlayRect.opacity = 0

        visible = true
        animateFrom(contentTransform.x, contentTransform.y)
    }

    function hideOverlay() {
        if (isAnimatingOut) {
            return
        }

        if (isAnimatingIn) {
            showAnimation.stop()
            contentOpacityAnimation.stop()
            isAnimatingIn = false
        }

        autoHideTimer.stop()
        animateToHide()
        hideAnimation.start()
    }

    function positionWindow() {
        const screenWidth = Utils.getAvailableDesktopWidth()
        const screenHeight = Utils.getAvailableDesktopHeight()
        const taskbarOffset = UserSettings.taskbarOffset
        const margin = 12

        // Position mapping: 0=top-left, 1=top-center, 2=top-right, 3=left, 4=right, 5=bottom-left, 6=bottom-center, 7=bottom-right
        switch (UserSettings.mediaOverlayPosition) {
        case 0: // top-left
            x = margin
            y = taskbarOffset + margin
            break
        case 1: // top-center
            x = (screenWidth - width) / 2
            y = taskbarOffset + margin
            break
        case 2: // top-right
            x = screenWidth - width - margin
            y = taskbarOffset + margin
            break
        case 3: // left
            x = margin
            y = (screenHeight - height) / 2
            break
        case 4: // right
            x = screenWidth - width - margin
            y = (screenHeight - height) / 2
            break
        case 5: // bottom-left
            x = margin
            y = screenHeight - height - taskbarOffset - margin
            break
        case 6: // bottom-center
            x = (screenWidth - width) / 2
            y = screenHeight - height - taskbarOffset - margin
            break
        case 7: // bottom-right
            x = screenWidth - width - margin
            y = screenHeight - height - taskbarOffset - margin
            break
        default: // fallback to top-center
            x = (screenWidth - width) / 2
            y = taskbarOffset + margin
            break
        }
    }

    function setInitialTransform() {
        // Set initial transform based on position
        switch (UserSettings.mediaOverlayPosition) {
        case 0: // top-left
        case 1: // top-center
        case 2: // top-right
            contentTransform.x = 0
            contentTransform.y = -height
            break
        case 3: // left
            contentTransform.x = -width
            contentTransform.y = 0
            break
        case 4: // right
            contentTransform.x = width
            contentTransform.y = 0
            break
        case 5: // bottom-left
        case 6: // bottom-center
        case 7: // bottom-right
            contentTransform.x = 0
            contentTransform.y = height
            break
        }
    }

    function animateFrom(fromX, fromY) {
        showAnimation.property = getAnimationProperty()
        showAnimation.from = getAnimationFrom()
        showAnimation.to = 0
        showAnimation.start()
    }

    function animateToHide() {
        hideAnimation.property = getAnimationProperty()
        hideAnimation.from = 0
        hideAnimation.to = getAnimationTo()
    }

    function getAnimationProperty() {
        switch (UserSettings.mediaOverlayPosition) {
        case 0: // top-left
        case 1: // top-center
        case 2: // top-right
        case 5: // bottom-left
        case 6: // bottom-center
        case 7: // bottom-right
            return "y"
        case 3: // left
        case 4: // right
            return "x"
        }
        return "y"
    }

    function getAnimationFrom() {
        switch (UserSettings.mediaOverlayPosition) {
        case 0: // top-left
        case 1: // top-center
        case 2: // top-right
            return -height
        case 3: // left
            return -width
        case 4: // right
            return width
        case 5: // bottom-left
        case 6: // bottom-center
        case 7: // bottom-right
            return height
        }
        return -height
    }

    function getAnimationTo() {
        return getAnimationFrom()
    }

    function resetTransform() {
        setInitialTransform()
    }

    Rectangle {
        id: overlayRect
        anchors.fill: parent
        color: Constants.panelColor
        radius: 5
        opacity: 0

        transform: Translate {
            x: contentTransform.x
            y: contentTransform.y
        }

        Rectangle {
            anchors.fill: parent
            color: "#00000000"
            radius: 5
            border.width: 1
            border.color: Constants.separatorColor
            opacity: 0.15
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            // Album art - span 2
            Rectangle {
                Layout.preferredWidth: parent.height
                Layout.preferredHeight: parent.height
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
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
                    source: "qrc:/icons/headset.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                    color: palette.text
                    opacity: 0.3
                    visible: MediaSessionBridge.mediaArt === ""
                }
            }

            // Title and Artist - span 1
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Label {
                    text: MediaSessionBridge.mediaTitle || qsTr("No media playing")
                    font.pixelSize: 13
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    maximumLineCount: 2
                }

                Label {
                    text: MediaSessionBridge.mediaArtist || ""
                    font.pixelSize: 11
                    opacity: 0.7
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    visible: MediaSessionBridge.mediaArtist !== ""
                }
            }
        }
    }
}
