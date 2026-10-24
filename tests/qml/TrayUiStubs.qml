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

    property bool isReady: true
    property int outputVolume: 50
    property bool outputMuted: false
    property string outputDeviceDisplayName: "Test speakers"
    property string inputDeviceDisplayName: "Test microphone"
    function getTrayIcon(volume, muted) { return "qrc:/icons/icon.png" }

    property int sourceCount: 1
    property bool canPreviousTrack: false
    property bool isMediaPlaying: false
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
    property int chatMix: 50
    property bool hasChatMixCapability: true
    property bool hasEqualizerPresetsCapability: true
    property var equalizerPresetNames: ["Flat", "Music"]
    property bool hasLightsCapability: true
    property bool hasBluetoothWhenPoweredOnCapability: true
    property bool hasVolumeLimiterCapability: true
    property bool hasVoicePromptsCapability: true
    property bool hasRotateToMuteCapability: true
    function refreshNow() {}
    function openLegacySoundSettings() {}
    function openModernSoundSettings() {}
}
