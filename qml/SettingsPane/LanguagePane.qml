pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3

    Label {
        text: qsTr("Language")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        CustomScrollView {
            anchors.fill: parent

            ColumnLayout {
                width: parent.width
                spacing: 3

                Card {
                    id: dlCard
                    Layout.fillWidth: true
                    title: qsTr("Update Translations")
                    description: qsTr("Download the latest translation files from GitHub")

                    property bool downloadInProgress: false

                    additionalControl: RowLayout {
                        spacing: 10

                        BusyIndicator {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            visible: dlCard.downloadInProgress
                            running: visible
                        }

                        NFButton {
                            text: qsTr("Download")
                            enabled: !dlCard.downloadInProgress
                            onClicked: {
                                dlCard.downloadInProgress = true
                                Updater.downloadLatestTranslations()
                            }
                        }
                    }

                    Connections {
                        target: Updater

                        function onTranslationDownloadFinished(success, message) {
                            dlCard.downloadInProgress = false
                            if (success) {
                                toastNotification.showToast(qsTr("Download completed successfully!"), true)
                                LanguageBridge.changeApplicationLanguage(UserSettings.languageIndex)
                            } else {
                                toastNotification.showToast(qsTr("Download failed: %1").arg(message), false)
                            }
                        }

                        function onTranslationDownloadError(errorMessage) {
                            dlCard.downloadInProgress = false
                            toastNotification.showToast(qsTr("Error: %1").arg(errorMessage), false)
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Auto update translations")
                    description: qsTr("Fetch for translations update at startup and every 4 hours")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.autoUpdateTranslations
                        onClicked: UserSettings.autoUpdateTranslations = checked
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Application language")

                    additionalControl: CustomComboBox {
                        Layout.preferredHeight: 35
                        model: {
                            let names = [qsTr("System")]
                            names = names.concat(LanguageBridge.getLanguageNativeNames())
                            return names
                        }
                        currentIndex: UserSettings.languageIndex
                        onActivated: {
                            UserSettings.languageIndex = currentIndex
                            LanguageBridge.changeApplicationLanguage(currentIndex)
                            currentIndex = UserSettings.languageIndex
                        }
                    }
                }

                Card {
                    id: progressCard
                    Layout.fillWidth: true
                    title: qsTr("Translation Progress")
                    description: Updater.hasTranslationProgressData() ?
                                     qsTr("Current language completion") :
                                     qsTr("Fetch for new translation to get data")

                    additionalControl: RowLayout {
                        spacing: 10
                        ProgressBar {
                            id: progressBar
                            from: 0
                            to: 100
                            visible: false  // Start as invisible
                            value: progressPercentage
                            Layout.topMargin: 2
                            property int progressPercentage: 0

                            function updateProgress() {
                                let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                progressPercentage = Updater.getTranslationProgress(currentLangCode)
                                value = progressPercentage
                            }

                            function updateVisibilityAndProgress() {
                                visible = Updater.hasTranslationProgressData()
                                if (visible) {
                                    updateProgress()
                                }
                            }

                            Component.onCompleted: {
                                updateVisibilityAndProgress()
                            }

                            Connections {
                                target: UserSettings

                                function onLanguageIndexChanged() {
                                    progressBar.updateProgress()
                                }
                            }

                            Connections {
                                target: Updater
                                function onTranslationDownloadFinished(success, message) {
                                    Qt.callLater(progressBar.updateVisibilityAndProgress)
                                }

                                function onTranslationProgressDataLoaded() {
                                    progressBar.updateVisibilityAndProgress()
                                }
                            }
                        }
                        Label {
                            text: progressBar.value + "%"
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Translation author")
                    description: ""

                    additionalControl: Label {
                        id: contributorLabel
                        text: {
                            if (!Updater.hasTranslationProgressData()) {
                                return qsTr("Unknown author")
                            }
                            let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                            let contributor = Updater.getTranslationContributor(currentLangCode)
                            return contributor ? contributor : qsTr("Unknown author")
                        }
                        opacity: 0.5

                        Connections {
                            target: UserSettings
                            function onLanguageIndexChanged() {
                                contributorLabel.text = Qt.binding(function() {
                                    if (!Updater.hasTranslationProgressData()) {
                                        return qsTr("Unknown author")
                                    }
                                    let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                    let contributor = Updater.getTranslationContributor(currentLangCode)
                                    return contributor ? contributor : qsTr("Unknown author")
                                })
                            }
                        }

                        Connections {
                            target: Updater
                            function onTranslationDownloadFinished(success, message) {
                                if (success) {
                                    contributorLabel.text = Qt.binding(function() {
                                        if (!Updater.hasTranslationProgressData()) {
                                            return qsTr("Unknown author")
                                        }
                                        let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                        let contributor = Updater.getTranslationContributor(currentLangCode)
                                        return contributor ? contributor : qsTr("Unknown author")
                                    })
                                }
                            }

                            function onTranslationProgressDataLoaded() {
                                contributorLabel.text = Qt.binding(function() {
                                    if (!Updater.hasTranslationProgressData()) {
                                        return qsTr("Unknown author")
                                    }
                                    let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                    let contributor = Updater.getTranslationContributor(currentLangCode)
                                    return contributor ? contributor : qsTr("Unknown author")
                                })
                            }
                        }
                    }
                }

                Card {
                    id: lastUpdatedCard
                    Layout.fillWidth: true
                    title: qsTr("Translation last updated")
                    description: ""

                    additionalControl: Label {
                        id: lastUpdatedLabel
                        text: {
                            if (!Updater.hasTranslationProgressData()) {
                                return qsTr("Unknown date")
                            }
                            let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                            let dateStr = Updater.getTranslationLastUpdated(currentLangCode)
                            return dateStr ? dateStr : qsTr("Unknown date")
                        }
                        opacity: 0.5

                        Connections {
                            target: UserSettings
                            function onLanguageIndexChanged() {
                                lastUpdatedLabel.text = Qt.binding(function() {
                                    if (!Updater.hasTranslationProgressData()) {
                                        return qsTr("Unknown date")
                                    }
                                    let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                    let dateStr = Updater.getTranslationLastUpdated(currentLangCode)
                                    return dateStr ? dateStr : qsTr("Unknown date")
                                })
                            }
                        }

                        Connections {
                            target: Updater
                            function onTranslationDownloadFinished(success, message) {
                                if (success) {
                                    lastUpdatedLabel.text = Qt.binding(function() {
                                        if (!Updater.hasTranslationProgressData()) {
                                            return qsTr("Unknown date")
                                        }
                                        let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                        let dateStr = Updater.getTranslationLastUpdated(currentLangCode)
                                        return dateStr ? dateStr : qsTr("Unknown date")
                                    })
                                }
                            }

                            function onTranslationProgressDataLoaded() {
                                lastUpdatedLabel.text = Qt.binding(function() {
                                    if (!Updater.hasTranslationProgressData()) {
                                        return qsTr("Unknown date")
                                    }
                                    let currentLangCode = LanguageBridge.getLanguageCodeFromIndex(UserSettings.languageIndex)
                                    let dateStr = Updater.getTranslationLastUpdated(currentLangCode)
                                    return dateStr ? dateStr : qsTr("Unknown date")
                                })
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: toastNotification

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20

            width: Math.min(toastText.implicitWidth + 40, parent.width - 40)
            height: 50
            radius: 8

            visible: false
            opacity: 0

            property bool isSuccess: true

            color: isSuccess ? "#4CAF50" : "#F44336"  // Green for success, red for error

            function showToast(message, success) {
                toastText.text = message
                isSuccess = success
                visible = true
                showAnimation.start()
                hideTimer.start()
            }

            Label {
                id: toastText
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            NumberAnimation {
                id: showAnimation
                target: toastNotification
                property: "opacity"
                from: 0
                to: 1
                duration: 300
                easing.type: Easing.OutQuad
            }

            NumberAnimation {
                id: hideAnimation
                target: toastNotification
                property: "opacity"
                from: 1
                to: 0
                duration: 300
                easing.type: Easing.InQuad
                onFinished: toastNotification.visible = false
            }

            Timer {
                id: hideTimer
                interval: 3000  // Show for 3 seconds
                onTriggered: hideAnimation.start()
            }
        }
    }
}
