# Sonara

*(سونارا) — commercial brand. Internal engine artifacts keep the `BoosterAPO` name (DLL, registry, `%ProgramData%\WinAudioBoosterPro`).*

**A self-contained Windows volume booster & audio enhancer.** Sonara ships its own
system-wide audio engine (an Audio Processing Object, APO) and a polished desktop
app — with **no dependency on Equalizer APO or any other third-party software**.

> Competes head-to-head with FxSound, Letasoft Sound Booster, Boom 3D and DFX.

## Highlights

- 🔊 **Real volume boost** up to +500% via a safe, look-ahead brick-wall limiter (no clipping).
- 🎛️ **10-band graphic EQ** (31 Hz – 16 kHz) with smooth RBJ biquad filters.
- ✨ **Enhancers**: Bass, Clarity, Ambience, Surround widening, and Loudness (dynamics).
- 🧩 **Own engine** — a native APO that processes the system audio stream directly. Nothing else to install.
- 💸 **Free at launch** — the full app is free to grow the user base; paid Pro tiers activate in a later phase (`LAUNCH_FREE` flag).
- 🌐 **Bilingual UI** (English / العربية) with RTL support.
- 🚀 Tray app, global hotkeys, run-on-startup.

## Repository layout

A monorepo with three clearly separated parts:

```
sonara/
├─ engine/            Native Windows audio engine (C++)
│  ├─ src/
│  │  ├─ dsp/         Portable, OS-independent DSP core (header-only)
│  │  ├─ apo/         Windows APO/COM shell that hosts the DSP core
│  │  ├─ dllmain.cpp  COM entry points + self-registration
│  │  └─ BoosterAPO.def
│  ├─ scripts/        PowerShell install/uninstall (register + attach engine)
│  ├─ test/           Portable DSP unit tests (run on any OS)
│  ├─ BoosterAPO.inf  Driver/APO information file
│  └─ CMakeLists.txt  Builds the engine DLL (Win+WDK) and the portable tests
├─ app/               Electron + React desktop app
│  ├─ electron/       Main process: param bridge, licensing, engine install
│  ├─ src/            React UI (control panel, presets, i18n)
│  └─ build/          Packaging resources (icon, NSIS hooks)
├─ docs/              ARCHITECTURE, BUILD, ROADMAP, COMMERCIAL
├─ .github/workflows/ CI (DSP tests → engine → installer)
├─ README.md
└─ LICENSE.md
```

The two halves communicate through a tiny memory-mapped parameter file
(`params.bin`); see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Quick start

```bash
# DSP tests (any OS with a C++20 compiler)
g++ -std=c++20 -O2 engine/test/dsp_test.cpp -o dsp_test && ./dsp_test
```

```powershell
# Engine DLL (Windows + Windows Driver Kit)
cmake -S engine -B build -A x64
cmake --build build --config Release

# App + installer
cd app && npm ci && npm run dist   # -> app/release/Sonara Setup.exe
```

Full instructions: [docs/BUILD.md](docs/BUILD.md).

## Status

- **DSP core**: implemented and unit-tested (7/7 passing).
- **Windows APO + installer**: implemented; must be compiled with the WDK and
  **code-signed** before distribution (Windows requires a signed APO to load).

See [docs/ROADMAP.md](docs/ROADMAP.md) and [docs/COMMERCIAL.md](docs/COMMERCIAL.md).

## License

Proprietary / commercial. © Sonara. See [LICENSE.md](LICENSE.md).
