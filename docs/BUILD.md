# Building Sonara

The project has two build targets: the **portable DSP tests** (any OS) and the
**Windows engine + installer** (Windows only).

## Prerequisites

| Target | Needs |
|--------|-------|
| DSP tests | Any C++20 compiler (g++, clang, or MSVC). |
| Engine DLL | Windows 10/11, Visual Studio 2022 (MSVC), **Windows Driver Kit (WDK)**, CMake ≥ 3.20. |
| Desktop app | Node.js ≥ 20, npm. |
| Distribution | An **EV code-signing certificate** (mandatory — Windows refuses to load an unsigned APO). |

## 1. Portable DSP tests (fast, runs in CI)

```bash
g++ -std=c++20 -O2 -Wall engine/test/dsp_test.cpp -o dsp_test && ./dsp_test
```

Or via CMake:

```bash
cmake -S engine -B build-tests -DWAB_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests
```

Expected: `ALL TESTS PASSED` (7/7).

## 2. The native engine (Windows + WDK)

```powershell
cmake -S engine -B build -A x64
cmake --build build --config Release
# -> build/Release/SonaraAPO.dll
```

If CMake warns that `audioenginebaseapo.h` was not found, install the WDK and
ensure `WDKContentRoot` is set in the environment.

### Code signing (required before install)

```powershell
signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 build/Release/SonaraAPO.dll
```

Windows `audiodg.exe` will silently refuse to load an unsigned APO.

## 3. The desktop app + installer

```powershell
cd app
npm ci
npm run dist        # tsc + vite build, then electron-builder (NSIS)
# -> app/release/Sonara Setup <version>.exe
```

`electron-builder` bundles `build/Release/SonaraAPO.dll` and the PowerShell
scripts (see `extraResources` in `app/package.json`) into the installer under
`resources/engine/`.

### Run in development

```powershell
cd app
npm install
npm run electron:start   # Vite dev server + Electron
```

In dev mode the app loads the engine DLL and scripts from the repo's `build/`
and `engine/scripts/` folders.

## 4. Install / uninstall the engine manually

```powershell
# from an elevated shell
engine\scripts\install-engine.ps1 -DllPath build\Release\SonaraAPO.dll
engine\scripts\uninstall-engine.ps1
```

These copy the DLL into `System32`, register the COM/APO server, attach it to
the chosen render endpoint's effect chain, and restart the audio service.

## CI

`.github/workflows/build.yml` runs three jobs: portable DSP tests (Linux) gate
every change; the engine builds on Windows with the WDK; the installer is built
last and uploaded as an artifact.
