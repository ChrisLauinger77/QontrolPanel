pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    spacing: 3
    readonly property var testProfiles: [
        { text: qsTr("1 - Error conditions"), value: 1 },
        { text: qsTr("2 - Charging battery"), value: 2 },
        { text: qsTr("3 - Basic battery"), value: 3 },
        { text: qsTr("4 - Battery unavailable"), value: 4 },
        { text: qsTr("5 - Timeout"), value: 5 },
        { text: qsTr("6 - Full battery"), value: 6 },
        { text: qsTr("7 - Low battery"), value: 7 },
        { text: qsTr("10 - Limited capabilities"), value: 10 }
    ]

    Label {
        text: HeadsetControlBridge.displayName
        font.pixelSize: 22
        font.bold: true
        visible: HeadsetControlBridge.anyDeviceFound
    }

    ProgressBar {
        Layout.bottomMargin: 15
        Layout.fillWidth: true
        from: 0
        to: 100
        value: HeadsetControlBridge.batteryLevel
        indeterminate: HeadsetControlBridge.batteryStatus === "BATTERY_CHARGING"
        visible: HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE" && HeadsetControlBridge.anyDeviceFound
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        CustomScrollView {
            anchors.fill: parent
            ColumnLayout {
                width: parent.width
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    visible: HeadsetControlBridge.lastError.length > 0
                    text: qsTr("Some headset settings could not be applied. Refresh to retry.") + "\n" + HeadsetControlBridge.lastError
                    wrapMode: Text.Wrap
                }

                Card {
                    Layout.fillWidth: true
                    visible: !HeadsetControlBridge.anyDeviceFound && UserSettings.headsetcontrolMonitoring && !HeadsetControlBridge.testModeEnabled
                    title: qsTr("No compatible device found.")
                    description: qsTr("Enable test mode below to simulate a supported headset and validate the HeadsetControl UI.")
                }

                Card {
                    Layout.fillWidth: true
                    visible: UserSettings.headsetcontrolMonitoring && (!HeadsetControlBridge.anyDeviceFound || HeadsetControlBridge.testModeEnabled)
                    title: qsTr("HeadsetControl test mode")
                    description: qsTr("Simulate a supported headset for testing. This stays enabled until the app closes.")

                    additionalControl: LabeledSwitch {
                        checked: HeadsetControlBridge.testModeEnabled
                        onClicked: HeadsetControlBridge.setTestModeEnabled(checked)
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: UserSettings.headsetcontrolMonitoring && (!HeadsetControlBridge.anyDeviceFound || HeadsetControlBridge.testModeEnabled)
                    title: qsTr("Test headset profile")
                    description: qsTr("Choose which synthetic headset scenario HeadsetControl should simulate.")
                    enabled: HeadsetControlBridge.testModeEnabled

                    additionalControl: CustomComboBox {
                        Layout.preferredHeight: 35
                        model: testProfiles
                        textRole: "text"
                        valueRole: "value"
                        function acceptedIndex() {
                            return testProfiles.findIndex(function(profile) {
                                return profile.value === HeadsetControlBridge.testProfile
                            })
                        }
                        currentIndex: acceptedIndex()
                        enabled: HeadsetControlBridge.testModeEnabled
                        onActivated: {
                            HeadsetControlBridge.setTestProfile(currentValue)
                            currentIndex = Qt.binding(acceptedIndex)
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Device battery")
                    visible: HeadsetControlBridge.batteryStatus !== "BATTERY_UNAVAILABLE" && HeadsetControlBridge.anyDeviceFound
                    description: qsTr("Current battery level of the connected headset")
                    additionalControl: Label {
                        readonly property string estimate: HeadsetBattery.formatTime(HeadsetControlBridge.batteryStatus,
                            HeadsetControlBridge.timeToFullMinutes, HeadsetControlBridge.timeToEmptyMinutes)
                        text: HeadsetControlBridge.batteryIcon + HeadsetControlBridge.batteryLevel + "%"
                            + (estimate.length > 0 ? " (" + estimate + ")" : "")
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Notification on low battery")
                    description: qsTr("Notify when the connected headset reaches %1% or lower.").arg(UserSettings.headsetcontrolLowBatteryThreshold)
                    additionalControl: RowLayout {
                        spacing: 8

                        SpinBox {
                            Layout.preferredHeight: 35
                            Layout.preferredWidth: 100
                            from: 1
                            to: 30
                            value: UserSettings.headsetcontrolLowBatteryThreshold
                            editable: true
                            onValueModified: {
                                UserSettings.headsetcontrolLowBatteryThreshold = value
                                value = Qt.binding(function() { return UserSettings.headsetcontrolLowBatteryThreshold })
                            }
                        }

                        Label {
                            text: "%"
                            opacity: 0.7
                        }

                        LabeledSwitch {
                            checked: UserSettings.enableNotifications
                            onClicked: {
                                UserSettings.enableNotifications = checked;
                                checked = Qt.binding(function() { return UserSettings.enableNotifications })
                            }
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Show battery status in panel footer")
                    additionalControl: LabeledSwitch {
                        checked: UserSettings.displayBatteryFooter
                        onClicked: {
                            UserSettings.displayBatteryFooter = checked;
                            checked = Qt.binding(function() { return UserSettings.displayBatteryFooter })
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("ChatMix")
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasChatMixCapability
                    description: qsTr("ChatMix value of the connected headset")
                    additionalControl: Label {
                        text: HeadsetControlBridge.chatMix
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Microphone mute (last reading)")
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.micMuted >= 0
                    description: qsTr("Updates at the headset fetch rate.")
                    additionalControl: Label {
                        text: HeadsetControlBridge.micMuted === 1 ? qsTr("Muted") : qsTr("Unmuted")
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: qsTr("Boom mic (last reading)")
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.micAttached >= 0
                    description: qsTr("Updates at the headset fetch rate.")
                    additionalControl: Label {
                        text: HeadsetControlBridge.micAttached === 1 ? qsTr("Attached") : qsTr("Detached")
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasEqualizerPresetsCapability
                    enabled: HeadsetControlBridge.hasEqualizerPresetsCapability
                    Layout.fillWidth: true
                    title: qsTr("Equalizer Preset")
                    description: HeadsetControlBridge.customEqualizerEnabled
                        ? qsTr("Selecting a preset stops applying the custom curve. Your custom curve is kept.")
                        : qsTr("Set the equalizer preset for your headset")
                    additionalControl: CustomComboBox {
                        Layout.preferredHeight: 35
                        model: HeadsetControlBridge.equalizerPresetNames
                        enabled: count > 0
                        function acceptedIndex() {
                            if (count <= 0) {
                                return -1;
                            }

                            return Math.min(Math.max(0, HeadsetControlBridge.equalizerPreset), count - 1);
                        }
                        currentIndex: acceptedIndex()
                        onActivated: {
                            HeadsetControlBridge.setEqualizerPreset(currentIndex);
                            currentIndex = Qt.binding(acceptedIndex)
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasEqualizerCapability
                    Layout.fillWidth: true
                    title: qsTr("Custom equalizer")
                    description: HeadsetControlBridge.equalizerBandCount > 0
                        ? qsTr("Save and restore a custom curve for this headset.")
                        : qsTr("Select an equalizer preset first to load this headset's band layout.")
                    additionalControl: LabeledSwitch {
                        objectName: "customEqualizerSwitch"
                        enabled: HeadsetControlBridge.equalizerBandCount > 0
                        checked: HeadsetControlBridge.customEqualizerEnabled
                        onClicked: {
                            HeadsetControlBridge.setCustomEqualizerEnabled(checked)
                            checked = Qt.binding(function() { return HeadsetControlBridge.customEqualizerEnabled })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasEqualizerCapability
                        && HeadsetControlBridge.equalizerBandCount > 0
                    Layout.fillWidth: true
                    title: qsTr("Start from preset")
                    description: qsTr("Copy a preset curve, then adjust its bands.")
                    additionalControl: CustomComboBox {
                        objectName: "equalizerStartPreset"
                        model: HeadsetControlBridge.equalizerPresetNames
                        enabled: HeadsetControlBridge.hasEqualizerPresetCurves
                        currentIndex: -1
                        displayText: currentIndex < 0 ? qsTr("Choose a preset") : currentText
                        onActivated: {
                            HeadsetControlBridge.startEqualizerFromPreset(currentIndex)
                            currentIndex = -1
                        }
                    }
                }

                Rectangle {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasEqualizerCapability
                        && HeadsetControlBridge.equalizerBandCount > 0
                    enabled: HeadsetControlBridge.customEqualizerEnabled
                    Layout.fillWidth: true
                    implicitHeight: equalizerRow.implicitHeight + 30
                    color: Constants.cardColor
                    border.color: Constants.cardBorderColor
                    radius: 5
                    // Band metadata has no center frequencies; keep the device's band order.
                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 15
                        contentWidth: equalizerRow.implicitWidth
                        contentHeight: height
                        clip: true
                        flickableDirection: Flickable.HorizontalFlick
                        RowLayout {
                            id: equalizerRow
                            spacing: 16
                            Repeater {
                                model: HeadsetControlBridge.equalizerBands
                                delegate: ColumnLayout {
                                    id: bandColumn
                                    required property int index
                                    required property real bandValue
                                    spacing: 8
                                    Label {
                                        text: qsTr("Band %1").arg(bandColumn.index + 1)
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    Slider {
                                        id: bandSlider
                                        objectName: "equalizerBand" + bandColumn.index
                                        orientation: Qt.Vertical
                                        Layout.preferredHeight: 150
                                        Layout.alignment: Qt.AlignHCenter
                                        from: HeadsetControlBridge.equalizerMinimum
                                        to: HeadsetControlBridge.equalizerMaximum
                                        stepSize: HeadsetControlBridge.equalizerStep
                                        snapMode: Slider.SnapAlways
                                        wheelEnabled: true
                                        focusPolicy: Qt.StrongFocus
                                        value: bandColumn.bandValue
                                        property string editDeviceKey: ""
                                        Accessible.name: qsTr("Band %1").arg(bandColumn.index + 1)
                                        function commit(deviceKey) {
                                            HeadsetControlBridge.setEqualizerBand(deviceKey, bandColumn.index, value)
                                            value = Qt.binding(function() { return bandColumn.bandValue })
                                        }
                                        onPressedChanged: {
                                            if (pressed)
                                                editDeviceKey = HeadsetControlBridge.activeDeviceKey
                                            else
                                                commit(editDeviceKey)
                                        }
                                        onMoved: {
                                            if (!pressed)
                                                commit(HeadsetControlBridge.activeDeviceKey)
                                        }
                                    }
                                    Label {
                                        text: Number(bandSlider.value).toLocaleString(Qt.locale(), 'f', 1)
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                            }
                        }
                        ScrollBar.horizontal: ScrollBar {}
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasInactiveTimeCapability
                    enabled: HeadsetControlBridge.hasInactiveTimeCapability
                    Layout.fillWidth: true
                    title: qsTr("Inactive time (minutes)")
                    description: qsTr("Set the time of inactivity after which your headset will enter power-saving mode")
                    additionalControl: RowLayout {
                        spacing: 12

                        NFSlider {
                            id: inactiveTimeSlider
                            from: 0
                            to: 90
                            value: Math.max(0, UserSettings.headsetcontrolInactiveTime)
                            Layout.preferredWidth: 180
                            onPressedChanged: {
                                if (!pressed) {
                                    UserSettings.headsetcontrolInactiveTime = Math.round(value);
                                    value = Qt.binding(function() { return Math.max(0, UserSettings.headsetcontrolInactiveTime) })
                                }
                            }
                            onWheelChanged: {
                                UserSettings.headsetcontrolInactiveTime = Math.round(value);
                                value = Qt.binding(function() { return Math.max(0, UserSettings.headsetcontrolInactiveTime) })
                            }
                        }

                        Label {
                            text: Math.round(inactiveTimeSlider.value).toString()
                            opacity: 0.7
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasLightsCapability
                    enabled: HeadsetControlBridge.hasLightsCapability
                    Layout.fillWidth: true
                    title: qsTr("Lights")
                    description: qsTr("Toggle RGB lights on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolLights
                        onClicked: {
                            UserSettings.headsetcontrolLights = checked;
                            checked = Qt.binding(function() { return UserSettings.headsetcontrolLights })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasRotateToMuteCapability
                    enabled: HeadsetControlBridge.hasRotateToMuteCapability
                    Layout.fillWidth: true
                    title: qsTr("Rotate-to-Mute")
                    description: qsTr("Toggle rotate-to-mute feature on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolRotateToMute
                        onClicked: {
                            UserSettings.headsetcontrolRotateToMute = checked;
                            checked = Qt.binding(function() { return UserSettings.headsetcontrolRotateToMute })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasHeadsetVolumeCapability
                    enabled: HeadsetControlBridge.hasHeadsetVolumeCapability
                    Layout.fillWidth: true
                    title: qsTr("Headset volume")
                    description: qsTr("Adjust the headset's hardware volume, separately from Windows volume")
                    implicitHeight: 110

                    additionalControl: ColumnLayout {
                        RowLayout {
                            NFSlider {
                                id: headsetVolumeSlider
                                from: 0
                                to: 128
                                readonly property int currentLevel: HeadsetControlBridge.pendingHeadsetVolumeLevel >= 0
                                    ? HeadsetControlBridge.pendingHeadsetVolumeLevel
                                    : HeadsetControlBridge.headsetVolumeLevel >= 0
                                        ? HeadsetControlBridge.headsetVolumeLevel : UserSettings.headsetcontrolHeadsetVolume
                                value: Math.max(0, currentLevel)
                                Layout.preferredWidth: 180
                                onPressedChanged: {
                                    if (!pressed) {
                                        HeadsetControlBridge.setHeadsetVolume(Math.round(value));
                                        value = Qt.binding(function() { return Math.max(0, headsetVolumeSlider.currentLevel) })
                                    }
                                }
                                onWheelChanged: {
                                    HeadsetControlBridge.setHeadsetVolume(Math.round(value));
                                    value = Qt.binding(function() { return Math.max(0, headsetVolumeSlider.currentLevel) })
                                }
                            }
                            Label {
                                text: headsetVolumeSlider.currentLevel < 0 ? qsTr("Not set")
                                    : Math.round(headsetVolumeSlider.value).toString()
                            }
                            Button {
                                objectName: "headsetVolumeUnset"
                                text: qsTr("Not set")
                                enabled: UserSettings.headsetcontrolHeadsetVolume >= 0
                                onClicked: HeadsetControlBridge.setHeadsetVolume(-1)
                            }
                        }
                        Label {
                            objectName: "headsetVolumeReadback"
                            text: HeadsetControlBridge.headsetVolumeLevel >= 0
                                ? qsTr("Device level (last reading): %1 / 128").arg(HeadsetControlBridge.headsetVolumeLevel)
                                : HeadsetControlBridge.canReadHeadsetVolume
                                    ? qsTr("No headset volume reading yet")
                                    : qsTr("Read-back unavailable; showing your saved choice")
                            opacity: 0.7
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasSidetoneCapability
                    enabled: HeadsetControlBridge.hasSidetoneCapability
                    Layout.fillWidth: true
                    title: qsTr("Sidetone")
                    description: qsTr("Adjust your voice feedback level")
                    additionalControl: RowLayout {
                        spacing: 12

                        NFSlider {
                            id: sidetoneSlider
                            from: 0
                            to: 128
                            readonly property int currentLevel: HeadsetControlBridge.pendingSidetoneLevel >= 0
                                ? HeadsetControlBridge.pendingSidetoneLevel
                                : HeadsetControlBridge.sidetoneLevel >= 0
                                    ? HeadsetControlBridge.sidetoneLevel : UserSettings.headsetcontrolSidetone
                            value: currentLevel
                            Layout.preferredWidth: 180
                            onPressedChanged: {
                                if (!pressed) {
                                    HeadsetControlBridge.setSidetone(Math.round(value));
                                    value = Qt.binding(function() { return sidetoneSlider.currentLevel })
                                }
                            }
                            onWheelChanged: {
                                HeadsetControlBridge.setSidetone(Math.round(value));
                                value = Qt.binding(function() { return sidetoneSlider.currentLevel })
                            }
                        }

                        Label {
                            text: Math.round(sidetoneSlider.value).toString()
                            opacity: 0.7
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasVoicePromptsCapability
                    enabled: HeadsetControlBridge.hasVoicePromptsCapability
                    Layout.fillWidth: true
                    title: qsTr("Voice Prompts")
                    description: qsTr("Toggle voice prompts on your headset")

                    additionalControl: LabeledSwitch {
                        checked: UserSettings.headsetcontrolVoicePrompts
                        onClicked: {
                            UserSettings.headsetcontrolVoicePrompts = checked;
                            checked = Qt.binding(function() { return UserSettings.headsetcontrolVoicePrompts })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasAncCapability
                    enabled: HeadsetControlBridge.hasAncCapability
                    Layout.fillWidth: true
                    title: qsTr("Noise cancelling")
                    description: qsTr("Choose the listening mode. Shows your saved choice; the headset cannot report its current mode.")

                    additionalControl: CustomComboBox {
                        model: [qsTr("Not set"), qsTr("Off"), qsTr("Noise cancelling"), qsTr("Ambient sound")]
                        currentIndex: UserSettings.headsetcontrolAnc + 1
                        onActivated: {
                            UserSettings.headsetcontrolAnc = currentIndex - 1;
                            currentIndex = Qt.binding(function() { return UserSettings.headsetcontrolAnc + 1 })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasAncStartupModeCapability
                    enabled: HeadsetControlBridge.hasAncStartupModeCapability
                    Layout.fillWidth: true
                    title: qsTr("Noise cancelling at power-on")
                    description: qsTr("Choose the listening mode when the headset powers on")

                    additionalControl: CustomComboBox {
                        model: [qsTr("Not set"), qsTr("Off"), qsTr("Noise cancelling"), qsTr("Ambient sound"), qsTr("Last mode at power-off")]
                        currentIndex: UserSettings.headsetcontrolAncStartupMode + 1
                        onActivated: {
                            UserSettings.headsetcontrolAncStartupMode = currentIndex - 1;
                            currentIndex = Qt.binding(function() { return UserSettings.headsetcontrolAncStartupMode + 1 })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasAncButtonModesCapability
                    enabled: HeadsetControlBridge.hasAncButtonModesCapability
                    Layout.fillWidth: true
                    implicitHeight: 190
                    title: qsTr("Noise cancelling button cycle")
                    description: qsTr("Choose at least one mode for the headset button to cycle through")

                    additionalControl: ColumnLayout {
                        Repeater {
                            model: [
                                { label: qsTr("Off"), mask: 1 },
                                { label: qsTr("Noise cancelling"), mask: 2 },
                                { label: qsTr("Ambient sound"), mask: 4 }
                            ]
                            delegate: CheckBox {
                                id: modeCheckBox
                                required property var modelData
                                objectName: "ancButtonMode" + modelData.mask
                                text: modelData.label
                                checked: UserSettings.headsetcontrolAncButtonModes >= 0
                                    && (UserSettings.headsetcontrolAncButtonModes & modelData.mask) !== 0
                                enabled: !checked || UserSettings.headsetcontrolAncButtonModes !== modelData.mask
                                onClicked: {
                                    const current = Math.max(0, UserSettings.headsetcontrolAncButtonModes);
                                    const modes = checked ? current | modelData.mask : current & ~modelData.mask;
                                    if (modes > 0)
                                        UserSettings.headsetcontrolAncButtonModes = modes;
                                    checked = Qt.binding(function() {
                                        return UserSettings.headsetcontrolAncButtonModes >= 0
                                            && (UserSettings.headsetcontrolAncButtonModes & modeCheckBox.modelData.mask) !== 0;
                                    })
                                }
                            }
                        }
                        Button {
                            objectName: "ancButtonModesUnset"
                            text: qsTr("Not set")
                            enabled: UserSettings.headsetcontrolAncButtonModes >= 0
                            onClicked: UserSettings.headsetcontrolAncButtonModes = -1
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasMicVolumeCapability
                    enabled: HeadsetControlBridge.hasMicVolumeCapability
                    Layout.fillWidth: true
                    title: qsTr("Microphone volume")
                    description: qsTr("Adjust the headset microphone volume (0–128)")

                    additionalControl: SpinBox {
                        from: -1
                        to: 128
                        value: UserSettings.headsetcontrolMicVolume
                        editable: true
                        textFromValue: function(value, locale) {
                            return value < 0 ? qsTr("Not set") : value.toLocaleString(locale, "f", 0)
                        }
                        valueFromText: function(text, locale) {
                            return text === qsTr("Not set") ? -1 : Number.fromLocaleString(locale, text)
                        }
                        onValueModified: {
                            UserSettings.headsetcontrolMicVolume = value;
                            value = Qt.binding(function() { return UserSettings.headsetcontrolMicVolume })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasNoiseFilterCapability
                    enabled: HeadsetControlBridge.hasNoiseFilterCapability
                    Layout.fillWidth: true
                    title: qsTr("Microphone noise filter")
                    description: qsTr("Choose the microphone noise filter strength")

                    additionalControl: CustomComboBox {
                        model: [qsTr("Not set"), qsTr("Off"), qsTr("Low"), qsTr("High")]
                        currentIndex: UserSettings.headsetcontrolNoiseFilter + 1
                        onActivated: {
                            UserSettings.headsetcontrolNoiseFilter = currentIndex - 1;
                            currentIndex = Qt.binding(function() { return UserSettings.headsetcontrolNoiseFilter + 1 })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasVolumeLimiterCapability
                    enabled: HeadsetControlBridge.hasVolumeLimiterCapability
                    Layout.fillWidth: true
                    title: qsTr("Volume limiter")
                    description: qsTr("Limit the headset playback volume")

                    additionalControl: CustomComboBox {
                        model: [qsTr("Not set"), qsTr("Off"), qsTr("On")]
                        currentIndex: UserSettings.headsetcontrolVolumeLimiter + 1
                        onActivated: {
                            UserSettings.headsetcontrolVolumeLimiter = currentIndex - 1;
                            currentIndex = Qt.binding(function() { return UserSettings.headsetcontrolVolumeLimiter + 1 })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasMicMuteLedBrightnessCapability
                    enabled: HeadsetControlBridge.hasMicMuteLedBrightnessCapability
                    Layout.fillWidth: true
                    title: qsTr("Mic mute LED brightness")
                    description: qsTr("Adjust the microphone mute indicator brightness")

                    additionalControl: CustomComboBox {
                        model: [qsTr("Not set"), qsTr("Off"), qsTr("Low"), qsTr("Medium"), qsTr("High")]
                        currentIndex: UserSettings.headsetcontrolMicMuteLedBrightness + 1
                        onActivated: {
                            UserSettings.headsetcontrolMicMuteLedBrightness = currentIndex - 1;
                            currentIndex = Qt.binding(function() { return UserSettings.headsetcontrolMicMuteLedBrightness + 1 })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasBluetoothWhenPoweredOnCapability
                    enabled: HeadsetControlBridge.hasBluetoothWhenPoweredOnCapability
                    Layout.fillWidth: true
                    title: qsTr("Bluetooth on at power-on")
                    description: qsTr("Enable Bluetooth when the headset powers on")

                    additionalControl: CustomComboBox {
                        model: [qsTr("Not set"), qsTr("Off"), qsTr("On")]
                        currentIndex: UserSettings.headsetcontrolBluetoothWhenPoweredOn + 1
                        onActivated: {
                            UserSettings.headsetcontrolBluetoothWhenPoweredOn = currentIndex - 1;
                            currentIndex = Qt.binding(function() { return UserSettings.headsetcontrolBluetoothWhenPoweredOn + 1 })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound && HeadsetControlBridge.hasBluetoothCallVolumeCapability
                    enabled: HeadsetControlBridge.hasBluetoothCallVolumeCapability
                    Layout.fillWidth: true
                    title: qsTr("Bluetooth call volume")
                    description: qsTr("Adjust the Bluetooth call volume (0–100)")

                    additionalControl: SpinBox {
                        from: -1
                        to: 100
                        value: UserSettings.headsetcontrolBluetoothCallVolume
                        editable: true
                        textFromValue: function(value, locale) {
                            return value < 0 ? qsTr("Not set") : value.toLocaleString(locale, "f", 0)
                        }
                        valueFromText: function(text, locale) {
                            return text === qsTr("Not set") ? -1 : Number.fromLocaleString(locale, text)
                        }
                        onValueModified: {
                            UserSettings.headsetcontrolBluetoothCallVolume = value;
                            value = Qt.binding(function() { return UserSettings.headsetcontrolBluetoothCallVolume })
                        }
                    }
                }

                Card {
                    visible: HeadsetControlBridge.anyDeviceFound
                    Layout.fillWidth: true
                    title: qsTr("Fetch rate (seconds)")
                    description: qsTr("How often the status from your headset is fetched. Lower values may increase battery usage.")
                    additionalControl: RowLayout {
                        spacing: 8

                        Button {
                            text: qsTr("Fetch")
                            onClicked: HeadsetControlBridge.refreshNow()
                        }

                        SpinBox {
                            from: 60
                            to: 3600
                            value: UserSettings.headsetcontrolFetchRate
                            editable: true
                            stepSize: 5
                            onValueModified: {
                                UserSettings.headsetcontrolFetchRate = value;
                                value = Qt.binding(function() { return UserSettings.headsetcontrolFetchRate })
                            }
                        }
                    }
                }

                Card {
                    visible: UserSettings.headsetcontrolMonitoring
                    Layout.fillWidth: true
                    title: qsTr("Version")
                    description: ""

                    additionalControl: Label {
                        text: Updater.getHeadsetControlVersion()
                        opacity: 0.5
                    }
                }

                Card {
                    visible: UserSettings.headsetcontrolMonitoring
                    Layout.fillWidth: true
                    title: qsTr("GitHub repository")
                    description: "https://github.com/Sapd/HeadsetControl"

                    additionalControl: Button {
                        text: qsTr("View on GitHub")
                        onClicked: Qt.openUrlExternally("https://github.com/Sapd/HeadsetControl#headsetcontrol")
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            opacity: 0.5
            text: qsTr("HeadsetControl monitoring is disabled\nYou can enable it in the Components tab.")
            visible: !UserSettings.headsetcontrolMonitoring
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
