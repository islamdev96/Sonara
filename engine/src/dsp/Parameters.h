// Parameters.h - Shared parameter block between the UI process and the audio
// engine. Laid out as a fixed, versioned POD so it can be memory-mapped
// (shared memory) and read lock-free by the real-time audio thread.
//
// This is the single source of truth for what the self-contained engine does.
// The Electron UI writes this block; the APO reads it every process callback.
#pragma once
#include <cstdint>

namespace wab::dsp {

static constexpr int    kNumEqBands   = 10;
static constexpr uint32_t kParamMagic = 0x57414250; // 'WABP'
static constexpr uint32_t kParamVersion = 2;

// Center frequencies (Hz) for the 10-band EQ. Matches the UI sliders.
static constexpr double kEqFrequencies[kNumEqBands] = {
    31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
};

#pragma pack(push, 4)
struct Parameters {
    uint32_t magic   = kParamMagic;   // sanity check for shared memory
    uint32_t version = kParamVersion; // layout version
    uint32_t seq     = 0;             // bumped on every UI write (tear detection)

    int32_t  enabled    = 1;          // master bypass (0 = passthrough)
    float    preampDb    = 0.0f;      // master make-up / boost in dB (the "booster")
    float    outputGainDb = 0.0f;     // final trim after limiter

    float    eqGainsDb[kNumEqBands] = {0,0,0,0,0,0,0,0,0,0}; // per-band gain in dB

    float    bass        = 0.0f;      // 0..1 -> low-shelf bass boost amount
    float    clarity     = 0.0f;      // 0..1 -> presence high-shelf
    float    ambience    = 0.0f;      // 0..1 -> subtle room/reverb send
    float    surround    = 0.0f;      // 0..1 -> stereo width / virtualization
    float    dynamic     = 0.0f;      // 0..1 -> compressor amount (loudness)

    int32_t  limiterOn   = 1;         // brickwall limiter to stop clipping
    float    limiterCeilingDb = -1.0f;// output ceiling

    uint32_t reserved[8] = {0,0,0,0,0,0,0,0}; // future use, keeps ABI stable
};
#pragma pack(pop)

} // namespace wab::dsp
