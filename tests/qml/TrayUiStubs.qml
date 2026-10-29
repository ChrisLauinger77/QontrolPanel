pragma Singleton
import QtQml

// Values and counters only: tray tests never start native services or save settings.
QtObject {
    property bool enableMediaSessionManager: true
    property bool headsetcontrolLights: false
    property int headsetcontrolBluetoothWhenPoweredOn: -1
    property int headsetcontrolVolumeLimiter: -1
    property bool headsetcontrolVoicePrompts: true
    property bool headsetcontrolRotateToMute: false
    property int headsetcontrolEqualizerPreset: 0
    property int headsetcontrolAnc: -1
    property bool enablePowerMenu: true
    property bool showPowerDialogConfirmation: false

    property bool isReady: true
    property int outputVolume: 50
    property bool outputMuted: false
    property string outputDeviceDisplayName: "Test speakers"
    property string inputDeviceDisplayName: "Test microphone"
    function getTrayIcon(volume, muted) { return "qrc:/icons/icon.png" }

    property int sourceCount: 1
    property bool canPreviousTrack: false
    property bool isMediaPlaying: false
    property string mediaTitle: ""
    property string mediaArtist: ""
    property int previousTrackCount: 0
    property int playPauseCount: 0
    property int nextTrackCount: 0
    function previousTrack() { previousTrackCount++ }
    function playPause() { playPauseCount++; isMediaPlaying = !isMediaPlaying }
    function nextTrack() { nextTrackCount++ }

    property bool anyDeviceFound: true
    property string deviceName: "Test headset"
    property string displayName: "HeadsetControl Test device"
    property string batteryStatus: "BATTERY_AVAILABLE"
    property string batteryIcon: ""
    property int batteryLevel: 50
    property int timeToFullMinutes: -1
    property int timeToEmptyMinutes: -1
    property int chatMix: 50
    property int micMuted: -1
    property int micAttached: -1
    property bool hasChatMixCapability: true
    property bool hasEqualizerPresetsCapability: true
    property int equalizerPreset: 0
    property bool customEqualizerEnabled: false
    function setEqualizerPreset(preset) { equalizerPreset = preset; customEqualizerEnabled = false }
    property var equalizerPresetNames: ["Flat", "Music"]
    property bool hasAncCapability: true
    property bool hasAncStartupModeCapability: true
    property bool hasAncButtonModesCapability: true
    property bool hasLightsCapability: true
    property bool hasBluetoothWhenPoweredOnCapability: true
    property bool hasVolumeLimiterCapability: true
    property bool hasVoicePromptsCapability: true
    property bool hasRotateToMuteCapability: true
    function refreshNow() {}

    property bool sleepSupported: true
    property bool hibernateSupported: false
    property bool uefiSupported: true
    property bool multipleUsers: false
    property bool busy: false
    property string lastPowerAction: ""
    function sleep() { lastPowerAction = "sleep" }
    function hibernate() { lastPowerAction = "hibernate" }
    function restart() { lastPowerAction = "restart" }
    function restartToUEFI() { lastPowerAction = "uefi" }
    function shutdown() { lastPowerAction = "shutdown" }
    function lockAccount() { lastPowerAction = "lock" }
    function signOut() { lastPowerAction = "signOut" }
    function switchAccount() { lastPowerAction = "switchAccount" }

    function openLegacySoundSettings() {}
    function openModernSoundSettings() {}
}
