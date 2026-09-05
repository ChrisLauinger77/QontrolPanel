# Build Guide

QontrolPanel is a Windows desktop application built with Qt 6, CMake, MSVC, vcpkg, hidapi, and a vendored HeadsetControl library.

## Requirements

- Windows 10 or newer.
- Git for Windows.
- Visual Studio 2022 with the `Desktop development with C++` workload and ATL for the v143 toolset.
- CMake 3.30 or newer.
- Ninja or the Visual Studio CMake generator.
- Qt 6.9 or newer for MSVC 2022 64-bit.
- vcpkg installed at `C:\vcpkg` or passed explicitly through `CMAKE_TOOLCHAIN_FILE`.
- vcpkg packages:
  - `hidapi:x64-windows`
  - `getopt-win32:x64-windows` for the vendored HeadsetControl build.

The CI workflow uses Qt `6.11.2` on the `windows-2025-vs2026` runner. Visual Studio 2026 hosts the build, while its MSVC 2022-compatible `v143` toolset matches the supported Qt binary kit.

## Clone

Clone with submodules:

```pwsh
git clone --recursive https://github.com/ChrisLauinger77/QontrolPanel.git
cd QontrolPanel
```

If the repository was cloned without submodules:

```pwsh
git submodule update --init --recursive
```

The build fails early if `dependencies/headsetcontrol/CMakeLists.txt` is missing.

## Install Dependencies

Install or update vcpkg:

```pwsh
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
git -C C:\vcpkg checkout --detach (Get-Content cmake/vcpkg-baseline.txt -Raw).Trim()
C:\vcpkg\bootstrap-vcpkg.bat
```

Install native packages:

```pwsh
C:\vcpkg\vcpkg install hidapi:x64-windows getopt-win32:x64-windows
```

Install Qt with the online installer. Select the MSVC 2022 64-bit kit for a stable Qt 6 release. Keep Qt Creator, CMake, and Ninja enabled.

## Configure

From a Visual Studio 2022 developer shell, Qt Creator, or another environment where Qt and MSVC are available:

```pwsh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Useful optional arguments:

```pwsh
-DCMAKE_BUILD_TYPE=Release
-DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
```

When using a multi-config generator such as Visual Studio, pass the build configuration during build and install instead of relying on `CMAKE_BUILD_TYPE`.

## Build

```pwsh
cmake --build build --config Release
```

Normal builds compile existing translations without editing `.ts` sources. Extract new source messages explicitly when needed:

```pwsh
cmake --build build --target update_translations
```

Review those changes and update only `i18n/*.ts`; compiled `.qm` files are generated outputs.

## Run Locally

During development, the easiest path is to run from Qt Creator with the configured kit.

If running manually, close any already-running QontrolPanel instance first. The application enforces a single instance through a local server named `QontrolPanel`; starting another instance sends `show_panel` to the existing process and exits.

## Install and Deploy Runtime Files

```pwsh
cmake --install build --config Release
```

The install step copies the executable, Qt runtime dependencies, QML dependencies, translations, and `hidapi.dll` when CMake can find it in the vcpkg installation. Native acrylic and Mica use Windows APIs already provided by the operating system and do not require the Windows App Runtime.

By default, this project sets:

```cmake
CMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install
```

The installed app will normally be under:

```text
build/install/bin/QontrolPanel.exe
```

## Common Build Issues

### HeadsetControl submodule is missing

Run:

```pwsh
git submodule update --init --recursive
```

### hidapi package not found

Install the vcpkg package and make sure CMake uses the vcpkg toolchain:

```pwsh
C:\vcpkg\vcpkg install hidapi:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Qt package not found

Set `CMAKE_PREFIX_PATH` to the Qt MSVC kit, or configure from Qt Creator:

```pwsh
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
```

### The app starts but immediately exits

Another QontrolPanel instance is probably running. Close the tray app or kill the existing process before launching the development build.

### Runtime cannot find Qt/QML dependencies

Run the install step. The project uses Qt deployment helpers during install; running directly from the raw build output may miss runtime files depending on the environment.

## CI Build

The main build workflow:

1. Checks out the repository with submodules.
2. Verifies the exact recorded HeadsetControl revision and builds a disposable copy, leaving the submodule source untouched.
3. Extracts the app version from `CMakeLists.txt`.
4. Selects the Visual Studio 2026 generator, installs ATL for `v143`, checks out the recorded vcpkg revision, and installs packages into an isolated build directory.
5. Installs Qt.
6. Configures, builds, runs CTest, and installs Release.
7. Checks that dependency and translation sources did not change.
8. Produces ZIP and installer artifacts, plus provenance containing source/dependency revisions, versions, sizes, and SHA-256 digests.

Local builds use the checked-out submodule revision. The scheduled dependency-update workflow proposes a reviewed pin update and explicitly dispatches its validation build.

The Release workflow requires a successful main-branch Build run ID. It checks out that run's source, downloads all artifacts from that one run, verifies provenance and file hashes, and refuses a version tag pointing to different source.

## Reliability tests

```pwsh
ctest --test-dir build -C Release --output-on-failure
```

A hardware-independent subset can also be configured on Linux:

```sh
cmake -S . -B /tmp/qontrol-tests -DQONTROLPANEL_CORE_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/qontrol-tests
ctest --test-dir /tmp/qontrol-tests --output-on-failure
```

Night Light binary-format and release-provenance tests require C++20 and Python 3.11 or newer. Qt Core/Qml/Network/Test development packages enable the logging, storage, model, cancellation, and worker-lifecycle tests as well. Windows release validation requires those Qt tests. These tests do not exercise real power actions or hardware.

The portable ZIP requires a compatible MSVC runtime. The installer checks the installed runtime against the actual bundled redistributable version and installs it when missing or older. Always install/deploy before launching `build/install/bin/QontrolPanel.exe` for manual testing.
