// StereoEnhancer.h - Mid/Side stereo widening + light ambience. Drives the
// "Surround" and "Ambience" controls. Mono-safe (collapses cleanly).
#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

namespace wab::dsp {

class StereoEnhancer {
public:
    void prepare(double sampleRate) noexcept {
        sr_ = sampleRate > 0 ? sampleRate : 48000.0;
        const int n = std::max<int>(1, static_cast<int>(0.018 * sr_)); // up to 18 ms
        ambBufL_.assign(n, 0.0f);
        ambBufR_.assign(n, 0.0f);
        ambLen_ = n; ambPos_ = 0;
    }

    void setWidth(float surround) noexcept { width_ = 1.0f + std::clamp(surround,0.0f,1.0f) * 1.2f; }
    void setAmbience(float amb) noexcept { ambMix_ = std::clamp(amb,0.0f,1.0f) * 0.35f; }

    // Process one stereo frame in place.
    // Includes mono-compatibility protection: if L≈R (common on laptop speakers),
    // width is automatically reduced to prevent comb filtering when the signal
    // is later collapsed to mono (e.g. Bluetooth, single-speaker playback).
    inline void processStereo(float& l, float& r) noexcept {
        // Mono-compatibility: measure how correlated the channels are.
        // monoCorr ≈ 1.0 for identical L/R (pure mono), ≈ 0.0 for decorrelated.
        const float sumAbs = std::fabs(l) + std::fabs(r) + 1e-9f;
        const float monoCorr = std::fabs(l + r) / sumAbs;
        // Reduce width when content is very mono (prevents destructive comb filter).
        const float safeWidth = width_ * (0.4f + 0.6f * (1.0f - monoCorr * monoCorr));

        // Mid/Side widening with mono-safe width.
        const float mid  = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * safeWidth;
        float outL = mid + side;
        float outR = mid - side;

        // Cross-fed delayed ambience (cheap Haas-style spaciousness).
        if (ambMix_ > 0.0001f) {
            const int readPos = ambPos_;
            const float dl = ambBufL_[readPos];
            const float dr = ambBufR_[readPos];
            ambBufL_[ambPos_] = outL;
            ambBufR_[ambPos_] = outR;
            if (++ambPos_ >= ambLen_) ambPos_ = 0;
            outL += ambMix_ * dr;
            outR += ambMix_ * dl;
        }
        l = outL; r = outR;
    }

    void reset() noexcept {
        std::fill(ambBufL_.begin(), ambBufL_.end(), 0.0f);
        std::fill(ambBufR_.begin(), ambBufR_.end(), 0.0f);
        ambPos_ = 0;
    }

private:
    double sr_ = 48000.0;
    float  width_ = 1.0f;
    float  ambMix_ = 0.0f;
    std::vector<float> ambBufL_, ambBufR_;
    int ambLen_ = 1, ambPos_ = 0;
};

} // namespace wab::dsp
