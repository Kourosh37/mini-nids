# Build commands by platform

Run commands from the repository root. Core prerequisites: CMake >=3.25, Ninja,
a C++20 compiler and GitHub HTTPS access on the first configure. All builds additionally
requires **Qt 6.8.3** with Core, Gui, Qml, Quick and QuickControls2. Tests also use
Qt Test. LinguistTools and a translation catalog are no longer required. Inter and
JetBrains Mono ship in the repository and are compiled into the GUI resources.

## Windows - MinGW

Install CMake and Ninja on PATH, Qt 6.8.3 and its matching MinGW 13.1 compiler.
The development SDKs and temporary Python tools previously used in this workspace
were removed during cleanup. The following paths are examples; use your installation:

```powershell
.\scripts\build\windows-qt.ps1 `
  -QtRoot C:/Qt/6.8.3/mingw_64 `
  -CompilerRoot C:/Qt/Tools/mingw1310_64 `
  -BuildDirectory build/windows-gui `
  -Configuration Release

.\build\windows-gui\apps\gui\Mini-NIDS.exe
```

The helper adds Qt/compiler DLL folders to PATH for this PowerShell session, configures
Ninja, builds and runs CTest. It defaults to Release; use `-Configuration Debug` for
debug symbols. On a different PC, install CMake/Ninja on PATH and replace QtRoot and
CompilerRoot with that PC's installation, for example `C:/Qt/6.8.3/mingw_64` and
`C:/Qt/Tools/mingw1310_64`. Use **matching MinGW 13.1** for this Qt kit. The installed
GCC 15.2 / Qt MinGW mix failed earlier runtime validation.

Core/CLI only, with a working compiler and CMake/Ninja on PATH:

```powershell
cmake --preset release
cmake --build --preset release --parallel 4
ctest --test-dir build/release --output-on-failure
.\build\release\apps\cli\mininids.exe --help
```

Package into a **new** staging directory after a successful GUI build:

```powershell
.\scripts\package\windows.ps1 `
  -QtRoot C:/Qt/6.8.3/mingw_64 `
  -CompilerRoot C:/Qt/Tools/mingw1310_64 `
  -BuildDirectory build/windows-gui `
  -StageDirectory build/windows-package
```

Install Inno Setup 6.3+ on the builder (`iscc.exe` on PATH, or pass `-Iscc` with its path).
The helper deploys runtime DLLs/QML modules, verifies them without SDK paths and creates
both a portable ZIP and an offline Setup.exe under `output/releases`. In the portable
folder run `Mini-NIDS.exe` at its root. End users need no Qt SDK or compiler installation.
See [release packaging](releases.md) for installer tests, OS baselines and user storage.

## Linux

For Ubuntu 24.04, install build tools if absent:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build
```

Qt must be installed separately with the **6.8.3 desktop gcc_64 kit**. Distribution
Qt packages may be another version and will not satisfy this project's exact pin.
For aqtinstall, Qt 6.8.3's Linux architecture identifier is `linux_gcc_64`; the
installed directory is still named `gcc_64`. Do not pass the directory name as the
aqt architecture.
For a Qt SDK installed under `$HOME/Qt`:

```sh
cmake --preset gui -B build/linux-gui \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64"
cmake --build build/linux-gui --parallel 4
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ctest --test-dir build/linux-gui --output-on-failure
./build/linux-gui/apps/gui/Mini-NIDS
```

Core/CLI only (no Qt required):

```sh
cmake --preset release
cmake --build --preset release --parallel 4
ctest --test-dir build/release --output-on-failure
./build/release/apps/cli/mininids --help
```

The Linux core was tested under Ubuntu WSL with Clang 18 and sanitizers. The Linux
GUI commands are provided for the configured Qt kit but have not been executed locally.
Use a separate build directory if a Windows preset directory exists on a shared disk:
append `-B build/linux-core` at configure and use that path for build/test/run.

## macOS

Install Xcode Command Line Tools, CMake/Ninja and the **Qt 6.8.3 macos kit**. Example
tool setup, when those tools are not already installed:

```sh
xcode-select --install
brew install cmake ninja
```

Then, for Qt installed under `$HOME/Qt`:

```sh
cmake --preset gui -B build/macos-gui \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"
cmake --build build/macos-gui --parallel 4
ctest --test-dir build/macos-gui --output-on-failure
open ./build/macos-gui/apps/gui/Mini-NIDS.app
```

The GUI is configured as a native `.app` bundle. Native packaging/signing/runtime
remain unverified locally and are checked by the release CI. For core only, use the Linux core commands
above with the macOS compiler. Do not treat these commands as executed build evidence.

## Android and iOS

Neither mobile target has a verified build/install workflow in this repository.
Qt SDK/NDK or Xcode kits, native document-provider integration and signing still need
implementation/verification. There is no truthful one-command APK/IPA build to provide
yet; use the desktop builds above.

Qt command-line reference: https://doc.qt.io/qt-6/cmake-get-started.html
Target behavior: https://doc.qt.io/qt-6/qt-add-executable.html

All core builds now link pinned SQLite for exact disk-backed traffic/endpoint counts.
The GUI shares the same target (`cmake/Database.cmake`). Optional Linux live runtime
packaging uses libpcap-dev at build time. Windows offline packages do not redistribute
Npcap; installing its capture driver is a separate live-only operation.
