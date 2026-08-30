# Contributing

Thanks for helping improve QontrolPanel. The project is a Windows-focused Qt application, so good changes usually combine careful C++/Windows behavior with small, clear QML updates.

## Getting Started

1. Clone the repository with submodules.
2. Install the Windows build requirements from `docs/BUILD.md`.
3. Build and run the app before starting larger changes.
4. Open an issue or discussion first for large UX changes, protocol changes, or release workflow changes.

```pwsh
git clone --recursive https://github.com/ChrisLauinger77/QontrolPanel.git
cd QontrolPanel
```

If you already cloned without submodules:

```pwsh
git submodule update --init --recursive
```

## Development Workflow

Create a topic branch:

```pwsh
git checkout -b feature/short-description
```

Make focused changes. Keep unrelated formatting, generated files, dependency updates, and submodule bumps out of the pull request unless they are part of the work.

Build locally:

```pwsh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Install/deploy runtime files when needed:

```pwsh
cmake --install build --config Release
```

## Pull Request Checklist

Before opening a pull request, check:

- The app builds with CMake.
- The changed feature was manually exercised on Windows.
- New or changed user-visible strings are included in Qt translation sources.
- New settings are persisted and loaded correctly.
- QML changes work at the intended panel and settings sizes.
- Headset changes are tested with test mode and, when possible, a real supported device.
- Display brightness changes are tested with the relevant hardware class: laptop panel, DDC/CI monitor, or both.
- The PR description explains what changed, how it was tested, and any known limitations.

## Style

- Use the existing C++ and QML style in nearby files.
- Prefer clear Qt signal/property flows over manual UI refresh logic.
- Keep QML controls declarative and avoid duplicating C++ business logic.
- Use descriptive log categories and actionable log messages.
- Avoid adding broad abstractions for one-off behavior.
- Keep comments short and useful.

## Translations

QontrolPanel uses Qt Linguist `.ts` files under `i18n/`. The CMake build wires translation generation through project helpers in `cmake/languages.cmake`.

When changing user-visible text:

1. Add or update strings in QML/C++.
2. Run a build so translation sources refresh.
3. Review changed `.ts` files for accidental churn.
4. Do not hand-edit translations unless you are intentionally translating.

Translator-specific notes live in `.github/TRANSLATIONS.md`.

## HeadsetControl Changes

Do not make headset device/protocol changes directly in the `dependencies/headsetcontrol` submodule. Implement them in the [upstream HeadsetControl repository](https://github.com/Sapd/HeadsetControl) and submit them there. QontrolPanel should usually only consume upstream capabilities, expose settings/UI, and update the pinned submodule commit when needed.

For headset-related PRs, explain:

- whether the change is in QontrolPanel, an upstream HeadsetControl pull request, or both;
- which headset model was tested;
- which capabilities were verified;
- whether `hidapi.dll` deployment or CMake linking changed.

See `docs/HEADSETCONTROL_INTEGRATION.md` and `docs/HID_DEVICES.md`.

## Reporting Bugs

Please include:

- QontrolPanel version;
- Windows version;
- install method;
- affected audio device, headset, or monitor model;
- whether the issue reproduces after restarting QontrolPanel;
- relevant log output or screenshots;
- steps to reproduce and expected behavior.

For crashes or startup failures, also mention whether another QontrolPanel instance was already running.

## Release Notes

User-facing changes should be written plainly:

- what changed;
- who benefits;
- any compatibility or hardware notes.

Avoid internal-only implementation details unless they help users understand a behavior change.
