# HeadsetControl Integration

QontrolPanel integrates HeadsetControl as an in-tree CMake dependency. This lets the app show headset battery state and expose vendor-specific controls without shelling out to the HeadsetControl CLI.

## Build Integration

The root `CMakeLists.txt` expects HeadsetControl at:

```text
dependencies/headsetcontrol
```

It is added with:

```cmake
add_subdirectory("${HEADSETCONTROL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/headsetcontrol" EXCLUDE_FROM_ALL)
```

The app links:

```cmake
headsetcontrol_lib
hidapi::hidapi
```

On MSVC, the project aligns the HeadsetControl runtime library with the main app:

```cmake
MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>"
```

This avoids mixing incompatible CRT settings between the application and vendored dependency.

## App-Side Classes

### `HeadsetControlMonitor`

`HeadsetControlMonitor` is the operational layer. It:

- owns the polling timer;
- discovers headsets through the HeadsetControl library;
- caches device state in `HeadsetControlDevice`;
- converts native battery status into app strings;
- computes capability booleans;
- exposes headset write operations;
- supports test profiles for UI development.

Important state includes:

- `m_cachedDevices`
- `m_headsets`
- `m_fetchTimer`
- `m_fetchIntervalMs`
- `m_hasSidetoneCapability`
- `m_hasLightsCapability`
- `m_hasRotateToMuteCapability`
- `m_hasChatMixCapability`
- `m_hasVoicePromptsCapability`
- `m_hasEqualizerPresetsCapability`
- `m_hasInactiveTimeCapability`

### `HeadsetControlBridge`

`HeadsetControlBridge` is the QML singleton. It:

- forwards monitor properties to QML;
- exposes `Q_INVOKABLE` commands for UI controls;
- connects monitor signals to QML notifications;
- tracks low-battery notification state;
- provides `batteryIcon` for presentation.

QML should use the bridge rather than holding a monitor pointer directly.

### `HeadsetControlDevice`

`HeadsetControlDevice` is a Qt metatype struct used for cached device data:

- `deviceName`
- `vendor`
- `product`
- `vendorId`
- `productId`
- `batteryStatus`
- `batteryLevel`
- `chatMix`
- `capabilities`

It represents app-facing data and should not grow protocol-specific behavior.

## Settings

Relevant settings live in `UserSettings`:

- `headsetcontrolMonitoring`
- `headsetcontrolLights`
- `headsetcontrolRotateToMute`
- `headsetcontrolVoicePrompts`
- `headsetcontrolEqualizerPreset`
- `headsetcontrolInactiveTime`
- `headsetcontrolSidetone`
- `headsetcontrolFetchRate`
- `headsetcontrolLowBatteryThreshold`
- `enableNotifications`

When adding a headset preference, keep persisted settings, bridge state, monitor behavior, and QML controls in sync.

## QML Integration

The HeadsetControl settings UI lives in:

```text
qml/SettingsPane/HeadsetControlPane.qml
```

UI controls should bind to capability properties so unsupported headset functions do not appear as broken controls. Prefer this pattern:

1. Read support from `HeadsetControlBridge.has...Capability`.
2. Read the saved value from `UserSettings`.
3. On user interaction, save through `UserSettings` and apply through `HeadsetControlBridge`.
4. Let bridge signals refresh UI state after polling.

## Polling Lifecycle

The monitor starts only when enabled. On start it:

1. Applies test-device configuration if test mode is enabled.
2. Starts the fetch timer.
3. Immediately fetches headset information.
4. Emits monitoring-state and data-change signals.

On stop it:

1. Stops the timer.
2. Clears cached devices and headset handles.
3. Resets capability flags and device state.
4. Emits state-change signals so QML clears stale UI.

## Battery and ChatMix

Battery state is normalized for QML as:

- `BATTERY_AVAILABLE`
- `BATTERY_CHARGING`
- `BATTERY_UNAVAILABLE`

Battery level uses `-1` when unavailable.

ChatMix uses `-1` when unavailable and otherwise follows the HeadsetControl value range. Application-side ChatMix behavior is coordinated with `AudioBridge`, which applies volume changes to configured communication apps and restores original volumes when needed.

## Error Handling

HeadsetControl API calls return result objects. App code should:

- validate capability before writing;
- validate that a headset is connected;
- clamp user-provided values to supported ranges;
- log failure details from `result.error().fullMessage()`;
- avoid throwing exceptions through QML.

This keeps unsupported devices and transient USB failures visible in diagnostics without crashing the tray app.

## Updating the Vendored Dependency

The GitHub build workflow updates the HeadsetControl submodule to the latest upstream master during CI. For local development, update deliberately:

```pwsh
git submodule update --init --recursive --remote --checkout dependencies/headsetcontrol
```

After updating:

1. Build QontrolPanel in Release.
2. Check that `headsetcontrol_lib` still exports the API used by `HeadsetControlMonitor`.
3. Confirm `hidapi.dll` is still deployed.
4. Exercise the HeadsetControl settings pane.
5. Test at least one real supported headset when changing protocol behavior.

## Where Changes Should Go

| Change type | Preferred location |
| --- | --- |
| New headset protocol or vendor quirk | `dependencies/headsetcontrol/lib/devices/` |
| New HeadsetControl capability mapping | `HeadsetControlMonitor` |
| New QML property or command | `HeadsetControlBridge` and `HeadsetControlMonitor` |
| New user setting | `UserSettings` and `HeadsetControlPane.qml` |
| UI-only layout or labels | `qml/SettingsPane/HeadsetControlPane.qml` and translations |
| Packaging/runtime dependency fix | Root `CMakeLists.txt` |

Keep app-side code focused on orchestration and presentation; keep device protocol details in HeadsetControl.
