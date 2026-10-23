# HeadsetControl Integration

QontrolPanel integrates HeadsetControl as an in-tree CMake dependency. This lets the app show headset battery state and expose vendor-specific controls without shelling out to the HeadsetControl CLI.

## Build Integration

The root `CMakeLists.txt` expects HeadsetControl at:

```text
dependencies/headsetcontrol
```

Upstream writes `lib/version.h` into its source directory. QontrolPanel copies the pinned source into the build directory first, preserving the submodule unchanged, and adds the copy with:

```cmake
add_subdirectory("${HEADSETCONTROL_BUILD_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/headsetcontrol" EXCLUDE_FROM_ALL)
```

The app links:

```cmake
headsetcontrol_lib
hidapi::hidapi
```

On MSVC, the project aligns the HeadsetControl runtime library with the main app:

```cmake
MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
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

- owns the dedicated monitor thread independently of AudioWorker;
- exposes copied `HeadsetControlState` snapshots to QML;
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

`vendor` and `product` use HeadsetControl's UTF-8 vendor and product names, falling back to the hexadecimal USB IDs when unavailable. Read the product name after device queries such as `getBattery()`: opening a dongle can refine it to the paired headset's name.

The state snapshot and bridge expose `displayName` for the tray tooltip, panel footer tooltip, and settings heading. It uses the product name, falling back to the generic driver `deviceName`. The driver name and VID/PID-based matching and settings identity remain unchanged. In test mode, the display name is `HeadsetControl Test device`.

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
3. On user interaction, call the bridge setter, which persists the desired value. The bridge also observes settings changes made elsewhere.
4. Let bridge signals refresh UI state after polling.

## Polling Lifecycle

The monitor starts only when enabled. On start it:

1. Applies test-device configuration if test mode is enabled.
2. Immediately fetches headset information.
3. Schedules the single-shot timer after polling completes, with a minimum 60-second interval.
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

## Sidetone read-back

The device cache, state snapshot, and bridge expose `sidetoneLevel` on the normalised 0–128 scale, or `-1` when unknown. Reachable devices advertising `CAP_SIDETONE_STATUS` are queried during polling, before pending settings are applied. The slider uses the reported level when available and the saved setting otherwise.

On the first successful read for the active headset, or when the read differs from the last applied level, the monitor adopts the device value as both its desired value and its new baseline. It clears sidetone retries and signals the bridge to update `UserSettings`. The bridge suppresses write feedback during adoption; unrelated settings snapshots omit sidetone. A request revision prevents an older adoption from overwriting a newer slider edit. Failed initial reads leave the level unknown and do not restore the default zero; explicit user edits can still write.

A successful slider write updates the reported level from the library's acknowledgement. While a command is pending, the bridge exposes its requested level separately so consecutive wheel edits accumulate. Snapshots carry the processed request revision; only completion of the latest request releases that optimistic value, including on failure. Subsequent polls adopt changes made on the headset or in another application. The baseline resets when monitoring stops or the active headset changes. Devices without read-back retain saved-level restoration and bounded write retries.

The upstream test device reads back a fixed level of `85`. Slider edits still call `setSidetone`; the next fetch returns to `85`, simulating an external change. Reliability tests filter discovery to this synthetic device before any queries or writes, and cover first-read adoption, explicit writes, external changes, reset, and unrelated settings updates. Policy tests additionally cover failed reads, retries, and write-only headsets.

## Error Handling

HeadsetControl API calls return result objects. App code should:

- validate capability before writing;
- validate that a headset is connected;
- clamp user-provided values to supported ranges;
- log failure details from `result.error().fullMessage()`;
- avoid throwing exceptions through QML.

The monitor never reads GUI-owned UserSettings. The bridge sends copied desired values, and the monitor records applied values only after successful writes. Failed settings are retried at most three times per desired value and active headset; a manual refresh resets the retry budget. An explicit sidetone edit also resets its budget, including a same-value retry. Exceptions and result errors become a visible bridge error. Duplicate setters with unchanged values do not duplicate successful HID writes.

The active device key uses VID/PID/name rather than enumeration index. Battery matching to audio endpoints remains model-based (VID/PID). The pinned upstream library deduplicates identical VID/PID devices and resolves HID paths by model, so this app cannot reliably select two physically distinct identical headsets. That requires a HeadsetControl fork/upstream change; it must not be patched in this submodule.

## Updating the Vendored Dependency

The release candidate build uses the recorded submodule revision. The scheduled updater proposes a pin change and dispatches `ci.yml` for validation without artifact uploads. Release candidates are built manually after merging the update. For intentional local dependency evaluation:

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
| New headset protocol or vendor quirk | A HeadsetControl fork and upstream pull request; then a reviewed submodule pin update |
| New HeadsetControl capability mapping | `HeadsetControlMonitor` |
| New QML property or command | `HeadsetControlBridge` and `HeadsetControlMonitor` |
| New user setting | `UserSettings` and `HeadsetControlPane.qml` |
| UI-only layout or labels | `qml/SettingsPane/HeadsetControlPane.qml` and translations |
| Packaging/runtime dependency fix | Root `CMakeLists.txt` |

Keep app-side code focused on orchestration and presentation; keep device protocol details in HeadsetControl.
