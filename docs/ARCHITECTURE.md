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
- conditionally starts the media session manager;
- creates the `QQmlApplicationEngine`;
- loads the `ChrisLauinger77.QontrolPanel` QML module and `Main.qml`;
- registers a tray icon image provider;
- listens for single-instance messages through `QLocalServer`;
- installs a Windows foreground-window event hook while the panel is visible so the panel can hide when focus moves elsewhere.

The UI is QML-first. Native behavior enters QML through C++ classes marked with `QML_ELEMENT` and, for app-wide state, `QML_SINGLETON`.

## Main Native Components

### Panel and Application Lifecycle

- `PanelEngine` owns the QML engine and top-level panel window.
- `SystemTray.qml` and related tray icon support expose the app as a tray utility.
- `StartupShortcutBridge` manages startup shortcut behavior.
- `LanguageBridge` applies the selected Qt translation and asks the QML engine to retranslate.
- `LogManager` centralizes log messages for diagnostics and the debug pane.

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

The audio UI consumes Qt models declared in `audiomodels.*` rather than directly polling Windows APIs.

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

`PowerBridge` exposes shutdown, restart, sleep, hibernate, lock, sign out, switch account, and restart-to-UEFI actions. It also checks support state for sleep, hibernate, UEFI, and multiple-user switching. Power operations touch privileged Windows APIs, so UI flows should keep confirmation behavior explicit and respect `UserSettings`.

### Global Shortcuts

`KeyboardShortcutManager` is both a QObject and a native event filter. It registers Windows hotkeys for panel toggling, ChatMix, microphone mute, and per-application volume controls. Hotkey IDs are stable for built-in shortcuts and allocated from `APP_HOTKEY_BASE_ID` for application-specific shortcuts.

## HeadsetControl Integration

The project builds the vendored HeadsetControl library from `dependencies/headsetcontrol` with `add_subdirectory`. QontrolPanel links to `headsetcontrol_lib` and `hidapi::hidapi`.

The integration has two app-side layers:

- `HeadsetControlMonitor` owns polling, capability detection, cached headset state, and write operations such as sidetone, lights, equalizer preset, inactive time, voice prompts, and rotate-to-mute.
- `HeadsetControlBridge` exposes monitor state and commands to QML as a singleton and handles low-battery notification state.

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

- top-level surfaces: `Main.qml`, `SettingsWindow.qml`, `MediaOverlay.qml`, `PowerMenu.qml`;
- settings panes under `qml/SettingsPane/`;
- shared controls under `qml/Common/`;
- QML singletons under `qml/Singletons/`.

QML should call C++ through bridge methods and properties rather than duplicating native logic.

## Build and Deployment

The app targets Windows with MSVC 2022 and Qt 6. The CMake build:

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
- Treat HeadsetControl as an upstream dependency: put headset-specific protocol work in the vendored library unless QontrolPanel only needs UI or orchestration changes.
- Update translations when changing user-visible strings.
