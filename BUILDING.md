## Building QontrolPanel from sources

### Prerequistes
- Git for Windows (for commit extraction at build time)
- Microsoft Visual Studio Community 2022
- Latest stable Qt
- vcpkg with `hidapi` and `getopt-win32`
- Python 3.11 or newer (for the tests that CMake enables by default)

See [docs/BUILD.md](docs/BUILD.md) for the complete build guide, including vcpkg setup and tests.

### Git for windows

Download [Git for windows](https://git-scm.com/downloads/win).  
If you plan on using git to push your changes, you will have to configure it properly for your needs.  
If used only for build time commit extraction, we do not care. You can just install it with default configuration.

### MSVC 2022

Download [MSVC 2022](https://visualstudio.microsoft.com/fr/thank-you-downloading-visual-studio/?sku=Community&channel=Release&version=VS2022&source=VSLandingPage&cid=2030&passive=false)

Tick `Desktop development with C++`

Tick the following:

![image](.assets/msvc_setup.png)

Wait for installation to complete and close visual studio editor.

### Qt

Download [Qt](https://www.qt.io/download-qt-installer-oss?hsLang=en). You will need an account.  
Fill in yout account information and tick `I'm an individual...`.

Choose custom installation.  
We need the following:
- In Qt, expand last stable one (No beta, prerelease etc) and tick MSVC 2022 64 bit.
- Under build tools, make sure Cmake and Ninja are checked
- Under QtCreator, tick QtCreator

You can then start the install

### Sources and dependencies

Clone the repository with submodules and install the vcpkg packages as described in [docs/BUILD.md](docs/BUILD.md#clone).

### Compiling the project

Open QtCreator, open project, and load QontrolPanel CMakeLists.txt.  
In the kit configuration, you can just leave it to default and click on configure project.

![image](.assets/qt_kit.png)

In QtCrator, bottom left, you can click the Run button.

![image](.assets/build_run.png)

Be sure to have closed any previous instance of QontrolPanel, or the new one will refuse to open.

Subsequant builds will be much faster than the initial one.
