pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3

    Label {
        text: qsTr("Renaming")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }

    Label {
        Layout.fillWidth: true
        visible: AudioBridge.lastError.length > 0
        text: AudioBridge.lastError
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Layout.bottomMargin: 15

        Label {
            text: qsTr("Show:")
        }

        Item {
            Layout.fillWidth: true
        }

        CustomComboBox {
            id: viewSelector
            model: [qsTr("Devices"), qsTr("Applications"), qsTr("Streams")]
            currentIndex: 0
        }
    }

    CustomScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            width: parent.width
            spacing: 3

            Repeater {
                model: AudioBridge.outputDevices

                Card {
                    id: deviceCard
                    Layout.fillWidth: true
                    visible: viewSelector.currentIndex === 0
                    required property var model
                    title: model.name || ""

                    additionalControl: TextField {
                        width: 325
                        placeholderText: qsTr("Custom name")
                        text: AudioBridge.getCustomDeviceName(deviceCard.model.name || "")

                        onTextEdited: {
                            if (!AudioBridge.setCustomDeviceName(deviceCard.model.name || "", text)) {
                                text = Qt.binding(function() { return AudioBridge.getCustomDeviceName(deviceCard.model.name || "") })
                            }
                        }
                    }
                }
            }

            Repeater {
                model: AudioBridge.inputDevices

                Card {
                    id: inDeviceCard
                    Layout.fillWidth: true
                    visible: viewSelector.currentIndex === 0
                    required property var model
                    title: model.name || ""

                    additionalControl: TextField {
                        width: 325
                        placeholderText: qsTr("Custom name")
                        text: AudioBridge.getCustomDeviceName(inDeviceCard.model.name || "")

                        onTextEdited: {
                            if (!AudioBridge.setCustomDeviceName(inDeviceCard.model.name || "", text)) {
                                text = Qt.binding(function() { return AudioBridge.getCustomDeviceName(inDeviceCard.model.name || "") })
                            }
                        }
                    }
                }
            }

            Repeater {
                model: AudioBridge.groupedApplications

                Card {
                    id: groupAppCard
                    visible: viewSelector.currentIndex === 1
                    Layout.fillWidth: true
                    required property var model
                    title: model.displayName || ""

                    additionalControl: TextField {
                        width: 325
                        placeholderText: qsTr("Custom name")
                        text: AudioBridge.getCustomExecutableName(groupAppCard.model.executableName || "")

                        onTextEdited: {
                            if (!AudioBridge.setCustomExecutableName(groupAppCard.model.executableName || "", text)) {
                                text = Qt.binding(function() { return AudioBridge.getCustomExecutableName(groupAppCard.model.executableName || "") })
                            }
                        }
                    }
                }
            }

            Repeater {
                model: AudioBridge.applications

                Card {
                    id: appCard
                    Layout.fillWidth: true
                    required property var model
                    title: model.name || ""
                    visible: viewSelector.currentIndex === 2

                    additionalControl: TextField {
                        width: 325
                        placeholderText: qsTr("Custom name")
                        text: AudioBridge.getCustomApplicationName(appCard.model.name || "", appCard.model.streamIndex || 0)

                        onTextEdited: {
                            if (!AudioBridge.setCustomApplicationName(appCard.model.name || "", appCard.model.streamIndex || 0, text)) {
                                text = Qt.binding(function() { return AudioBridge.getCustomApplicationName(appCard.model.name || "", appCard.model.streamIndex || 0) })
                            }
                        }
                    }
                }
            }
        }
    }
}
