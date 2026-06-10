# Roadmap

## v1.0 — Foundation (current)
- [x] Portable DSP core: biquad EQ, bass/clarity, compressor, look-ahead limiter, stereo enhancer.
- [x] Unit tests for the DSP core (transparency, stability, limiter ceiling, multichannel).
- [x] Self-contained Windows APO engine (no Equalizer APO dependency).
- [x] Shared-memory parameter bridge (no native Node addon).
- [x] Electron control app: presets, 10-band EQ, enhancers, visualizer, tray, hotkeys.
- [x] Commercial scaffolding: trial, signed license keys, free/pro tiers.
- [x] NSIS installer + CI.
- [ ] **Code-sign the DLL (EV cert) and validate APO load on Windows 10/11.**
- [ ] Per-output-device selection + per-device profiles.

## v1.1 — Polish & parity
- [ ] Parametric EQ mode (adjustable Q/frequency) in addition to graphic EQ.
- [ ] Spectrum analyzer fed by real engine output (ring buffer back to UI).
- [ ] Auto-gain / loudness normalization (EBU R128-style).
- [ ] More presets + community preset import/export.
- [ ] Crossfeed for headphones; room/speaker virtualization.

## v1.2 — Commercial growth
- [ ] Online activation + subscription management portal.
- [ ] Auto-update (electron-updater) with signed releases.
- [ ] Telemetry (opt-in) + crash reporting.
- [ ] Localization beyond EN/AR.
- [ ] WHQL submission for the driver/APO.

## v2.0 — Differentiation
- [ ] Per-application volume & effect routing.
- [ ] AI-assisted auto-tuning based on content type.
- [ ] Microphone/input enhancement engine (noise suppression, gate).
- [ ] macOS port (CoreAudio AudioServerPlugIn) reusing the same DSP core.
