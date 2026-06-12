// StatusBlock.h - Reverse IPC channel: the APO writes processing status
// (heartbeat + audio levels) to a shared file that the Electron UI reads.
// This proves the engine is actually loaded and processing audio, replacing
// the unreliable tasklist-based detection. Also enables a real VU meter.
//
// File:  %ProgramData%\Sonara\status.bin
// Write: APO (inside audiodg.exe) — every ~100ms during APOProcess()
// Read:  Electron main process — every ~200ms via statusBridge.cjs
#pragma once
#include <cstdint>

namespace wab {

static constexpr uint32_t kStatusMagic = 0x57414253; // 'WABS'

#pragma pack(push, 4)
struct StatusBlock {
    uint32_t magic        = kStatusMagic;
    uint32_t seq          = 0;            // bumped on every write
    uint64_t heartbeatMs  = 0;            // GetTickCount64() at last write
    float    rmsLeft      = 0.0f;         // output RMS (0.0 .. ~1.0+)
    float    rmsRight     = 0.0f;
    float    peakLeft     = 0.0f;         // output peak (0.0 .. ~1.0+)
    float    peakRight    = 0.0f;
    uint32_t sampleRate   = 0;            // negotiated sample rate
    uint32_t channels     = 0;            // negotiated channel count
    char     activeDevice[128] = {};      // active default playback device friendly name (UTF-8)
    float    rawSamples[256] = {};        // latest 256 contiguous audio samples (mono/left)
    uint32_t reserved[6]  = {};           // future use, keeps ABI stable
};
#pragma pack(pop)

} // namespace wab
