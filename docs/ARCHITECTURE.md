# QontrolPanel Architecture

QontrolPanel is a Windows desktop utility built with Qt 6, C++20, QML, Windows system APIs, and a vendored HeadsetControl library. The application runs primarily as a tray-resident audio panel: a single process owns the QML user interface, exposes native Windows functionality through QML singleton bridges, and keeps long-running system watchers behind C++ managers.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Main Qt/CMake build, QML module registration, translation generation, install/deploy rules. |
| `src/` | C++ implementation files for application startup, bridge objects, Windows integrations, audio, monitor, media, power, updater, and settings behavior. |
| `include/` | Public headers for the C++ classes used by the application. Most QML-facing classes are declared here. |
| `qml/` | Main QML windows, panels, settings panes, common controls, and singleton QML helpers. |
| `resources/` | Qt resource collections for icons, sounds, and Windows resources. |
| `i18n/` | Qt Linguist `.ts` translation source files. |
| `i18n_compiled/` | Compiled translation metadata and generated outputs used by release workflows. |
| `cmake/` | Supporting CMake scripts, currently including language metadata. |
| `dependencies/headsetcontrol/` | Vendored HeadsetControl source used as a CMake subdirectory. |
| `tools/` | Release and installer support files. |
| `.github/workflows/` | CI, translation, release, submodule update, and winget publication workflows. |

## Runtime Shape

`src/main.cpp` creates a `QApplication`, configures application metadata, and enforces a single-instance behavior through a `QLocalSocket` named `QontrolPanel`. If another instance already exists, the new process sends `show_panel` to the existing process and exits.

The first process creates `PanelEngine`, which:

- initializes persistent user settings;
- prepares the media service; its bridge controls monitoring from saved preferences;
- creates the `QQmlApplicationEngine`;
- loads the `ChrisLauinger77.QontrolPanel` QML module and `Main.qml`;
- registers a tray icon image provider;
- listens for single-instance messages through `QLocalServer`;
- installs a Windows foreground-window event hook while the panel is visible so the panel can hide when focus moves outside its grouped native surfaces.

The UI is QML-first. Native behavior enters QML through C++ classes marked with `QML_ELEMENT` and, for app-wide state, `QML_SINGLETON`.

## Main Native Components

### Panel and Application Lifecycle

- `PanelEngine` owns the QML engine and coordinates the main controls and media panel windows as one focus group.
- `SystemTray.qml` and related tray icon support expose the app as a tray utility.
- `StartupShortcutBridge` manages startup shortcut behavior.
- `LanguageBridge` applies the selected Qt translation and asks the QML engine to retranslate.
- `LogManager` keeps a mutex-protected 500-entry history and coalesces UI snapshots, including before the debug pane has been opened.

### Settings

`UserSettings` is a QML singleton backed by Qt settings-style persistence. It exposes configuration for panel layout, visible components, language, global shortcuts, ChatMix, HeadsetControl polling, monitor controls, notifications, media overlay, power menu behavior, and slider sensitivity.

When adding a setting, update all relevant layers:

1. Add the property, getter, setter, signal, member, default, load, and save behavior in `UserSettings`.
2. Bind the setting in QML.
3. Wire any bridge or manager behavior that should react to the setting.
4. Add translation strings if the UI exposes labels or help text.

### Audio

`AudioManager` contains the low-level Windows audio session and endpoint work. `AudioBridge` is the QML-facing singleton that exposes:

- output and input volume/mute;
- default input/output device selection;
- application/session volume and mute control;
- grouped application mixer models;
- communication app lists for ChatMix behavior;
- custom names, icons, locks, and background mute state;
- input/output/application audio level monitoring.

Audio runs in a COM MTA. Native callback targets are invalidated before callback unregistration. Session IDs come from Core Audio's session instance identifier. `audiotypes.h` contains portable values; `audiomodels.*` exposes them to QML and preserves model indexes when row identity is unchanged. `nativeimage.*` provides shared QImage conversion for audio and media workers.

`AudioBridge` still owns grouping and ChatMix policy, while `JsonStore` handles bounded, schema-checked reads and atomic JSON writes. Background mute keys are case-folded and only mute changes owned by this feature are restored.

### Media Sessions

`MediaSessionManager` handles Windows media session monitoring and transport control. `MediaSessionBridge` exposes title, artist, art, playback state, and play/pause/next/previous commands to QML. Media monitoring is gated by user settings because it uses Windows media-session APIs and may not be needed by every user.

### Display and Brightness

`MonitorManager` is the QML-facing singleton. It delegates work to `MonitorWorker` running in a dedicated `QThread`. `MonitorManagerImpl` contains the Windows-specific implementation for:

- enumerating displays;
- using DDC/CI VCP code `0x10` for external monitor brightness;
- using WMI for laptop/internal display brightness;
- checking and toggling Windows Night Light;
- managing COM and WMI lifetime on the worker thread.

Display work should stay off the UI thread. New display operations should follow the existing async worker pattern.

### Power Actions

`PowerBridge` exposes shutdown, restart, sleep, hibernate, lock, sign out, switch account, and restart-to-UEFI actions. It also checks support state for sleep, hibernate, UEFI, and multiple-user switching. `PowerWorker` performs capability queries and actions off the GUI thread. Capability properties are cached and notify QML when ready. Failed operations are reported through the bridge and tray. Shutdown and restart allow Windows to negotiate application shutdown rather than forcing applications closed; existing confirmation preferences still apply.

### Global Shortcuts

`KeyboardShortcutManager` is both a QObject and a native event filter. It registers Windows hotkeys for panel toggling, ChatMix, microphone mute, and per-application volume controls. Hotkey IDs are stable for built-in shortcuts and allocated from `APP_HOTKEY_BASE_ID` for application-specific shortcuts.

## HeadsetControl Integration

The project builds a disposable copy of the pinned HeadsetControl submodule with `add_subdirectory`; this confines upstream source-directory generation to the build directory. QontrolPanel links to `headsetcontrol_lib` and `hidapi::hidapi`.

The integration has two app-side layers:

- `HeadsetControlMonitor` owns polling, capability detection, cached headset state, and write operations such as sidetone, lights, equalizer preset, inactive time, voice prompts, and rotate-to-mute.
- `HeadsetControlBridge` owns the headset thread independently of audio, sends copied desired settings, receives value snapshots, and handles low-battery notifications. Disabling audio does not stop headset controls.

See `docs/HEADSETCONTROL_INTEGRATION.md` for details.

## QML Module

The executable registers a Qt QML module:

```cmake
qt_add_qml_module(QontrolPanel
    URI ChrisLauinger77.QontrolPanel
    VERSION 1.0
    QML_FILES ...
)
```

Most UI is organized as:

- top-level surfaces: `Main.qml`, `MainMediaWindow.qml`, `SettingsWindow.qml`, `MediaOverlay.qml`, `PowerMenu.qml`;
- settings panes under `qml/SettingsPane/`;
- shared controls under `qml/Common/`;
- QML singletons under `qml/Singletons/`.

QML should call C++ through bridge methods and properties rather than duplicating native logic.

### Windows Backdrop Materials and Settings Chrome

`WindowsBackdrop` owns the native material lifecycle for the main controls panel, its separate media card, `MediaOverlay`, and `ChatMixNotification`. It dynamically enables Windows' acrylic window accent policy on each exact-size Qt window, with a theme-aware translucent luminosity color that keeps the live desktop hue visible behind these non-activating tray surfaces. The material is reapplied when a window becomes visible or the system color scheme changes.

The acrylic path is dispatcher-free and does not require the Windows App Runtime. This avoids coupling Qt's window lifecycle to CoreMessaging. If the compatibility API is unavailable, `WindowsBackdrop` uses the documented DWM transient system backdrop instead.

The settings window is a transparent Qt window whose native caption is removed by `WindowChrome` after HWND creation. On Windows 11 22H2 and newer, Qt Quick uses its OpenGL renderer and requests an alpha buffer before the first window is created; the default Direct3D swapchain is opaque to the DWM backdrop. Older Windows versions retain Qt's default renderer. Deployments include Qt's software OpenGL implementation so systems without a sufficient accelerated OpenGL driver can still render the application. Settings also deliberately avoids Qt's `FramelessWindowHint`, which would otherwise turn that surface into a layered window that cannot expose the DWM material correctly. The window requests the documented DWM main-window system backdrop (Mica) and extends the DWM frame through the client area, matching the material intended for long-lived Windows settings surfaces. `WindowChrome` owns its Windows native event handling and restores the expected move, resize, system-menu, minimize, maximize, title-bar double-click, and Snap Layout behavior. Closing the settings window, including through Alt+F4, hides the persistent QML window instead of destroying its HWND so this native chrome registration remains valid when the window is reopened.

Mica is exposed through the transparent Qt client area when Windows accepts the backdrop request. DWM owns the active-to-inactive material transition; QML does not observe focus to swap colors or reapply the backdrop. High Contrast, DWM composition, the Windows transparency-effects setting, and the native result are checked when the material is enabled; unavailable effects retain the standard opaque `Constants.panelColor` fallback. DWM does not reliably retheme a visible Mica surface after a Qt application color-scheme change, so the settings window uses the correctly themed opaque fallback for the remainder of that visible session and retries Mica after the window is hidden and reopened. Its setting cards use a single translucent layer fill so they remain distinct against either surface.

## Build and Deployment

The app targets Windows with Qt 6 and the MSVC `v143` toolset. CI hosts that toolset on Visual Studio 2026. The CMake build:

- requires CMake 3.30 or newer;
- defaults to the vcpkg toolchain at `c:/vcpkg/scripts/buildsystems/vcpkg.cmake`;
- uses C++20;
- requires Qt Core, Gui, Qml, Quick, Widgets, LinguistTools, and Network;
- requires `hidapi` through vcpkg;
- builds HeadsetControl from the vendored source tree;
- generates version, language, and Windows resource metadata;
- compiles Qt resources and translations;
- installs QML runtime dependencies with `qt_generate_deploy_qml_app_script`.

See `docs/BUILD.md` for local build instructions.

## Data Flow

Typical UI-to-native flow:

1. QML reads a bridge singleton property or calls a `Q_INVOKABLE`.
2. The bridge validates or normalizes input.
3. The bridge calls a manager or worker object.
4. Native code updates cached state.
5. Native code emits Qt signals.
6. QML bindings update automatically.

For slow or blocking Windows APIs, use the existing worker-thread pattern instead of calling directly from QML-triggered UI code.

## Design Principles

- Keep Windows API details in C++ managers and bridges, not in QML.
- Keep QML responsive by using async managers for polling or slow native operations.
- Prefer Qt models for list-like UI data.
- Keep user settings centralized in `UserSettings`.
- Do not edit the HeadsetControl submodule. Protocol changes belong in a fork and an upstream pull request; app orchestration belongs here.
- Update translations when changing user-visible strings.

## Ownership and shutdown

QML singletons use explicit factories. Most bridge objects are engine-owned; settings and logging are intentionally process-owned so workers and bridge destructors can still use them. The headset bridge is application-owned and explicitly shut down by `PanelEngine`. Windows attached surfaces remain tracked through guarded pointers.

Component preferences are observed in C++ so services do not depend on a particular QML settings page being instantiated. Workers receive copied inputs and publish values through queued connections. Audio and display callbacks include generation checks so retired workers cannot overwrite a restarted service's state.

`workerthreads.*` retires workers asynchronously: queued restoration requests run first, cleanup runs on the worker's own thread, then the thread exits and destroys its worker. No running QThread is deleted or force-terminated. `PanelEngine` destroys the QML engine synchronously and allows a combined ten-second deadline for retired workers. If a native driver remains blocked, the process exits without C++ static destruction, preventing global HID/COM teardown racing that driver. This is a failure path, not successful resource cleanup.

See [Reliability changes and validation](RELIABILITY.md) for remaining limitations and the hardware validation matrix.
