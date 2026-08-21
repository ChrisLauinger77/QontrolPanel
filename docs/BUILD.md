# Build Guide

QontrolPanel is a Windows desktop application built with Qt 6, CMake, MSVC, vcpkg, hidapi, and a vendored HeadsetControl library.

## Requirements

- Windows 10 or newer.
- Git for Windows.
- Visual Studio 2022 with the `Desktop development with C++` workload and ATL for the v143 toolset.
- CMake 3.30 or newer.
- NuGet CLI (included with Visual Studio) for restoring Windows App SDK components.
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
C:\vcpkg\bootstrap-vcpkg.bat
```

Install native packages:

```pwsh
C:\vcpkg\vcpkg install hidapi:x64-windows getopt-win32:x64-windows
```

Restore the Windows App SDK and C++/WinRT packages:

```pwsh
nuget restore packages.config -PackagesDirectory build/packages -NonInteractive
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

The build produces the `QontrolPanel` executable and also updates Qt translation source files through the `update_translations` dependency.

## Run Locally

During development, the easiest path is to run from Qt Creator with the configured kit.

If running manually, close any already-running QontrolPanel instance first. The application enforces a single instance through a local server named `QontrolPanel`; starting another instance sends `show_panel` to the existing process and exits.

## Install and Deploy Runtime Files

```pwsh
cmake --install build --config Release
```

The install step copies the executable, Qt runtime dependencies, QML dependencies, translations, `hidapi.dll` when CMake can find it in the vcpkg installation, and the Windows App SDK bootstrap DLL.

QontrolPanel uses the framework-dependent Windows App SDK 2.4 runtime. The release installer installs the matching runtime automatically. For a manually deployed build or ZIP artifact, install the Microsoft x64 Windows App Runtime 2.4 before launching the app. If the runtime is unavailable, QontrolPanel continues with the less configurable DWM backdrop fallback.

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

### Windows App SDK package not found

Restore `packages.config` into the build directory before configuring:

```pwsh
nuget restore packages.config -PackagesDirectory build/packages -NonInteractive
```

### The app starts but immediately exits

Another QontrolPanel instance is probably running. Close the tray app or kill the existing process before launching the development build.

### Runtime cannot find Qt/QML dependencies

Run the install step. The project uses Qt deployment helpers during install; running directly from the raw build output may miss runtime files depending on the environment.

## CI Build

The main build workflow:

1. Checks out the repository with submodules.
2. Updates the HeadsetControl submodule to the latest upstream master for the workflow run.
3. Extracts the app version from `CMakeLists.txt`.
4. Selects the Visual Studio 2026 generator, installs ATL for the MSVC `v143` compatibility toolset, and sets up vcpkg with the same toolset.
5. Restores the Windows App SDK packages and installs Qt.
6. Configures, builds, and installs Release.
7. Cleans changed translation files.
8. Uploads a ZIP artifact.
9. Downloads the Windows App Runtime redistributable and builds an Inno Setup installer that installs it.

Local builds should usually use the checked-out submodule revision unless you are intentionally validating an upstream HeadsetControl update.
