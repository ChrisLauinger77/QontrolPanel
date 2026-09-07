pragma Singleton
import QtQuick

// Native services are replaced by value-only fixtures; tests use the real UserSettings.
QtObject {
    property color cardColor: "#202020"
    property color cardBorderColor: "#404040"
    property color footerColor: "#202020"
    property bool darkMode: true
    property string lastError: ""
    property var commAppsList: []
    property int applyCount: 0
    property int restoreCount: 0
    property int appliedVolume: -1
    function applyChatMixToApplications(value) { applyCount++; appliedVolume = value }
    function restoreOriginalVolumes() { restoreCount++ }
    function getShortcutState() { return false }
    function setStartupShortcut(value) {}
    function setStyle(value) {}
    function getHeadsetControlVersion() { return "test" }

    property bool anyDeviceFound: true
    property string deviceName: "Test headset"
    property int batteryLevel: 50
    property string batteryStatus: "BATTERY_AVAILABLE"
    property string batteryIcon: ""
    property int chatMix: 50
    property bool testModeEnabled: true
    property int testProfile: 1
    property var equalizerPresetNames: ["Flat", "Music", "Voice"]
    property bool hasChatMixCapability: true
    property bool hasEqualizerPresetsCapability: true
    property bool hasInactiveTimeCapability: true
    property bool hasLightsCapability: true
    property bool hasRotateToMuteCapability: true
    property bool hasSidetoneCapability: true
    property bool hasVoicePromptsCapability: true
    function setTestModeEnabled(value) { testModeEnabled = value }
    function setTestProfile(value) { testProfile = value }
    function refreshNow() {}
}
