pragma Singleton
import QtQuick
import ChrisLauinger77.QontrolPanel

// Native services are replaced by value-only fixtures; tests use the real UserSettings.
QtObject {
    property color cardColor: "#202020"
    property color cardBorderColor: "#404040"
    property color footerColor: "#202020"
    property color footerBorderColor: "#404040"
    property bool darkMode: true
    property string lastError: ""
    property var commAppsList: []
    property var groupedApplications: [{displayName: "Test app", executableName: "test-app"}]
    property var appVolumeHotkeys: []
    property bool globalShortcutsSuspended: false
    property int applyCount: 0
    property int restoreCount: 0
    property int appliedVolume: -1
    property int clearRememberedVolumesCount: 0
    function applyChatMixToApplications(value) { applyCount++; appliedVolume = value }
    function restoreOriginalVolumes() { restoreCount++ }
    function clearRememberedApplicationVolumes() { clearRememberedVolumesCount++; return true }
    function getShortcutState() { return false }
    function setStartupShortcut(value) {}
    function setStyle(value) {}
    function getHeadsetControlVersion() { return "test" }

    property bool anyDeviceFound: true
    property string deviceName: "Test headset"
    property string displayName: "HeadsetControl Test device"
    property int batteryLevel: 50
    property int timeToFullMinutes: -1
    property int timeToEmptyMinutes: -1
    property string batteryStatus: "BATTERY_AVAILABLE"
    property string batteryIcon: ""
    property int chatMix: 50
    property int micMuted: -1
    property int micAttached: -1
    property bool hasHeadsetVolumeCapability: true
    property bool canReadHeadsetVolume: true
    property int headsetVolumeLevel: -1
    property int pendingHeadsetVolumeLevel: -1
    property int headsetVolumeWriteCount: 0
    property bool deferHeadsetVolumeWrites: false
    function setHeadsetVolume(value) {
        UserSettings.headsetcontrolHeadsetVolume = value
        if (UserSettings.headsetcontrolHeadsetVolume === value && value >= 0) {
            headsetVolumeWriteCount++
            pendingHeadsetVolumeLevel = value
            if (!deferHeadsetVolumeWrites) {
                if (canReadHeadsetVolume)
                    headsetVolumeLevel = value
                pendingHeadsetVolumeLevel = -1
            }
        }
    }
    property int sidetoneLevel: -1
    property int sidetoneWriteCount: 0
    property int pendingSidetoneLevel: -1
    property bool deferSidetoneWrites: false
    function setSidetone(value) {
        UserSettings.headsetcontrolSidetone = value
        if (UserSettings.headsetcontrolSidetone === value) {
            sidetoneWriteCount++
            pendingSidetoneLevel = value
            if (!deferSidetoneWrites) {
                if (sidetoneLevel >= 0)
                    sidetoneLevel = value
                pendingSidetoneLevel = -1
            }
        }
    }
    property bool testModeEnabled: true
    property int testProfile: 1
    property string activeDeviceKey: "f00b:a00c:Test headset"
    property bool hasEqualizerCapability: false
    property int equalizerBandCount: 10
    property real equalizerMinimum: -12
    property real equalizerMaximum: 12
    property real equalizerStep: 0.5
    property bool hasEqualizerPresetCurves: true
    property int equalizerPreset: UserSettings.headsetcontrolEqualizerPreset
    property bool customEqualizerEnabled: (UserSettings.headsetcontrolEqualizers[activeDeviceKey] || {}).custom || false
    property int equalizerWriteCount: 0
    property var equalizerBands: ListModel {
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
        ListElement { bandValue: 0 }
    }
    function setEqualizerPreset(preset) { UserSettings.headsetcontrolEqualizerPreset = preset }
    function setCustomEqualizerEnabled(enabled) {
        // Detach QML map references so only the explicit assignment attempts a save.
        let entries = JSON.parse(JSON.stringify(UserSettings.headsetcontrolEqualizers))
        entries[activeDeviceKey] = {custom: enabled, preset: 0, bands: [0,0,0,0,0,0,0,0,0,0]}
        UserSettings.headsetcontrolEqualizers = entries
    }
    function startEqualizerFromPreset(preset) { setCustomEqualizerEnabled(true) }
    function setEqualizerBand(key, band, value) {
        if (key !== activeDeviceKey) return
        // Detach QML map references so only the explicit assignment attempts a save.
        let entries = JSON.parse(JSON.stringify(UserSettings.headsetcontrolEqualizers))
        let entry = entries[key]
        if (!entry) return
        entry.bands[band] = value
        entries[key] = entry
        UserSettings.headsetcontrolEqualizers = entries
        let accepted = UserSettings.headsetcontrolEqualizers[key].bands[band]
        if (accepted === value) equalizerWriteCount++
        equalizerBands.setProperty(band, "bandValue", accepted)
    }
    property var equalizerPresetNames: ["Flat", "Music", "Voice"]
    property bool hasChatMixCapability: true
    property bool hasEqualizerPresetsCapability: true
    property bool hasInactiveTimeCapability: true
    property bool hasBluetoothCallVolumeCapability: true
    property bool hasBluetoothWhenPoweredOnCapability: true
    property bool hasMicMuteLedBrightnessCapability: true
    property bool hasVolumeLimiterCapability: true
    property bool hasNoiseFilterCapability: true
    property bool hasMicVolumeCapability: true
    property bool hasAncCapability: true
    property bool hasAncStartupModeCapability: true
    property bool hasAncButtonModesCapability: true
    property bool hasLightsCapability: true
    property bool hasRotateToMuteCapability: true
    property bool hasSidetoneCapability: true
    property bool hasVoicePromptsCapability: true
    function setTestModeEnabled(value) { testModeEnabled = value }
    function setTestProfile(value) { testProfile = value }
    function refreshNow() {}

    property string mediaTitle: "Test title"
    property string mediaArtist: "Test artist"
    property bool isMediaPlaying: false
    property string mediaArt: ""
    property string sourceName: "Test player"
    property string sourceIcon: ""
    property int sourceCount: 1
    property bool canPreviousTrack: false
    property bool hasMediaTimeline: false
    property bool canSeek: false
    property real mediaPositionMs: 0
    property real mediaDurationMs: 0
    property real mediaMinimumSeekMs: 0
    property real mediaMaximumSeekMs: 0
    property real mediaPlaybackRate: 1
    property int previousTrackCount: 0
    property int seekCount: 0
    property real lastSeekPositionMs: -1
    signal mediaInfoChanged()
    function previousTrack() { previousTrackCount++ }
    function playPause() {}
    function nextTrack() {}
    function nextSource() {}
    function seekTo(positionMs) { seekCount++; lastSeekPositionMs = positionMs }
}
