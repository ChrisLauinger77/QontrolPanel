# Agent Guide

This file gives coding agents the short version of how to work safely in QontrolPanel.

## Project Summary

QontrolPanel is a Windows tray application for audio, media, display, power, shortcut, and headset controls. It is built with Qt 6, C++20, QML, CMake, MSVC, vcpkg, hidapi, and a vendored HeadsetControl library.

Read these docs before larger changes:

- `docs/ARCHITECTURE.md`
- `docs/BUILD.md`
- `docs/HID_DEVICES.md`
- `docs/HEADSETCONTROL_INTEGRATION.md`
- `docs/RELIABILITY.md`

## Important Paths

- `src/`: C++ implementations.
- `include/`: C++ headers and QML-facing singleton declarations.
- `qml/`: QML UI.
- `qml/Common/`: reusable QML controls.
- `qml/SettingsPane/`: settings pages.
- `qml/Singletons/`: QML-only singletons.
- `resources/`: icons, sounds, resource files.
- `i18n/`: Qt translation source files.
- `dependencies/headsetcontrol/`: vendored HeadsetControl dependency.
- `cmake/`: project CMake helpers.
- `tools/`: installer and release support.
- `tests/`: portable and Qt reliability tests.

## Coding Guidance

- Follow existing Qt naming, signal, slot, and singleton patterns.
- Keep Windows API details in C++ managers and bridges.
- Keep QML focused on presentation and binding to bridge properties.
- Do not block the UI thread with device enumeration, monitor brightness work, HID polling, network calls, or slow Windows API calls.
- Prefer adding behavior to an existing bridge/manager when it belongs to that domain.
- Use Qt models for list data shown in QML.
- Update `UserSettings` consistently when adding settings: property, getter, setter, signal, member, defaults, load, save, and QML binding.
- Update translations when adding user-visible strings.
- Keep log messages in a stable untranslated form; do not pass `LOG_*` or `LogManager` message text through `tr()` or `qsTr()`.
- Keep HeadsetControl orchestration and UI work in QontrolPanel. Protocol/device changes require a HeadsetControl fork and upstream pull request; never patch the submodule in place.

## Ownership and Reliability

- Preserve explicit QML singleton factories and ownership. Settings and logging outlive bridge teardown; the application owns the headset bridge and shuts it down explicitly.
- Observe component settings in C++ so service startup, enablement, and shutdown do not depend on a QML settings page being open. Headset monitoring must remain independent of audio enablement.
- Send copied inputs and value snapshots through queued connections, registering custom metatypes as needed. Workers must not read GUI-owned settings directly. Keep worker-owned COM/native resources on that thread, including initialization and release; use QImage rather than QPixmap for worker-side image processing.
- Reuse `workerthreads.*` for asynchronous cleanup and retirement. Preserve generation guards against stale callbacks and wait for retiring audio/display workers before replacing them. Never delete a running QThread, force-terminate it, or introduce blocking queued calls from the UI thread.
- Preserve stable session identities and model indexes on data-only updates. Changing persisted stream-rule or executable identities requires a compatibility migration; see `docs/RELIABILITY.md`.
- Use `JsonStore` for existing JSON policy files, including bounded/schema-checked reads, corrupt-file recovery, and atomic writes. Validate persisted settings and surface save failures.
- Use `replybatch.h` when cancelling updater reply batches: detach replies and disconnect callbacks before aborting, because abort can emit completion synchronously. Keep installer size and SHA-256 verification before launch.

## Build Commands

Typical Windows build:

Use the recorded HeadsetControl revision and `cmake/vcpkg-baseline.txt`; see `docs/BUILD.md` for dependency/toolchain setup. Windows validation requires Python 3.11 or newer and Qt Test in addition to the app's Qt modules.

```pwsh
git submodule update --init --recursive
C:\vcpkg\vcpkg install hidapi:x64-windows getopt-win32:x64-windows
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
cmake --install build --config Release
```

Qt Creator with the MSVC 2022 64-bit kit is also a supported path.

## Validation

When possible, validate changes with:

- a CMake configure;
- a Release build;
- CTest with all three Windows suites passing: `nightlight_data`, `release_provenance`, and `qt_reliability`;
- before manually testing the executable, build the `INSTALL` target and run `build/install/bin/QontrolPanel.exe`; the executable in the raw build directory may not have its required runtime files;
- manual launch from Qt Creator or the installed output;
- affected settings pane interaction;
- real-device validation for audio, HID, display, hotkey, or power behavior.

For the portable subset on Linux:

```sh
cmake -S . -B /tmp/qontrol-tests -DQONTROLPANEL_CORE_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/qontrol-tests
ctest --test-dir /tmp/qontrol-tests --output-on-failure --no-tests=error
```

Without Qt development packages, only the Night Light and provenance suites run locally. Report the tested revision, platform, and actual suites; link the run when using Windows CI. A successful build or CI run does not establish interactive or real-device behavior. Use the validation matrix in `docs/RELIABILITY.md` for affected native features.

For documentation-only changes, spelling, links, and consistency with `CMakeLists.txt` and existing source files are the main checks.

## Risk Areas

- Windows COM/WMI lifetime in monitor code.
- Hotkey registration and native event filtering.
- Audio session/device enumeration.
- Headset HID writes and polling intervals.
- Single-instance behavior through `QLocalServer`.
- Explicit translation extraction and generated-file churn.
- Installer and release workflow assumptions.

## Repository Hygiene

- Do not rewrite unrelated files.
- Respect the line-ending rules in `.gitattributes`: use LF for documentation, configuration, build scripts, and translation sources; use CRLF for C++, QML, and Windows resource files.
- Do not renormalize unrelated files or include line-ending churn in focused changes.
- Never modify files in the HeadsetControl submodule. If a HeadsetControl source change is required, tell the user that it must be implemented in a fork of HeadsetControl and submitted upstream as a pull request. The pinned submodule commit may be updated when required, but always inform the user about the update.
- Keep dependency and generated-file churn out of focused code changes.
- Normal builds must leave the HeadsetControl submodule and translation sources unchanged. CMake builds a disposable HeadsetControl copy because upstream generates files in its source directory. Do not use `git submodule update --remote` during ordinary builds or validation.
- When updating translations, edit only the `.ts` files in `i18n/`; `.qm` files are generated by CI.
- Extract new translation messages explicitly with `cmake --build build --target update_translations`, then review the `.ts` changes; normal builds only compile the existing catalogs. On Linux without a full app configuration, use Qt 6 `lupdate` directly as described in `docs/BUILD.md` under "Extract translation messages on Linux"; the core-tests-only configuration does not create this target.
- Mention any validation that could not be run locally, especially because this app targets Windows.
