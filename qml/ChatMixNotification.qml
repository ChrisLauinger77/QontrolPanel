import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import QtQuick.Controls.impl
import QtQuick.Window
import ChrisLauinger77.QontrolPanel

ApplicationWindow {
    id: notificationWindow
    width: calculateWidth()
    height: 50
    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#00000000"

    property string message: ""
    property bool isAnimatingIn: false
    property bool isAnimatingOut: false
    property string notificationType: "chatmix" // "chatmix" or "micmute"
    property bool isMuted: false
    property bool chatMixEffectiveEnabled: false
    property bool nativeBackdropActive: false
    property real restingY: 0

    transientParent: null

    function calculateWidth() {
        // Measure all possible messages and use the widest
        let measurements = [
            chatMixEnabledMeasure.implicitWidth,
            chatMixDisabledMeasure.implicitWidth,
            chatMixInactiveMeasure.implicitWidth,
            micMutedMeasure.implicitWidth,
            micUnmutedMeasure.implicitWidth
        ]
        let maxWidth = Math.max(...measurements)
        return maxWidth + 20 + 12 + 30 // icon width + spacing + margins
    }

    Component.onCompleted: {
        positionWindow()
        Qt.callLater(updateNativeBackdrop)
    }

    function updateNativeBackdrop() {
        nativeBackdropActive = WindowsBackdrop.applyTransientBackdrop(notificationWindow)
    }

    Connections {
        target: Qt.application.styleHints

        function onColorSchemeChanged() {
            notificationWindow.updateNativeBackdrop()
        }
    }

    Connections {
        target: KeyboardShortcutManager

        function onChatMixToggleRequested() {
            UserSettings.chatMixEnabled = !UserSettings.chatMixEnabled

            if (UserSettings.chatMixShortcutNotification) {
                notificationType = "chatmix"
                chatMixEffectiveEnabled = UserSettings.activateChatmix && UserSettings.chatMixEnabled
                var message = UserSettings.activateChatmix
                    ? (UserSettings.chatMixEnabled ? qsTr("ChatMix Enabled") : qsTr("ChatMix Disabled"))
                    : qsTr("ChatMix is not activated")
                notificationWindow.showNotification(message)

                if (chatMixEffectiveEnabled) {
                    Utils.playNotificationSound(true)
                } else {
                    Utils.playNotificationSound(false)
                }
            }
        }

        function onMicMuteToggleRequested() {
            Qt.callLater(function() {
                notificationType = "micmute"
                isMuted = !AudioBridge.inputMuted
                var message = isMuted ? qsTr("Microphone Muted") : qsTr("Microphone Unmuted")
                notificationWindow.showNotification(message)

                if (isMuted) {
                    Utils.playNotificationSound(false)
                } else {
                    Utils.playNotificationSound(true)
                }
            })
        }
    }

    Timer {
        id: autoHideTimer
        interval: 3000
        repeat: false
        onTriggered: notificationWindow.hideNotification()
    }

    PropertyAnimation {
        id: showAnimation
        target: notificationWindow
        property: "y"
        duration: 300
        easing.type: Easing.OutQuad
        onStarted: {
            notificationWindow.isAnimatingIn = true
            if (notificationWindow.nativeBackdropActive) {
                notificationRect.opacity = 1
            } else if (!contentOpacityAnimation.running && notificationRect.opacity < 1) {
                contentOpacityAnimation.from = notificationRect.opacity
                contentOpacityAnimation.start()
            }
        }
        onFinished: {
            notificationWindow.isAnimatingIn = false
            autoHideTimer.start()
        }
    }

    PropertyAnimation {
        id: hideAnimation
        target: notificationWindow
        property: "y"
        duration: 250
        easing.type: Easing.OutQuad
        onStarted: {
            notificationWindow.isAnimatingOut = true
            if (!notificationWindow.nativeBackdropActive
                    && !hideOpacityAnimation.running && notificationRect.opacity > 0) {
                hideOpacityAnimation.from = notificationRect.opacity
                hideOpacityAnimation.start()
            }
        }
        onFinished: {
            notificationWindow.visible = false
            notificationWindow.isAnimatingOut = false
            notificationWindow.y = notificationWindow.restingY
        }
    }

    PropertyAnimation {
        id: contentOpacityAnimation
        target: notificationRect
        property: "opacity"
        duration: 200
        easing.type: Easing.OutQuad
        from: 0
        to: 1
    }

    PropertyAnimation {
        id: hideOpacityAnimation
        target: notificationRect
        property: "opacity"
        duration: 150
        easing.type: Easing.InQuad
        from: 1
        to: 0
    }

    // Hidden measurement texts for all possible messages
    Text {
        id: chatMixEnabledMeasure
        text: qsTr("ChatMix Enabled")
        font.pixelSize: 14
        visible: false
    }

    Text {
        id: chatMixDisabledMeasure
        text: qsTr("ChatMix Disabled")
        font.pixelSize: 14
        visible: false
    }

    Text {
        id: chatMixInactiveMeasure
        text: qsTr("ChatMix is not activated")
        font.pixelSize: 14
        visible: false
    }

    Text {
        id: micMutedMeasure
        text: qsTr("Microphone Muted")
        font.pixelSize: 14
        visible: false
    }

    Text {
        id: micUnmutedMeasure
        text: qsTr("Microphone Unmuted")
        font.pixelSize: 14
        visible: false
    }

    function showNotification(text) {
        if ((visible && !isAnimatingOut) || isAnimatingIn) {
            message = text
            autoHideTimer.restart()
            return
        }

        message = text
        if (isAnimatingOut) {
            const currentY = y
            hideAnimation.stop()
            hideOpacityAnimation.stop()
            isAnimatingOut = false

            showAnimation.from = currentY
            showAnimation.to = restingY

            let progress = Math.abs(currentY - restingY) / notificationWindow.height
            showAnimation.duration = Math.max(150, 300 * progress)
            showAnimation.start()
            return
        }

        positionWindow()
        y = restingY - height
        notificationRect.opacity = nativeBackdropActive ? 1 : 0

        showAnimation.from = y
        showAnimation.to = restingY
        showAnimation.duration = 300

        visible = true
        showAnimation.start()
    }

    function hideNotification() {
        if (isAnimatingOut) {
            return
        }

        if (isAnimatingIn) {
            showAnimation.stop()
            contentOpacityAnimation.stop()
            isAnimatingIn = false
        }

        autoHideTimer.stop()
        hideAnimation.from = y
        hideAnimation.to = restingY - height
        hideAnimation.start()
    }

    function positionWindow() {
        const screenWidth = Utils.getAvailableDesktopWidth()
        x = (screenWidth - width) / 2
        y = UserSettings.panelPosition === 0 ? 60 : 12
        restingY = y
    }

    Rectangle {
        id: notificationRect
        anchors.fill: parent
        color: notificationWindow.nativeBackdropActive ? "transparent" : Constants.panelColor
        radius: 5
        opacity: 0

        Rectangle {
            anchors.fill: parent
            color: "#00000000"
            radius: 5
            border.width: 1
            border.color: Constants.separatorColor
            opacity: 0.15
        }

        RowLayout {
            id: contentRow
            anchors.fill: parent
            anchors.margins: 15
            spacing: 12

            IconImage {
                source: {
                    if (notificationType === "micmute") {
                        return isMuted ? "qrc:/icons/mic_muted.svg" : "qrc:/icons/mic.svg"
                    } else {
                        return "qrc:/icons/headset.svg"
                    }
                }
                sourceSize.width: 20
                sourceSize.height: 20
                color: {
                    if (notificationType === "micmute") {
                        return isMuted ? palette.text : palette.accent
                    } else {
                        return notificationWindow.chatMixEffectiveEnabled ? palette.accent : palette.text
                    }
                }
                opacity: {
                    if (notificationType === "micmute") {
                        return isMuted ? 0.5 : 1.0
                    } else {
                        return notificationWindow.chatMixEffectiveEnabled ? 1.0 : 0.5
                    }
                }
                Layout.alignment: Qt.AlignVCenter

                Behavior on color {
                    ColorAnimation {
                        duration: 200
                        easing.type: Easing.OutQuad
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutQuad
                    }
                }
            }

            Label {
                text: notificationWindow.message
                font.pixelSize: 14
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
