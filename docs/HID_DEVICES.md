# HID Devices

QontrolPanel uses HID access through the vendored HeadsetControl library. The app itself does not implement vendor USB protocols directly; it links HeadsetControl and consumes its device abstraction.

## Dependency Chain

```text
QML HeadsetControlPane
        |
HeadsetControlBridge
        |
HeadsetControlMonitor
        |
headsetcontrol_lib
        |
hidapi
        |
USB HID headset
```

`hidapi` is provided by vcpkg and linked as `hidapi::hidapi`. During install, CMake attempts to copy `hidapi.dll` from the vcpkg installation into the app runtime directory.

## Device Support

Supported devices are defined in `dependencies/headsetcontrol/lib/devices/` and registered by the HeadsetControl library. The application currently consumes the capabilities reported by that library rather than maintaining a separate QontrolPanel support table.

Common supported capability families include:

- battery status and battery level;
- sidetone;
- lights;
- rotate-to-mute;
- ChatMix;
- voice prompts;
- equalizer presets;
- inactive time.

Not every headset supports every capability. UI controls should be hidden or disabled based on capability properties from `HeadsetControlBridge`.

## Capability Flow

`HeadsetControlMonitor` periodically refreshes headset state. It caches detected devices and exposes aggregate state for the first active headset through bridge properties:

- `deviceName`
- `batteryStatus`
- `batteryLevel`
- `batteryIcon`
- `chatMix`
- `equalizerPresetNames`
- `anyDeviceFound`
- capability booleans such as `hasSidetoneCapability` and `hasChatMixCapability`

When the monitor state changes, Qt signals notify `HeadsetControlBridge`, which forwards QML property notifications.

## Polling

Headset polling is controlled by user settings:

- `headsetcontrolMonitoring`
- `headsetcontrolFetchRate`
- `headsetcontrolLowBatteryThreshold`

The default monitor interval is initialized to 30 seconds and can be changed through the bridge with `setFetchRate`.

Keep polling conservative. USB HID reads can be relatively expensive, some devices are slow to respond, and repeated writes may affect battery-powered devices.

## Write Operations

The app can request device writes through `HeadsetControlMonitor`:

- `setLights(bool enabled)`
- `setRotateToMute(bool enabled)`
- `setVoicePrompts(bool enabled)`
- `setEqualizerPreset(int preset)`
- `setSidetone(int value)`
- `setInactiveTime(int value)`

Each write operation checks the relevant capability and verifies that a headset is connected before calling the HeadsetControl API. Callers should still keep the UI capability-gated to avoid presenting unsupported controls.

## Adding or Updating Device Support

Device protocol support belongs in the vendored HeadsetControl source, not in QontrolPanel bridge code.

Use this path when adding a device:

1. Add or update the device implementation under `dependencies/headsetcontrol/lib/devices/`.
2. Register the device with HeadsetControl's device registry.
3. Add or update HeadsetControl tests where practical.
4. Rebuild QontrolPanel and verify the capability list exposed through `HeadsetControlMonitor`.
5. Update QontrolPanel UI only if a newly supported capability needs a new user-facing control.

If the device already works in HeadsetControl but not in QontrolPanel, inspect the app-side capability mapping in `HeadsetControlMonitor::getCapabilityList` and related update methods before changing lower-level device code.

## Test Mode

`HeadsetControlMonitor` and `HeadsetControlBridge` include test mode properties and invokables:

- `testModeEnabled`
- `testProfile`
- `setTestModeEnabled(bool enabled)`
- `setTestProfile(int profile)`

Use test mode for UI and settings work when a physical supported headset is not available. Do not treat test mode as proof that USB HID communication works; it only validates the app-side behavior.

## Troubleshooting

### No device found

- Confirm the headset is supported by the vendored HeadsetControl version.
- Check whether another vendor tool is holding exclusive access.
- Verify that `hidapi.dll` is available next to the installed executable.
- Enable debug logging and inspect the HeadsetControl category messages.

### Capability missing

- Confirm the device implementation advertises the capability in HeadsetControl.
- Check whether QontrolPanel maps that capability in `HeadsetControlMonitor`.
- Verify that the setting or UI control is not hidden by a separate app setting.

### Write fails

- Check the log message emitted by the specific setter.
- Confirm the headset remains connected between polling and write.
- Test the same operation with HeadsetControl directly if possible.
